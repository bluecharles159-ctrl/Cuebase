#include "ElementDesign.h"
#include "MirrorGeometry.h"
#include "BevelGeometry.h"

#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLinearGradient>
#include <QPainter>
#include "SurfaceEffects.h"
#include <QTransform>
#include <QUuid>
#include <QtMath>
#include <algorithm>

// ------------------------------------------------------------------ interpolation

QStringList interpolationKinds() { return {"Constant", "Linear", "Bezier", "Smooth", "Ease In", "Ease Out", "Back", "Bounce"}; }

double interpolate(const QString &k, double t) {
    t = qBound(0.0, t, 1.0);
    if (k == "Constant") return t >= 1.0 ? 1.0 : 0.0;
    if (k == "Linear") return t;
    if (k == "Ease In") return t * t;
    if (k == "Ease Out") return 1 - (1 - t) * (1 - t);
    if (k == "Back") {  // overshoots a little, then settles
        const double c1 = 1.70158, c3 = c1 + 1;
        return 1 + c3 * qPow(t - 1, 3) + c1 * qPow(t - 1, 2);
    }
    if (k == "Bounce") {
        const double n1 = 7.5625, d1 = 2.75;
        if (t < 1 / d1) return n1 * t * t;
        if (t < 2 / d1) { t -= 1.5 / d1; return n1 * t * t + 0.75; }
        if (t < 2.5 / d1) { t -= 2.25 / d1; return n1 * t * t + 0.9375; }
        t -= 2.625 / d1;
        return n1 * t * t + 0.984375;
    }
    return t * t * (3 - 2 * t);  // Smooth
}

// ------------------------------------------------------------------ objects and keys

DProps DObject::at(qint64 ms) const {
    DProps p = base;
    for (const auto &track : tracks)
        setChannelValue(p, track.property, track.evaluate(ms, channelValue(base, track.property)));
    for (auto it = overrides.begin(); it != overrides.end(); ++it) setChannelValue(p, it.key(), it.value());
    return p;
}

int DObject::keyIndexNear(qint64 ms, int tol) const {
    for (int i = 0; i < keys.size(); ++i)
        if (qAbs(keys[i].ms - ms) <= tol) return i;
    return -1;
}

void DObject::setKey(qint64 ms, const DProps &p) {
    for (const auto &path : transformChannels()) keyChannel(path, int(ms), channelValue(p, path));
}

QMatrix4x4 ElementDesign::matrixOf(const DProps &p) {
    QMatrix4x4 m;
    m.translate(p.loc);
    m.rotate(p.rot.z(), 0, 0, 1);
    m.rotate(p.rot.y(), 0, 1, 0);
    m.rotate(p.rot.x(), 1, 0, 0);
    m.scale(p.scl);
    return m;
}

DObject *ElementDesign::find(const QString &id) {
    for (DObject &o : objects)
        if (o.id == id) return &o;
    return nullptr;
}
const DObject *ElementDesign::find(const QString &id) const {
    for (const DObject &o : objects)
        if (o.id == id) return &o;
    return nullptr;
}
DObject *ElementDesign::camera() {
    for (DObject &o : objects)
        if (o.type == "Camera") return &o;
    return nullptr;
}
const DObject *ElementDesign::camera() const {
    for (const DObject &o : objects)
        if (o.type == "Camera") return &o;
    return nullptr;
}

void ElementDesign::ensureCamera() {
    if (camera()) return;
    DObject c;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    c.name = "Camera";
    c.type = "Camera";
    c.locked = true;
    c.base.loc = {0, -10, 0};
    c.base.rot = {90, 0, 0};
    c.cam.fov = qRadiansToDegrees(2.0 * qAtan((kDesignW / kPxPerUnit) / 20.0));
    objects.prepend(c);
}

bool ElementDesign::isEmpty() const {
    for (const DObject &o : objects)
        if (o.isRenderable() || o.type=="Audio" || o.type=="Effect") return false;
    return true;
}

ElementDesign ElementDesign::defaultScene() {
    ElementDesign d;
    d.ensureCamera();
    d.camera()->cam.ortho = true;
    d.camera()->cam.orthoScale = kDesignW / kPxPerUnit;
    DObject l;
    l.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    l.name = "Light";
    l.type = "Light";
    l.base.loc = {3.5f, -5.0f, 6.0f};
    d.objects.append(l);
    return d;
}

// ------------------------------------------------------------------ view camera

bool ViewCam::toScreen(const QVector3D &w, const QRectF &r, QPointF *out, double *depth) const {
    const QVector3D c = view.map(w);
    const double d = -c.z();
    double nx, ny;
    if (ortho) {
        nx = c.x() / (orthoScale / 2);
        ny = c.y() / (orthoScale / 2) * aspect;
    } else {
        if (d < nearClip) return false;
        nx = c.x() / d * f;
        ny = c.y() / d * f * aspect;
    }
    *out = QPointF(r.left() + (nx * 0.5 + 0.5) * r.width(), r.top() + (0.5 - ny * 0.5) * r.height());
    if (depth) *depth = d;
    return true;
}

void ViewCam::ray(const QPointF &px, const QRectF &r, QVector3D *origin, QVector3D *dir) const {
    const double nx = (px.x() - r.left()) / r.width() * 2 - 1, ny = 1 - (px.y() - r.top()) / r.height() * 2;
    const QMatrix4x4 inv = view.inverted();
    if (ortho) {
        *origin = inv.map(QVector3D(float(nx * orthoScale / 2), float(ny * orthoScale / 2 / aspect), 0));
        *dir = inv.mapVector(QVector3D(0, 0, -1)).normalized();
    } else {
        *origin = inv.map(QVector3D(0, 0, 0));
        *dir = inv.mapVector(QVector3D(float(nx / f), float(ny / (f * aspect)), -1)).normalized();
    }
}

QVector3D ViewCam::position() const { return view.inverted().map(QVector3D(0, 0, 0)); }
QVector3D ViewCam::forward() const { return view.inverted().mapVector(QVector3D(0, 0, -1)).normalized(); }

