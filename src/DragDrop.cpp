#include "DragDrop.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWidget>

namespace {

// The floating picture that follows the pointer.
class Ghost : public QWidget {
public:
    Ghost() : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    void setPixmap(const QPixmap &p) {
        m_pix = p;
        resize(int(p.width() / p.devicePixelRatio()), int(p.height() / p.devicePixelRatio()));
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setOpacity(0.82);
        p.drawPixmap(0, 0, m_pix);
    }

private:
    QPixmap m_pix;
};

QPixmap makeGhost(const DragPayload &d) {
    const int w = qBound(70, d.widthPx, 260), h = 40;
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(d.color);
    p.drawRoundedRect(QRectF(1, 1, w - 2, h - 2), 8, 8);
    QFont f;
    f.setPixelSize(12);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(8, 0, w - 16, h), Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(f).elidedText(d.label, Qt::ElideRight, w - 16));
    return pm;
}

} // namespace

DragController *DragController::instance() {
    static DragController *c = new DragController(qApp);
    return c;
}

DragController::DragController(QObject *parent) : QObject(parent) {}

void DragController::begin(const DragPayload &p, const QPoint &globalPos, const QPixmap &ghost) {
    if (m_state != Idle || !p.valid()) return;
    m_payload = p;
    m_pix = ghost.isNull() ? makeGhost(p) : ghost;
    auto *g = new Ghost;
    g->setPixmap(m_pix);
    m_ghost = g;
    m_ghost->move(globalPos - QPoint(m_payload.grabDx, m_pix.height() / 2));
    m_ghost->show();
    m_state = Dragging;
    QApplication::setOverrideCursor(Qt::ClosedHandCursor);
    qApp->installEventFilter(this);
    if (m_target) m_target->dragMove(m_payload, globalPos);
}

bool DragController::eventFilter(QObject *, QEvent *e) {
    if (m_state == Idle) return false;
    switch (e->type()) {
    case QEvent::MouseMove: {
        auto *m = static_cast<QMouseEvent *>(e);
        const QPoint g = m->globalPosition().toPoint();
        if (m_state == Dragging) {
            m_ghost->move(g - QPoint(m_payload.grabDx, m_pix.height() / 2));
            if (m_target && m_target->dragMove(m_payload, g) && !m_payload.clickToPlace) finish(true, g);
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto *m = static_cast<QMouseEvent *>(e);
        if (m->button() != Qt::LeftButton) return true;
        const QPoint g = m->globalPosition().toPoint();
        if (m_state == Dragging) finish(true, g);
        end();
        return true;
    }
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent *>(e)->key() == Qt::Key_Escape) {
            if (m_state == Dragging) finish(false, QPoint());
            end();
            return true;
        }
        return false;
    case QEvent::ApplicationDeactivate:
        if (m_state == Dragging) finish(false, QPoint());
        end();
        return false;
    default:
        return false;
    }
}

// Drops (or cancels) but keeps swallowing the mouse until the button is let go.
void DragController::finish(bool drop, const QPoint &globalPos) {
    if (m_state != Dragging) return;
    m_state = Stopped;
    if (m_ghost) {
        m_ghost->hide();
        m_ghost->deleteLater();
        m_ghost = nullptr;
    }
    QApplication::restoreOverrideCursor();
    if (m_target) {
        if (drop) m_target->dragDrop(m_payload, globalPos);
        m_target->dragLeave();
    }
}

void DragController::end() {
    if (m_state == Dragging) finish(false, QPoint());
    m_state = Idle;
    qApp->removeEventFilter(this);
}

// ------------------------------------------------------------- list helper

namespace {
class ListDrag : public QObject {
public:
    ListDrag(QListWidget *l, std::function<bool(QListWidgetItem *, DragPayload &)> mk) : QObject(l), m_list(l), m_make(std::move(mk)) {
        l->viewport()->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *, QEvent *e) override {
        switch (e->type()) {
        case QEvent::MouseButtonPress: {
            auto *m = static_cast<QMouseEvent *>(e);
            m_item = m->button() == Qt::LeftButton ? m_list->itemAt(m->position().toPoint()) : nullptr;
            m_press = m->position().toPoint();
            break;
        }
        case QEvent::MouseMove: {
            auto *m = static_cast<QMouseEvent *>(e);
            if (!m_item || !(m->buttons() & Qt::LeftButton) || DragController::instance()->active()) break;
            if ((m->position().toPoint() - m_press).manhattanLength() < 8) break;
            DragPayload p;
            QListWidgetItem *it = m_item;
            m_item = nullptr;
            if (m_make(it, p)) {
                DragController::instance()->begin(p, m->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
            m_item = nullptr;
            break;
        default:
            break;
        }
        return false;
    }

private:
    QListWidget *m_list;
    std::function<bool(QListWidgetItem *, DragPayload &)> m_make;
    QListWidgetItem *m_item = nullptr;
    QPoint m_press;
};
} // namespace

void makeListDraggable(QListWidget *list, std::function<bool(QListWidgetItem *, DragPayload &)> make) {
    new ListDrag(list, std::move(make));
}
