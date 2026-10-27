#pragma once

#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

#include "Data.h"

class Playout;

class WorkCanvas : public QWidget {
    Q_OBJECT
public:
    WorkCanvas(Library *lib, Playout *play, QWidget *parent = nullptr);
    QRect cameraRect() const;
    void setDrawMode(bool on);
    bool drawMode() const { return m_draw; }

signals:
    void audioDroppedOnScene(const QString &audioId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Guide { bool horiz = false; int pos = 0; };
    QRect objectRect(const Element &e) const;
    void writeRect(Element &e, const QRect &r);
    int hitTest(const QPoint &pos) const;
    QRect snapRect(const QRect &r, int skipIndex);
    void drawObject(QPainter &p, const Element &e, const QRect &r, bool selected);
    QPixmap cachedPixmap(const QString &path);

    Library *m_lib;
    Playout *m_play;
    bool m_draw = false;
    bool m_dragging = false;
    bool m_drawing = false;
    QPoint m_press;
    QRect m_drawBox;
    QRect m_dragStart;
    int m_hit = -1;
    QVector<Guide> m_guides;
    QHash<QString, QPixmap> m_pixmaps;
    QPoint m_grab;
};
