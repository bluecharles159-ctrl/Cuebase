#include "Playout.h"

#include <QAudioOutput>
#include <QDateTime>
#include <QFile>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

Playout::Playout(Library *lib, QObject *parent) : QObject(parent), m_lib(lib) {
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_sink = new QVideoSink(this);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoSink(m_sink);

    m_clock.start();
    m_tick.setInterval(40);
    connect(&m_tick, &QTimer::timeout, this, &Playout::onTick);
    m_tick.start();

    connect(lib, &Library::programsChanged, this, &Playout::rebuild);
    connect(lib, &Library::adsChanged, this, &Playout::rebuild);
    connect(lib, &Library::elementsChanged, this, &Playout::rebuild);
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

// Build the timeline: Program -> [Break In] -> Ad -> [Break Out] -> next Program
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
        Segment s;
        s.type = Segment::Program;
        s.programId = p.id;
        s.title = p.title;
        s.kind = p.kind;
        s.file = p.file;
        s.ms = p.seconds * 1000;
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
        m_segs << a;
        addBreak(p.breakOut, 1);
    }

    if (had) {
        int found = -1;
        for (int i = 0; i < m_segs.size(); ++i)
            if (m_segs[i].programId == keyId && m_segs[i].type == keyType && m_segs[i].part == keyPart) found = i;
        if (found >= 0) m_index = found;
        else stop();
    }
    emit segmentsChanged();
}

void Playout::startMedia(const Segment &s) {
    if (!s.file.isEmpty() && QFile::exists(s.file)) {
        m_player->setSource(QUrl::fromLocalFile(s.file));
        m_player->play();
    } else {
        m_player->stop();
        m_player->setSource(QUrl());
        m_sink->setVideoFrame(QVideoFrame());
    }
}

void Playout::enter() {
    m_segElapsed = 0;
    if (m_state == Playing) startMedia(m_segs[m_index]);
    emit segmentChanged();
}

void Playout::markFinished(const QString &programId) {
    if (m_finished.value(programId, 0) > 0) return;
    m_finished[programId] = QDateTime::currentMSecsSinceEpoch();
    emit programFinished(programId);
}

void Playout::start(int segment) {
    if (m_segs.isEmpty()) return;
    if (m_state == Stopped) {
        m_finished.clear();
        m_total = 0;
        m_state = Playing;
        m_index = -1;
        emit stateChanged();
    }
    jumpTo(segment);
}

void Playout::jumpTo(int segment) {
    if (segment < 0 || segment >= m_segs.size()) return;
    if (m_state == Stopped) {
        start(segment);
        return;
    }
    // everything before the target program counts as played
    const int target = m_lib->programIndex(m_segs[segment].programId);
    for (int i = 0; i < target; ++i) markFinished(m_lib->programs[i].id);
    const bool wasPaused = m_state == Paused;
    m_state = Playing;
    m_index = segment;
    enter();
    if (wasPaused) emit stateChanged();
}

void Playout::playProgram(const QString &programId) {
    for (int i = 0; i < m_segs.size(); ++i)
        if (m_segs[i].type == Segment::Program && m_segs[i].programId == programId) {
            jumpTo(i);
            return;
        }
}

void Playout::stop() {
    if (m_state == Stopped) return;
    m_state = Stopped;
    m_index = -1;
    m_finished.clear();
    m_player->stop();
    m_player->setSource(QUrl());
    m_sink->setVideoFrame(QVideoFrame());
    emit segmentChanged();
    emit stateChanged();
}

void Playout::finishAll() {
    m_state = Stopped;
    m_index = -1;
    m_player->stop();
    m_player->setSource(QUrl());
    m_sink->setVideoFrame(QVideoFrame());
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
    emit stateChanged();
}

void Playout::advance() {
    if (m_segs[m_index].type == Segment::Program) markFinished(m_segs[m_index].programId);
    if (m_index + 1 < m_segs.size()) {
        ++m_index;
        enter();
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
    emit ticked();
}
