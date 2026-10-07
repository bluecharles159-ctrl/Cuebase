#pragma once

#include <QPixmap>
#include <QScrollArea>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include "Data.h"

class Playout;
class QHBoxLayout;
class QPropertyAnimation;
class QWheelEvent;

// One program as a poster card: picture, status pill, type badge, title, and its time slot underneath.
// Click it to open the program in the Programs tab; drag it onto the timeline to move it or copy it to a layer.
class ProgramCard : public QWidget {
    Q_OBJECT
public:
    static constexpr int kW = 150, kPosterH = 208, kH = 238;
    explicit ProgramCard(const ProgramItem &p, QWidget *parent = nullptr);
    QString programId() const { return m_p.id; }
    void setState(const QString &state);   // "now", "next", "queued" or "played"
    void setTimeText(const QString &t);
    bool played() const { return m_state == "played"; }

signals:
    void clicked(const QString &programId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    ProgramItem m_p;
    QPixmap m_poster;
    QString m_state = "queued", m_time;
    QPoint m_press;
    bool m_down = false, m_hover = false;
};

// The programs queue: the playing program sits at the left; finished ones slide off to the
// left (faded), can be scrolled back to, snap away after ~4.5s and disappear after ~10 min.
class QueueStrip : public QScrollArea {
    Q_OBJECT
public:
    static constexpr int kHeight = ProgramCard::kH + 26;
    QueueStrip(Library *lib, Playout *play, QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuild();
    void refreshStates();
    void refreshTimes();
    int targetX() const;
    void scrollToTarget(bool animate);

    Library *m_lib;
    Playout *m_play;
    QWidget *m_host;
    QHBoxLayout *m_lay;
    QWidget *m_tail;
    QVector<ProgramCard *> m_cards;
    QWidget *m_empty = nullptr;
    QTimer m_snapTimer, m_expireTimer, m_clockTimer;
    QPropertyAnimation *m_anim;
    bool m_programmatic = false;
    int m_lastCurrent = -2;
};