ViewCam ElementDesign::sceneCamera(qint64 ms, double aspect) const {
    ViewCam v;
    v.aspect = aspect;
    if (const DObject *c = camera()) {
        v.view = worldMatrix(*c, ms).inverted();
        const auto camera = c->evaluated(ms);
        v.ortho = camera.cam.ortho;
        v.orthoScale = camera.cam.orthoScale;
        v.f = 1.0 / qTan(qDegreesToRadians(qBound(5.0, camera.cam.fov, 170.0) / 2));
    } else {
        v.view.translate(0, 0, -10);
        v.view.rotate(-90, 1, 0, 0);
    }
    return v;
}

// ------------------------------------------------------------------ json

namespace {
QJsonArray vec(const QVector3D &v) { return {double(v.x()), double(v.y()), double(v.z())}; }
QVector3D vecOf(const QJsonValue &j, const QVector3D &def = {}) {
    const QJsonArray a = j.toArray();
    return a.size() == 3 ? QVector3D(float(a[0].toDouble()), float(a[1].toDouble()), float(a[2].toDouble())) : def;
}
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8); }
} // namespace

QString ElementDesign::toJson() const {
    QJsonArray arr;
    for (const DObject &o : objects) {
        QJsonArray keys;
        for (const DKey &k : o.keys) {
            QJsonObject kj{{"ms", k.ms}, {"loc", vec(k.p.loc)}, {"rot", vec(k.p.rot)}, {"scl", vec(k.p.scl)},
                           {"opacity", k.p.opacity}, {"interp", k.interp}};
            if (!k.channels.isEmpty()) {
                QJsonArray ch;
                for (const QString &c : k.channels) ch.append(c);
                kj["channels"] = ch;
            }
            keys.append(kj);
        }
        QJsonArray tracks;
        for (const auto &track : o.tracks) {
            QJsonArray values;
            for (const auto &key : track.keys)
                values.append(QJsonObject{{"ms", key.ms}, {"value", key.value}, {"interpolation", key.interpolation},
                    {"left",QJsonArray{key.leftHandle.x(),key.leftHandle.y()}},{"right",QJsonArray{key.rightHandle.x(),key.rightHandle.y()}},{"customHandles",key.customHandles},{"handleType",key.handleType}});
            if (!values.isEmpty()) tracks.append(QJsonObject{{"property", track.property}, {"keys", values}});
        }
        QJsonArray mods;
        for (const Modifier &m : o.mods)
            mods.append(QJsonObject{{"type", m.type}, {"on", m.on}, {"ax", m.ax}, {"ay", m.ay}, {"az", m.az}, {"count", m.count},
                                    {"rel", vec(m.rel)}, {"cst", vec(m.cst)}, {"thickness", m.thickness}, {"offset", m.offset},
                                    {"width", m.width}, {"op", m.op}, {"target", m.target}, {"options",m.options}});
        const DProps bp = o.base;
        QJsonObject j{{"id", o.id}, {"name", o.name}, {"type", o.type}, {"visible", o.visible},
                      {"layout", QJsonObject{{"insetLeft", o.insetLeft}, {"insetRight", o.insetRight}, {"insetTop", o.insetTop}, {"insetBottom", o.insetBottom}, {"borderRadius", o.borderRadius}}},
                      {"base", QJsonObject{{"loc", vec(bp.loc)}, {"rot", vec(bp.rot)}, {"scl", vec(bp.scl)}, {"opacity", bp.opacity}}},
                      {"tracks", tracks}, {"style", o.style}, {"locked", o.locked}, {"mods", mods},
                      {"mat", QJsonObject{{"color", o.mat.color.name()}, {"alpha", o.mat.alpha}, {"emission", o.mat.emission},
                                          {"roughness", o.mat.roughness}, {"shadeless", o.mat.shadeless}}}};
        j["defaults"]=o.defaults;j["parent"]=o.parentId;
        QJsonArray inverse;for(int row=0;row<4;++row)for(int col=0;col<4;++col)inverse.append(o.parentInverse(row,col));j["parentInverse"]=inverse;
        if (o.type == "Mesh") j["mesh"] = o.mesh.toJson();
        if (o.type == "Text") j["text"] = o.text;
        if (o.type == "Text" || o.type == "Image" || o.type == "Shape") {
            j["fontPx"] = o.fontPx;
            j["w"] = o.w;
            j["h"] = o.h;
            j["image"] = o.image;
        }
        if (o.type == "Light")
            j["light"] = QJsonObject{{"kind", o.light.kind}, {"color", o.light.color.name()}, {"energy", o.light.energy},
                                     {"spotSize", o.light.spotSize}, {"spotBlend", o.light.spotBlend}, {"radius", o.light.radius}};
        if (o.type == "Camera")
            j["cam"] = QJsonObject{{"fov", o.cam.fov}, {"ortho", o.cam.ortho}, {"orthoScale", o.cam.orthoScale}};
        j["phys"] = QJsonObject{{"enabled", o.phys.enabled}, {"active", o.phys.active}, {"mass", o.phys.mass},
                                {"friction", o.phys.friction}, {"bounce", o.phys.bounce}};
        arr.append(j);
    }
    const QJsonObject w{{"color", world.color.name()}, {"horizon", world.horizon.name()}, {"zenith", world.zenith.name()},
                        {"ambientColor", world.ambientColor.name()}, {"fogColor", world.fogColor.name()}, {"gradient", world.gradient},
                        {"transparent", world.transparent}, {"ambient", world.ambient}, {"fog", world.fog}, {"gravity", world.gravity},
                        {"floor", world.floor}};
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"version", 6}, {"animationCues",animationCues}, {"cursor",vec(cursor)}, {"start",startMs}, {"duration", durationMs}, {"loop", loop}, {"world", w}, {"objects", arr}})
                                 .toJson(QJsonDocument::Compact));
}

