#include "Playout.h"
#include "DesignAudio.h"
#include <QDateTime>

#include <QAudioOutput>
#include <QDateTime>
#include <QFile>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>
#include <memory>

Playout::Playout(Library *lib, QObject *parent) : QObject(parent), m_lib(lib) {
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_sink = new QVideoSink(this);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoSink(m_sink);
    m_audio->setVolume(float(m_volume));

    // media that starts part-way in (the second half of a cut program) is seeked once it has loaded
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus st) {
        if ((st == QMediaPlayer::LoadedMedia || st == QMediaPlayer::BufferedMedia) && m_pendingSeek >= 0) {
            const qint64 pos = m_pendingSeek;
            m_pendingSeek = -1;
            if (pos > 0) m_player->setPosition(pos);
        }
    });

    m_clock.start();
    m_tick.setInterval(40);
    connect(&m_tick, &QTimer::timeout, this, &Playout::onTick);
    m_tick.start();

    connect(lib, &Library::programsChanged, this, &Playout::rebuild);
    connect(lib, &Library::adsChanged, this, &Playout::rebuild);
    connect(lib, &Library::elementsChanged, this, &Playout::rebuild);
    connect(lib, &Library::placementsChanged, this, [this] { syncSounds(); });
    connect(lib, &Library::layersChanged, this, [this] { applyAudio(); });
    rebuild();
}

const Segment *Playout::current() const {
    if (m_state == Stopped || m_index < 0 || m_index >= m_segs.size()) return nullptr;
    return &m_segs[m_index];
}

int Playout::currentProgramIndex() const {
    const Segment *s = current();
    return s ? m_lib->programIndex(s->programId) : -1;
}

bool Playout::canBreak() const {
    const Segment *s = current();
    if (!s || s->type != Segment::Program || m_index + 1 >= m_segs.size()) return false;
    const Segment &n = m_segs[m_index + 1];
    return n.type != Segment::Program && n.programId == s->programId;
}

qint64 Playout::breakCountdownMs() const {
    const Segment *s = current();
    if (!canBreak()) return -1;
    return qMax<qint64>(0, s->ms - m_segElapsed);
}

bool Playout::continues(int i) const {
    if (i < 0 || i + 1 >= m_segs.size()) return false;
    const Segment &a = m_segs[i], &b = m_segs[i + 1];
    return a.type == Segment::Program && b.type == Segment::Program && !a.spot && !b.spot && !a.file.isEmpty() &&
           a.file == b.file && b.offsetMs == a.offsetMs + a.ms;
}

qint64 Playout::nextProgramCountdownMs() const {
    if (!isLive() || m_index < 0) return -1;
    for (int j = m_index + 1; j < m_segs.size(); ++j) {
        if (m_segs[j].type != Segment::Program) continue;
        if (continues(j - 1)) continue;  // the other half of a cut: the programme is still the same one
        return qMax<qint64>(0, m_starts[j] - timelinePosMs());
    }
    return -1;
}

qint64 Playout::programEndCountdownMs() const {
    const Segment *s = current();
    if (!s || s->type != Segment::Program || s->spot) return -1;
    int j = m_index;
    while (continues(j)) ++j;
    return qMax<qint64>(0, m_starts[j] + m_segs[j].ms - timelinePosMs());
}

qint64 Playout::remainingMs() const {
    return qMax<qint64>(0, m_totalMs - (isLive() ? timelinePosMs() : m_cueMs));
}

int Playout::groupFirst(int segment) const {
    if (segment < 0 || segment >= m_segs.size()) return -1;
    int i = segment;
    while (i > 0 && m_segs[i - 1].programId == m_segs[segment].programId) --i;
    return i;
}

int Playout::groupLast(int segment) const {
    if (segment < 0 || segment >= m_segs.size()) return -1;
    int i = segment;
    while (i + 1 < m_segs.size() && m_segs[i + 1].programId == m_segs[segment].programId) ++i;
    return i;
}

// ---------------------------------------------------------------- geometry

int Playout::blockWidth(const Segment &s) {
    const int sec = s.ms / 1000;
    return s.type == Segment::Program ? qBound(140, int(sec * 1.2), 720)
         : s.type == Segment::Ad      ? qBound(90, sec * 3, 240)
                                      : 60;
}

