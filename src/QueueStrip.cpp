#include "QueueStrip.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

#include "DragDrop.h"
#include "Playout.h"

namespace {
constexpr int kSpacing = 12;
constexpr qint64 kExpireMs = 10 * 60 * 1000;
constexpr int kSnapMs = 4500;

QFont pxFont(int px, bool bold) {
    QFont f("Segoe UI");
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}
} // namespace

// ------------------------------------------------------------------ card

ProgramCard::ProgramCard(const ProgramItem &p, QWidget *parent) : QWidget(parent), m_p(p) {
    setFixedSize(kW, kH);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    if (!p.image.isEmpty()) m_poster = QPixmap(p.image);
    setToolTip(p.description.isEmpty() ? p.title : p.title + "\n" + p.description);
}

void ProgramCard::setState(const QString &s) {
    if (s == m_state) return;
    m_state = s;
    update();
}

void ProgramCard::setTimeText(const QString &t) {
    if (t == m_time) return;
    m_time = t;
    update();
}

void ProgramCard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF poster(1, 1, kW - 2, kPosterH - 2);
    const bool played = m_state == "played";
    p.setOpacity(played ? 0.45 : 1.0);

    QPainterPath clip;
    clip.addRoundedRect(poster, 16, 16);
    p.save();
    p.setClipPath(clip);
    if (!m_poster.isNull()) {  // fill the card, cropping what overflows
        const QPixmap scaled = m_poster.scaled(poster.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawPixmap(poster.topLeft() - QPointF((scaled.width() - poster.width()) / 2, (scaled.height() - poster.height()) / 2), scaled);
    } else {
        const QColor c = kindColor(m_p.kind);
        QLinearGradient g(poster.topLeft(), poster.bottomRight());
        g.setColorAt(0, c.darker(150));
        g.setColorAt(1, QColor("#0b1220"));
        p.fillRect(poster, g);
    }
    QLinearGradient shade(0, kPosterH - 90, 0, kPosterH);  // darkens the bottom so the title reads on any picture
    shade.setColorAt(0, QColor(0, 0, 0, 0));
    shade.setColorAt(1, QColor(0, 0, 0, 215));
    p.fillRect(QRectF(0, kPosterH - 90, kW, 90), shade);
    p.restore();

    if (m_hover || m_state == "now" || m_state == "next") {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(m_state == "now" ? QColor("#3b82f6") : m_state == "next" ? QColor(255, 255, 255, 140) : QColor(255, 255, 255, 90),
                      m_state == "now" ? 2.5 : 1.5));
        p.drawRoundedRect(poster.adjusted(1, 1, -1, -1), 15, 15);
    }

    // top left: status pill
    const QString status = m_state == "now" ? "Showing" : m_state == "next" ? "Next" : played ? "Played" : QString();
    p.setFont(pxFont(11, true));
    if (!status.isEmpty()) {
        const int w = QFontMetrics(p.font()).horizontalAdvance(status) + 18;
        const QRectF r(9, 9, w, 20);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(110, 110, 118, 150));
        p.drawRoundedRect(r, 10, 10);
        p.setPen(QColor(255, 255, 255, 235));
        p.drawText(r, Qt::AlignCenter, status);
    }
    // top right: type badge
    {
        const int w = QFontMetrics(p.font()).horizontalAdvance(m_p.kind) + 18;
        const QRectF r(kW - 9 - w, 9, w, 20);
        p.setPen(Qt::NoPen);
        p.setBrush(kindColor(m_p.kind));
        p.drawRoundedRect(r, 10, 10);
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, m_p.kind);
    }
    // bottom left: title
    p.setFont(pxFont(15, true));
    const QRect tr(12, kPosterH - 56, kW - 24, 46);
    p.setPen(QColor(0, 0, 0, 160));
    p.drawText(tr.translated(1, 1), Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, m_p.title);
    p.setPen(Qt::white);
    p.drawText(tr, Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, m_p.title);

    // under the card: when it airs
    p.setFont(pxFont(13, true));
    p.setPen(QColor("#e5e9f0"));
    p.drawText(QRect(0, kPosterH + 6, kW, kH - kPosterH - 6), Qt::AlignHCenter | Qt::AlignTop, m_time);
}

