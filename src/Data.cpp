#include "Data.h"
#include "CueSequence.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QJsonObject>

qint64 Element::lengthMs() const {
    if(!cueBoard.isEmpty()){
        struct CachedLength{QString design;QJsonArray board;QJsonObject settings;qint64 duration;};
        static thread_local QHash<QString,CachedLength> cache;
        if(cache.size()>1024)cache.clear();auto found=cache.constFind(id);
        if(found==cache.cend()||found->design!=design||found->board!=cueBoard||found->settings!=cueBoardSettings){
            const auto duration=compileCueBoard(ElementDesign::fromJson(design),cueBoard,cueBoardSettings).duration();cache[id]={design,cueBoard,cueBoardSettings,duration};if(duration>0)return duration;
        }else if(found->duration>0)return found->duration;
    }
    if(type=="Custom"){auto json=QJsonDocument::fromJson(design.toUtf8()).object();const auto duration=qint64(json["duration"].toDouble());if(duration>0)return duration;}
    return qMax(1,seconds)*1000LL;
}
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <QtMath>
#include <algorithm>

Library::Library(QObject *parent) : QObject(parent) {
    connect(this,&Library::programsChanged,this,[this]{if(!m_cutEdit){m_cutUndo.clear();m_cutRedo.clear();}});
}

// ---- transitions ------------------------------------------------------------

namespace {
double ease(double x) {
    x = qBound(0.0, x, 1.0);
    return x * x * (3.0 - 2.0 * x);
}
QJsonObject fxToJson(const Transition &t) {
    return {{"in", t.in}, {"out", t.out}, {"inMs", t.inMs}, {"outMs", t.outMs}};
}
Transition fxFromJson(const QJsonObject &j) {
    Transition t;
    t.in = j["in"].toString("None");
    t.out = j["out"].toString("None");
    t.inMs = j["inMs"].toInt(600);
    t.outMs = j["outMs"].toInt(600);
    return t;
}
} // namespace

// k: 0 = fully hidden, 1 = fully shown. isIn tells which end of the clip this is.
static void applyFx(FxState &s, const QString &kind, double k, bool isIn) {
    const double away = 1.0 - k;
    if (kind == "Fade") s.opacity *= k;
    else if (kind == "Slide Left") s.dx += isIn ? away : -away;
    else if (kind == "Slide Right") s.dx += isIn ? -away : away;
    else if (kind == "Slide Up") s.dy += isIn ? away : -away;
    else if (kind == "Slide Down") s.dy += isIn ? -away : away;
    else if (kind == "Zoom In") {
        s.scale *= isIn ? 0.6 + 0.4 * k : 1.0 + 0.4 * away;
        s.opacity *= k;
    } else if (kind == "Zoom Out") {
        s.scale *= isIn ? 1.4 - 0.4 * k : 1.0 - 0.4 * away;
        s.opacity *= k;
    } else if (kind == "Wipe") s.reveal = qMin(s.reveal, k);
}

FxState transitionAt(const Transition &t, qint64 elapsedMs, qint64 totalMs) {
    FxState s;
    if (t.in != "None" && t.inMs > 0 && elapsedMs < t.inMs) applyFx(s, t.in, ease(double(elapsedMs) / t.inMs), true);
    const qint64 left = totalMs - elapsedMs;
    if (t.out != "None" && t.outMs > 0 && left < t.outMs) applyFx(s, t.out, ease(double(left) / t.outMs), false);
    return s;
}

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
const Ad *Library::ad(const QString &id) const {
    for (const Ad &a : ads) if (a.id == id) return &a;
    return nullptr;
}
const Element *Library::element(const QString &id) const {
    for (const Element &e : elements) if (e.id == id) return &e;
    return nullptr;
}
int Library::programIndex(const QString &id) const {
    for (int i = 0; i < programs.size(); ++i) if (programs[i].id == id) return i;
    return -1;
}
Placement *Library::placement(const QString &id) {
    for (Placement &p : placements) if (p.id == id) return &p;
    return nullptr;
}

// ---- timeline editing -------------------------------------------------------

