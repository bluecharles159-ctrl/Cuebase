#include "Data.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

Library::Library(QObject *parent) : QObject(parent) {}

QString Library::newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString Library::filePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/cuebase.json";
}

const ProgramItem *Library::program(const QString &id) const {
    for (const ProgramItem &p : programs) if (p.id == id) return &p;
    return nullptr;
}
ProgramItem *Library::programRef(const QString &id) {
    for (ProgramItem &p : programs) if (p.id == id) return &p;
    return nullptr;
}
const Ad *Library::ad(const QString &id) const {
    for (const Ad &a : ads) if (a.id == id) return &a;
    return nullptr;
}
const Element *Library::element(const QString &id) const {
    for (const Element &e : elements) if (e.id == id) return &e;
    return nullptr;
}
Element *Library::elementRef(const QString &id) {
    for (Element &e : elements) if (e.id == id) return &e;
    return nullptr;
}
const AudioAsset *Library::audio(const QString &id) const {
    for (const AudioAsset &a : audios) if (a.id == id) return &a;
    return nullptr;
}
AudioAsset *Library::audioRef(const QString &id) {
    for (AudioAsset &a : audios) if (a.id == id) return &a;
    return nullptr;
}
const TimelineAudio *Library::tlAudio(const QString &id) const {
    for (const TimelineAudio &a : timelineAudio) if (a.id == id) return &a;
    return nullptr;
}
TimelineAudio *Library::tlAudioRef(const QString &id) {
    for (TimelineAudio &a : timelineAudio) if (a.id == id) return &a;
    return nullptr;
}
int Library::programIndex(const QString &id) const {
    for (int i = 0; i < programs.size(); ++i) if (programs[i].id == id) return i;
    return -1;
}

static QJsonObject programToJson(const ProgramItem &p) {
    return QJsonObject{{"id", p.id}, {"title", p.title}, {"kind", p.kind}, {"file", p.file},
                       {"adId", p.adId}, {"breakIn", p.breakIn}, {"breakOut", p.breakOut},
                       {"seconds", p.seconds}, {"expandedWidth", p.expandedWidth}, {"inMs", p.inMs}};
}
static ProgramItem programFromJson(const QJsonObject &j) {
    ProgramItem p;
    p.id = j["id"].toString();
    p.title = j["title"].toString();
    p.kind = j["kind"].toString("Live");
    p.file = j["file"].toString();
    p.adId = j["adId"].toString();
    p.breakIn = j["breakIn"].toString();
    p.breakOut = j["breakOut"].toString();
    p.seconds = j["seconds"].toInt(60);
    p.expandedWidth = j["expandedWidth"].toInt(0);
    p.inMs = j["inMs"].toInt(0);
    return p;
}
static QJsonObject elementToJson(const Element &e) {
    return QJsonObject{{"id", e.id}, {"name", e.name}, {"type", e.type}, {"text", e.text},
                       {"color", e.color}, {"position", e.position}, {"image", e.image},
                       {"seconds", e.seconds}, {"live", e.live}, {"freeform", e.freeform},
                       {"x", e.x}, {"y", e.y}, {"w", e.w}, {"h", e.h},
                       {"noise", e.noise}, {"physics", e.physics}, {"gravity", e.gravity}};
}
static Element elementFromJson(const QJsonObject &j) {
    Element e;
    e.id = j["id"].toString();
    e.name = j["name"].toString();
    e.type = j["type"].toString("Logo");
    e.text = j["text"].toString();
    e.color = j["color"].toString("#2563eb");
    e.position = j["position"].toString("Top Left");
    e.image = j["image"].toString();
    e.seconds = j["seconds"].toInt(4);
    e.live = j["live"].toBool();
    e.freeform = j["freeform"].toBool();
    e.x = j["x"].toDouble(0.12);
    e.y = j["y"].toDouble(0.12);
    e.w = j["w"].toDouble(0.22);
    e.h = j["h"].toDouble(0.14);
    e.noise = j["noise"].toDouble(0);
    e.physics = j["physics"].toBool();
    e.gravity = j["gravity"].toDouble(420);
    return e;
}
static QJsonObject audioToJson(const AudioAsset &a) {
    return QJsonObject{{"id", a.id}, {"name", a.name}, {"file", a.file}, {"seconds", a.seconds},
                       {"speed", a.speed}, {"fadeIn", a.fadeIn}, {"fadeOut", a.fadeOut},
                       {"echo", a.echo}, {"offset", a.offset}};
}
static AudioAsset audioFromJson(const QJsonObject &j) {
    AudioAsset a;
    a.id = j["id"].toString();
    a.name = j["name"].toString();
    a.file = j["file"].toString();
    a.seconds = j["seconds"].toInt(8);
    a.speed = j["speed"].toDouble(1.0);
    a.fadeIn = j["fadeIn"].toDouble(0);
    a.fadeOut = j["fadeOut"].toDouble(0);
    a.echo = j["echo"].toDouble(0);
    a.offset = j["offset"].toDouble(0);
    return a;
}
static QJsonObject tlToJson(const TimelineAudio &a) {
    return QJsonObject{{"id", a.id}, {"audioId", a.audioId}, {"startMs", a.startMs},
                       {"seconds", a.seconds}, {"expandedWidth", a.expandedWidth}, {"inScene", a.inScene}};
}
static TimelineAudio tlFromJson(const QJsonObject &j) {
    TimelineAudio a;
    a.id = j["id"].toString();
    a.audioId = j["audioId"].toString();
    a.startMs = j["startMs"].toInt(0);
    a.seconds = j["seconds"].toInt(8);
    a.expandedWidth = j["expandedWidth"].toInt(0);
    a.inScene = j["inScene"].toBool();
    return a;
}

