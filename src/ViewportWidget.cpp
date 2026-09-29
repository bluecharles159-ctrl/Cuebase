#include "ViewportWidget.h"

#include <QFile>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QVideoFrame>
#include <QVideoSink>
#include <cmath>

#include "Playout.h"

namespace {

QFont uiFont(int px, bool bold = false) {
    QFont f("Segoe UI");
    f.setPixelSize(qMax(8, px));
    f.setBold(bold);
    return f;
}

QPoint anchorFor(const QRect &r, const QString &pos, const QSize &s, int margin) {
    int x = r.left() + margin, y = r.top() + margin;
    if (pos.contains("Right")) x = r.right() - margin - s.width();
    if (pos.contains("Center")) x = r.center().x() - s.width() / 2;
    if (pos.startsWith("Bottom")) y = r.bottom() - margin - s.height();
    return {x, y};
}

} // namespace

ViewportWidget::ViewportWidget(Library *lib, Playout *play, QWidget *parent)
    : QWidget(parent), m_lib(lib), m_play(play) {
    auto refresh = [this] { update(); };
    connect(play, &Playout::ticked, this, refresh);
    connect(play, &Playout::segmentChanged, this, refresh);
    connect(play, &Playout::stateChanged, this, refresh);
    connect(lib, &Library::displayChanged, this, refresh);
    connect(lib, &Library::elementsChanged, this, refresh);
    connect(lib, &Library::elementLiveChanged, this, refresh);
    connect(play->videoSink(), &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &f) {
        m_frame = f.isValid() ? f.toImage() : QImage();
        update();
    });
}

QPixmap ViewportWidget::cachedPixmap(const QString &path) {
    if (!m_pixmaps.contains(path)) m_pixmaps.insert(path, QPixmap(path));
    return m_pixmaps.value(path);
}

void ViewportWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    p.setClipPath(clip);
    p.fillRect(rect(), QColor("#050912"));

    const int chromeH = 34;
    const QRect area(0, 0, width(), height() - chromeH);
    int w = area.width(), h = int(w * 9.0 / 16.0);
    if (h > area.height()) {
        h = area.height();
        w = int(h * 16.0 / 9.0);
    }
    const QRect screen(area.x() + (area.width() - w) / 2, area.y() + (area.height() - h) / 2, w, h);

    p.save();
    p.setClipRect(screen);
    drawScreen(p, screen);
    p.restore();
    drawChrome(p, QRect(0, height() - chromeH, width(), chromeH));

    p.setClipping(false);
    p.setPen(QPen(QColor("#243044"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
}

void ViewportWidget::drawScreen(QPainter &p, const QRect &r) {
    const Segment *seg = m_play->current();
    const int H = r.height();

    // ---- background + main content ----
    if (!seg) {
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, QColor("#0b1220"));
        g.setColorAt(1, QColor("#020617"));
        p.fillRect(r, g);
        p.setPen(QColor("#475569"));
        p.setFont(uiFont(H * 9 / 100, true));
        p.drawText(r.adjusted(0, 0, 0, -H / 8), Qt::AlignCenter, "OFF AIR");
        p.setFont(uiFont(H * 4 / 100));
        p.drawText(r.adjusted(0, H / 6, 0, 0), Qt::AlignCenter, "Press Start Broadcast to go live");
    } else if (!seg->file.isEmpty() && !m_frame.isNull() && seg->type != Segment::Break) {
        p.fillRect(r, Qt::black);
        const QImage scaled = m_frame.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawImage(r.center() - QPoint(scaled.width() / 2, scaled.height() / 2), scaled);
    } else {
        QColor base = seg->type == Segment::Program ? kindColor(seg->kind)
                    : seg->type == Segment::Ad      ? QColor("#16a34a")
                                                    : QColor(seg->color);
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, base.darker(260));
        g.setColorAt(1, QColor("#020617"));
        p.fillRect(r, g);

        if (seg->type == Segment::Break) {
            const QRect card(r.left() + r.width() / 8, r.center().y() - H / 6, r.width() * 3 / 4, H / 3);
            p.setPen(Qt::NoPen);
            p.setBrush(base);
            p.drawRoundedRect(card, H / 20, H / 20);
            p.setPen(Qt::white);
            p.setFont(uiFont(H * 6 / 100, true));
            p.drawText(card.adjusted(H / 20, 0, -H / 20, 0), Qt::AlignCenter | Qt::TextWordWrap, seg->text);
        } else {
            p.setPen(QColor(255, 255, 255, 150));
            p.setFont(uiFont(H * 35 / 1000, true));
            const QString label = seg->type == Segment::Ad ? "ADVERT" : "NOW PLAYING  ·  " + seg->kind.toUpper();
            p.drawText(r.adjusted(0, 0, 0, -H / 6), Qt::AlignCenter, label);
            p.setPen(Qt::white);
            p.setFont(uiFont(H * 10 / 100, true));
            p.drawText(r.adjusted(H / 10, H / 8, -H / 10, 0), Qt::AlignCenter | Qt::TextWordWrap, seg->title);
        }
    }

    if (m_play->state() == Playout::Paused) {
        p.fillRect(r, QColor(0, 0, 0, 140));
        p.setPen(Qt::white);
        p.setFont(uiFont(H * 8 / 100, true));
        p.drawText(r, Qt::AlignCenter, "PAUSED");
    }

    // ---- on-screen elements (live ones only; Ad Elements only during ads) ----
    for (const Element &e : m_lib->elements) {
        if (!e.live || e.type == "Break") continue;
        if (e.type == "Ad Element" && !(seg && seg->type == Segment::Ad)) continue;
        drawElement(p, r, e);
    }

    // ---- watermark ----
    const DisplaySettings &d = m_lib->display;
    if (d.watermarkOn && !d.watermarkText.isEmpty()) {
        p.setFont(uiFont(H * 35 / 1000, true));
        p.setPen(QColor(255, 255, 255, 95));
        const int m = r.width() * 2 / 100;
        p.drawText(r.adjusted(m, m, -m, -m), Qt::AlignRight | Qt::AlignBottom, d.watermarkText);
    }

    // ---- safe-area guides ----
    if (d.safeArea) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 90), 1, Qt::DashLine));
        for (double f : {0.9, 0.8}) {
            const int w = int(r.width() * f), h = int(r.height() * f);
            p.drawRect(QRect(r.center().x() - w / 2, r.center().y() - h / 2, w, h));
        }
    }
}

