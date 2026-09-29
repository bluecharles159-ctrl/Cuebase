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

void Library::load() {
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) {
        seed();
        save();
        return;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    programs.clear();
    ads.clear();
    elements.clear();
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
        p.seconds = j["seconds"].toInt(60);
        programs.append(p);
    }
    for (const QJsonValue &v : o["ads"].toArray()) {
        const QJsonObject j = v.toObject();
        Ad a;
        a.id = j["id"].toString();
        a.name = j["name"].toString();
        a.file = j["file"].toString();
        a.seconds = j["seconds"].toInt(15);
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
        elements.append(e);
    }
    const QJsonObject d = o["display"].toObject();
    display.channel = d["channel"].toString(display.channel);
    display.resolution = d["resolution"].toString(display.resolution);
    display.fps = d["fps"].toInt(display.fps);
    display.bitrate = d["bitrate"].toDouble(display.bitrate);
    display.watermarkOn = d["watermarkOn"].toBool(display.watermarkOn);
    display.watermarkText = d["watermarkText"].toString(display.watermarkText);
    display.safeArea = d["safeArea"].toBool(display.safeArea);
}

void Library::save() const {
    QJsonArray pa, aa, ea;
    for (const ProgramItem &p : programs)
        pa.append(QJsonObject{{"id", p.id}, {"title", p.title}, {"kind", p.kind}, {"file", p.file},
                              {"adId", p.adId}, {"breakIn", p.breakIn}, {"breakOut", p.breakOut},
                              {"seconds", p.seconds}});
    for (const Ad &a : ads)
        aa.append(QJsonObject{{"id", a.id}, {"name", a.name}, {"file", a.file}, {"seconds", a.seconds}});
    for (const Element &e : elements)
        ea.append(QJsonObject{{"id", e.id}, {"name", e.name}, {"type", e.type}, {"text", e.text},
                              {"color", e.color}, {"position", e.position}, {"image", e.image},
                              {"seconds", e.seconds}, {"live", e.live}});
    const QJsonObject d{{"channel", display.channel}, {"resolution", display.resolution},
                        {"fps", display.fps}, {"bitrate", display.bitrate},
                        {"watermarkOn", display.watermarkOn}, {"watermarkText", display.watermarkText},
                        {"safeArea", display.safeArea}};
    const QJsonObject o{{"programs", pa}, {"ads", aa}, {"elements", ea}, {"display", d}};
    QFile f(filePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument(o).toJson());
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
