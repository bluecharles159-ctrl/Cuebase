#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include "Data.h"
#include "ElementDesign.h"

class Playout;
class QMediaPlayer;
class QVideoSink;
struct Segment;

// The live monitor: paints the current video/graphics, the on-screen elements and the timeline bar.
// Hovering it shows a scrubber: drag it to look ahead or back on your own screen. That preview playhead
// never touches the broadcast; "Current" snaps the monitor back to what is on air.
class ViewportWidget : public QWidget {
    Q_OBJECT
public:
    ViewportWidget(Library *lib, Playout *play, QWidget *parent = nullptr);
    QSize minimumSizeHint() const override { return {320, 200}; }
    bool detached() const { return m_detached; }
    qint64 viewMs() const { return m_viewMs; }
    QImage programFrame(QSize size);

signals:
    void viewModeChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Geo {
        QRect screen, chrome, bar, track, trackHit, playBtn, currentBtn;
    };
    // what the monitor is showing right now: the broadcast, or the preview playhead
    struct View {
        const Segment *seg = nullptr;
        qint64 posMs = 0;        // position on the timeline
        qint64 segElapsed = 0;
        qint64 tcMs = 0;
        bool paused = false;
        bool onAir = false;
        const QImage *frame = nullptr;
    };

    Geo viewGeo() const;
    View makeView() const;
    bool barVisible() const { return m_hover || m_detached || m_scrub; }
    void drawScreen(QPainter &p, const QRect &r, const View &v, bool guides = true);
    void drawElement(QPainter &p, const QRect &r, const Element &e, qint64 tMs);
    const ElementDesign &designFor(const Element &e);
    struct Range { int a = -1, b = -1; };
    Range scrubRange() const;                       // the segments of the one program the scrubber covers
    double rangeUnit(qint64 ms, const Range &r) const;
    qint64 rangeMs(double unit, const Range &r) const;
    void drawChrome(QPainter &p, const QRect &r, const View &v);
    void drawScrubber(QPainter &p, const Geo &g, const View &v);
    QPixmap cachedPixmap(const QString &path);

    void detachAt(qint64 ms);
    void scrubTo(int x);
    void goCurrent();
    void syncViewer(bool forceSeek);
    void tickViewer();

    Library *m_lib;
    Playout *m_play;
    QImage m_frame, m_vframe;
    QHash<QString, QPixmap> m_pixmaps;
    QHash<QString, QPair<QString, ElementDesign>> m_designs;  // parsed custom designs, by element id

    // the preview player: its own silent media player, never the broadcast's
    QMediaPlayer *m_vplayer = nullptr;
    QVideoSink *m_vsink = nullptr;
    QTimer m_vtimer;
    QElapsedTimer m_vclock;
    qint64 m_vlast = 0, m_viewMs = 0, m_vpending = -1;
    int m_viewSeg = -1;
    QPoint m_hoverPt{-1, -1};
    bool m_detached = false, m_viewPlaying = true, m_hover = false, m_scrub = false;
};
