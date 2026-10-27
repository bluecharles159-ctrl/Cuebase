#include "WorkCanvas.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include "Playout.h"
#include "Toast.h"

namespace {
constexpr int kSnap = 6;
QFont uiFont(int px, bool bold = false) {
    QFont f("Segoe UI");
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}
} // namespace

WorkCanvas::WorkCanvas(Library *lib, Playout *play, QWidget *parent)
    : QWidget(parent), m_lib(lib), m_play(play) {
    setObjectName("workCanvas");
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    auto refresh = [this] { update(); };
    connect(lib, &Library::elementsChanged, this, refresh);
    connect(lib, &Library::elementLiveChanged, this, refresh);
    connect(lib, &Library::selectionChanged, this, refresh);
    connect(lib, &Library::audioChanged, this, refresh);
    connect(lib, &Library::timelineAudioChanged, this, refresh);
    connect(play, &Playout::ticked, this, refresh);
}

void WorkCanvas::setDrawMode(bool on) {
    m_draw = on;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

QRect WorkCanvas::cameraRect() const {
    const QRect area = rect().adjusted(18, 42, -18, -18);
    int w = area.width(), h = int(w * 9.0 / 16.0);
    if (h > area.height()) {
        h = area.height();
        w = int(h * 16.0 / 9.0);
    }
    return {area.x() + (area.width() - w) / 2, area.y() + (area.height() - h) / 2, w, h};
}

QRect WorkCanvas::objectRect(const Element &e) const {
    const QRect cam = cameraRect();
    if (e.freeform) {
        return QRect(int(cam.x() + e.x * cam.width()), int(cam.y() + e.y * cam.height()),
                     qMax(24, int(e.w * cam.width())), qMax(18, int(e.h * cam.height())));
    }
    const int H = cam.height();
    const int px = e.type == "Logo" ? H * 7 / 100 : H * 5 / 100;
    const QFontMetrics fm(uiFont(px, true));
    const int pad = px * 6 / 10;
    QSize size(fm.horizontalAdvance(e.text.isEmpty() ? e.name : e.text) + pad * 2, fm.height() + pad);
    int x = cam.left() + 12, y = cam.top() + 12;
    if (e.position.contains("Right")) x = cam.right() - 12 - size.width();
    if (e.position.contains("Center")) x = cam.center().x() - size.width() / 2;
    if (e.position.startsWith("Bottom")) y = cam.bottom() - 12 - size.height();
    return {QPoint(x, y), size};
}

void WorkCanvas::writeRect(Element &e, const QRect &r) {
    const QRect cam = cameraRect();
    e.freeform = true;
    e.x = double(r.x() - cam.x()) / qMax(1, cam.width());
    e.y = double(r.y() - cam.y()) / qMax(1, cam.height());
    e.w = double(r.width()) / qMax(1, cam.width());
    e.h = double(r.height()) / qMax(1, cam.height());
}

int WorkCanvas::hitTest(const QPoint &pos) const {
    for (int i = m_lib->elements.size() - 1; i >= 0; --i) {
        const Element &e = m_lib->elements[i];
        if (e.type == "Break") continue;
        if (objectRect(e).adjusted(-2, -2, 2, 2).contains(pos)) return i;
    }
    return -1;
}

QRect WorkCanvas::snapRect(const QRect &r, int skipIndex) {
    m_guides.clear();
    QVector<int> xs, ys;
    const QRect cam = cameraRect();
    xs << cam.left() << cam.center().x() << cam.right();
    ys << cam.top() << cam.center().y() << cam.bottom();
    for (int i = 0; i < m_lib->elements.size(); ++i) {
        if (i == skipIndex || m_lib->elements[i].type == "Break") continue;
        const QRect o = objectRect(m_lib->elements[i]);
        xs << o.left() << o.center().x() << o.right();
        ys << o.top() << o.center().y() << o.bottom();
    }
    int dx = 0, dy = 0;
    int bestX = kSnap + 1, bestY = kSnap + 1;
    const int candX[] = {r.left(), r.center().x(), r.right()};
    const int candY[] = {r.top(), r.center().y(), r.bottom()};
    int snapToX = 0, snapToY = 0;
    int whichX = 0, whichY = 0;
    for (int v : xs) {
        for (int c = 0; c < 3; ++c) {
            const int d = v - candX[c];
            if (qAbs(d) < bestX) {
                bestX = qAbs(d);
                dx = d;
                snapToX = v;
                whichX = c;
            }
        }
    }
    for (int v : ys) {
        for (int c = 0; c < 3; ++c) {
            const int d = v - candY[c];
            if (qAbs(d) < bestY) {
                bestY = qAbs(d);
                dy = d;
                snapToY = v;
                whichY = c;
            }
        }
    }
    QRect out = r;
    if (bestX <= kSnap) {
        out.translate(dx, 0);
        m_guides.append({false, snapToX});
        Q_UNUSED(whichX);
    }
    if (bestY <= kSnap) {
        out.translate(0, dy);
        m_guides.append({true, snapToY});
        Q_UNUSED(whichY);
    }
    return out;
}

QPixmap WorkCanvas::cachedPixmap(const QString &path) {
    if (!m_pixmaps.contains(path)) m_pixmaps.insert(path, QPixmap(path));
    return m_pixmaps.value(path);
}

void WorkCanvas::drawObject(QPainter &p, const Element &e, const QRect &r, bool selected) {
    QRect box = r;
    if (e.noise > 0.001) {
        const double t = m_play->totalElapsed() * 0.04 + e.noise * 12;
        box.translate(int(qSin(t) * 4 * e.noise), int(qCos(t * 1.3) * 4 * e.noise));
    }
    if (e.physics) {
        const double t = m_play->totalElapsed() / 1000.0;
        const double bounce = qAbs(qSin(t * qSqrt(qMax(40.0, e.gravity) / 400.0))) * 18;
        box.translate(0, int(-bounce));
    }
    const QColor color(e.color);
    if (e.type == "Rectangle") {
        p.setPen(selected ? QPen(Qt::white, 2) : QPen(color, 1.5));
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 70));
        p.drawRoundedRect(box, 6, 6);
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(e.type == "Logo" ? QColor(0, 0, 0, 140) : color);
        p.drawRoundedRect(box, box.height() / 4.0, box.height() / 4.0);
        p.setPen(e.type == "Logo" ? color : Qt::white);
        p.setFont(uiFont(qMax(10, box.height() / 3), true));
        p.drawText(box, Qt::AlignCenter, e.text.isEmpty() ? e.name : e.text);
    }
    if (selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#38bdf8"), 1.5, Qt::DashLine));
        p.drawRect(box.adjusted(-3, -3, 3, 3));
    }
}

void WorkCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    p.setClipPath(clip);
    p.fillRect(rect(), QColor("#0b1220"));

    p.setPen(QColor("#64748b"));
    p.setFont(uiFont(11, true));
    p.drawText(QRect(16, 10, width() - 32, 24), Qt::AlignLeft | Qt::AlignVCenter,
               m_draw ? "DRAW  ·  drag a rectangle" : "WORK  ·  drag to snap  ·  R to draw");

    const QRect cam = cameraRect();
    p.fillRect(rect(), QColor(2, 6, 23, 168));
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.fillRect(cam, QColor("#050912"));

    p.save();
    p.setClipRect(cam);
    p.fillRect(cam, QColor("#07101d"));
    p.setPen(QColor(255, 255, 255, 18));
    for (int x = cam.left(); x < cam.right(); x += 24) p.drawLine(x, cam.top(), x, cam.bottom());
    p.restore();

    p.setClipping(false);
    p.setClipPath(clip);
    p.setPen(QPen(QColor("#38bdf8"), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRect(cam);
    p.setPen(QColor("#64748b"));
    p.setFont(uiFont(10));
    p.drawText(QRect(cam.left(), cam.top() - 16, 80, 14), "CAMERA");

    for (int i = 0; i < m_lib->elements.size(); ++i) {
        const Element &e = m_lib->elements[i];
        if (e.type == "Break") continue;
        const bool sel = m_lib->selectedKind == "element" && m_lib->selectedId == e.id;
        drawObject(p, e, objectRect(e), sel);
    }

    if (m_drawing) {
        p.setPen(QPen(QColor("#38bdf8"), 1, Qt::DashLine));
        p.setBrush(QColor(56, 189, 248, 40));
        p.drawRect(m_drawBox.normalized());
    }

    p.setPen(QPen(QColor("#f472b6"), 1));
    for (const Guide &g : m_guides) {
        if (g.horiz) p.drawLine(0, g.pos, width(), g.pos);
        else p.drawLine(g.pos, 0, g.pos, height());
    }

    p.setClipping(false);
    p.setPen(QPen(QColor("#243044"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
}

void WorkCanvas::mousePressEvent(QMouseEvent *e) {
    setFocus();
    m_press = e->pos();
    if (m_draw) {
        m_drawing = true;
        m_drawBox = QRect(m_press, QSize(1, 1));
        update();
        return;
    }
    m_hit = hitTest(e->pos());
    if (m_hit >= 0) {
        m_lib->checkpoint();
        m_lib->select("element", m_lib->elements[m_hit].id);
        m_dragging = true;
        m_dragStart = objectRect(m_lib->elements[m_hit]);
        m_grab = e->pos() - m_dragStart.topLeft();
    } else {
        m_lib->select({}, {});
    }
    update();
}

void WorkCanvas::mouseMoveEvent(QMouseEvent *e) {
    if (m_drawing) {
        m_drawBox = QRect(m_press, e->pos()).normalized();
        m_drawBox = snapRect(m_drawBox, -1);
        update();
        return;
    }
    if (m_dragging && m_hit >= 0) {
        QRect r = m_dragStart;
        r.moveTopLeft(e->pos() - m_grab);
        r = snapRect(r, m_hit);
        writeRect(m_lib->elements[m_hit], r);
        update();
        return;
    }
}

void WorkCanvas::mouseReleaseEvent(QMouseEvent *) {
    if (m_drawing) {
        m_drawing = false;
        const QRect box = m_drawBox.normalized();
        if (box.width() > 12 && box.height() > 12) {
            m_lib->checkpoint();
            Element el;
            el.id = Library::newId();
            el.name = "Rectangle";
            el.type = "Rectangle";
            el.color = "#38bdf8";
            el.live = true;
            writeRect(el, box);
            m_lib->elements.append(el);
            m_lib->commitElements();
            m_lib->select("element", el.id);
            ToastHost::notify(this, "element", "Shape added successfully", el.name);
        }
        m_guides.clear();
        update();
        return;
    }
    if (m_dragging && m_hit >= 0) {
        m_lib->commitElements();
    }
    m_dragging = false;
    m_guides.clear();
    update();
}

void WorkCanvas::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_R) {
        setDrawMode(!m_draw);
        return;
    }
    if (e->key() == Qt::Key_V) {
        setDrawMode(false);
        return;
    }
    if (e->key() == Qt::Key_Delete && m_lib->selectedKind == "element") {
        m_lib->checkpoint();
        for (int i = 0; i < m_lib->elements.size(); ++i)
            if (m_lib->elements[i].id == m_lib->selectedId) m_lib->elements.removeAt(i);
        m_lib->select({}, {});
        m_lib->commitElements();
        return;
    }
    QWidget::keyPressEvent(e);
}

void WorkCanvas::dragEnterEvent(QDragEnterEvent *e) {
    if (e->mimeData()->hasFormat("application/x-cuebase-audio")) e->acceptProposedAction();
}

void WorkCanvas::dragMoveEvent(QDragMoveEvent *e) {
    if (e->mimeData()->hasFormat("application/x-cuebase-audio")) e->acceptProposedAction();
}

void WorkCanvas::dropEvent(QDropEvent *e) {
    const QString id = QString::fromUtf8(e->mimeData()->data("application/x-cuebase-audio"));
    if (id.isEmpty()) return;
    emit audioDroppedOnScene(id);
    e->acceptProposedAction();
}