QByteArray Library::snapshot() const {
    QJsonArray pa, aa, ea, au, ta, na;
    for (const ProgramItem &p : programs) pa.append(programToJson(p));
    for (const Ad &a : ads)
        aa.append(QJsonObject{{"id", a.id}, {"name", a.name}, {"file", a.file}, {"seconds", a.seconds}});
    for (const Element &e : elements) ea.append(elementToJson(e));
    for (const AudioAsset &a : audios) au.append(audioToJson(a));
    for (const TimelineAudio &a : timelineAudio) ta.append(tlToJson(a));
    for (const PlanNote &n : notes)
        na.append(QJsonObject{{"id", n.id}, {"text", n.text}, {"x", n.x}, {"y", n.y},
                              {"w", n.w}, {"h", n.h}, {"color", n.color}});
    const QJsonObject d{{"channel", display.channel}, {"resolution", display.resolution},
                        {"fps", display.fps}, {"bitrate", display.bitrate},
                        {"watermarkOn", display.watermarkOn}, {"watermarkText", display.watermarkText},
                        {"safeArea", display.safeArea}};
    const QJsonObject o{{"programs", pa}, {"ads", aa}, {"elements", ea}, {"audios", au},
                        {"timelineAudio", ta}, {"notes", na}, {"display", d},
                        {"selectedKind", selectedKind}, {"selectedId", selectedId}};
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

void Library::applySnapshot(const QByteArray &raw, bool emitAll) {
    const QJsonObject o = QJsonDocument::fromJson(raw).object();
    programs.clear();
    ads.clear();
    elements.clear();
    audios.clear();
    timelineAudio.clear();
    notes.clear();
    for (const QJsonValue &v : o["programs"].toArray()) programs.append(programFromJson(v.toObject()));
    for (const QJsonValue &v : o["ads"].toArray()) {
        const QJsonObject j = v.toObject();
        ads.append(Ad{j["id"].toString(), j["name"].toString(), j["file"].toString(), j["seconds"].toInt(15)});
    }
    for (const QJsonValue &v : o["elements"].toArray()) elements.append(elementFromJson(v.toObject()));
    for (const QJsonValue &v : o["audios"].toArray()) audios.append(audioFromJson(v.toObject()));
    for (const QJsonValue &v : o["timelineAudio"].toArray()) timelineAudio.append(tlFromJson(v.toObject()));
    for (const QJsonValue &v : o["notes"].toArray()) {
        const QJsonObject j = v.toObject();
        notes.append(PlanNote{j["id"].toString(), j["text"].toString(), j["x"].toInt(32), j["y"].toInt(32),
                              j["w"].toInt(220), j["h"].toInt(148), j["color"].toString("#f5e6c8")});
    }
    const QJsonObject d = o["display"].toObject();
    display.channel = d["channel"].toString(display.channel);
    display.resolution = d["resolution"].toString(display.resolution);
    display.fps = d["fps"].toInt(display.fps);
    display.bitrate = d["bitrate"].toDouble(display.bitrate);
    display.watermarkOn = d["watermarkOn"].toBool(display.watermarkOn);
    display.watermarkText = d["watermarkText"].toString(display.watermarkText);
    display.safeArea = d["safeArea"].toBool(display.safeArea);
    selectedKind = o["selectedKind"].toString();
    selectedId = o["selectedId"].toString();
    save();
    if (emitAll) {
        emit programsChanged();
        emit adsChanged();
        emit elementsChanged();
        emit elementLiveChanged();
        emit displayChanged();
        emit audioChanged();
        emit timelineAudioChanged();
        emit notesChanged();
        emit selectionChanged();
        emit historyChanged();
    }
}

void Library::load() {
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) {
        seed();
        save();
        return;
    }
    applySnapshot(f.readAll(), false);
}

void Library::save() const {
    QFile f(filePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument::fromJson(snapshot()).toJson());
}

