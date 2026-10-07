#include "TimelineWidget.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygon>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cstdlib>
#include <climits>

#include "Data.h"
#include "Playout.h"

namespace {
constexpr int kCapY = 14;                                   // the playhead's little flag sits above the lanes
constexpr int kMainTop = 26;                                // top of the programs lane
constexpr int kMainH = 52;
constexpr int kLabelTop = kMainTop + kMainH + 4;
constexpr int kLaneTop = kLabelTop + 20;                    // first extra timeline
constexpr int kLaneH = 26;
constexpr int kLaneGap = 4;
constexpr int kBadge = 13;                                  // scissors badge radius
constexpr int kSlotR = 10;                                  // "+" radius

void drawPill(QPainter &p, int cx, const QString &text, const QColor &bg, int viewW) {
    QFont f;
    f.setPixelSize(11);
    f.setBold(true);
    p.setFont(f);
    const QFontMetrics fm(f);
    const int w = fm.horizontalAdvance(text) + 14;
    const int x = qBound(0, cx - w / 2, qMax(0, viewW - w));
    const QRect r(x, 1, w, 15);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, 7, 7);
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, text);
}

QColor textOn(const QColor &bg) { return bg.lightness() > 150 ? QColor("#0b1220") : QColor(Qt::white); }

QColor segColor(const Segment &s) {
    return s.type == Segment::Program ? kindColor(s.kind) : s.type == Segment::Ad ? QColor("#16a34a") : QColor("#f59e0b");
}
} // namespace

TimelineWidget::TimelineWidget(Playout *play, Library *lib, QWidget *parent)
    : QWidget(parent), m_play(play), m_lib(lib) {
    connect(play, &Playout::segmentsChanged, this, [this] { relayout(); update(); });
    connect(play, &Playout::segmentChanged, this, [this] { update(); });
    connect(play, &Playout::stateChanged, this, [this] { update(); });
    connect(play, &Playout::ticked, this, [this] { update(); });
    connect(lib, &Library::placementsChanged, this, [this] { layoutPlacements(); notifyHeight(); update(); });
    connect(lib, &Library::elementsChanged, this, [this] { update(); });
    connect(lib, &Library::layersChanged, this, [this] { update(); });
    setMouseTracking(true);
    DragController::instance()->setTarget(this);
    relayout();
}

// ---------------------------------------------------------------- layout

int TimelineWidget::lanesShown() const { return m_reveal ? 3 : m_lib->maxLayer(); }
int TimelineWidget::laneY(int lane) const { return kLaneTop + (lane - 1) * (kLaneH + kLaneGap); }
int TimelineWidget::wantedHeight() const {
    const int n = lanesShown();
    return n == 0 ? kLabelTop + 20 : laneY(n) + kLaneH + 6;
}

void TimelineWidget::notifyHeight() {
    const int h = wantedHeight();
    if (h == m_lastH) return;
    m_lastH = h;
    setMinimumHeight(h);
    updateGeometry();
    emit heightWanted(h);
}

int TimelineWidget::laneAt(const QPoint &pos) const {
    for (int k = 1; k <= lanesShown(); ++k)
        if (QRect(0, laneY(k) - 2, width(), kLaneH + 4).contains(pos)) return k;
    return 0;
}

void TimelineWidget::relayout() {
    m_blocks.clear();
    m_slots.clear();
    const auto &segs = m_play->segments();
    int x = 10;
    for (int i = 0; i < segs.size(); ++i) {
        const int w = Playout::blockWidth(segs[i]);
        m_blocks.append({x, w});
        x += w + 4;
        // a "+" insert point sits on the cut after the last segment of a program that was cut: it overlaps
        // the two pieces so it takes no room of its own
        const bool last = i + 1 == segs.size() || segs[i + 1].programId != segs[i].programId;
        const ProgramItem *pi = last ? m_lib->program(segs[i].programId) : nullptr;
        if (pi && pi->slotAfter)
            m_slots.append({QRect(x - 2 - kSlotR, kMainTop + (kMainH - 2 * kSlotR) / 2, 2 * kSlotR, 2 * kSlotR), pi->id});
    }
    m_totalW = qMax(400, x + 10);
    setMinimumWidth(m_totalW);
    layoutPlacements();
    notifyHeight();
    updateGeometry();
}

