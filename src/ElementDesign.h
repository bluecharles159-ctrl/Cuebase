#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QJsonArray>
#include <QMatrix4x4>
#include <QPainterPath>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector3D>
#include <memory>

#include "Mesh.h"

class QPainter;
class RigidBodySimulation;

// A custom screen element made in the Build workspace: a small 3D scene (meshes, text, images, lights and a
// camera) with keyframed animation, modifiers, simple physics and a world. Scene units are metres, Z is up,
// like Blender. The default camera looks along +Y, so "front view" is what a viewer sees.
constexpr int kDesignW = 1920;  // the old 2D canvas, kept for converting designs made before Build went 3D
constexpr int kDesignH = 1080;
constexpr double kPxPerUnit = 120.0;

QStringList interpolationKinds();  // Constant Linear Smooth Ease In Ease Out Back Bounce
double interpolate(const QString &kind, double t);

struct DProps {
    QVector3D loc{0, 0, 0}, rot{0, 0, 0}, scl{1, 1, 1};  // rot in degrees, XYZ euler order
    double opacity = 1.0;
};

struct DKey {
    int ms = 0;
    DProps p;
    QString interp = "Smooth";  // how the move from this key to the next one eases
    // Properties explicitly keyed by the user. Empty means a legacy/full transform key.
    QStringList channels;
};

struct AnimationKey {
    int ms = 0;
    double value = 0;
    QString interpolation = "Bezier";
    // Handle offsets from this key, in milliseconds and property units.
    QPointF leftHandle{-100, 0}, rightHandle{100, 0};
    bool customHandles = false;
    QString handleType="Auto";
};

struct PropertyTrack {
    QString property;
    QVector<AnimationKey> keys;
    double evaluate(qint64 ms, double fallback) const;
    int indexAt(qint64 ms) const;
    void setKey(int ms, double value, const QString &interpolation = QString());
    QPointF handle(int index, bool right) const;
};

QStringList transformChannels();
double channelValue(const DProps &p, const QString &path);
void setChannelValue(DProps &p, const QString &path, double value);

struct Material {
    QColor color{Qt::white};
    double alpha = 1.0, emission = 0.0, roughness = 0.5;
    bool shadeless = false;  // flat colour, ignores lights
};

struct LightData {
    QString kind = "Point";  // Point, Sun, Spot, Area
    QColor color{Qt::white};
    double energy = 1000, spotSize = 45, spotBlend = 0.15, radius = 0.1;
};

struct PhysicsData {
    bool enabled = false, active = true;  // active bodies fall; passive ones just stand in the way
    double mass = 1.0, friction = 0.5, bounce = 0.4;
};

struct Modifier {
    QString type;  // Mirror, Array, Solidify, Bevel, Boolean
    bool on = true;
    bool ax = true, ay = false, az = false;       // Mirror
    int count = 3;                                // Array
    QVector3D rel{1.1f, 0, 0}, cst{0, 0, 0};      // Array: relative (to size) + constant offset
    double thickness = 0.1, offset = -1;          // Solidify
    double width = 0.1;                           // Bevel
    QString op = "Difference", target;            // Boolean
    QJsonObject options;
};

struct CameraData {
    double fov = 39.6;  // horizontal, degrees
    bool ortho = false;
    double orthoScale = 16;  // width in units when orthographic
};

struct DObject {
    QString id, name, type = "Mesh";  // Mesh, Text, Image, Light, Camera
    Mesh mesh;
    QString text = "Text";
    int fontPx = 96;
    QString image;
    double w = 6, h = 1.5;  // Text / Image: size of the plane in units
    // CSS-inspired 2D layout controls. They are stored independently from 3D transform.
    double insetLeft = 0, insetRight = 0, insetTop = 0, insetBottom = 0;
    double borderRadius = 0;
    Material mat;
    LightData light;
    PhysicsData phys;
    CameraData cam;
    QVector<Modifier> mods;
    bool visible = true, locked = false;
    // Base/default transform. It exists independently from animation keys.
    // An object is allowed to have zero animation keys.
    DProps base;
    QVector<DKey> keys;  // legacy/compatibility key storage; frame 0 is no longer mandatory
    QVector<PropertyTrack> tracks;
    QJsonObject style;
    QJsonObject defaults;
    QString parentId;
    QMatrix4x4 parentInverse;
    double propertyDefault(const QString &path) const;
    void captureDefaults();
    double propertyBase(const QString &path) const;
    double propertyValue(const QString &path, qint64 ms) const;
    void setPropertyBase(const QString &path, double value);
    DObject evaluated(qint64 ms) const;
    void fitTextLayout();
    // Unkeyed edits override evaluation until the playhead moves; they never alter animation keys.
    QHash<QString, double> overrides;
    const PropertyTrack *track(const QString &property) const;
    PropertyTrack *track(const QString &property);
    void keyChannel(const QString &property, int ms, double value);
    void migrateKeys();
    void refreshLegacyKeys();