int Playout::segmentAtMs(qint64 ms, qint64 *offset) const {
    if (m_segs.isEmpty()) {
        if (offset) *offset = 0;
        return -1;
    }
    ms = qBound<qint64>(0, ms, qMax<qint64>(0, m_totalMs - 1));
    int idx = 0;
    for (int k = 0; k < m_segs.size(); ++k) {
        if (m_starts[k] <= ms) idx = k;
        else break;
    }
    if (offset) *offset = ms - m_starts[idx];
    return idx;
}

qint64 Playout::timelinePosMs() const {
    const int i = index();
    if (i < 0 || i >= m_starts.size()) return 0;
    return m_starts[i] + m_segElapsed;
}

qint64 Playout::groupEndMs(const QString &programId) const {
    for (int i = m_segs.size() - 1; i >= 0; --i)
        if (m_segs[i].programId == programId) return m_starts[i] + m_segs[i].ms;
    return m_totalMs;
}

qint64 Playout::programStartMs(const QString &programId) const {
    for (int i = 0; i < m_segs.size(); ++i)
        if (m_segs[i].programId == programId) return m_starts[i];
    return 0;
}

double Playout::unitAt(int segment, bool end) const {
    if (m_unitStarts.size() < 2) return 0.0;
    const double total = m_unitStarts.last();
    if (total <= 0) return 0.0;
    const int i = qBound(0, segment + (end ? 1 : 0), int(m_unitStarts.size()) - 1);
    return m_unitStarts[i] / total;
}

double Playout::msToUnit(qint64 ms) const {
    if (m_segs.isEmpty() || m_unitStarts.size() != m_segs.size() + 1) return 0.0;
    const double total = m_unitStarts.last();
    if (total <= 0) return 0.0;
    if (ms <= 0) return 0.0;
    for (int i = 0; i < m_segs.size(); ++i) {
        if (ms < m_starts[i] + m_segs[i].ms) {
            const double f = double(ms - m_starts[i]) / m_segs[i].ms;
            return (m_unitStarts[i] + (m_unitStarts[i + 1] - m_unitStarts[i]) * f) / total;
        }
    }
    return 1.0;
}

qint64 Playout::unitToMs(double unit) const {
    if (m_segs.isEmpty() || m_unitStarts.size() != m_segs.size() + 1) return 0;
    const double total = m_unitStarts.last();
    const double t = qBound(0.0, unit, 1.0) * total;
    for (int i = 0; i < m_segs.size(); ++i) {
        if (t <= m_unitStarts[i + 1]) {
            const double w = m_unitStarts[i + 1] - m_unitStarts[i];
            const double f = w > 0 ? (t - m_unitStarts[i]) / w : 0.0;
            return m_starts[i] + qint64(m_segs[i].ms * qBound(0.0, f, 1.0));
        }
    }
    return m_totalMs;
}