// Everything placed on the extra timelines; each timeline has one row, later blocks overlap earlier ones.
void TimelineWidget::layoutPlacements() {
    m_pblocks.clear();
    if (m_blocks.isEmpty()) return;
    for (const Placement &pl : std::as_const(m_lib->placements)) {
        const int lane = qBound(1, pl.layer, 3);
        const int x0 = msToX(pl.startMs);
        const int x1 = qMax(msToX(pl.startMs + qint64(pl.seconds) * 1000), x0 + 76);
        m_pblocks.append({pl.id, pl.type, QRect(x0, laneY(lane) + 1, x1 - x0, kLaneH - 3), lane});
    }
}

int TimelineWidget::endX() const { return m_blocks.isEmpty() ? 10 : m_blocks.last().x + m_blocks.last().w; }

// timeline time <-> pixels (piecewise: every segment is drawn at its own scale)
int TimelineWidget::msToX(qint64 ms) const {
    const auto &segs = m_play->segments();
    if (segs.isEmpty() || m_blocks.size() != segs.size()) return 10;
    if (ms <= 0) return m_blocks[0].x;
    for (int i = 0; i < segs.size(); ++i) {
        const qint64 st = m_play->segmentStartMs(i);
        if (ms < st + segs[i].ms)
            return m_blocks[i].x + int(double(m_blocks[i].w) * double(ms - st) / segs[i].ms);
    }
    return endX();
}

qint64 TimelineWidget::xToMs(double x) const {
    const auto &segs = m_play->segments();
    if (segs.isEmpty() || m_blocks.size() != segs.size()) return 0;
    if (x <= m_blocks[0].x) return 0;
    for (int i = 0; i < segs.size(); ++i) {
        const Block &b = m_blocks[i];
        if (x <= b.x + b.w) {
            if (x < b.x) return m_play->segmentStartMs(i);  // in the gap in front of this block
            return m_play->segmentStartMs(i) + qint64(double(segs[i].ms) * (x - b.x) / b.w);
        }
    }
    return m_play->totalMs();
}

int TimelineWidget::playheadX() const {
    if (m_drag == Drag::Playhead && m_moved) return qBound(m_blocks.isEmpty() ? 10 : m_blocks[0].x, m_dragX, endX());
    return msToX(m_play->playheadMs());
}

int TimelineWidget::blockAt(const QPoint &pos) const {
    if (pos.y() < kMainTop || pos.y() > kMainTop + kMainH) return -1;
    for (int i = 0; i < m_blocks.size(); ++i)
        if (pos.x() >= m_blocks[i].x && pos.x() <= m_blocks[i].x + m_blocks[i].w) return i;
    return -1;
}

const TimelineWidget::PBlock *TimelineWidget::hitPlacement(const QPoint &pos) const {
    for (int i = m_pblocks.size() - 1; i >= 0; --i)
        if (m_pblocks[i].rect.contains(pos)) return &m_pblocks[i];
    return nullptr;
}

const TimelineWidget::Slot *TimelineWidget::hitSlot(const QPoint &pos) const {
    for (const Slot &s : m_slots)
        if (s.rect.adjusted(-2, -2, 2, 2).contains(pos)) return &s;
    return nullptr;
}

// The cut tool: the program under the cursor, where it would be cut, and where the line is drawn.
bool TimelineWidget::cutTarget(int *segment, int *atSec, int *lineX) const {
    if (!m_hasHover || m_drag != Drag::None || m_blocks.isEmpty()) return false;
    if (m_hover.y() < kMainTop || m_hover.y() > kMainTop + kMainH) return false;
    if (hitSlot(m_hover)) return false;
    const auto &segs = m_play->segments();
    for (int i = 0; i < m_blocks.size(); ++i) {
        const Block &b = m_blocks[i];
        if (m_hover.x() > b.x + 2 && m_hover.x() < b.x + b.w - 2) {
            const Segment &s = segs[i];
            const int secs = s.ms / 1000;
            if (s.type != Segment::Program || s.spot || secs < 2) return false;
            const double f = double(m_hover.x() - b.x) / b.w;
            const int at = qBound(1, qRound(f * secs), secs - 1);
            *segment = i;
            *atSec = at;
            *lineX = b.x + int(double(b.w) * at / secs);
            return true;
        }
    }
    return false;
}

