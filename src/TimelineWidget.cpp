#include "TimelineWidget.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygon>

#include "Data.h"
#include "Playout.h"

TimelineWidget::TimelineWidget(Playout *play, QWidget *parent) : QWidget(parent), m_play(play) {
    connect(play, &Playout::segmentsChanged, this, [this] { relayout(); update(); });
    connect(play, &Playout::segmentChanged, this, [this] { update(); });
    connect(play, &Playout::stateChanged, this, [this] { update(); });
    connect(play, &Playout::ticked, this, [this] { update(); });
    setCursor(Qt::PointingHandCursor);
    relayout();
}

void TimelineWidget::relayout() {
    m_blocks.clear();
    int x = 10;
    for (const Segment &s : m_play->segments()) {
        const int sec = s.ms / 1000;
        int w = s.type == Segment::Program ? qBound(140, int(sec * 1.2), 720)
              : s.type == Segment::Ad      ? qBound(90, sec * 3, 240)
                                           : 60;
        m_blocks.append({x, w});
        x += w + 4;
    }
    m_totalW = qMax(400, x + 10);
    setMinimumWidth(m_totalW);
    updateGeometry();
}

void TimelineWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto &segs = m_play->segments();

    if (segs.isEmpty()) {
        p.setPen(QColor("#64748b"));
        p.drawText(rect(), Qt::AlignCenter, "The timeline is empty. Add programs in the Program Planner.");
        return;
    }

    const int top = 6, h = 52;
    int startSec = 0;
    for (int i = 0; i < segs.size(); ++i) {
        const Segment &s = segs[i];
        const QRect r(m_blocks[i].x, top, m_blocks[i].w, h);
        QColor c = s.type == Segment::Program ? kindColor(s.kind)
                 : s.type == Segment::Ad      ? QColor("#16a34a")
                                                : QColor("#f59e0b");
        const bool isCurrent = i == m_play->index();
        p.setPen(isCurrent ? QPen(Qt::white, 2) : Qt::NoPen);
        p.setBrush(isCurrent ? c : c.darker(160));
        p.drawRoundedRect(r, 8, 8);

        p.setPen(Qt::white);
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(12);
        p.setFont(f);
        const QRect inner = r.adjusted(8, 4, -8, -4);
        if (s.type == Segment::Break) {
            p.drawText(r, Qt::AlignCenter, "BR");
        } else {
            const QFontMetrics fm(f);
            p.drawText(inner, Qt::AlignLeft | Qt::AlignTop,
                       fm.elidedText((s.type == Segment::Ad ? "AD · " : "") + s.title, Qt::ElideRight, inner.width()));
            f.setBold(false);
            f.setPixelSize(11);
            p.setFont(f);
            p.setPen(QColor(255, 255, 255, 190));
            p.drawText(inner, Qt::AlignLeft | Qt::AlignBottom,
                       (s.type == Segment::Program ? s.kind + " · " : "") + formatDuration(s.ms / 1000));
        }

        p.setPen(QColor("#64748b"));
        QFont tf = font();
        tf.setPixelSize(10);
        p.setFont(tf);
        p.drawText(QRect(r.left(), top + h + 4, 80, 14), Qt::AlignLeft, formatDuration(startSec));
        startSec += s.ms / 1000;
    }

    // playhead
    const int idx = m_play->index();
    if (idx >= 0 && idx < m_blocks.size() && segs[idx].ms > 0) {
        const double f = qBound(0.0, double(m_play->segmentElapsed()) / segs[idx].ms, 1.0);
        const int x = m_blocks[idx].x + int(m_blocks[idx].w * f);
        p.setPen(QPen(QColor("#ef4444"), 2));
        p.drawLine(x, 0, x, top + h + 2);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#ef4444"));
        p.drawPolygon(QPolygon({QPoint(x - 5, 0), QPoint(x + 5, 0), QPoint(x, 7)}));
    }
}

void TimelineWidget::mousePressEvent(QMouseEvent *e) {
    for (int i = 0; i < m_blocks.size(); ++i)
        if (e->position().x() >= m_blocks[i].x && e->position().x() <= m_blocks[i].x + m_blocks[i].w) {
            emit segmentClicked(i);
            return;
        }
}