QString Library::cutProgram(const QString &id, int atSec, bool slot) {
    const int i = programIndex(id);
    if (i < 0) return {};
    ProgramItem b = programs[i];
    if (b.isAdSpot() || !b.elementId.isEmpty() || atSec < 1 || qint64(atSec)*1000 >= b.lengthMs()) return {};
    m_cutUndo.append(programs);if(m_cutUndo.size()>100)m_cutUndo.removeFirst();m_cutRedo.clear();
    QScopedValueRollback<bool> transaction(m_cutEdit,true);

    // second half: the rest of the media, keeps the ad and breaks and any "+" that was after the program
    b.id = newId();
    b.durationMs=programs[i].lengthMs()-qint64(atSec)*1000;
    b.seconds=int((b.durationMs+999)/1000);
    b.offsetMs = programs[i].offsetMs + atSec * 1000;

    // first half: plays up to the cut, no ad or breaks, a "+" sits after it
    ProgramItem &a = programs[i];
    a.seconds = atSec;a.durationMs=qint64(atSec)*1000;
    a.adId.clear();
    a.breakIn.clear();
    a.breakOut.clear();
    a.slotAfter = slot;

    const QString newId2 = b.id;
    programs.insert(i + 1, b);
    commitPrograms();
    return newId2;
}

bool Library::undoProgramCut(){
    if(m_cutUndo.isEmpty())return false;QScopedValueRollback<bool> transaction(m_cutEdit,true);
    m_cutRedo.append(programs);programs=m_cutUndo.takeLast();commitPrograms();return true;
}
bool Library::redoProgramCut(){
    if(m_cutRedo.isEmpty())return false;QScopedValueRollback<bool> transaction(m_cutEdit,true);
    m_cutUndo.append(programs);programs=m_cutRedo.takeLast();commitPrograms();return true;
}
void Library::insertAfter(const QString &afterId, ProgramItem item) {
    if (item.id.isEmpty()) item.id = newId();
    const int i = programIndex(afterId);
    if (i < 0) {
        programs.append(item);
    } else {
        item.slotAfter = programs[i].slotAfter;  // the "+" stays in front of whatever followed
        programs[i].slotAfter = false;
        programs.insert(i + 1, item);
    }
    commitPrograms();
}

void Library::insertBefore(const QString &beforeId, ProgramItem item) {
    if (item.id.isEmpty()) item.id = newId();
    const int i = programIndex(beforeId);
    if (i < 0) {
        programs.append(item);
    } else {
        if (i > 0 && programs[i - 1].slotAfter) {  // dropped into a cut: the "+" stays in front of what followed
            item.slotAfter = true;
            programs[i - 1].slotAfter = false;
        }
        programs.insert(i, item);
    }
    commitPrograms();
}

int Library::maxLayer() const {
    int m = 0;
    for (const Placement &p : placements) m = qMax(m, p.layer);
    return m;
}

void Library::moveItemBefore(const QString &id, const QString &beforeId) {
    if (id == beforeId) return;
    const int from = programIndex(id);
    if (from < 0) return;
    if (from + 1 < programs.size() && programs[from + 1].id == beforeId) return;  // already there
    if (beforeId.isEmpty() && from == programs.size() - 1) return;
    ProgramItem item = programs[from];
    if (item.slotAfter && from > 0) programs[from - 1].slotAfter = true;  // hand the "+" back
    item.slotAfter = false;
    programs.removeAt(from);
    int to = beforeId.isEmpty() ? programs.size() : programIndex(beforeId);
    if (to < 0) to = programs.size();
    programs.insert(to, item);
    if (to > 0 && programs[to - 1].slotAfter) {  // moved into a cut: the "+" stays in front of what followed
        programs[to].slotAfter = true;
        programs[to - 1].slotAfter = false;
    }
    commitPrograms();
}

void Library::removeItem(const QString &id) {
    const int i = programIndex(id);
    if (i < 0) return;
    if (programs[i].slotAfter && i > 0) programs[i - 1].slotAfter = true;
    programs.removeAt(i);
    commitPrograms();
}