ElementDesign ElementDesign::fromJson(const QString &json) {
    ElementDesign d;
    const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
    d.animationCues=root["animationCues"].toArray();
    d.durationMs = root["duration"].toInt(5000);
    d.cursor=vecOf(root["cursor"]);
    d.startMs=qBound(0,root["start"].toInt(),qMax(0,d.durationMs-1));
    d.loop = root["loop"].toBool(true);

    if (!root.contains("version")) {  // a flat 2D design from before Build went 3D: rebuild it on a front-facing set
        d.world.transparent = true;
        d.ensureCamera();
        DObject *cam = d.camera();
        cam->cam.ortho = true;
        cam->cam.orthoScale = 16;
        for (const QJsonValue &v : root["objects"].toArray()) {
            const QJsonObject j = v.toObject();
            DObject o;
            o.id = j["id"].toString(newId());
            o.name = j["name"].toString("Object");
            const QString t = j["type"].toString("Rect");
            o.type = t == "Text" ? "Text" : "Mesh";
            if (t == "Rect") o.mesh = Mesh::primitive("Plane");
            if (t == "Ellipse") o.mesh = Mesh::primitive("Circle");
            o.text = j["text"].toString("Text");
            o.fontPx = j["fontPx"].toInt(96);
            o.mat.color = QColor(j["fill"].toString("#2563eb"));
            o.mat.shadeless = true;
            o.visible = j["visible"].toBool(true);
            double w0 = 0, h0 = 0;  // a text keeps the size of its first key as its plane; later keys scale it
            for (const QJsonValue &kv : j["keys"].toArray()) {
                const QJsonObject kj = kv.toObject();
                const double x = kj["x"].toDouble(860), y = kj["y"].toDouble(440), w = kj["w"].toDouble(200), h = kj["h"].toDouble(200);
                const double sc = kj["scale"].toDouble(1);
                if (o.keys.isEmpty()) {
                    w0 = qMax(1.0, w);
                    h0 = qMax(1.0, h);
                    if (o.type == "Text") {
                        o.w = w0 / kPxPerUnit;
                        o.h = h0 / kPxPerUnit;
                    }
                }
                DKey k;
                k.ms = kj["ms"].toInt(0);
                k.p.loc = QVector3D(float((x + w / 2 - kDesignW / 2.0) / kPxPerUnit), 0, float((kDesignH / 2.0 - (y + h / 2)) / kPxPerUnit));
                k.p.rot = QVector3D(90, float(kj["rot"].toDouble(0)), 0);
                if (o.type == "Text") k.p.scl = QVector3D(float(sc * w / w0), float(sc * h / h0), 1);
                else k.p.scl = QVector3D(float(sc * w / kPxPerUnit / 2), float(sc * h / kPxPerUnit / 2), 1);
                k.p.opacity = kj["opacity"].toDouble(1);
                o.keys.append(k);
            }
            if (!o.keys.isEmpty()) o.base = o.keys.first().p;
            std::sort(o.keys.begin(), o.keys.end(), [](const DKey &a, const DKey &b) { return a.ms < b.ms; });
            o.migrateKeys();
            d.objects.append(o);
        }
        return d;
    }

    const QJsonObject w = root["world"].toObject();
    d.world.color = QColor(w["color"].toString("#10131c"));
    d.world.horizon = QColor(w["horizon"].toString("#8fb0d6"));
    d.world.zenith = QColor(w["zenith"].toString("#1d3a6b"));
    d.world.ambientColor = QColor(w["ambientColor"].toString("#ffffff"));
    d.world.fogColor = QColor(w["fogColor"].toString("#9aa7b8"));
    d.world.gradient = w["gradient"].toBool(false);
    d.world.transparent = w["transparent"].toBool(true);
    d.world.ambient = w["ambient"].toDouble(0.35);
    d.world.fog = w["fog"].toDouble(0);
    d.world.gravity = w["gravity"].toDouble(9.81);
    d.world.floor = w["floor"].toBool(true);

    for (const QJsonValue &v : root["objects"].toArray()) {
        const QJsonObject j = v.toObject();
        DObject o;
        o.id = j["id"].toString(newId());
        o.name = j["name"].toString("Object");
        o.type = j["type"].toString("Mesh");
        o.visible = j["visible"].toBool(true);
        const QJsonObject m = j["mat"].toObject();
        o.mat.color = QColor(m["color"].toString("#c8ccd4"));
        o.mat.alpha = m["alpha"].toDouble(1);
        o.mat.emission = m["emission"].toDouble(0);
        o.mat.roughness = m["roughness"].toDouble(0.5);
        o.mat.shadeless = m["shadeless"].toBool(false);
        if (o.type == "Mesh") o.mesh = Mesh::fromJson(j["mesh"].toObject());
        o.text = j["text"].toString("Text");
        o.fontPx = j["fontPx"].toInt(96);
        o.w = j["w"].toDouble(6);
        o.h = j["h"].toDouble(1.5);
        const QJsonObject ly = j["layout"].toObject();
        o.insetLeft = ly["insetLeft"].toDouble(0);
        o.insetRight = ly["insetRight"].toDouble(0);
        o.insetTop = ly["insetTop"].toDouble(0);
        o.insetBottom = ly["insetBottom"].toDouble(0);
        o.borderRadius = ly["borderRadius"].toDouble(0);
        o.image = j["image"].toString();
        const QJsonObject b = j["base"].toObject();
        o.base.loc = vecOf(b["loc"], {});
        o.base.rot = vecOf(b["rot"], {});
        o.base.scl = vecOf(b["scl"], {1, 1, 1});
        o.base.opacity = b.contains("opacity") ? b["opacity"].toDouble(1) : 1.0;
        const QJsonObject l = j["light"].toObject();
        o.light.kind = l["kind"].toString("Point");
        o.light.color = QColor(l["color"].toString("#ffffff"));
        o.light.energy = l["energy"].toDouble(1000);
        o.light.spotSize = l["spotSize"].toDouble(45);
        o.light.spotBlend = l["spotBlend"].toDouble(0.15);
        o.light.radius = l["radius"].toDouble(0.1);
        const QJsonObject c = j["cam"].toObject();
        o.cam.fov = c["fov"].toDouble(39.6);
        o.cam.ortho = c["ortho"].toBool(false);
        o.cam.orthoScale = c["orthoScale"].toDouble(16);
        const QJsonObject p = j["phys"].toObject();
        o.phys.enabled = p["enabled"].toBool(false);
        o.phys.active = p["active"].toBool(true);
        o.phys.mass = p["mass"].toDouble(1);
        o.phys.friction = p["friction"].toDouble(0.5);
        o.phys.bounce = p["bounce"].toDouble(0.4);
        for (const QJsonValue &mv : j["mods"].toArray()) {
            const QJsonObject mj = mv.toObject();
            Modifier md;
            md.type = mj["type"].toString();
            md.on = mj["on"].toBool(true);
            md.ax = mj["ax"].toBool(true);
            md.ay = mj["ay"].toBool(false);
            md.az = mj["az"].toBool(false);
            md.count = mj["count"].toInt(3);
            md.rel = vecOf(mj["rel"], {1.1f, 0, 0});
            md.cst = vecOf(mj["cst"]);
            md.thickness = mj["thickness"].toDouble(0.1);
            md.offset = mj["offset"].toDouble(-1);
            md.width = mj["width"].toDouble(0.1);
            md.op = mj["op"].toString("Difference");
            md.target = mj["target"].toString();
            md.options = mj["options"].toObject();
            o.mods.append(md);
        }
        for (const QJsonValue &kv : j["keys"].toArray()) {
            const QJsonObject kj = kv.toObject();
            DKey k;
            k.ms = kj["ms"].toInt(0);
            k.p.loc = vecOf(kj["loc"]);
            k.p.rot = vecOf(kj["rot"]);
            k.p.scl = vecOf(kj["scl"], {1, 1, 1});
            k.p.opacity = kj["opacity"].toDouble(1);
            k.interp = kj["interp"].toString("Smooth");
            for (const QJsonValue &cv : kj["channels"].toArray()) k.channels << cv.toString();
            o.keys.append(k);
        }
        if (!j.contains("base") && !o.keys.isEmpty()) o.base = o.keys.first().p;
        std::sort(o.keys.begin(), o.keys.end(), [](const DKey &a, const DKey &b) { return a.ms < b.ms; });
        if (root["version"].toInt() >= 4) {
            o.keys.clear();
            for (const auto &tv : j["tracks"].toArray()) {
                const auto tj = tv.toObject();
                PropertyTrack track;
                track.property = tj["property"].toString();
                for (const auto &kv : tj["keys"].toArray()) {
                    const auto kj = kv.toObject();
                    track.setKey(qMax(0, kj["ms"].toInt()), kj["value"].toDouble(), kj["interpolation"].toString("Smooth"));
                    auto &key=track.keys[track.indexAt(qMax(0,kj["ms"].toInt()))];
                    auto left=kj["left"].toArray(),right=kj["right"].toArray();
                    if(left.size()==2&&right.size()==2){key.leftHandle={left[0].toDouble(),left[1].toDouble()};key.rightHandle={right[0].toDouble(),right[1].toDouble()};key.customHandles=kj["customHandles"].toBool();}
                    key.handleType=kj["handleType"].toString(key.customHandles?"Free":"Auto");
                }
                if (!track.keys.isEmpty()) o.tracks.append(track);
            }
            o.refreshLegacyKeys();
        } else o.migrateKeys();
        o.locked = j["locked"].toBool(o.type == "Camera");
        o.style = j["style"].toObject();
        if(!o.style.contains("corner.linked")&&(o.style.contains("corner.tl")||o.style.contains("corner.tr")||o.style.contains("corner.br")||o.style.contains("corner.bl")))o.style["corner.linked"]=false;
        o.defaults=j["defaults"].toObject();o.parentId=j["parent"].toString();
        const auto inverse=j["parentInverse"].toArray();if(inverse.size()==16)for(int row=0;row<4;++row)for(int col=0;col<4;++col)o.parentInverse(row,col)=float(inverse[row*4+col].toDouble());
        d.objects.append(o);
    }
    d.ensureCamera();
    return d;
}

