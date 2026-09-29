#pragma once

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QWidget>

#include "Data.h"

class Playout;

// The live monitor: paints the current video/graphics, the on-screen elements and
// the timeline bar with the "Breaking in" countdown.
class ViewportWidget : public QWidget {
    Q_OBJECT
public:
    ViewportWidget(Library *lib, Playout *play, QWidget *parent = nullptr);
    QSize minimumSizeHint() const override { return {320, 200}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawScreen(QPainter &p, const QRect &r);
    void drawElement(QPainter &p, const QRect &r, const Element &e);
    void drawChrome(QPainter &p, const QRect &r);
    QPixmap cachedPixmap(const QString &path);

    Library *m_lib;
    Playout *m_play;
    QImage m_frame;
    QHash<QString, QPixmap> m_pixmaps;
};