void Library::shiftPlacements(qint64 from, qint64 by) {
    bool any = false;
    for (Placement &p : placements)
        if (p.startMs >= from) {
            p.startMs += by;
            any = true;
        }
    if (any) commitPlacements();
}

void Library::load() {
    m_cutUndo.clear();m_cutRedo.clear();
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) {
        seed();
        save();
        return;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    production=o["production"].toObject();
    programs.clear();
    ads.clear();
    elements.clear();
    placements.clear();
    for (const QJsonValue &v : o["programs"].toArray()) {
        const QJsonObject j = v.toObject();
        ProgramItem p;
        p.id = j["id"].toString();
        p.title = j["title"].toString();
        p.kind = j["kind"].toString("Live");
        p.file = j["file"].toString();
        p.adId = j["adId"].toString();
        p.breakIn = j["breakIn"].toString();
        p.breakOut = j["breakOut"].toString();
        p.replayInEffect=j["replayInEffect"].toString("preset:Fade");p.replayOutEffect=j["replayOutEffect"].toString("preset:Fade");
        p.seconds = j["seconds"].toInt(60);
        p.durationMs=qint64(qBound(0.,j["durationMs"].toDouble(),2147483647.));p.elementId=j["elementId"].toString();
        p.offsetMs = j["offsetMs"].toInt(0);
        p.slotAfter = j["slotAfter"].toBool(false);
        p.description = j["description"].toString();
        p.image = j["image"].toString();
        p.fx = fxFromJson(j["fx"].toObject());
        programs.append(p);
    }
    for (const QJsonValue &v : o["ads"].toArray()) {
        const QJsonObject j = v.toObject();
        Ad a;
        a.id = j["id"].toString();
        a.name = j["name"].toString();
        a.file = j["file"].toString();
        a.seconds = j["seconds"].toInt(15);
        a.fx = fxFromJson(j["fx"].toObject());
        ads.append(a);
    }
    for (const QJsonValue &v : o["elements"].toArray()) {
        const QJsonObject j = v.toObject();
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
        e.design = j["design"].toString();
        e.description=j["description"].toString();e.playlist=j["playlist"].toString(e.type=="Break"?"Break screens":e.type=="Logo"?"Logos":"Overlays");e.thumbnail=j["thumbnail"].toString();e.previewMs=j["previewMs"].toDouble();e.cueBoard=j["cueBoard"].toArray();e.cueBoardSettings=j["cueBoardSettings"].toObject();
        e.previewX=j["previewX"].toDouble();e.previewY=j["previewY"].toDouble();e.previewW=j["previewW"].toDouble(1);e.previewH=j["previewH"].toDouble(1);
        e.fx = fxFromJson(j["fx"].toObject());
        elements.append(e);
    }
    for(auto v:o["elementPlaylists"].toArray())if(!v.toString().trimmed().isEmpty()&&!elementPlaylists.contains(v.toString()))elementPlaylists.append(v.toString());
    for(const auto &e:elements)if(!elementPlaylists.contains(e.playlist))elementPlaylists.append(e.playlist);
    for (const QJsonValue &v : o["placements"].toArray()) {
        const QJsonObject j = v.toObject();
        Placement pl;
        pl.id = j["id"].toString();
        pl.type = j["type"].toString("Element");
        pl.refId = j["refId"].toString();
        pl.name = j["name"].toString();
        pl.file = j["file"].toString();
        pl.startMs = qint64(j["startMs"].toDouble());
        pl.seconds = j["seconds"].toInt(8);
        pl.durationMs=qint64(j["durationMs"].toDouble());pl.boardSnapshot=j["boardSnapshot"].toObject();pl.boardCycleMs=j["boardCycleMs"].toDouble();
        pl.layer = qBound(1, j["layer"].toInt(1), 3);
        pl.offsetMs = j["offsetMs"].toInt(0);
        if(j["zIndex"].isDouble())pl.zIndex=qBound(-10000,j["zIndex"].toInt(),10000);
        pl.kind = j["kind"].toString();
        placements.append(pl);
    }
    layers = QVector<LayerState>(4);
    const QJsonArray lay = o["layers"].toArray();
    for (int i = 0; i < 4 && i < lay.size(); ++i) {
        const QJsonObject j = lay[i].toObject();
        layers[i].muted = j["muted"].toBool(false);
        layers[i].visible = j["visible"].toBool(true);
        layers[i].volume = qBound(0, j["volume"].toInt(100), 100);
    }
    const QJsonObject d = o["display"].toObject();
    display.channel = d["channel"].toString(display.channel);
    display.channelLogo=d["channelLogo"].toString();display.channelLogoElement=d["channelLogoElement"].toString();display.channelDescription=d["channelDescription"].toString();
    display.resolution = d["resolution"].toString(display.resolution);
    display.fps = d["fps"].toInt(display.fps);
    display.bitrate = d["bitrate"].toDouble(display.bitrate);
    display.watermarkOn = d["watermarkOn"].toBool(display.watermarkOn);
    display.watermarkText = d["watermarkText"].toString(display.watermarkText);
    display.safeArea = d["safeArea"].toBool(display.safeArea);
    display.resScale = d["resScale"].toInt(display.resScale);
    display.pixAspX = d["pixAspX"].toDouble(display.pixAspX);
    display.pixAspY = d["pixAspY"].toDouble(display.pixAspY);
    display.fpsBase = d["fpsBase"].toDouble(display.fpsBase);
    display.motionBlur = d["motionBlur"].toBool(display.motionBlur);
    display.shutter = d["shutter"].toDouble(display.shutter);
    display.mbSamples = d["mbSamples"].toInt(display.mbSamples);
    display.colorView = d["colorView"].toString(display.colorView);
    display.colorLook = d["colorLook"].toString(display.colorLook);
    display.exposure = d["exposure"].toDouble(display.exposure);
    display.gamma = d["gamma"].toDouble(display.gamma);
    display.regionOn = d["regionOn"].toBool(display.regionOn);
    display.rx0 = d["rx0"].toDouble(display.rx0);
    display.ry0 = d["ry0"].toDouble(display.ry0);
    display.rx1 = d["rx1"].toDouble(display.rx1);
    display.ry1 = d["ry1"].toDouble(display.ry1);
}