// ------------------------------------------------------------------ physics

namespace {
void localBounds(const DObject &o, QVector3D &mn, QVector3D &mx) {
    if (o.type == "Mesh" && o.mesh.bounds(mn, mx)) return;
    mn = QVector3D(float(-o.w / 2), float(-o.h / 2), -0.01f);
    mx = QVector3D(float(o.w / 2), float(o.h / 2), 0.01f);
}
void aabb(const DObject &o, const DProps &p, const QVector3D &pos, QVector3D &lo, QVector3D &hi) {
    QVector3D mn, mx;
    localBounds(o, mn, mx);
    const QVector3D s(qAbs(p.scl.x()), qAbs(p.scl.y()), qAbs(p.scl.z()));
    // an object turned a quarter way round swaps its extents
    QMatrix4x4 r;
    r.rotate(p.rot.z(), 0, 0, 1);
    r.rotate(p.rot.y(), 0, 1, 0);
    r.rotate(p.rot.x(), 1, 0, 0);
    lo = QVector3D(1e9f, 1e9f, 1e9f);
    hi = QVector3D(-1e9f, -1e9f, -1e9f);
    for (int i = 0; i < 8; ++i) {
        const QVector3D c = r.map(QVector3D((i & 1 ? mx : mn).x(), (i & 2 ? mx : mn).y(), (i & 4 ? mx : mn).z()) * s) + pos;
        lo = QVector3D(qMin(lo.x(), c.x()), qMin(lo.y(), c.y()), qMin(lo.z(), c.z()));
        hi = QVector3D(qMax(hi.x(), c.x()), qMax(hi.y(), c.y()), qMax(hi.z(), c.z()));
    }
}
quint64 mix(quint64 h, quint64 x) { return (h ^ x) * 1099511628211ULL; }
quint64 hashVec(quint64 h, const QVector3D &v) { return mix(mix(mix(h, qHash(v.x())), qHash(v.y())), qHash(v.z())); }
} // namespace

quint64 ElementDesign::physicsSignature() const {
    quint64 h = 1469598103934665603ULL;
    h = mix(h, qHash(world.gravity));
    h = mix(h, world.floor);
    for (const DObject &o : objects) {
        if (!o.phys.enabled) continue;
        h = mix(h, qHash(o.id));
        h = mix(h, o.phys.active);
        h = mix(h, qHash(o.phys.mass));
        h = mix(h, qHash(o.phys.friction));
        h = mix(h, qHash(o.phys.bounce));
        const DProps p = o.at(0);
        h = hashVec(hashVec(hashVec(h, p.loc), p.rot), p.scl);
        h = mix(h, o.mesh.v.size());
        QVector3D mn, mx;
        localBounds(o, mn, mx);
        h = hashVec(hashVec(h, mn), mx);
        if (!o.phys.active) {  // passive bodies may be animated: their later keys matter too
            for (const DKey &k : o.keys) h = hashVec(mix(h, k.ms), k.p.loc);
        }
    }
    return h;
}

