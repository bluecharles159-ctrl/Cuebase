#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

// ---- shared vocabulary ------------------------------------------------------

inline const QStringList &programKinds() {
    static const QStringList k{"Live", "Documentary", "Lifestyle", "News", "Sports"};
    return k;
}
inline const QStringList &elementTypes() {
    static const QStringList t{"Logo", "Banner", "Ad Element", "Break", "Custom"};
    return t;
}
inline const QStringList &transitionKinds() {
    static const QStringList t{"None", "Fade", "Slide Left", "Slide Right", "Slide Up", "Slide Down",
                               "Zoom In", "Zoom Out", "Wipe"};
    return t;
}
inline const QStringList &screenPositions() {
    static const QStringList p{"Top Left", "Top Right", "Bottom Left", "Bottom Right", "Top Center", "Bottom Center"};
    return p;
}
inline QColor kindColor(const QString &kind) {
    if (kind == "Live") return QColor("#dc2626");
    if (kind == "Documentary") return QColor("#2563eb");
    if (kind == "Lifestyle") return QColor("#9333ea");
    if (kind == "News") return QColor("#d97706");
    return QColor("#16a34a");
}
inline QString formatDuration(int seconds) {
    const int h = seconds / 3600, m = seconds / 60 % 60, s = seconds % 60;
    if (h > 0) return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

// ---- transitions ------------------------------------------------------------

// How something moves in when it starts and moves out when it ends.
struct Transition {
    QString in = "None", out = "None";
    int inMs = 600, outMs = 600;
};

// What a transition does to the picture at one moment.
struct FxState {
    double opacity = 1.0;
    double dx = 0.0, dy = 0.0;   // offset as a fraction of the width / height
    double scale = 1.0;
    double reveal = 1.0;         // 0..1 wipe, left to right
};
FxState transitionAt(const Transition &t, qint64 elapsedMs, qint64 totalMs);

// ---- records ----------------------------------------------------------------

struct Ad {
    QString id, name, file;
    int seconds = 15;
    Transition fx;
};

struct Element {
    qint64 lengthMs() const;
    QString id, name, type = "Logo", text, color = "#2563eb", position = "Top Left", image;
    int seconds = 4;    // Break elements: how long the bumper stays on screen
    bool live = false;  // Logo / Banner / Ad Element: currently shown on the viewport
    QString design;     // "Custom" elements: the animated design made in the Build workspace (JSON)
    Transition fx;
};

struct ProgramItem {
    QString id, title, kind = "Live", file, adId, breakIn, breakOut;
    int seconds = 60;
    int offsetMs = 0;        // where in the media file this piece starts (a cut makes a second piece)
    bool slotAfter = false;  // a "+" insert point sits right after this item on the timeline
    QString description, image;  // shown on the program card and in the Programs tab
    Transition fx;

    // kind "Ad" is a standalone ad spot placed on the timeline (its adId points at the ad)
    bool isAdSpot() const { return kind == QLatin1String("Ad"); }
};

// Something laid on one of the extra timeline layers: a screen element, a sound, an ad or a clip.
struct Placement {
    qint64 durationMs=0;
    qint64 lengthMs() const {return durationMs>0?durationMs:qint64(seconds)*1000;}
    QString id, type = "Element", refId, name, file;  // type: "Element", "Sound", "Ad" or "Clip"
    qint64 startMs = 0;                                // position on the timeline
    int seconds = 8;                                   // how long it stays / plays
    int layer = 1;                                     // 1..3 = the second, third and fourth timeline
    int offsetMs = 0;                                  // Clip: where in the media file it starts
    QString kind;                                      // Clip: the program type (for its colour)
};

// Controls for one timeline layer. Layer 0 is the main timeline: it is always the one shown and broadcast.
struct LayerState {
    bool muted = false;
    bool visible = true;
    int volume = 100;
};

struct DisplaySettings {
    QString channel = "TV1 — Main", resolution = "1920x1080";
    int fps = 25;
    double bitrate = 8.5;
    bool watermarkOn = true;
    QString watermarkText = "CUEBASE";
    bool safeArea = false;
    // ---- render-style settings, after Blender's Output / Render properties
    int resScale = 100;                      // resolution percentage
    double pixAspX = 1.0, pixAspY = 1.0;     // pixel aspect (Advanced)
    double fpsBase = 1.0;                    // 1.001 for 29.97 and friends (Advanced)
    bool motionBlur = false;                 // applies to 3D screen elements
    double shutter = 0.5;                    // fraction of a frame the shutter stays open
    int mbSamples = 5;
    QString colorView = "Standard", colorLook = "None";
    double exposure = 0.0, gamma = 1.0;
    bool regionOn = false;                   // only this part of the picture is output
    double rx0 = 0.0, ry0 = 0.0, rx1 = 1.0, ry1 = 1.0;

    int resX() const { return qMax(16, resolution.section('x', 0, 0).toInt()); }
    int resY() const { return qMax(16, resolution.section('x', 1, 1).toInt()); }
    double aspect() const { return qBound(0.25, (resX() * pixAspX) / (resY() * pixAspY), 6.0); }
};

// ---- the library: everything the user creates -------------------------------

class Library : public QObject {
    Q_OBJECT
public:
    explicit Library(QObject *parent = nullptr);

    QList<ProgramItem> programs;
    QList<Ad> ads;
    QList<Element> elements;
    QList<Placement> placements;
    QVector<LayerState> layers = QVector<LayerState>(4);  // 0 = main, 1..3 = extra timelines
    DisplaySettings display;

    // set while the user is filling a "+" on the timeline: what they chose there and where it goes
    QString pendingKind;    // "", "Clip", "Ad Center" or "Element"
    QString pendingAfter;   // id of the timeline item the "+" sits behind
    int pendingLayer = 1;   // "Layer Ad" / "Layer Element": which extra timeline it goes on
    void setPending(const QString &kind, const QString &afterId) {
        pendingKind = kind;
        pendingAfter = afterId;
        emit pendingChanged();
    }
    void clearPending() { setPending(QString(), QString()); }

    const ProgramItem *program(const QString &id) const;
    const Ad *ad(const QString &id) const;
    const Element *element(const QString &id) const;
    int programIndex(const QString &id) const;
    Placement *placement(const QString &id);
    static QString newId();

    // ---- timeline editing (each one commits and notifies) ----
    // Splits a program in two at atSec. Returns the id of the second half ("" if it can't be cut).
    // slot = true leaves a "+" insert point between the halves.
    QString cutProgram(const QString &id, int atSec, bool slot = true);
    // Inserts an item right after another one; the "+" insert point stays in front of what followed.
    void insertAfter(const QString &afterId, ProgramItem item);
    // Inserts an item right in front of another one ("" = at the very end).
    void insertBefore(const QString &beforeId, ProgramItem item);
    // The highest extra layer that has something on it (0 = none, the main timeline only).
    int maxLayer() const;
    // Moves an item so it sits in front of beforeId ("" = at the very end).
    void moveItemBefore(const QString &id, const QString &beforeId);
    void removeItem(const QString &id);
    // Pushes placements at/after `from` later by `by` ms (used when something is inserted before them).
    void shiftPlacements(qint64 from, qint64 by);

    void load();
    void save() const;

    void commitPrograms() { save(); emit programsChanged(); }
    void commitAds() { save(); emit adsChanged(); }
    void commitElements() { save(); emit elementsChanged(); }
    void commitElementLive() { save(); emit elementLiveChanged(); }
    void commitPlacements() { save(); emit placementsChanged(); }
    void commitDisplay() { save(); emit displayChanged(); }
    void commitLayers() { save(); emit layersChanged(); }
    // Asks the main window to open the Build workspace (id "" = a brand-new element) or a program in Programs.
    void requestBuild(const QString &elementId = QString()) { emit buildRequested(elementId); }
    void requestOpenProgram(const QString &programId) { emit openProgramRequested(programId); }

signals:
    void programsChanged();
    void adsChanged();
    void elementsChanged();
    void elementLiveChanged();
    void placementsChanged();
    void pendingChanged();
    void displayChanged();
    void layersChanged();
    void buildRequested(const QString &elementId);
    void openProgramRequested(const QString &programId);

private:
    void seed();
    static QString filePath();
};