void Library::save() const {
    saveChecked();
}
bool Library::saveChecked(QString *error) const {
    QJsonArray pa, aa, ea, la;
    for (const ProgramItem &p : programs)
        pa.append(QJsonObject{{"id", p.id}, {"title", p.title}, {"kind", p.kind}, {"file", p.file},
                              {"adId", p.adId}, {"breakIn", p.breakIn}, {"breakOut", p.breakOut},
                              {"replayInEffect",p.replayInEffect},{"replayOutEffect",p.replayOutEffect},
                              {"seconds", p.seconds}, {"durationMs",double(p.durationMs)}, {"elementId",p.elementId}, {"offsetMs", p.offsetMs}, {"slotAfter", p.slotAfter},
                              {"description", p.description}, {"image", p.image}, {"fx", fxToJson(p.fx)}});
    for (const Ad &a : ads)
        aa.append(QJsonObject{{"id", a.id}, {"name", a.name}, {"file", a.file}, {"seconds", a.seconds},
                              {"fx", fxToJson(a.fx)}});
    for (const Element &e : elements)
        ea.append(QJsonObject{{"id", e.id}, {"name", e.name}, {"type", e.type}, {"text", e.text},
                              {"color", e.color}, {"position", e.position}, {"image", e.image},
                              {"seconds", e.seconds}, {"live", e.live}, {"design", e.design},
                              {"description",e.description},{"playlist",e.playlist},{"thumbnail",e.thumbnail},{"previewMs",double(e.previewMs)},{"cueBoard",e.cueBoard},{"cueBoardSettings",e.cueBoardSettings},
                              {"previewX",e.previewX},{"previewY",e.previewY},{"previewW",e.previewW},{"previewH",e.previewH},
                              {"fx", fxToJson(e.fx)}});
    for (const Placement &pl : placements)
        la.append(QJsonObject{{"id", pl.id}, {"type", pl.type}, {"refId", pl.refId}, {"name", pl.name},
                              {"file", pl.file}, {"startMs", double(pl.startMs)}, {"seconds", pl.seconds}, {"durationMs",double(pl.durationMs)},{"boardSnapshot",pl.boardSnapshot},{"boardCycleMs",double(pl.boardCycleMs)},{"zIndex",pl.zIndex?QJsonValue(*pl.zIndex):QJsonValue(QJsonValue::Null)},
                              {"layer", pl.layer}, {"offsetMs", pl.offsetMs}, {"kind", pl.kind}});
    QJsonArray lay;
    for (const LayerState &s : layers)
        lay.append(QJsonObject{{"muted", s.muted}, {"visible", s.visible}, {"volume", s.volume}});
    const QJsonObject d{{"channel", display.channel},{"channelLogo",display.channelLogo},{"channelLogoElement",display.channelLogoElement},{"channelDescription",display.channelDescription}, {"resolution", display.resolution},
                        {"fps", display.fps}, {"bitrate", display.bitrate},
                        {"watermarkOn", display.watermarkOn}, {"watermarkText", display.watermarkText},
                        {"safeArea", display.safeArea}, {"resScale", display.resScale}, {"pixAspX", display.pixAspX},
                        {"pixAspY", display.pixAspY}, {"fpsBase", display.fpsBase}, {"motionBlur", display.motionBlur},
                        {"shutter", display.shutter}, {"mbSamples", display.mbSamples}, {"colorView", display.colorView},
                        {"colorLook", display.colorLook}, {"exposure", display.exposure}, {"gamma", display.gamma},
                        {"regionOn", display.regionOn}, {"rx0", display.rx0}, {"ry0", display.ry0}, {"rx1", display.rx1},
                        {"ry1", display.ry1}};
    const QJsonObject o{{"programs", pa}, {"ads", aa}, {"elements", ea}, {"elementPlaylists",QJsonArray::fromStringList(elementPlaylists)}, {"placements", la}, {"layers", lay}, {"display", d},{"production",production}};
    QSaveFile f(filePath());const auto bytes=QJsonDocument(o).toJson();
    if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()){if(error)*error=f.errorString();return false;}return true;
}