bool TimelineWidget::overScissors(const QPoint &pos) const {
    int seg, at, lx;
    if (!cutTarget(&seg, &at, &lx)) return false;
    return QLineF(pos, QPointF(lx, kMainTop + kMainH / 2)).length() <= kBadge;
}

// ---------------------------------------------------------------- dragging onto the timeline

// Where would a dragged program / ad land on the main timeline? Whichever program it covers most decides:
// dropped on its right half it goes after it, otherwise before it. Covering half of it (or half of itself)
// stops the drag and drops it on the spot.
bool TimelineWidget::resolveMainDrop(const DragPayload &d, const QPoint &pos) {
    m_ins = {};
    const auto &segs = m_play->segments();
    if (m_blocks.isEmpty() || m_blocks.size() != segs.size()) {
        m_ins = {true, QString(), 10, -1};
        return false;
    }
    const int gl = pos.x() - d.grabDx, gw = qMax(20, d.widthPx), gr = gl + gw, gc = gl + gw / 2;
    auto eligible = [&](int i) {
        const Segment &s = segs[i];
        return (s.type == Segment::Program || s.spot) && !(d.kind == "program" && s.programId == d.id);
    };

    int best = -1;
    double bestFrac = 0;
    for (int i = 0; i < segs.size(); ++i) {
        if (!eligible(i)) continue;
        const Block &b = m_blocks[i];
        const int ov = qMin(gr, b.x + b.w) - qMax(gl, b.x);
        if (ov <= 0) continue;
        const double f = qMax(double(ov) / b.w, double(ov) / gw);
        if (f > bestFrac) {
            bestFrac = f;
            best = i;
        }
    }
    if (best < 0) {  // nothing covered: the nearest program wins
        int bestDist = INT_MAX;
        for (int i = 0; i < segs.size(); ++i) {
            if (!eligible(i)) continue;
            const int dist = qAbs(gc - (m_blocks[i].x + m_blocks[i].w / 2));
            if (dist < bestDist) {
                bestDist = dist;
                best = i;
            }
        }
    }
    if (best < 0) return false;

    const bool right = gc >= m_blocks[best].x + m_blocks[best].w / 2;
    const QString pid = segs[best].programId;
    if (right) {
        const int last = m_play->groupLast(best);
        int j = m_lib->programIndex(pid) + 1;
        if (j < m_lib->programs.size() && d.kind == "program" && m_lib->programs[j].id == d.id) ++j;
        m_ins.beforeId = j < m_lib->programs.size() ? m_lib->programs[j].id : QString();
        m_ins.x = m_blocks[last].x + m_blocks[last].w + 2;
    } else {
        m_ins.beforeId = pid;
        m_ins.x = m_blocks[m_play->groupFirst(best)].x - 2;
    }
    m_ins.valid = true;
    m_ins.seg = best;
    return bestFrac >= 0.5;
}

bool TimelineWidget::dragMove(const DragPayload &d, const QPoint &g) {
    m_ins = {};
    m_dropLane = 0;
    const QPoint pos = mapFromGlobal(g);
    QWidget *vp = parentWidget();
    bool inside = rect().contains(pos) && (!vp || vp->rect().contains(mapTo(vp, pos)));

    const bool wantsLane = d.kind == "element" || d.kind == "placement";
    const bool below = pos.y() > kMainTop + kMainH;
    const bool wasRevealed = m_reveal;
    if (inside && (wantsLane || below)) m_reveal = true;
    if (m_reveal != wasRevealed) notifyHeight();

    bool commit = false;
    if (inside) {
        autoScroll(pos.x());
        const int lane = laneAt(pos);
        const int left = qMax(m_blocks.isEmpty() ? 10 : m_blocks.first().x, pos.x() - d.grabDx);
        if (lane > 0) {
            m_dropLane = lane;
            m_dropMs = xToMs(left);
        } else if (pos.y() <= kLabelTop + 14) {
            if (wantsLane) {
                const Placement *pl = d.kind == "placement" ? m_lib->placement(d.id) : nullptr;
                m_dropLane = pl ? qBound(1, pl->layer, 3) : 1;
                m_dropMs = xToMs(left);
            } else {
                commit = resolveMainDrop(d, pos);
            }
        }
    }
    m_hover = pos;
    m_hasHover = inside;
    update();
    return commit;
}

