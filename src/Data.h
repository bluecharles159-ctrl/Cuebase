#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

// ---- shared vocabulary ------------------------------------------------------

inline const QStringList &programKinds() {
    static const QStringList k{"Live", "Documentary", "Lifestyle", "News", "Sports"};
    return k;
}
inline const QStringList &elementTypes() {
    static const QStringList t{"Logo", "Banner", "Ad Element", "Break"};
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

// ---- records ----------------------------------------------------------------

struct Ad {
    QString id, name, file;
    int seconds = 15;
};

struct Element {
    QString id, name, type = "Logo", text, color = "#2563eb", position = "Top Left", image;
    int seconds = 4;    // Break elements: how long the bumper stays on screen
    bool live = false;  // Logo / Banner / Ad Element: currently shown on the viewport
};

struct ProgramItem {
    QString id, title, kind = "Live", file, adId, breakIn, breakOut;
    int seconds = 60;
};

struct DisplaySettings {
    QString channel = "TV1 — Main", resolution = "1920x1080";
    int fps = 25;
    double bitrate = 8.5;
    bool watermarkOn = true;
    QString watermarkText = "CUEBASE";
    bool safeArea = false;
};

// ---- the library: everything the user creates -------------------------------

class Library : public QObject {
    Q_OBJECT
public:
    explicit Library(QObject *parent = nullptr);

    QList<ProgramItem> programs;
    QList<Ad> ads;
    QList<Element> elements;
    DisplaySettings display;

    const ProgramItem *program(const QString &id) const;
    const Ad *ad(const QString &id) const;
    const Element *element(const QString &id) const;
    int programIndex(const QString &id) const;
    static QString newId();

    void load();
    void save() const;

    void commitPrograms() { save(); emit programsChanged(); }
    void commitAds() { save(); emit adsChanged(); }
    void commitElements() { save(); emit elementsChanged(); }
    void commitElementLive() { save(); emit elementLiveChanged(); }
    void commitDisplay() { save(); emit displayChanged(); }

signals:
    void programsChanged();
    void adsChanged();
    void elementsChanged();
    void elementLiveChanged();
    void displayChanged();

private:
    void seed();
    static QString filePath();
};