void ProgramCard::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) return;
    m_press = e->position().toPoint();
    m_down = true;
}

void ProgramCard::mouseMoveEvent(QMouseEvent *e) {
    if (!m_down || !(e->buttons() & Qt::LeftButton) || DragController::instance()->active()) return;
    if ((e->position().toPoint() - m_press).manhattanLength() < 8) return;
    m_down = false;
    const int sec = m_p.seconds;
    DragPayload d;
    d.kind = "program";
    d.id = m_p.id;
    d.label = m_p.title;
    d.color = kindColor(m_p.kind);
    d.widthPx = m_p.isAdSpot() ? qBound(90, sec * 3, 240) : qBound(140, int(sec * 1.2), 720);
    d.grabDx = qMin(d.widthPx, 260) / 2;
    DragController::instance()->begin(d, e->globalPosition().toPoint());
}

void ProgramCard::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && m_down) emit clicked(m_p.id);
    m_down = false;
}

void ProgramCard::enterEvent(QEnterEvent *) {
    m_hover = true;
    update();
}

void ProgramCard::leaveEvent(QEvent *) {
    m_hover = false;
    update();
}

// ------------------------------------------------------------------ strip

QueueStrip::QueueStrip(Library *lib, Playout *play, QWidget *parent)
    : QScrollArea(parent), m_lib(lib), m_play(play) {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setFixedHeight(kHeight);

    m_host = new QWidget;
    m_lay = new QHBoxLayout(m_host);
    m_lay->setContentsMargins(2, 2, 2, 8);
    m_lay->setSpacing(kSpacing);
    m_lay->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_tail = new QWidget;
    setWidget(m_host);

    m_anim = new QPropertyAnimation(horizontalScrollBar(), "value", this);
    m_anim->setDuration(450);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QPropertyAnimation::finished, this, [this] { m_programmatic = false; });

    m_snapTimer.setSingleShot(true);
    m_snapTimer.setInterval(kSnapMs);
    connect(&m_snapTimer, &QTimer::timeout, this, [this] { scrollToTarget(true); });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        if (!m_programmatic) m_snapTimer.start(); // user scrolled back: snap back after ~4.5s
    });

    m_expireTimer.setInterval(5000);
    connect(&m_expireTimer, &QTimer::timeout, this, [this] { refreshStates(); });
    m_expireTimer.start();
    m_clockTimer.setInterval(1000);
    connect(&m_clockTimer, &QTimer::timeout, this, [this] { refreshTimes(); });
    m_clockTimer.start();

    connect(lib, &Library::programsChanged, this, [this] { rebuild(); });
    connect(lib, &Library::adsChanged, this, [this] { rebuild(); });
    connect(play, &Playout::stateChanged, this, [this] { refreshStates(); });
    connect(play, &Playout::segmentChanged, this, [this] { refreshStates(); });
    connect(play, &Playout::programFinished, this, [this] { refreshStates(); });
    connect(play, &Playout::segmentsChanged, this, [this] { refreshTimes(); });
    rebuild();
}

void QueueStrip::rebuild() {
    for (ProgramCard *c : std::as_const(m_cards)) {
        m_lay->removeWidget(c);
        c->hide();
        c->deleteLater();
    }
    m_cards.clear();
    if (m_empty) {
        m_lay->removeWidget(m_empty);
        m_empty->deleteLater();
        m_empty = nullptr;
    }
    m_lay->removeWidget(m_tail);

    if (m_lib->programs.isEmpty()) {
        auto *empty = new QLabel("No programs queued. Add some in the Programs tab.");
        empty->setStyleSheet("color:palette(placeholder-text); padding:20px;");
        m_lay->addWidget(empty);
        m_empty = empty;
    }
    for (const ProgramItem &p : m_lib->programs) {
        auto *card = new ProgramCard(p);
        connect(card, &ProgramCard::clicked, m_lib, [this](const QString &id) { m_lib->requestOpenProgram(id); });
        m_lay->addWidget(card);
        m_cards.append(card);
    }
    m_lay->addWidget(m_tail);
    m_lastCurrent = -2;
    refreshStates();
    refreshTimes();
}