void ElementDesign::ensurePhysics(qint64 ms) const {
    const quint64 sig = physicsSignature();
    if (sig != m_physSig || m_physT < 0 || ms + 8 < m_physT) {
        m_phys.clear();
        for (const DObject &o : objects)
            if (o.phys.enabled && o.phys.active) m_phys[o.id] = {o.at(0).loc, QVector3D()};
        m_physSig = sig;
        m_physT = 0;
    }
    while (m_physT < ms) {
        stepPhysics(m_physT);
        m_physT += 8;
    }
}

void ElementDesign::stepPhysics(qint64 tms) const {
    const double dt = 0.008;
    for (const DObject &a : objects) {
        if (!a.phys.enabled || !a.phys.active) continue;
        PhysState &sa = m_phys[a.id];
        sa.vel.setZ(sa.vel.z() - float(world.gravity * dt));
        sa.pos += sa.vel * float(dt);
        const DProps pa = a.at(0);
        QVector3D lo, hi;
        aabb(a, pa, sa.pos, lo, hi);
        const float fr = float(qBound(0.0, 1.0 - a.phys.friction * 0.12, 1.0));
        if (world.floor && lo.z() < 0) {
            sa.pos.setZ(sa.pos.z() - lo.z());
            if (sa.vel.z() < 0) sa.vel.setZ(-sa.vel.z() * float(a.phys.bounce));
            if (qAbs(sa.vel.z()) < 0.35f) sa.vel.setZ(0);
            sa.vel.setX(sa.vel.x() * fr);
            sa.vel.setY(sa.vel.y() * fr);
            aabb(a, pa, sa.pos, lo, hi);
        }
        for (const DObject &b : objects) {
            if (&b == &a || !b.phys.enabled) continue;
            const bool bActive = b.phys.active;
            if (bActive && b.id < a.id) continue;  // each active pair once
            const DProps pb = bActive ? b.at(0) : b.at(tms);
            const QVector3D bpos = bActive ? m_phys[b.id].pos : pb.loc;
            QVector3D blo, bhi;
            aabb(b, pb, bpos, blo, bhi);
            const float ov[3] = {qMin(hi.x(), bhi.x()) - qMax(lo.x(), blo.x()), qMin(hi.y(), bhi.y()) - qMax(lo.y(), blo.y()),
                                 qMin(hi.z(), bhi.z()) - qMax(lo.z(), blo.z())};
            if (ov[0] <= 0 || ov[1] <= 0 || ov[2] <= 0) continue;
            int ax = 0;
            if (ov[1] < ov[ax]) ax = 1;
            if (ov[2] < ov[ax]) ax = 2;
            auto comp = [](const QVector3D &v, int i) { return i == 0 ? v.x() : i == 1 ? v.y() : v.z(); };
            auto setc = [](QVector3D &v, int i, float x) { if (i == 0) v.setX(x); else if (i == 1) v.setY(x); else v.setZ(x); };
            const float sign = comp((lo + hi) / 2, ax) < comp((blo + bhi) / 2, ax) ? -1.0f : 1.0f;
            const float e = float(qMin(a.phys.bounce, b.phys.bounce));
            if (!bActive) {
                setc(sa.pos, ax, comp(sa.pos, ax) + sign * ov[ax]);
                float va = comp(sa.vel, ax);
                if (va * sign < 0) setc(sa.vel, ax, -va * float(a.phys.bounce));
                if (qAbs(comp(sa.vel, ax)) < 0.35f) setc(sa.vel, ax, 0);
                for (int k = 0; k < 3; ++k)
                    if (k != ax) setc(sa.vel, k, comp(sa.vel, k) * fr);
            } else {
                PhysState &sb = m_phys[b.id];
                setc(sa.pos, ax, comp(sa.pos, ax) + sign * ov[ax] / 2);
                setc(sb.pos, ax, comp(sb.pos, ax) - sign * ov[ax] / 2);
                const float ma = float(a.phys.mass), mb = float(b.phys.mass);
                const float va = comp(sa.vel, ax), vb = comp(sb.vel, ax);
                setc(sa.vel, ax, ((ma - e * mb) * va + (1 + e) * mb * vb) / (ma + mb));
                setc(sb.vel, ax, ((mb - e * ma) * vb + (1 + e) * ma * va) / (ma + mb));
            }
            aabb(a, pa, sa.pos, lo, hi);
        }
    }
}

DProps ElementDesign::propsAt(const DObject &o, qint64 ms) const {
    DProps p = o.at(ms);
    if (o.phys.enabled && o.phys.active) {
        ensurePhysics(qMax<qint64>(0, ms));
        const auto it = m_phys.constFind(o.id);
        if (it != m_phys.constEnd()) p.loc = it->pos;
    }
    return p;
}

// ------------------------------------------------------------------ modifiers

namespace {
quint64 meshHash(const Mesh &m) {
    quint64 h = 1469598103934665603ULL;
    h = mix(h, m.v.size());
    h = mix(h, qHashBits(m.v.constData(), size_t(m.v.size()) * sizeof(QVector3D)));
    for (const QVector<int> &L : m.f) {
        h = mix(h, L.size());
        for (int i : L) h = mix(h, quint64(i));
    }
    return h;
}
QString modSig(const Modifier &m) {
    return QString("%1|%2|%3%4%5|%6|%7,%8,%9|%10,%11,%12|%13|%14|%15|%16|%17")
        .arg(m.type).arg(m.on).arg(m.ax).arg(m.ay).arg(m.az).arg(m.count).arg(m.rel.x()).arg(m.rel.y()).arg(m.rel.z())
        .arg(m.cst.x()).arg(m.cst.y()).arg(m.cst.z()).arg(m.thickness).arg(m.offset).arg(m.width).arg(m.op).arg(m.target)+QString::fromUtf8(QJsonDocument(m.options).toJson(QJsonDocument::Compact));
}
} // namespace

