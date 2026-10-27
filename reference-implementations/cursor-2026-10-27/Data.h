#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

inline const QStringList &programKinds() {
    static const QStringList k{"Live", "Documentary", "Lifestyle", "News", "Sports"};
    return k;
}
inline const QStringList &elementTypes() {
    static const QStringList t{"Logo", "Banner", "Ad Element", "Break", "Rectangle"};
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
inline QString formatMs(qint64 ms) {
    ms = qMax<qint64>(0, ms);
    const int s = int(ms / 1000), f = int((ms % 1000) / 10);
    const int m = s / 60, sec = s % 60;
    return QString("%1:%2.%3").arg(m).arg(sec, 2, 10, QChar('0')).arg(f, 2, 10, QChar('0'));
}

struct Ad {
    QString id, name, file;
    int seconds = 15;
};

struct Element {
    QString id, name, type = "Logo", text, color = "#2563eb", position = "Top Left", image;
    int seconds = 4;
    bool live = false;
    bool freeform = false;
    double x = 0.12, y = 0.12, w = 0.22, h = 0.14;
    double noise = 0;
    bool physics = false;
    double gravity = 420;
};

struct ProgramItem {
    QString id, title, kind = "Live", file, adId, breakIn, breakOut;
    int seconds = 60;
    int expandedWidth = 0;
    int inMs = 0;
};

struct AudioAsset {
    QString id, name, file;
    int seconds = 8;
    double speed = 1.0;
    double fadeIn = 0;
    double fadeOut = 0;
    double echo = 0;
    double offset = 0;
};

struct TimelineAudio {
    QString id, audioId;
    int startMs = 0;
    int seconds = 8;
    int expandedWidth = 0;
    bool inScene = false;
};

struct PlanNote {
    QString id, text;
    int x = 32, y = 32, w = 220, h = 148;
    QString color = "#f5e6c8";
};

struct DisplaySettings {
    QString channel = "TV1 — Main", resolution = "1920x1080";
    int fps = 25;
    double bitrate = 8.5;
    bool watermarkOn = true;
    QString watermarkText = "CUEBASE";
    bool safeArea = false;
};

class Library : public QObject {
    Q_OBJECT
public:
    explicit Library(QObject *parent = nullptr);

    QList<ProgramItem> programs;
    QList<Ad> ads;
    QList<Element> elements;
    QList<AudioAsset> audios;
    QList<TimelineAudio> timelineAudio;
    QList<PlanNote> notes;
    DisplaySettings display;
    QString selectedKind;
    QString selectedId;

    const ProgramItem *program(const QString &id) const;
    ProgramItem *programRef(const QString &id);
    const Ad *ad(const QString &id) const;
    const Element *element(const QString &id) const;
    Element *elementRef(const QString &id);
    const AudioAsset *audio(const QString &id) const;
    AudioAsset *audioRef(const QString &id);
    const TimelineAudio *tlAudio(const QString &id) const;
    TimelineAudio *tlAudioRef(const QString &id);
    int programIndex(const QString &id) const;
    static QString newId();

    void load();
    void save() const;

    void checkpoint();
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    void undo();
    void redo();

    void select(const QString &kind, const QString &id);
    bool cutProgram(const QString &id, double ratio);
    bool trimProgram(const QString &id, double ratio);
    QString addTimelineAudio(const QString &audioId, int startMs, bool inScene = false);
    void setAudioInScene(const QString &timelineOrAssetId, bool on);

    void commitPrograms() { finishEdit("programs"); }
    void commitAds() { finishEdit("ads"); }
    void commitElements() { finishEdit("elements"); }
    void commitElementLive() { save(); emit elementLiveChanged(); }
    void commitDisplay() { save(); emit displayChanged(); }
    void commitAudio() { finishEdit("audio"); }
    void commitTimelineAudio() { finishEdit("timelineAudio"); }
    void commitNotes() { finishEdit("notes"); }

signals:
    void programsChanged();
    void adsChanged();
    void elementsChanged();
    void elementLiveChanged();
    void displayChanged();
    void audioChanged();
    void timelineAudioChanged();
    void notesChanged();
    void selectionChanged();
    void historyChanged();

private:
    void seed();
    static QString filePath();
    QByteArray snapshot() const;
    void applySnapshot(const QByteArray &raw, bool emitAll);
    void finishEdit(const QString &what);
    void emitFor(const QString &what);

    QVector<QByteArray> m_undo, m_redo;
    bool m_restoring = false;
};
