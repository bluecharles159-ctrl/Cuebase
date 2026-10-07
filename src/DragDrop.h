#pragma once

#include <QColor>
#include <QListWidget>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QString>
#include <functional>

class QWidget;

// What is being dragged towards the timeline.
//   kind: "program"   an item of the programs queue (reorders it, or copies it onto an extra layer)
//         "ad"        an ad from the Ad Center
//         "element"   a screen element
//         "placement" something already sitting on an extra layer
struct DragPayload {
    QString kind, id, label;
    QColor color{"#475569"};
    int widthPx = 120;      // how wide it would be on the timeline
    int grabDx = 30;        // where inside that width it was picked up
    bool fromTimeline = false;
    bool clickToPlace = false;
    bool valid() const { return !kind.isEmpty() && !id.isEmpty(); }
};

// Something the controller can drop onto (the timeline).
class DropTarget {
public:
    virtual ~DropTarget() = default;
    // The pointer moved. Return true to stop the drag right here and drop it (50% overlap rule).
    virtual bool dragMove(const DragPayload &p, const QPoint &globalPos) = 0;
    virtual void dragDrop(const DragPayload &p, const QPoint &globalPos) = 0;
    virtual void dragLeave() = 0;
};

// A drag that Cuebase draws and steers itself. Unlike QDrag it can be stopped by the target while the mouse
// button is still down, which the timeline does as soon as the dragged block covers half of what is under it.
class DragController : public QObject {
    Q_OBJECT
public:
    static DragController *instance();
    void setTarget(DropTarget *t) { m_target = t; }
    bool active() const { return m_state != Idle; }
    // Starts a drag. Call it from a mouse-move handler while the left button is held.
    void begin(const DragPayload &p, const QPoint &globalPos, const QPixmap &ghost = QPixmap());

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    enum State { Idle, Dragging, Stopped };
    explicit DragController(QObject *parent = nullptr);
    void finish(bool drop, const QPoint &globalPos);
    void end();

    DropTarget *m_target = nullptr;
    QWidget *m_ghost = nullptr;
    QPixmap m_pix;
    DragPayload m_payload;
    State m_state = Idle;
};

// Lets items of a list be picked up and dragged onto the timeline. make() fills the payload for an item
// (return false to make that item undraggable).
void makeListDraggable(QListWidget *list, std::function<bool(QListWidgetItem *, DragPayload &)> make);