void TimelineWidget::dragDrop(const DragPayload &d, const QPoint &g) {
    dragMove(d, g);
    if (m_dropLane >= 1) emit dropped(d, m_dropLane, m_dropMs, QString());
    else if (m_ins.valid) emit dropped(d, 0, 0, m_ins.beforeId);
}

void TimelineWidget::dragLeave() {
    if (m_drag == Drag::Foreign) m_drag = Drag::None;
    m_reveal = false;
    m_ins = {};
    m_dropLane = 0;
    m_liftedId.clear();
    m_moved = false;
    notifyHeight();
    updateCursor();
    update();
}

// ---------------------------------------------------------------- painting

void TimelineWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto &segs = m_play->segments();

    if (segs.isEmpty() || m_blocks.size() != segs.size()) {
        p.setPen(QColor("#64748b"));
        p.drawText(rect(), Qt::AlignCenter, "The timeline is empty. Add programs in the Programs tab.");
        return;
    }

    // ---- programs lane
    auto block = [&](const QRect &r, const Segment &s, bool current, double opacity) {
        p.save();
        p.setOpacity(opacity);
        const QColor c = segColor(s);
        p.setPen(current ? QPen(Qt::white, 2) : Qt::NoPen);
        p.setBrush(current ? c : c.darker(160));
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
        p.restore();
    };

    int startSec = 0;
    for (int i = 0; i < segs.size(); ++i) {
        const Segment &s = segs[i];
        const QRect r(m_blocks[i].x, kMainTop, m_blocks[i].w, kMainH);
        const bool lifted = !m_liftedId.isEmpty() && s.programId == m_liftedId;
        const bool target = m_ins.valid && m_ins.seg == i;
        block(r, s, i == m_play->index(), lifted ? 0.3 : 1.0);
        if (target) {
            p.setPen(QPen(QColor("#facc15"), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-1, -1, 1, 1), 8, 8);
        }
        p.setPen(QColor("#64748b"));
        QFont tf = font();
        tf.setPixelSize(10);
        p.setFont(tf);
        p.drawText(QRect(r.left(), kLabelTop, 80, 14), Qt::AlignLeft, formatDuration(startSec));
        startSec += s.ms / 1000;
    }

    // ---- extra timelines (layers 2-4)
    for (int k = 1; k <= lanesShown(); ++k) {
        const QRect lane(2, laneY(k), width() - 4, kLaneH);
        const bool hot = m_dropLane == k;
        const LayerState &ls = m_lib->layers.value(k);
        p.setPen(Qt::NoPen);
        p.setBrush(hot ? QColor(59, 130, 246, 70) : QColor(255, 255, 255, 8));
        p.drawRoundedRect(lane, 7, 7);
        if (hot) {
            p.setPen(QPen(QColor("#3b82f6"), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(lane.adjusted(1, 1, -1, -1), 7, 7);
        } else if (m_reveal) {
            p.setPen(QPen(QColor(255, 255, 255, 45), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(lane.adjusted(1, 1, -1, -1), 7, 7);
        }
        QFont lf = font();
        lf.setPixelSize(9);
        lf.setBold(true);
        p.setFont(lf);
        p.setPen(QColor(148, 163, 184, hot ? 230 : 120));
        QString tag = QString("TIMELINE %1").arg(k + 1);
        if (ls.muted) tag += "  ·  MUTED";
        if (!ls.visible) tag += "  ·  HIDDEN";
        p.drawText(lane.adjusted(8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, tag);
    }

    // ---- what is placed on them
    for (const PBlock &pb : std::as_const(m_pblocks)) {
        const Placement *pl = m_lib->placement(pb.id);
        if (!pl || pb.lane > lanesShown()) continue;
        QColor c = pb.type == "Sound" ? QColor("#0d9488")
                 : pb.type == "Ad"    ? QColor("#16a34a")
                 : pb.type == "Clip"  ? kindColor(pl->kind)
                                      : QColor("#64748b");
        if (pb.type == "Element")
            if (const Element *e = m_lib->element(pl->refId)) c = e->type == "Custom" ? QColor("#6366f1") : QColor(e->color);
        const LayerState &ls = m_lib->layers.value(pb.lane);
        const bool lifted = pb.id == m_liftedId;
        p.setOpacity(lifted ? 0.3 : (ls.muted && pb.type != "Element") || !ls.visible ? 0.55 : 1.0);
        p.setPen(Qt::NoPen);
        p.setBrush(c.darker(125));
        p.drawRoundedRect(pb.rect, 6, 6);
        QFont f = font();
        f.setPixelSize(11);
        f.setBold(true);
        p.setFont(f);
        p.setPen(textOn(c.darker(125)));
        const QString prefix = pb.type == "Sound" ? "♪ " : pb.type == "Ad" ? "AD · " : "";
        const QString label = prefix + pl->name + "  ·  " + formatDuration(int(pl->startMs / 1000));
        p.drawText(pb.rect.adjusted(7, 0, -5, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetrics(f).elidedText(label, Qt::ElideRight, pb.rect.width() - 12));
        p.setOpacity(1.0);
    }

    // ---- "+" insert points left by a cut: they sit on the join between the two pieces
    for (const Slot &s : std::as_const(m_slots)) {
        const bool hot = m_hasHover && s.rect.adjusted(-2, -2, 2, 2).contains(m_hover);
        p.setPen(QPen(QColor("#0b1220"), 2));
        p.setBrush(hot ? QColor("#3b82f6") : QColor("#2563eb"));
        p.drawEllipse(s.rect);
        p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap));
        const QPoint c = s.rect.center();
        p.drawLine(c.x() - 4, c.y(), c.x() + 4, c.y());
        p.drawLine(c.x(), c.y() - 4, c.x(), c.y() + 4);
    }

    // ---- a program / ad being dragged over the main lane: where it will go
    if (m_ins.valid && m_drag != Drag::Playhead) {
        p.setPen(QPen(QColor("#facc15"), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(m_ins.x, kMainTop - 5, m_ins.x, kMainTop + kMainH + 5);
    }

    // ---- the cut tool: dashed line through the program and a scissors in the middle of it
    int cutSeg, cutSec, cutX;
    if (!DragController::instance()->active() && cutTarget(&cutSeg, &cutSec, &cutX)) {
        p.setPen(QPen(QColor(255, 255, 255, 215), 1.5, Qt::DashLine));
        p.drawLine(cutX, kMainTop - 2, cutX, kMainTop + kMainH + 2);
        const QPointF c(cutX, kMainTop + kMainH / 2);
        const bool hot = QLineF(m_hover, c).length() <= kBadge;
        p.setPen(Qt::NoPen);
        p.setBrush(hot ? QColor("#ffffff") : QColor("#e2e8f0"));
        p.drawEllipse(c, double(kBadge), double(kBadge));
        QFont sf("Segoe UI Symbol");
        sf.setPixelSize(15);
        p.setFont(sf);
        p.setPen(QColor("#0b1220"));
        p.drawText(QRectF(c.x() - kBadge, c.y() - kBadge, kBadge * 2, kBadge * 2), Qt::AlignCenter, QString(QChar(0x2702)));
        drawPill(p, cutX, "Cut at " + formatDuration(int((m_play->segmentStartMs(cutSeg) + qint64(cutSec) * 1000) / 1000)),
                 QColor("#475569"), width());
    }

    // ---- playhead (drag it to move where the broadcast plays from)
    const int px = playheadX();
    const bool grabbing = m_drag == Drag::Playhead && m_moved;
    const QColor pc = m_play->isLive() || grabbing ? QColor("#ef4444") : QColor("#64748b");
    p.setPen(QPen(pc, 2));
    p.drawLine(px, kMainTop - 2, px, kMainTop + kMainH + 2);
    p.setPen(Qt::NoPen);
    p.setBrush(pc);
    p.drawRoundedRect(QRect(px - 6, kCapY, 12, 8), 2, 2);
    p.drawPolygon(QPolygon({QPoint(px - 6, kCapY + 6), QPoint(px + 6, kCapY + 6), QPoint(px, kCapY + 12)}));
    if (grabbing) drawPill(p, px, formatDuration(int(xToMs(px) / 1000)), QColor("#ef4444"), width());
}

// ---------------------------------------------------------------- mouse

void TimelineWidget::updateCursor() {
    Qt::CursorShape c = Qt::ArrowCursor;
    if (m_drag == Drag::Playhead) c = Qt::SizeHorCursor;
    else if (m_drag == Drag::Foreign) c = Qt::ClosedHandCursor;
    else if (m_hasHover) {
        const QPoint pos = m_hover;
        if (overScissors(pos) || hitSlot(pos)) c = Qt::PointingHandCursor;
        else if (std::abs(pos.x() - playheadX()) <= 7 && pos.y() >= kCapY && pos.y() <= kMainTop + kMainH + 2) c = Qt::SizeHorCursor;
        else if (hitPlacement(pos)) c = Qt::OpenHandCursor;
        else {
            const int i = blockAt(pos);
            if (i >= 0) {
                const Segment &s = m_play->segments()[i];
                c = (s.type == Segment::Program || s.spot) ? Qt::OpenHandCursor : Qt::PointingHandCursor;
            }
        }
    }
    setCursor(c);
}

void TimelineWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || m_blocks.isEmpty()) return;
    const QPoint pos = e->position().toPoint();
    m_hover = pos;
    m_hasHover = true;
    m_pressPos = pos;
    m_moved = false;
    m_drag = Drag::None;
    m_dragX = pos.x();
    m_cand = DragPayload();
    m_candSeg = -1;

    // 1. scissors: cut the program here
    int cutSeg, cutSec, cutX;
    if (cutTarget(&cutSeg, &cutSec, &cutX) && overScissors(pos)) {
        emit cutRequested(m_play->segments()[cutSeg].programId, cutSec);
        return;
    }
    // 2. "+" between the two halves of a cut
    if (const Slot *s = hitSlot(pos)) {
        const QString id = s->programId;
        emit addRequested(id, mapToGlobal(QPoint(s->rect.center().x(), s->rect.top())));
        return;
    }
    // 3. playhead
    if (std::abs(pos.x() - playheadX()) <= 7 && pos.y() >= kCapY && pos.y() <= kMainTop + kMainH + 2) {
        m_drag = Drag::Playhead;
        return;
    }
    // 4. something on an extra timeline: picks it up
    if (const PBlock *pb = hitPlacement(pos)) {
        if (const Placement *pl = m_lib->placement(pb->id)) {
            m_cand.kind = "placement";
            m_cand.id = pb->id;
            m_cand.label = pl->name;
            m_cand.color = pb->type == "Sound" ? QColor("#0d9488") : pb->type == "Ad" ? QColor("#16a34a") : QColor("#64748b");
            m_cand.widthPx = pb->rect.width();
            m_cand.grabDx = pos.x() - pb->rect.left();
            m_cand.fromTimeline = true;
            m_drag = Drag::Press;
        }
        return;
    }
    // 5. a program / break / ad on the main lane: a click jumps there, a drag moves programs and ad spots
    const int i = blockAt(pos);
    if (i >= 0) {
        const Segment &s = m_play->segments()[i];
        m_candSeg = i;
        m_drag = Drag::Press;
        if (s.type == Segment::Program || s.spot) {
            m_cand.kind = "program";
            m_cand.id = s.programId;
            m_cand.label = s.title;
            m_cand.color = segColor(s);
            m_cand.widthPx = m_blocks[i].w;
            m_cand.grabDx = pos.x() - m_blocks[i].x;
            m_cand.fromTimeline = true;
        }
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *e) {
    const QPoint pos = e->position().toPoint();
    m_hover = pos;
    m_hasHover = true;
    if (m_drag == Drag::Playhead) {
        if (!m_moved && (pos - m_pressPos).manhattanLength() < 4) return;
        m_moved = true;
        m_dragX = pos.x();
        autoScroll(pos.x());
        update();
        return;
    }
    if (m_drag == Drag::Press) {
        if ((pos - m_pressPos).manhattanLength() < 5 || !m_cand.valid()) return;
        m_moved = true;
        m_liftedId = m_cand.id;
        m_drag = Drag::Foreign;
        DragController::instance()->begin(m_cand, e->globalPosition().toPoint());
        update();
        return;
    }
    if (m_drag == Drag::Foreign) return;
    updateCursor();
    update();
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) return;
    const Drag d = m_drag;
    const int x = m_dragX;
    const bool moved = m_moved;
    const int seg = m_candSeg;
    m_drag = Drag::None;
    m_moved = false;
    m_candSeg = -1;
    if (m_blocks.isEmpty()) {
        update();
        return;
    }
    if (d == Drag::Playhead && moved) m_play->seekTo(xToMs(qBound(m_blocks.first().x, x, endX())));
    else if (d == Drag::Press && seg >= 0) emit segmentClicked(seg);
    updateCursor();
    update();
}

void TimelineWidget::leaveEvent(QEvent *) {
    m_hasHover = false;
    update();
}

void TimelineWidget::contextMenuEvent(QContextMenuEvent *e) {
    const QPoint pos = e->pos();
    e->accept();

    if (const PBlock *pb = hitPlacement(pos)) {
        const QString id = pb->id;
        QMenu menu(this);
        QAction *len = menu.addAction("Set length...");
        QAction *rm = menu.addAction("Remove from timeline");
        QAction *chosen = menu.exec(e->globalPos());
        if (chosen == len) emit placementLengthRequested(id);
        else if (chosen == rm) emit placementRemoveRequested(id);
        return;
    }
    if (const Slot *s = hitSlot(pos)) {
        const QString id = s->programId;
        const QPoint at = mapToGlobal(QPoint(s->rect.center().x(), s->rect.top()));
        QMenu menu(this);
        QAction *add = menu.addAction("Add something here...");
        if (menu.exec(e->globalPos()) == add) emit addRequested(id, at);
        return;
    }
    const int i = blockAt(pos);
    if (i >= 0 && m_play->segments()[i].spot) {
        const QString id = m_play->segments()[i].programId;
        QMenu menu(this);
        QAction *rm = menu.addAction("Remove ad from timeline");
        if (menu.exec(e->globalPos()) == rm) emit spotRemoveRequested(id);
    }
}

QScrollBar *TimelineWidget::hbar() const {
    QWidget *vp = parentWidget();
    auto *sa = vp ? qobject_cast<QScrollArea *>(vp->parentWidget()) : nullptr;
    return sa ? sa->horizontalScrollBar() : nullptr;
}

void TimelineWidget::autoScroll(int localX) {
    QScrollBar *sb = hbar();
    QWidget *vp = parentWidget();
    if (!sb || !vp) return;
    const int vx = mapTo(vp, QPoint(localX, 0)).x();
    if (vx < 30) sb->setValue(sb->value() - 20);
    else if (vx > vp->width() - 30) sb->setValue(sb->value() + 20);
}

// Shift + wheel (or a sideways wheel) scrolls the timeline; the plain wheel scrolls the page it sits in.
void TimelineWidget::wheelEvent(QWheelEvent *e) {
    QScrollBar *sb = hbar();
    const bool sideways = e->angleDelta().x() != 0 || (e->modifiers() & Qt::ShiftModifier);
    if (sb && sideways && sb->maximum() > 0) {
        const int d = e->angleDelta().x() != 0 ? e->angleDelta().x() : e->angleDelta().y();
        sb->setValue(sb->value() - d);
        e->accept();
        return;
    }
    e->ignore();
}
