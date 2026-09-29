#include "QueueStrip.h"

#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QStyle>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "Data.h"
#include "Playout.h"

namespace {
constexpr int kCardW = 190;
constexpr int kCardH = 92;
constexpr int kSpacing = 10;
constexpr qint64 kExpireMs = 10 * 60 * 1000;
constexpr int kSnapMs = 4500;
} // namespace

QueueStrip::QueueStrip(Library *lib, Playout *play, QWidget *parent)
    : QScrollArea(parent), m_lib(lib), m_play(play) {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

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

    connect(lib, &Library::programsChanged, this, [this] { rebuild(); });
    connect(lib, &Library::adsChanged, this, [this] { rebuild(); });
    connect(play, &Playout::stateChanged, this, [this] { refreshStates(); });
    connect(play, &Playout::segmentChanged, this, [this] { refreshStates(); });
    connect(play, &Playout::programFinished, this, [this] { refreshStates(); });
    rebuild();
}

void QueueStrip::rebuild() {
    for (QFrame *c : std::as_const(m_cards)) {
        m_lay->removeWidget(c);
        c->hide();
        c->deleteLater();
    }
    m_cards.clear();
    m_lay->removeWidget(m_tail);

    if (m_lib->programs.isEmpty()) {
        auto *empty = new QLabel("No programs queued. Add some in the Program Planner.");
        empty->setStyleSheet("color:#64748b;");
        auto *card = new QFrame;
        auto *l = new QVBoxLayout(card);
        l->addWidget(empty);
        m_lay->addWidget(card);
        m_cards.append(card);
    }
    for (const ProgramItem &p : m_lib->programs) {
        auto *card = new QFrame;
        card->setObjectName("qcard");
        card->setFixedSize(kCardW, kCardH);
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(10, 8, 10, 8);
        l->setSpacing(4);

        auto *top = new QHBoxLayout;
        auto *badge = new QLabel(p.kind);
        badge->setStyleSheet(QString("background:%1; border-radius:4px; padding:1px 8px; font-weight:600; color:white;")
                                 .arg(kindColor(p.kind).name()));
        auto *status = new QLabel;
        status->setObjectName("qstatus");
        status->setStyleSheet("color:#94a3b8; font-size:11px; font-weight:700;");
        top->addWidget(badge);
        top->addStretch();
        top->addWidget(status);
        auto *title = new QLabel(p.title);
        title->setStyleSheet("font-weight:700; font-size:14px;");
        title->setWordWrap(true);
        auto *meta = new QLabel(formatDuration(p.seconds) + (m_lib->ad(p.adId) ? "  ·  + ad break" : ""));
        meta->setStyleSheet("color:#94a3b8; font-size:11px;");
        l->addLayout(top);
        l->addWidget(title);
        l->addStretch();
        l->addWidget(meta);

        card->setGraphicsEffect(new QGraphicsOpacityEffect(card));
        card->setProperty("pid", p.id);
        m_lay->addWidget(card);
        m_cards.append(card);
    }
    m_lay->addWidget(m_tail);
    m_lastCurrent = -2;
    refreshStates();
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
        QFrame *card = m_cards[i];
        const QString id = m_lib->programs[i].id;
        const qint64 fin = m_play->finishedAt(id);
        const bool expired = fin > 0 && now - fin > kExpireMs;
        if (card->isVisible() == expired) layoutChanged = true;
        card->setVisible(!expired);

        const char *state = fin > 0 ? "played" : i == cur ? "now" : i == nextIdx ? "next" : "queued";
        card->setProperty("state", state);
        card->style()->unpolish(card);
        card->style()->polish(card);
        if (auto *st = card->findChild<QLabel *>("qstatus"))
            st->setText(fin > 0 ? "PLAYED" : i == cur ? "NOW" : i == nextIdx ? "NEXT" : "");
        if (auto *fx = qobject_cast<QGraphicsOpacityEffect *>(card->graphicsEffect()))
            fx->setOpacity(fin > 0 ? 0.4 : 1.0);
    }

    m_tail->setFixedWidth(qMax(0, viewport()->width() - kCardW));
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

void QueueStrip::wheelEvent(QWheelEvent *e) {
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - e->angleDelta().y());
    e->accept();
}

void QueueStrip::resizeEvent(QResizeEvent *e) {
    QScrollArea::resizeEvent(e);
    m_tail->setFixedWidth(qMax(0, viewport()->width() - kCardW));
}