// Build the timeline: Program -> [Break In] -> Ad -> [Break Out] -> next Program
// (a standalone ad spot is simply one Ad segment)
void Playout::rebuild() {
    QString keyId;
    Segment::Type keyType = Segment::Program;
    int keyPart = 0;
    const bool had = isLive() && m_index >= 0 && m_index < m_segs.size();
    if (had) {
        keyId = m_segs[m_index].programId;
        keyType = m_segs[m_index].type;
        keyPart = m_segs[m_index].part;
    }

    m_segs.clear();
    const auto &progs = m_lib->programs;
    for (int i = 0; i < progs.size(); ++i) {
        const ProgramItem &p = progs[i];

        if (p.isAdSpot()) {
            const Ad *spotAd = m_lib->ad(p.adId);
            Segment a;
            a.type = Segment::Ad;
            a.spot = true;
            a.programId = p.id;
            a.title = spotAd ? spotAd->name : p.title;
            a.kind = "Ad";
            a.file = spotAd ? spotAd->file : QString();
            a.ms = (spotAd ? spotAd->seconds : p.seconds) * 1000;
            a.fx = spotAd ? spotAd->fx : Transition();
            m_segs << a;
            continue;
        }

        Segment s;
        s.type = Segment::Program;
        s.programId = p.id;
        s.title = p.title;
        s.kind = p.kind;
        s.file = p.file;
        s.offsetMs = p.offsetMs;
        s.ms = p.seconds * 1000;
        s.fx = p.fx;
        m_segs << s;

        const Ad *ad = m_lib->ad(p.adId);
        if (!ad) continue;
        const QString next = i + 1 < progs.size() ? progs[i + 1].title : QString("Cuebase");
        auto addBreak = [&](const QString &elementId, int part) {
            const Element *e = m_lib->element(elementId);
            if (!e || e->type != "Break") return;
            Segment b;
            b.type = Segment::Break;
            b.part = part;
            b.programId = p.id;
            b.title = e->name;
            b.color = e->color;
            b.ms = qMax(1, e->seconds) * 1000;
            b.text = e->text;
            b.text.replace("{title}", p.title).replace("{next}", next).replace("{ad}", ad->name);
            m_segs << b;
        };
        addBreak(p.breakIn, 0);
        Segment a;
        a.type = Segment::Ad;
        a.programId = p.id;
        a.title = ad->name;
        a.kind = "Ad";
        a.file = ad->file;
        a.ms = ad->seconds * 1000;
        a.fx = ad->fx;
        m_segs << a;
        addBreak(p.breakOut, 1);
    }

    // start times + width weights (before anything can query them)
    m_starts.clear();
    m_unitStarts.clear();
    m_totalMs = 0;
    double w = 0;
    for (const Segment &s : std::as_const(m_segs)) {
        m_starts << m_totalMs;
        m_unitStarts << w;
        m_totalMs += s.ms;
        w += blockWidth(s);
    }
    m_unitStarts << w;
    m_cueMs = qBound<qint64>(0, m_cueMs, qMax<qint64>(0, m_totalMs - 1));

    if (had) {
        int found = -1;
        for (int i = 0; i < m_segs.size(); ++i)
            if (m_segs[i].programId == keyId && m_segs[i].type == keyType && m_segs[i].part == keyPart) found = i;
        if (found >= 0) m_index = found;
        else stop();
    }
    emit segmentsChanged();
}

// ------------------------------------------------------------------ media

void Playout::startMedia(const Segment &s, qint64 inSegmentMs) {
    if (!s.file.isEmpty() && QFile::exists(s.file)) {
        const QUrl url = QUrl::fromLocalFile(s.file);
        const qint64 pos = s.offsetMs + inSegmentMs;
        if (m_player->source() == url) {
            // same file as before (e.g. the second half of a cut): just move within it
            m_pendingSeek = -1;
            m_player->setPosition(pos);
            m_player->play();
        } else {
            m_pendingSeek = pos;
            m_player->setSource(url);
            m_player->play();
        }
    } else {
        m_pendingSeek = -1;
        m_player->stop();
        m_player->setSource(QUrl());
        m_sink->setVideoFrame(QVideoFrame());
    }
}

void Playout::enter(qint64 inSegmentMs) {
    m_segElapsed = inSegmentMs;
    if (m_state == Playing) startMedia(m_segs[m_index], inSegmentMs);
    emit segmentChanged();
}

void Playout::markFinished(const QString &programId) {
    if (m_finished.value(programId, 0) > 0) return;
    m_finished[programId] = QDateTime::currentMSecsSinceEpoch();
    emit programFinished(programId);
}

void Playout::start(int segment, qint64 inSegmentMs) {
    if (m_segs.isEmpty()) return;
    if (m_state == Stopped) {
        m_finished.clear();
        m_total = 0;
        m_state = Playing;
        m_index = -1;
        emit stateChanged();
    }
    jumpTo(segment, inSegmentMs);
}

void Playout::jumpTo(int segment, qint64 inSegmentMs) {
    if (segment < 0 || segment >= m_segs.size()) return;
    if (m_state == Stopped) {
        start(segment, inSegmentMs);
        return;
    }
    // everything before the target program counts as played, everything from it on is queued again
    const int target = m_lib->programIndex(m_segs[segment].programId);
    for (int i = 0; i < m_lib->programs.size(); ++i) {
        const QString &pid = m_lib->programs[i].id;
        if (i < target) markFinished(pid);
        else m_finished.remove(pid);
    }
    const bool wasPaused = m_state == Paused;
    m_state = Playing;
    m_index = segment;
    m_total = m_starts[segment] + inSegmentMs;
    resetSounds();
    enter(qBound<qint64>(0, inSegmentMs, qint64(m_segs[segment].ms) - 1));
    syncSounds();
    if (wasPaused) emit stateChanged();
}