void Library::checkpoint() {
    if (m_restoring) return;
    m_undo.append(snapshot());
    if (m_undo.size() > 80) m_undo.removeFirst();
    m_redo.clear();
    emit historyChanged();
}

void Library::undo() {
    if (m_undo.isEmpty()) return;
    m_redo.append(snapshot());
    m_restoring = true;
    applySnapshot(m_undo.takeLast(), true);
    m_restoring = false;
}

void Library::redo() {
    if (m_redo.isEmpty()) return;
    m_undo.append(snapshot());
    m_restoring = true;
    applySnapshot(m_redo.takeLast(), true);
    m_restoring = false;
}

void Library::finishEdit(const QString &what) {
    save();
    emitFor(what);
}

void Library::emitFor(const QString &what) {
    if (what == "programs") emit programsChanged();
    else if (what == "ads") emit adsChanged();
    else if (what == "elements") emit elementsChanged();
    else if (what == "audio") emit audioChanged();
    else if (what == "timelineAudio") emit timelineAudioChanged();
    else if (what == "notes") emit notesChanged();
}

void Library::select(const QString &kind, const QString &id) {
    if (selectedKind == kind && selectedId == id) return;
    selectedKind = kind;
    selectedId = id;
    emit selectionChanged();
}

bool Library::cutProgram(const QString &id, double ratio) {
    ratio = qBound(0.08, ratio, 0.92);
    const int idx = programIndex(id);
    if (idx < 0) return false;
    checkpoint();
    ProgramItem left = programs[idx];
    const int total = qMax(2, left.seconds);
    int leftSec = qMax(1, int(total * ratio));
    int rightSec = qMax(1, total - leftSec);
    if (leftSec + rightSec > total) leftSec = total - rightSec;
    ProgramItem right = left;
    right.id = newId();
    right.title = left.title;
    right.seconds = rightSec;
    right.inMs = left.inMs + leftSec * 1000;
    right.expandedWidth = 0;
    left.seconds = leftSec;
    left.adId.clear();
    left.breakIn.clear();
    left.breakOut.clear();
    programs[idx] = left;
    programs.insert(idx + 1, right);
    save();
    emit programsChanged();
    return true;
}

bool Library::trimProgram(const QString &id, double ratio) {
    ratio = qBound(0.08, ratio, 0.95);
    ProgramItem *p = programRef(id);
    if (!p) return false;
    checkpoint();
    p->seconds = qMax(1, int(p->seconds * ratio));
    save();
    emit programsChanged();
    return true;
}

QString Library::addTimelineAudio(const QString &audioId, int startMs, bool inScene) {
    const AudioAsset *src = audio(audioId);
    if (!src) return {};
    checkpoint();
    TimelineAudio t;
    t.id = newId();
    t.audioId = audioId;
    t.startMs = qMax(0, startMs);
    t.seconds = src->seconds;
    t.inScene = inScene;
    timelineAudio.append(t);
    save();
    emit timelineAudioChanged();
    return t.id;
}

void Library::setAudioInScene(const QString &timelineOrAssetId, bool on) {
    checkpoint();
    for (TimelineAudio &t : timelineAudio) {
        if (t.id == timelineOrAssetId || t.audioId == timelineOrAssetId) {
            t.inScene = on;
            save();
            emit timelineAudioChanged();
            return;
        }
    }
    if (on && audio(timelineOrAssetId)) addTimelineAudio(timelineOrAssetId, 0, true);
}

void Library::seed() {
    Element brIn{newId(), "Break In", "Break", "We'll be right back after this break", "#f59e0b", "Bottom Center", "", 4, false};
    Element brOut{newId(), "Break Out", "Break", "Now showing: {next}", "#0ea5e9", "Bottom Center", "", 4, false};
    Element logo{newId(), "Channel Logo", "Logo", "CUEBASE", "#60a5fa", "Top Left", "", 4, true};
    logo.freeform = true;
    logo.x = 0.04;
    logo.y = 0.05;
    logo.w = 0.18;
    logo.h = 0.1;
    elements = {brIn, brOut, logo};

    ads = {Ad{newId(), "Sample Ad", "", 8}};
    audios = {AudioAsset{newId(), "thunder_strike", "", 6, 1.0, 0.1, 0.2, 0.0, 0.0}};

    ProgramItem a{newId(), "Live News", "Live", "", ads[0].id, brIn.id, brOut.id, 20};
    ProgramItem b{newId(), "Nature", "Documentary", "", "", "", "", 25};
    ProgramItem c{newId(), "Good Life", "Lifestyle", "", ads[0].id, brIn.id, brOut.id, 20};
    programs = {a, b, c};

    notes = {PlanNote{newId(), "Tonight's rundown\n\nOpen on Live News.\nHold nature B-roll under VO.", 36, 40, 240, 160, "#f5e6c8"}};
}
