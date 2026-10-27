#include "PlanBoard.h"

#include <QGraphicsDropShadowEffect>
#include <QInputDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "Data.h"

PlanBoard::PlanBoard(Library *lib, QWidget *parent) : QWidget(parent), m_lib(lib) {
    setMouseTracking(true);
    connect(lib, &Library::notesChanged, this, [this] { update(); });
}

int PlanBoard::hitTest(const QPoint &pos) const {
    for (int i = m_lib->notes.size() - 1; i >= 0; --i) {
        const PlanNote &n = m_lib->notes[i];
        if (QRect(n.x, n.y, n.w, n.h).contains(pos)) return i;
    }
    return -1;
}

void PlanBoard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#0c1322"));
    p.setPen(QColor("#64748b"));
    p.drawText(QRect(12, 8, width() - 24, 20), "PLAN BOARD  ·  double-click empty space for a note");

    for (const PlanNote &n : m_lib->notes) {
        const QRect r(n.x, n.y, n.w, n.h);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 90));
        p.drawRoundedRect(r.translated(0, 6), 10, 10);
        p.setBrush(QColor(n.color));
        p.setPen(QPen(QColor(0, 0, 0, 40), 1));
        p.drawRoundedRect(r, 10, 10);
        p.setPen(QColor("#1c1917"));
        QFont f("Segoe UI");
        f.setPixelSize(13);
        p.setFont(f);
        p.drawText(r.adjusted(14, 12, -14, -12), Qt::TextWordWrap, n.text);
    }
}

void PlanBoard::mousePressEvent(QMouseEvent *e) {
    m_hit = hitTest(e->pos());
    if (m_hit >= 0) {
        m_dragging = true;
        m_lib->checkpoint();
        m_grab = e->pos() - QPoint(m_lib->notes[m_hit].x, m_lib->notes[m_hit].y);
    }
}

void PlanBoard::mouseMoveEvent(QMouseEvent *e) {
    if (!m_dragging || m_hit < 0) return;
    m_lib->notes[m_hit].x = qMax(8, e->pos().x() - m_grab.x());
    m_lib->notes[m_hit].y = qMax(28, e->pos().y() - m_grab.y());
    update();
}

void PlanBoard::mouseReleaseEvent(QMouseEvent *) {
    if (m_dragging) m_lib->commitNotes();
    m_dragging = false;
}

void PlanBoard::mouseDoubleClickEvent(QMouseEvent *e) {
    m_hit = hitTest(e->pos());
    if (m_hit >= 0) {
        bool ok = false;
        const QString t = QInputDialog::getMultiLineText(this, "Note", "Text", m_lib->notes[m_hit].text, &ok);
        if (ok) {
            m_lib->checkpoint();
            m_lib->notes[m_hit].text = t;
            m_lib->commitNotes();
        }
        return;
    }
    bool ok = false;
    const QString t = QInputDialog::getMultiLineText(this, "New note", "Text", "New note", &ok);
    if (!ok) return;
    m_lib->checkpoint();
    PlanNote n;
    n.id = Library::newId();
    n.text = t;
    n.x = e->pos().x() - 40;
    n.y = e->pos().y() - 20;
    m_lib->notes.append(n);
    m_lib->commitNotes();
}