void Playout::seekTo(qint64 timelineMs) {
    if (m_segs.isEmpty()) return;
    qint64 off = 0;
    const int i = segmentAtMs(timelineMs, &off);
    if (i < 0) return;
    if (m_state == Stopped) {  // off air nothing plays: just remember where Start Broadcast should begin
        m_cueMs = m_starts[i] + off;
        emit ticked();
    } else {
        jumpTo(i, off);
    }
}

void Playout::startFromCue() {
    if (m_state != Stopped || m_segs.isEmpty()) return;
    qint64 off = 0;
    const int i = segmentAtMs(m_cueMs, &off);
    start(qMax(0, i), off);
}

void Playout::playProgram(const QString &programId) {
    for (int i = 0; i < m_segs.size(); ++i)
        if ((m_segs[i].type == Segment::Program || m_segs[i].spot) && m_segs[i].programId == programId) {
            jumpTo(i);
            return;
        }
}

void Playout::stop() {
    if (m_state == Stopped) return;
    m_state = Stopped;
    m_index = -1;
    m_finished.clear();
    m_pendingSeek = -1;
    m_player->stop();
    m_player->setSource(QUrl());
    m_sink->setVideoFrame(QVideoFrame());
    syncSounds();
    emit segmentChanged();
    emit stateChanged();
}

void Playout::finishAll() {
    m_state = Stopped;
    m_index = -1;
    m_pendingSeek = -1;
    m_player->stop();
    m_player->setSource(QUrl());
    m_sink->setVideoFrame(QVideoFrame());
    syncSounds();
    emit segmentChanged();
    emit stateChanged();
}

void Playout::togglePause() {
    if (m_state == Playing) {
        m_state = Paused;
        m_player->pause();
    } else if (m_state == Paused) {
        m_state = Playing;
        if (m_player->source().isValid()) m_player->play();
    } else {
        return;
    }
    syncSounds();
    emit stateChanged();
}

void Playout::advance() {
    const Segment &cur = m_segs[m_index];
    if (cur.type == Segment::Program || cur.spot) markFinished(cur.programId);
    if (m_index + 1 < m_segs.size()) {
        if (continues(m_index)) {
            // a cut with nothing placed in between: the media just keeps playing, nothing is reloaded or re-seeked
            const qint64 over = qBound<qint64>(0, m_segElapsed - cur.ms, qint64(200));
            ++m_index;
            m_segElapsed = qMin<qint64>(over, m_segs[m_index].ms - 1);
            emit segmentChanged();
        } else {
            ++m_index;
            enter(0);
        }
    } else {
        finishAll();
    }
}

void Playout::skip() {
    if (!isLive()) return;
    markFinished(m_segs[m_index].programId);
    for (int i = m_index + 1; i < m_segs.size(); ++i)
        if (m_segs[i].type == Segment::Program) {
            jumpTo(i);
            return;
        }
    finishAll();
}

void Playout::breakNow() {
    if (canBreak()) jumpTo(m_index + 1);
}

void Playout::onTick() {
    const qint64 now = m_clock.elapsed();
    const qint64 dt = qMin<qint64>(now - m_lastTick, 500);
    m_lastTick = now;
    if (m_state != Playing || m_index < 0) return;
    m_segElapsed += dt;
    m_total += dt;
    if (m_segElapsed >= m_segs[m_index].ms) advance();
    if (m_state == Playing) syncSounds();
    emit ticked();
}

// ----------------------------------------------------------------- volume

void Playout::setVolume(double v) {
    m_volume = qBound(0.0, v, 1.0);
    applyAudio();
}

void Playout::setMuted(bool m) {
    m_muted = m;
    applyAudio();
}

// Master volume/mute combined with each timeline's own volume/mute (timeline 0 is the main one).
void Playout::layerAudio(const Placement &pl, Sfx &s) const {
    if (!s.out) return;
    const LayerState &ls = m_lib->layers.value(qBound(0, pl.layer, 3));
    s.out->setVolume(float(m_volume * ls.volume / 100.0));
    s.out->setMuted(m_muted || ls.muted);
}

void Playout::applyAudio() {
    syncDesignAudio();
    const LayerState &main = m_lib->layers.value(0);
    m_audio->setVolume(float(m_volume * main.volume / 100.0));
    m_audio->setMuted(m_muted || main.muted);
    for (const Placement &pl : std::as_const(m_lib->placements)) {
        const auto it = m_sfx.find(pl.id);
        if (it != m_sfx.end()) layerAudio(pl, *it);
    }
}

// ----------------------------------------------------------------- sounds