const Mesh &ElementDesign::evalMesh(const DObject &o, qint64 ms) const {
    bool any = false;
    for (const Modifier &m : o.mods) any = any || m.on;
    if (!any || m_evalDepth > 3) return o.mesh;

    quint64 sig = meshHash(o.mesh);
    QMatrix4x4 inv;
    const DObject *targets[8] = {};
    for (int i = 0; i < o.mods.size(); ++i) {
        const Modifier &m = o.mods[i];
        sig = mix(sig, qHash(modSig(m)));
        if(m.on&&m.type=="Mirror")if(const auto *target=find(m.target);target&&target!=&o){const auto basis=worldMatrix(o,ms).inverted()*worldMatrix(*target,ms);sig=mix(sig,qHashBits(basis.constData(),16*sizeof(float)));}
        if (m.on && m.type == "Boolean" && i < 8) {
            const DObject *t = find(m.target);
            if (!t || t == &o || t->type != "Mesh") continue;
            targets[i] = t;
            ++m_evalDepth;
            sig = mix(sig, meshHash(evalMesh(*t, ms)));
            --m_evalDepth;
            inv = worldMatrix(o, ms).inverted() * worldMatrix(*t, ms);
            sig = mix(sig, qHashBits(inv.constData(), 16 * sizeof(float)));
        }
    }
    auto hit = m_evalCache.find(o.id);
    if (hit != m_evalCache.end() && hit->first == sig) return hit->second;

    Mesh r = o.mesh;
    for (int i = 0; i < o.mods.size(); ++i) {
        const Modifier &m = o.mods[i];
        if (!m.on) continue;
        if (m.type == "Mirror") {
            QMatrix4x4 basis;
            if(const auto *target=find(m.target);target&&target!=&o)basis=worldMatrix(o,ms).inverted()*worldMatrix(*target,ms);
            r=mirrorGeometry(r,m,basis);
        }
        else if (m.type == "Array") r = Mesh::arrayed(r, m.count, m.rel, m.cst);
        else if (m.type == "Solidify") r = Mesh::solidified(r, m.thickness, m.offset);
        else if (m.type == "Bevel") r = bevelGeometry(r,m);
        else if (m.type == "Boolean" && i < 8 && targets[i]) {
            ++m_evalDepth;
            Mesh t = evalMesh(*targets[i], ms);
            --m_evalDepth;
            t.transform(worldMatrix(o, ms).inverted() * worldMatrix(*targets[i], ms));
            r = Mesh::boolean(r, t, m.op);
        }
    }
    m_evalCache[o.id] = qMakePair(sig, r);
    return m_evalCache[o.id].second;
}

// ------------------------------------------------------------------ textures

// ------------------------------------------------------------------ painting

namespace {

struct Item {
    QPainterPath backdropClip;double backdrop=0,brightness=1;
    QPainterPath clip;
    double depth = 0;
    QPolygonF poly;
    QColor color;
    double alpha = 1;
    bool textured = false;
    QPainter::CompositionMode blend = QPainter::CompositionMode_SourceOver;
    QImage img;
    QTransform tr;
};

struct LightW {
    QVector3D pos, dir;
    LightData d;
};

QVector3D newell(const QVector<QVector3D> &p) {
    QVector3D n;
    for (int k = 0; k < p.size(); ++k) {
        const QVector3D &a = p[k], &b = p[(k + 1) % p.size()];
        n += QVector3D((a.y() - b.y()) * (a.z() + b.z()), (a.z() - b.z()) * (a.x() + b.x()), (a.x() - b.x()) * (a.y() + b.y()));
    }
    return n.normalized();
}

QColor toColor(float r, float g, float b) {
    return QColor::fromRgbF(qBound(0.0f, r, 1.0f), qBound(0.0f, g, 1.0f), qBound(0.0f, b, 1.0f));
}

} // namespace

