#include "Toast.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QTimer>
#include <QGraphicsDropShadowEffect>

ToastHost::ToastHost(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    m_card = new QWidget(this);
    m_card->setStyleSheet("background:#0f172a; border:1px solid #334155; border-radius:10px;");
    auto *shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 160));
    m_card->setGraphicsEffect(shadow);
    auto *row = new QHBoxLayout(m_card);
    row->setContentsMargins(12, 10, 16, 10);
    row->setSpacing(10);
    m_icon = new QLabel;
    m_icon->setStyleSheet("font-size:16px; background:transparent; border:none;");
    m_text = new QLabel;
    m_text->setStyleSheet("background:transparent; border:none; color:#e2e8f0; font-weight:600;");
    m_text->setMaximumWidth(360);
    row->addWidget(m_icon);
    row->addWidget(m_text, 1);
    m_card->hide();
    m_hide = new QTimer(this);
    m_hide->setSingleShot(true);
    connect(m_hide, &QTimer::timeout, this, [this] { m_card->hide(); });
}

void ToastHost::layoutToast() {
    if (!parentWidget()) return;
    setGeometry(parentWidget()->rect());
    m_card->adjustSize();
    const int x = (width() - m_card->width()) / 2;
    const int y = height() - m_card->height() - 28;
    m_card->move(qMax(12, x), qMax(12, y));
}

void ToastHost::resizeEvent(QResizeEvent *) { layoutToast(); }

void ToastHost::showNotice(const QString &kind, const QString &message, const QString &detail) {
    QString icon = "●";
    if (kind == "audio") icon = "🔊";
    else if (kind == "save") icon = "💾";
    else if (kind == "video") icon = "🎬";
    else if (kind == "program") icon = "🎞";
    else if (kind == "element") icon = "🧩";
    m_icon->setText(icon);
    QString line = message;
    if (!detail.isEmpty()) {
        QString d = detail;
        if (d.size() > 28) d = d.left(25) + QStringLiteral("…");
        line += "  (" + d + ")";
    }
    m_text->setText(line);
    m_card->show();
    layoutToast();
    m_hide->start(2800);
}

void ToastHost::notify(QWidget *context, const QString &kind, const QString &message, const QString &detail) {
    QWidget *w = context;
    while (w && !qobject_cast<ToastHost *>(w->findChild<ToastHost *>())) {
        if (w->parentWidget()) w = w->parentWidget();
        else break;
    }
    ToastHost *host = w ? w->findChild<ToastHost *>() : nullptr;
    if (!host && context) {
        QWidget *top = context->window();
        host = top ? top->findChild<ToastHost *>() : nullptr;
    }
    if (host) host->showNotice(kind, message, detail);
}