// Sounds placed on the timeline play while the broadcast is inside their window.
void Playout::syncSounds() {
    syncDesignAudio();
    // drop players of sounds that were removed
    const QStringList known = m_sfx.keys();
    for (const QString &id : known)
        if (!m_lib->placement(id)) {
            stopSound(id);
            m_soundsOn.remove(id);
        }

    const bool on = m_state == Playing;
    const qint64 pos = timelinePosMs();
    for (const Placement &pl : std::as_const(m_lib->placements)) {
        if (pl.type == "Element") continue;  // elements are drawn by the viewport, only media makes sound
        const bool active = on && pos >= pl.startMs && pos < pl.startMs + qint64(pl.seconds) * 1000;
        const bool fired = m_soundsOn.contains(pl.id);
        if (active && !fired) {
            m_soundsOn.insert(pl.id);
            playSound(pl, pos - pl.startMs);
        } else if (!active && fired) {
            m_soundsOn.remove(pl.id);
            stopSound(pl.id);
        }
    }
}

void Playout::resetSounds() {
    if(m_designAudio)m_designAudio->sync({});
    for (const QString &id : std::as_const(m_soundsOn)) stopSound(id);
    m_soundsOn.clear();
}

void Playout::playSound(const Placement &pl, qint64 inMs) {
    if (pl.file.isEmpty() || !QFile::exists(pl.file)) return;
    Sfx &s = m_sfx[pl.id];
    if (!s.player) {
        s.player = new QMediaPlayer(this);
        s.out = new QAudioOutput(this);
        s.player->setAudioOutput(s.out);
        emit audioSourceCreated(s.player,s.out);
    }
    layerAudio(pl, s);
    inMs += pl.offsetMs;  // a clip on an extra timeline can start part-way into its media
    QMediaPlayer *player = s.player;
    const QUrl url = QUrl::fromLocalFile(pl.file);
    if (player->source() != url) {
        player->setSource(url);
        if (inMs > 0) {
            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn = connect(player, &QMediaPlayer::mediaStatusChanged, player,
                            [player, inMs, conn](QMediaPlayer::MediaStatus st) {
                                if (st == QMediaPlayer::LoadedMedia || st == QMediaPlayer::BufferedMedia) {
                                    player->setPosition(inMs);
                                    QObject::disconnect(*conn);
                                }
                            });
        }
    } else {
        player->setPosition(inMs);
    }
    player->play();
}

void Playout::stopSound(const QString &id) {
    const auto it = m_sfx.constFind(id);
    if (it != m_sfx.constEnd() && it->player) it->player->stop();
}

void Playout::syncDesignAudio(){
    if(!m_designAudio){m_designAudio=new DesignAudio(this);m_designAudio->created=[this](QMediaPlayer *player,QAudioOutput *output){emit audioSourceCreated(player,output);};
        connect(this,&Playout::stateChanged,this,&Playout::syncDesignAudio);
        connect(m_lib,&Library::elementsChanged,this,&Playout::syncDesignAudio);connect(m_lib,&Library::elementLiveChanged,this,&Playout::syncDesignAudio);
    }
    QList<DesignAudioCue> cues;QSet<QString> drawn;
    auto append=[&](const Element &element,qint64 time,double gain){
        if(element.type!="Custom"||drawn.contains(element.id))return;drawn.insert(element.id);
        auto found=m_audioDesigns.find(element.id);if(found==m_audioDesigns.end()||found->first!=element.design)found=m_audioDesigns.insert(element.id,{element.design,ElementDesign::fromJson(element.design)});
        cues.append(designAudioCues(found->second,time,m_state==Playing,element.id+"/",gain));
    };
    if(m_state!=Stopped){const double gain=m_muted?0:m_volume;
        for(const auto &element:m_lib->elements)if(element.live)append(element,QDateTime::currentMSecsSinceEpoch(),gain);
        for(const auto &placement:m_lib->placements){const auto layer=m_lib->layers.value(qBound(0,placement.layer,3));
            if(placement.type!="Element"||!layer.visible||timelinePosMs()<placement.startMs||timelinePosMs()>=placement.startMs+qint64(placement.seconds)*1000)continue;
            if(const auto *element=m_lib->element(placement.refId))append(*element,timelinePosMs()-placement.startMs,layer.muted?0:gain*layer.volume/100.);
        }
    }
    m_designAudio->sync(cues);
}