    DProps at(qint64 ms) const;  // eased interpolation between the keys
    int keyIndexNear(qint64 ms, int toleranceMs = 1) const;
    void setKey(qint64 ms, const DProps &p);
    bool isRenderable() const { return type == "Mesh" || type == "Text" || type == "Image" || type == "Shape"; }
};

struct World {
    QColor color{"#10131c"}, horizon{"#8fb0d6"}, zenith{"#1d3a6b"}, ambientColor{Qt::white}, fogColor{"#9aa7b8"};
    bool gradient = false;
    bool transparent = true;  // film transparent: the scene is laid over the video
    double ambient = 0.35, fog = 0.0;
    double gravity = 9.81;
    bool floor = true;        // physics: a ground at z = 0
};

// A point of view: the scene camera, or the user's orbit view in the editor.
struct ViewCam {
    QMatrix4x4 view;  // world -> camera (camera looks down -Z)
    bool ortho = false;
    double f = 2.7475;  // 1 / tan(fov / 2), horizontal
    double orthoScale = 16;
    double aspect = 16.0 / 9.0;
    double nearClip = 0.05;

    bool toScreen(const QVector3D &w, const QRectF &r, QPointF *out, double *depth = nullptr) const;
    void ray(const QPointF &px, const QRectF &r, QVector3D *origin, QVector3D *dir) const;
    QVector3D position() const;
    QVector3D forward() const;
};

struct PaintOpts {
    enum Shade { Wire, Solid, Rendered } shade = Rendered;
    bool background = false;  // fill with the world colour
    bool clip = true;
    bool twoSided = false;
    QSet<QString> skip;
};

struct RenderSettings {  // how the display settings reach the 3D renderer
    bool motionBlur = false;
    double shutter = 0.5;  // fraction of a frame
    int samples = 5;
    double frameMs = 40;
};

struct ElementDesign {
    QJsonArray animationCues;
    QVector3D cursor;
    QVector<DObject> objects;
    World world;
    int durationMs = 5000;
    int startMs = 0;
    bool loop = true;

    bool isEmpty() const;  // nothing to draw
    QString toJson() const;
    static ElementDesign fromJson(const QString &json);
    static ElementDesign defaultScene();  // an empty broadcast composition with a camera and key light

    DObject *find(const QString &id);
    const DObject *find(const QString &id) const;
    DObject *camera();
    const DObject *camera() const;
    void ensureCamera();

    static QMatrix4x4 matrixOf(const DProps &p);
    DProps propsAt(const DObject &o, qint64 ms) const;  // keys + physics
    QMatrix4x4 physicsWorldMatrix(const DObject &o,qint64 ms) const;
    QMatrix4x4 worldMatrix(const DObject &o, qint64 ms) const;
    QMatrix4x4 parentMatrix(const DObject &o, qint64 ms) const;
    bool effectiveVisible(const DObject &o) const;
    QPainterPath overflowClip(const DObject &o, qint64 ms, const ViewCam &cam, const QRectF &rect) const;
    const Mesh &evalMesh(const DObject &o, qint64 ms) const;  // modifiers applied
    ViewCam sceneCamera(qint64 ms, double aspect) const;
    QImage textureFor(const DObject &o) const;

    // Draws the scene seen from cam into rect.
    void paint(QPainter &p, const QRectF &rect, const ViewCam &cam, qint64 ms, const PaintOpts &opts) const;
    // Draws what the scene camera sees into target (what gets broadcast).
    void render(QPainter &p, const QRectF &target, qint64 tMs, bool clip = true, const RenderSettings *rs = nullptr,bool loopPlayback=true) const;

    mutable QHash<QString, QPair<quint64, Mesh>> m_evalCache;
    mutable QHash<QString, QPair<QString, QImage>> m_texCache;
    mutable std::shared_ptr<RigidBodySimulation> m_physics;
    mutable int m_evalDepth = 0;

private:
    void ensurePhysics(qint64 ms) const;
    quint64 physicsSignature() const;
};