void ViewportWidget::drawElement(QPainter &p, const QRect &r, const Element &e) {
    const int H = r.height();
    const int margin = r.width() * 3 / 100;
    const QColor color(e.color);

    if (e.type == "Logo" && !e.image.isEmpty() && QFile::exists(e.image)) {
        const QPixmap pm = cachedPixmap(e.image).scaledToHeight(H * 14 / 100, Qt::SmoothTransformation);
        p.drawPixmap(anchorFor(r, e.position, pm.size(), margin), pm);
        return;
    }

    const int px = e.type == "Logo" ? H * 7 / 100 : e.type == "Banner" ? H * 55 / 1000 : H * 45 / 1000;
    const QFont font = uiFont(px, true);
    const QFontMetrics fm(font);
    const int pad = px * 6 / 10;
    QSize size(fm.horizontalAdvance(e.text) + pad * 2, fm.height() + pad);
    if (e.type == "Banner" && e.position.contains("Center")) size.setWidth(r.width() * 6 / 10);

    const QRect box(anchorFor(r, e.position, size, margin), size);
    p.setFont(font);
    p.setPen(Qt::NoPen);
    if (e.type == "Logo") {
        p.setBrush(QColor(0, 0, 0, 120));
        p.drawRoundedRect(box, box.height() / 3.0, box.height() / 3.0);
        p.setPen(color);
    } else {
        p.setBrush(color);
        p.drawRoundedRect(box, box.height() / 4.0, box.height() / 4.0);
        p.setPen(Qt::white);
    }
    p.drawText(box, Qt::AlignCenter, e.text);
}

void ViewportWidget::drawChrome(QPainter &p, const QRect &r) {
    p.fillRect(r, QColor("#0a0f1c"));

    // thin timeline bar that moves as the video plays
    const QRect track(r.left(), r.top(), r.width(), 3);
    p.fillRect(track, QColor("#1e293b"));
    const Segment *seg = m_play->current();
    if (seg && seg->ms > 0) {
        const double f = qBound(0.0, double(m_play->segmentElapsed()) / seg->ms, 1.0);
        QColor c = seg->type == Segment::Program ? kindColor(seg->kind)
                 : seg->type == Segment::Ad      ? QColor("#16a34a")
                                                 : QColor("#f59e0b");
        p.fillRect(QRect(track.left(), track.top(), int(track.width() * f), track.height()), c);
    }

    // left: ON AIR + timecode
    const bool live = m_play->isLive();
    const int cy = r.top() + 3 + (r.height() - 3) / 2;
    p.setPen(Qt::NoPen);
    p.setBrush(live ? QColor("#ef4444") : QColor("#475569"));
    p.drawEllipse(QPoint(r.left() + 14, cy), 4, 4);
    p.setFont(uiFont(12, true));
    p.setPen(live ? QColor("#ef4444") : QColor("#64748b"));
    p.drawText(QRect(r.left() + 24, r.top() + 3, 80, r.height() - 3), Qt::AlignVCenter | Qt::AlignLeft,
               live ? "ON AIR" : "OFF AIR");

    const qint64 ms = m_play->totalElapsed();
    const int fps = qMax(1, m_lib->display.fps);
    const int s = int(ms / 1000), f = int((ms % 1000) * fps / 1000);
    auto two = [](int v) { return QString("%1").arg(v, 2, 10, QChar('0')); };
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    mono.setPixelSize(12);
    p.setFont(mono);
    p.setPen(QColor("#64748b"));
    p.drawText(QRect(r.left() + 96, r.top() + 3, 160, r.height() - 3), Qt::AlignVCenter | Qt::AlignLeft,
               QString("TC %1:%2:%3:%4").arg(two(s / 3600), two(s / 60 % 60), two(s % 60), two(f)));

    // right: Breaking in Ns
    const qint64 left = m_play->breakCountdownMs();
    if (left >= 0) {
        const int secs = int(std::ceil(left / 1000.0));
        const QString t = "Breaking in " + (secs < 60 ? QString("%1s").arg(secs) : formatDuration(secs));
        p.setFont(uiFont(12, true));
        const int tw = QFontMetrics(p.font()).horizontalAdvance(t);
        p.setPen(QColor("#e2e8f0"));
        p.drawText(QRect(r.right() - tw - 14, r.top() + 3, tw + 4, r.height() - 3), Qt::AlignVCenter | Qt::AlignRight, t);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#ef4444"));
        p.drawEllipse(QPoint(r.right() - tw - 24, cy), 4, 4);
    }
}