void QueueStrip::refreshTimes() {
    // the broadcast clock: where the timeline is now is "now"
    const QDateTime base = QDateTime::currentDateTime().addMSecs(-m_play->playheadMs());
    for (int i = 0; i < m_cards.size() && i < m_lib->programs.size(); ++i) {
        const QString id = m_lib->programs[i].id;
        const qint64 a = m_play->programStartMs(id), b = m_play->groupEndMs(id);
        m_cards[i]->setTimeText(base.addMSecs(a).toString("HH:mm") + "-" + base.addMSecs(b).toString("HH:mm"));
    }
}

void QueueStrip::refreshStates() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const int cur = m_play->currentProgramIndex();
    int nextIdx = -1;
    for (int i = 0; i < m_lib->programs.size(); ++i)
        if (i != cur && m_play->finishedAt(m_lib->programs[i].id) == 0 && (cur < 0 || i > cur)) {
            nextIdx = i;
            break;
        }

    bool layoutChanged = false;
    for (int i = 0; i < m_cards.size() && i < m_lib->programs.size(); ++i) {
        ProgramCard *card = m_cards[i];
        const QString id = m_lib->programs[i].id;
        const qint64 fin = m_play->finishedAt(id);
        const bool expired = fin > 0 && now - fin > kExpireMs;
        if (card->isVisible() == expired) layoutChanged = true;
        card->setVisible(!expired);
        card->setState(fin > 0 ? "played" : i == cur ? "now" : i == nextIdx ? "next" : "queued");
    }

    m_tail->setFixedWidth(qMax(0, viewport()->width() - ProgramCard::kW));
    m_lay->activate();
    if (cur != m_lastCurrent) {
        m_lastCurrent = cur;
        scrollToTarget(true); // the next program takes the left; finished ones slide away
    } else if (layoutChanged && !m_snapTimer.isActive()) {
        scrollToTarget(false);
    }
}

int QueueStrip::targetX() const {
    const int cur = m_play->currentProgramIndex();
    int idx = cur;
    if (idx < 0) {
        for (int i = 0; i < m_lib->programs.size(); ++i)
            if (m_play->finishedAt(m_lib->programs[i].id) == 0) {
                idx = i;
                break;
            }
    }
    if (idx < 0 || idx >= m_cards.size()) return 0;
    return m_cards[idx]->x() - m_lay->contentsMargins().left();
}

void QueueStrip::scrollToTarget(bool animate) {
    m_snapTimer.stop();
    m_lay->activate();
    const int x = targetX();
    m_anim->stop();
    m_programmatic = true;
    if (animate) {
        m_anim->setStartValue(horizontalScrollBar()->value());
        m_anim->setEndValue(x);
        m_anim->start();
    } else {
        horizontalScrollBar()->setValue(x);
        m_programmatic = false;
    }
}

// sideways wheel / shift scrolls the cards; the plain wheel scrolls the page
void QueueStrip::wheelEvent(QWheelEvent *e) {
    const bool sideways = e->angleDelta().x() != 0 || (e->modifiers() & Qt::ShiftModifier);
    if (!sideways || horizontalScrollBar()->maximum() <= 0) {
        e->ignore();
        return;
    }
    const int d = e->angleDelta().x() != 0 ? e->angleDelta().x() : e->angleDelta().y();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d);
    e->accept();
}

void QueueStrip::resizeEvent(QResizeEvent *e) {
    QScrollArea::resizeEvent(e);
    m_tail->setFixedWidth(qMax(0, viewport()->width() - ProgramCard::kW));
}
