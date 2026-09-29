#pragma once

#include <QScrollArea>
#include <QTimer>
#include <QVector>

class Library;
class Playout;
class QFrame;
class QHBoxLayout;
class QPropertyAnimation;
class QWheelEvent;

// The programs queue: the playing program sits at the left; finished ones slide off to the
// left (faded), can be scrolled back to, snap away after ~4.5s and disappear after ~10 min.
class QueueStrip : public QScrollArea {
    Q_OBJECT
public:
    QueueStrip(Library *lib, Playout *play, QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuild();
    void refreshStates();
    int targetX() const;
    void scrollToTarget(bool animate);

    Library *m_lib;
    Playout *m_play;
    QWidget *m_host;
    QHBoxLayout *m_lay;
    QWidget *m_tail;
    QVector<QFrame *> m_cards;
    QTimer m_snapTimer, m_expireTimer;
    QPropertyAnimation *m_anim;
    bool m_programmatic = false;
    int m_lastCurrent = -2;
};