void ElementDesign::paint(QPainter &p, const QRectF &rect, const ViewCam &cam, qint64 ms, const PaintOpts &o) const {
    if(p.device()->devType()!=QInternal::Image){bool needs=false;for(const auto &object:objects)needs|=object.propertyValue("backdrop.blur",ms)>0||object.propertyValue("backdrop.brightness",ms)!=1;
        if(needs){QImage image(rect.size().toSize(),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter composite(&image);composite.translate(-rect.topLeft());paint(composite,rect,cam,ms,o);composite.end();p.drawImage(rect.topLeft(),image);return;}}
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (o.clip) p.setClipRect(rect, Qt::IntersectClip);

    if (o.background) {
        if (world.gradient) {
            QLinearGradient g(rect.topLeft(), rect.bottomLeft());
            g.setColorAt(0, world.zenith);
            g.setColorAt(1, world.horizon);
            p.fillRect(rect, g);
        } else {
            p.fillRect(rect, world.color);
        }
    }

    QVector<LightW> lights;
    if (o.shade == PaintOpts::Rendered)
        for (const DObject &l : objects)
            if (l.type == "Light" && l.visible) {
                const QMatrix4x4 W = worldMatrix(l, ms);
                lights.append({W.map(QVector3D(0, 0, 0)), W.mapVector(QVector3D(0, 0, -1)).normalized(), l.evaluated(ms).light});
            }

    auto blendMode = [](const QString &mode) {
        const QStringList names{"Normal","Multiply","Screen","Overlay","Darken","Lighten","Color Dodge","Color Burn","Hard Light","Soft Light","Difference","Exclusion"};
        const QPainter::CompositionMode modes[] = {QPainter::CompositionMode_SourceOver,QPainter::CompositionMode_Multiply,QPainter::CompositionMode_Screen,QPainter::CompositionMode_Overlay,QPainter::CompositionMode_Darken,QPainter::CompositionMode_Lighten,QPainter::CompositionMode_ColorDodge,QPainter::CompositionMode_ColorBurn,QPainter::CompositionMode_HardLight,QPainter::CompositionMode_SoftLight,QPainter::CompositionMode_Difference,QPainter::CompositionMode_Exclusion};
        return modes[qMax(0,names.indexOf(mode))];
    };
    const QVector3D camPos = cam.position(), camFwd = cam.forward();
    QVector<Item> items;

    auto projCam = [&](const QVector3D &c, QPointF *out) {
        const double d = -c.z();
        double nx, ny;
        if (cam.ortho) {
            nx = c.x() / (cam.orthoScale / 2);
            ny = c.y() / (cam.orthoScale / 2) * cam.aspect;
        } else {
            nx = c.x() / d * cam.f;
            ny = c.y() / d * cam.f * cam.aspect;
        }
        *out = QPointF(rect.left() + (nx * 0.5 + 0.5) * rect.width(), rect.top() + (0.5 - ny * 0.5) * rect.height());
    };

    auto shadeFace = [&](const Material &m, QVector3D n, const QVector3D &center, double depth) {
        const QVector3D toCam = (cam.ortho ? -camFwd : (camPos - center)).normalized();
        const float base[3] = {float(m.color.redF()), float(m.color.greenF()), float(m.color.blueF())};
        if (o.shade == PaintOpts::Solid) {
            const float lam = qMax(0.0f, QVector3D::dotProduct(n, toCam));
            const float k = 0.32f + 0.68f * lam, sp = qPow(lam, 24.0f) * 0.22f;
            return toColor(base[0] * k + sp, base[1] * k + sp, base[2] * k + sp);
        }
        if (m.shadeless) return m.color;
        const float amb = float(world.ambient);
        float r = base[0] * amb * float(world.ambientColor.redF()), g = base[1] * amb * float(world.ambientColor.greenF()),
              b = base[2] * amb * float(world.ambientColor.blueF());
        float sr = 0, sg = 0, sb = 0;
        const float shin = 8 + 120 * float(1 - qBound(0.0, m.roughness, 1.0)), specK = 0.55f * float(1 - qBound(0.0, m.roughness, 1.0));
        for (const LightW &L : lights) {
            QVector3D ld;
            float I;
            if (L.d.kind == "Sun") {
                ld = -L.dir;
                I = float(L.d.energy) * 0.3f;
            } else {
                const QVector3D v = L.pos - center;
                const float d2 = qMax(0.05f, v.lengthSquared());
                ld = v / qSqrt(d2);
                I = float(L.d.energy) / (4.0f * float(M_PI) * d2) * 0.3f;
                if (L.d.kind == "Spot") {
                    const float half = float(qDegreesToRadians(L.d.spotSize / 2));
                    const float ca = QVector3D::dotProduct(-ld, L.dir), outer = qCos(half), inner = qCos(half * float(1 - L.d.spotBlend));
                    I *= qBound(0.0f, (ca - outer) / (inner - outer + 1e-4f), 1.0f);
                }
            }
            float lam = QVector3D::dotProduct(n, ld);
            if (L.d.kind == "Area") lam = (lam + 0.3f) / 1.3f;  // soft: an area light wraps round
            if (lam <= 0 || I <= 0) continue;
            const float lr = float(L.d.color.redF()) * I, lg = float(L.d.color.greenF()) * I, lb = float(L.d.color.blueF()) * I;
            r += base[0] * lam * lr;
            g += base[1] * lam * lg;
            b += base[2] * lam * lb;
            const QVector3D h = (ld + toCam).normalized();
            const float s = qPow(qMax(0.0f, QVector3D::dotProduct(n, h)), shin) * specK;
            sr += s * lr;
            sg += s * lg;
            sb += s * lb;
        }
        r += base[0] * float(m.emission) + sr;
        g += base[1] * float(m.emission) + sg;
        b += base[2] * float(m.emission) + sb;
        QColor c = toColor(r, g, b);
        if (world.fog > 0 && !cam.ortho) {
            const double f = 1 - qExp(-world.fog * depth);
            c = QColor::fromRgbF(c.redF() * (1 - f) + world.fogColor.redF() * f, c.greenF() * (1 - f) + world.fogColor.greenF() * f,
                                 c.blueF() * (1 - f) + world.fogColor.blueF() * f);
        }
        return c;
    };

    for (const DObject &source : objects) {
        const DObject obj = source.evaluated(ms);
        if (!effectiveVisible(obj) || !obj.isRenderable() || o.skip.contains(obj.id)) continue;
        const DProps pr = propsAt(obj, ms);
        const QMatrix4x4 W = worldMatrix(obj,ms);
        const auto clip=overflowClip(obj,ms,cam,rect);
        const double alpha = qBound(0.0, pr.opacity * obj.mat.alpha, 1.0);
        if (alpha <= 0.003) continue;

        if (obj.type == "Mesh") {
            const Mesh &m = evalMesh(obj, ms);
            if (m.f.isEmpty()) continue;
            const bool flip = W.determinant() < 0;
            QVector<QVector3D> wv(m.v.size()), cv(m.v.size());
            for (int i = 0; i < m.v.size(); ++i) {
                wv[i] = W.map(m.v[i]);
                cv[i] = cam.view.map(wv[i]);
            }
            const bool cull = !o.twoSided && o.shade != PaintOpts::Wire && m.isClosed();
            for (const QVector<int> &L0 : m.f) {
                QVector<int> L = L0;
                if (flip) std::reverse(L.begin(), L.end());
                QVector<QVector3D> wp, vp;
                QVector3D center;
                for (int i : L) {
                    wp << wv[i];
                    vp << cv[i];
                    center += wv[i];
                }
                center /= float(L.size());
                QVector3D n = newell(wp);
                const QVector3D toCam = cam.ortho ? -camFwd : (camPos - center);
                const float facing = QVector3D::dotProduct(n, toCam);
                if (cull && facing <= 0) continue;
                if (facing < 0) n = -n;

                QVector<QVector3D> clipped;  // keep what is in front of the camera
                if (cam.ortho) {
                    clipped = vp;
                } else {
                    for (int k = 0; k < vp.size(); ++k) {
                        const QVector3D &a = vp[k], &b = vp[(k + 1) % vp.size()];
                        const bool ia = -a.z() >= cam.nearClip, ib = -b.z() >= cam.nearClip;
                        if (ia) clipped << a;
                        if (ia != ib) {
                            const float t = float((-cam.nearClip - a.z()) / (b.z() - a.z()));
                            clipped << a + (b - a) * t;
                        }
                    }
                }
                if (clipped.size() < 3) continue;
                Item it;it.clip=clip;
                double depth = 0;
                for (const QVector3D &c : clipped) {
                    QPointF pt;
                    projCam(c, &pt);
                    it.poly << pt;
                    depth += -c.z();
                }
                it.depth = depth / clipped.size();
                it.color = shadeFace(obj.mat, n, center, it.depth);
                it.alpha = alpha;
            it.blend = blendMode(obj.style["blend"].toString());
                it.blend = blendMode(obj.style["blend"].toString());
                items.append(it);
            }
        } else {  // Text / Image: a flat picture on a plane
            const QImage tex = textureFor(obj);
            const double paddingX = qMax(0, tex.width() - qBound(1, qRound(obj.w*kPxPerUnit), 4096)) / (2*kPxPerUnit);
            const double paddingY = qMax(0, tex.height() - qBound(1, qRound(obj.h*kPxPerUnit), 4096)) / (2*kPxPerUnit);
            const double hw = obj.w / 2 + paddingX, hh = obj.h / 2 + paddingY;
            const QVector3D corners[4] = {{float(-hw), float(hh), 0}, {float(hw), float(hh), 0}, {float(hw), float(-hh), 0}, {float(-hw), float(-hh), 0}};
            QPolygonF dst;
            double depth = 0;
            bool ok = true;
            for (const QVector3D &c : corners) {
                QPointF pt;
                double d;
                if (!cam.toScreen(W.map(c), rect, &pt, &d)) {
                    ok = false;
                    break;
                }
                dst << pt;
                depth += d;
            }
            if (!ok || tex.isNull()) continue;
            QPolygonF src;
            src << QPointF(0, 0) << QPointF(tex.width(), 0) << QPointF(tex.width(), tex.height()) << QPointF(0, tex.height());
            Item it;it.clip=clip;
            if (!QTransform::quadToQuad(src, dst, it.tr)) continue;
            it.backdrop=obj.propertyBase("backdrop.blur");it.brightness=obj.propertyBase("backdrop.brightness");
            const double px=paddingX*kPxPerUnit,py=paddingY*kPxPerUnit;
            it.backdropClip=it.tr.map(surfaceOutline(QRectF(px,py,tex.width()-2*px,tex.height()-2*py),obj));
            it.textured = true;
            it.img = tex;
            it.depth = depth / 4;
            it.alpha = alpha;
            it.blend = blendMode(obj.style["blend"].toString());
            items.append(it);
        }
    }

    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.depth > b.depth; });  // far first
    const QTransform base = p.transform();
    for (const Item &it : items) {
        p.save();p.setClipPath(it.clip,Qt::IntersectClip);
        p.setOpacity(it.alpha);
        p.setCompositionMode(it.blend);
        if(it.alpha>0&&(it.backdrop>0||it.brightness!=1)&&p.device()->devType()==QInternal::Image){
            auto *device=static_cast<QImage*>(p.device());const double scale=qSqrt(qAbs((it.tr*base).determinant()));const auto source=device->copy().convertToFormat(QImage::Format_ARGB32_Premultiplied);auto blurred=backdropImage(source,qRound(it.backdrop*scale),it.brightness);
            if(it.alpha<1)for(int y=0;y<blurred.height();++y){auto *row=reinterpret_cast<QRgb*>(blurred.scanLine(y));const auto *old=reinterpret_cast<const QRgb*>(source.constScanLine(y));for(int x=0;x<blurred.width();++x){auto mix=[&](int a,int b){return qRound(a*(1-it.alpha)+b*it.alpha);};row[x]=qRgba(mix(qRed(old[x]),qRed(row[x])),mix(qGreen(old[x]),qGreen(row[x])),mix(qBlue(old[x]),qBlue(row[x])),mix(qAlpha(old[x]),qAlpha(row[x])));}}
            p.save();p.setClipPath(it.backdropClip,Qt::IntersectClip);p.resetTransform();p.setOpacity(1);p.setCompositionMode(QPainter::CompositionMode_Source);p.drawImage(0,0,blurred);p.restore();
        }
        if (it.textured) {
            p.setTransform(it.tr * base);
            p.drawImage(0, 0, it.img);
            p.setTransform(base);
        } else if (o.shade == PaintOpts::Wire) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(170, 182, 200), 1));
            p.drawPolygon(it.poly);
        } else {
            p.setBrush(it.color);
            p.setPen(QPen(it.color, 0.9));  // the thin outline hides hairline gaps between neighbours
            p.drawPolygon(it.poly);
        }
        p.restore();
    }
    p.restore();
}

