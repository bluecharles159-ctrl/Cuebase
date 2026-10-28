#pragma once

#include <QPoint>
#include <QElapsedTimer>
#include <QTimer>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QPointer>

#include "DragDrop.h"

class Library;
class Playout;
class QScrollBar;
class TimelineMedia;

// Paints the broadcast timeline: programs, break (BR) bumpers and ads on the main lane, a draggable playhead
// and the hover "cut" tool. Under it sit up to three extra timelines (layers) for screen elements, sounds, ads
// and clips. They stay hidden until something is on them, or until something is dragged below the main lane.
// The main timeline is the one shown in the viewport and broadcast.
class TimelineWidget : public QWidget, public DropTarget {
    Q_OBJECT
public:
    TimelineWidget(Playout *play, Library *lib, QWidget *parent = nullptr);
    QSize sizeHint() const override { return {m_totalW, wantedHeight()}; }
    QSize minimumSizeHint() const override { return {m_totalW, wantedHeight()}; }
    int wantedHeight() const;
    void fitView();
    void zoomView(double factor);
    void revealPlayhead();
    void setFixedPlayhead(bool enabled);
    Q_INVOKABLE void reloadPreferences();
    bool fixedPlayhead() const { return m_fixedPlayhead; }
    bool followingPlayhead() const { return m_fixedPlayhead && !m_followPaused; }
    int blockX(int index) const { return index >= 0 && index < m_blocks.size() ? m_blocks[index].x : 0; }
    int blockW(int index) const { return index >= 0 && index < m_blocks.size() ? m_blocks[index].w : 0; }
    QRect placementRect(const QString &id) const { for(const auto &block:m_pblocks)if(block.id==id)return block.rect;return {}; }

    // DropTarget
    bool dragMove(const DragPayload &p, const QPoint &globalPos) override;
    void dragDrop(const DragPayload &p, const QPoint &globalPos) override;
    void dragLeave() override;

signals:
    void segmentClicked(int index);
    void cutRequested(const QString &programId, int atSec);
    void addRequested(const QString &afterProgramId, const QPoint &globalPos);
    void spotRemoveRequested(const QString &spotId);
    void placementRemoveRequested(const QString &id);
    void placementLengthRequested(const QString &id);
    // lane 0 = the main timeline (insert in front of beforeId, "" = at the end); 1..3 = an extra timeline at startMs
    void dropped(const DragPayload &payload, int lane, qint64 startMs, const QString &beforeId);
    void heightWanted(int height);
    void replayRequested(qint64 from,qint64 to);
    void navigationChanged();

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched,QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    struct Block { int x, w; };
    struct Slot { QRect rect; QString programId; };
    struct PBlock { QString id, type; QRect rect; int lane; };
    struct Insert { bool valid = false; QString beforeId; int x = 0; int seg = -1; };
    enum class Drag { None, Playhead, Press, Foreign, TrimLeft, TrimRight, Pan, Range };
    qint64 m_rangeStart=-1,m_rangeEnd=-1;
    double m_pixelsPerSecond=14;
    bool m_fixedPlayhead=true,m_followPaused=false;
    bool m_thumbnails=true,m_waveforms=true;
    TimelineMedia *m_media=nullptr;
    QString m_selectedPlacement;
    int mainHeight() const { return 40+(m_thumbnails?48:0)+(m_waveforms?20:0); }
    int labelTop() const { return 36+mainHeight()+4; }
    int laneTop() const { return labelTop()+20; }
    int m_originX=10;
    QPointer<QScrollBar> m_navigationBar;
    int viewportWidth() const;
    void followPlayhead();
    void pauseFollowing();
    QElapsedTimer m_glassClock;
    QTimer m_glassTimer;
    QString m_trimId;
    bool m_trimProgram=false;
    qint64 m_trimGroupEnd=0;
    qint64 m_trimStart=0,m_trimLength=0;
    int m_trimOffset=0,m_panScroll=0;

    void relayout();
    void layoutPlacements();
    int msToX(qint64 ms) const;
    qint64 xToMs(double x) const;
    int playheadX() const;
    int endX() const;
    int lanesShown() const;
    int laneY(int lane) const;
    int laneAt(const QPoint &pos) const;              // extra lane under pos (1..3), 0 if none
    void notifyHeight();
    int blockAt(const QPoint &pos) const;             // main-lane block under pos, -1 if none
    bool cutTarget(int *segment, int *atSec, int *lineX) const;
    bool overScissors(const QPoint &pos) const;
    const PBlock *hitPlacement(const QPoint &pos) const;
    const Slot *hitSlot(const QPoint &pos) const;
    bool resolveMainDrop(const DragPayload &d, const QPoint &pos);  // true = covered by half: drop now
    QScrollBar *hbar() const;
    void autoScroll(int localX);
    void updateCursor();

    Playout *m_play;
    Library *m_lib;
    QVector<Block> m_blocks;
    QVector<Slot> m_slots;
    QVector<PBlock> m_pblocks;
    int m_totalW = 400, m_lastH = 0;

    Drag m_drag = Drag::None;
    int m_dragX = 0;
    DragPayload m_cand;      // what a press on a block would pick up
    int m_candSeg = -1;
    QString m_liftedId;      // the item being carried away (drawn faded where it was)
    QPoint m_pressPos, m_hover;
    bool m_moved = false, m_hasHover = false;

    // drag-over state
    bool m_reveal = false;
    int m_dropLane = 0;
    qint64 m_dropMs = 0;
    Insert m_ins;
};