// First launch only: a small starter set so everything can be tested straight away.
void Library::seed() {
    Element brIn{newId(), "Break In", "Break", "We'll be right back after this break", "#f59e0b", "Bottom Center", "", 4, false};
    Element brOut{newId(), "Break Out", "Break", "Now showing: {next}", "#0ea5e9", "Bottom Center", "", 4, false};
    Element logo{newId(), "Channel Logo", "Logo", "CUEBASE", "#60a5fa", "Top Left", "", 4, true};
    elements = {brIn, brOut, logo};

    ads = {Ad{newId(), "Sample Ad", "", 8}};

    ProgramItem a{newId(), "Live News", "Live", "", ads[0].id, brIn.id, brOut.id, 20};
    ProgramItem b{newId(), "Nature", "Documentary", "", "", "", "", 25};
    ProgramItem c{newId(), "Good Life", "Lifestyle", "", ads[0].id, brIn.id, brOut.id, 20};
    programs = {a, b, c};
}

Element Library::placementElement(const Placement &pl)const{Element e;if(const auto *source=element(pl.refId))e=*source;if(!pl.boardSnapshot.isEmpty()){e.design=pl.boardSnapshot["design"].toString();e.cueBoard=pl.boardSnapshot["cueBoard"].toArray();e.cueBoardSettings=pl.boardSnapshot["settings"].toObject();e.name=pl.name;e.type="Custom";}if(pl.zIndex)e.cueBoardSettings["zIndex"]=*pl.zIndex;e.id="placement:"+pl.id;e.live=false;e.cueBoardSettings["_placementLength"]=double(pl.lengthMs());e.cueBoardSettings["_cycle"]=double(pl.boardCycleMs);return e;}