void ElementDesign::render(QPainter &p, const QRectF &target, qint64 tMs, bool clip, const RenderSettings *rs,bool loopPlayback) const {
    if (isEmpty() || target.width() < 2 || target.height() < 2) return;
    qint64 t = tMs;
    if (durationMs > startMs && loop && loopPlayback && tMs >= durationMs) t = startMs+(tMs-startMs)%(durationMs-startMs);
    else t = qBound<qint64>(0, tMs, qint64(durationMs));
    const double aspect = target.width() / target.height();
    PaintOpts o;
    o.shade = PaintOpts::Rendered;
    o.background = !world.transparent;
    o.clip = clip;

    if (rs && rs->motionBlur && rs->samples > 1) {
        const QSize sz = target.size().toSize();
        const int N = qBound(2, rs->samples, 16);
        QVector<quint32> sum(sz.width() * sz.height() * 4, 0);
        for (int i = 0; i < N; ++i) {
            const qint64 ts = qMax<qint64>(0, t + qint64(((i + 0.5) / N - 0.5) * rs->shutter * rs->frameMs));
            QImage img(sz, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QPainter q(&img);
            PaintOpts oo = o;
            oo.clip = true;
            paint(q, QRectF(0, 0, sz.width(), sz.height()), sceneCamera(ts, aspect), ts, oo);
            q.end();
            for (int y = 0; y < sz.height(); ++y) {
                const uchar *line = img.constScanLine(y);
                quint32 *s = sum.data() + y * sz.width() * 4;
                for (int x = 0; x < sz.width() * 4; ++x) s[x] += line[x];
            }
        }
        QImage out(sz, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < sz.height(); ++y) {
            uchar *line = out.scanLine(y);
            const quint32 *s = sum.constData() + y * sz.width() * 4;
            for (int x = 0; x < sz.width() * 4; ++x) line[x] = uchar(s[x] / N);
        }
        p.save();
        if (clip) p.setClipRect(target, Qt::IntersectClip);
        p.drawImage(target.topLeft(), out);
        p.restore();
        return;
    }
    paint(p, target, sceneCamera(t, aspect), t, o);
}
