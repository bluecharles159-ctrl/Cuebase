#pragma once

#include <QFrame>

// The small popup shown when the "+" between two cut pieces is pressed:
// Clip, Sound, Ad Center and Element. Emits chosen("Clip" | "Sound" | "Ad Center" | "Element").
class AddMenu : public QFrame {
    Q_OBJECT
public:
    explicit AddMenu(QWidget *parent = nullptr);
    void popupAbove(const QPoint &globalAnchor);  // opens centred above the "+" button

signals:
    void chosen(const QString &what);
};
