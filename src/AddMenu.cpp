#include "AddMenu.h"

#include <QGuiApplication>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

AddMenu::AddMenu(QWidget *parent) : QFrame(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *card = new QFrame;
    card->setObjectName("addCard");
    card->setStyleSheet(
        "QFrame#addCard { background:rgba(44,44,48,245); border:1px solid rgba(255,255,255,40); border-radius:14px; }"
        "QPushButton { background:transparent; border:none; border-radius:8px; padding:8px 30px;"
        "              font-size:12px; font-weight:600; color:#e5e7eb; }"
        "QPushButton:hover { background:rgba(255,255,255,30); }");
    outer->addWidget(card);

    auto *l = new QVBoxLayout(card);
    l->setContentsMargins(8, 8, 8, 8);
    l->setSpacing(2);
    for (const QString &name : {QStringLiteral("Clip"), QStringLiteral("Sound"), QStringLiteral("Ad Center"),
                                QStringLiteral("Element")}) {
        auto *b = new QPushButton(name);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumWidth(150);
        l->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, name] {
            close();
            emit chosen(name);
        });
    }
}

void AddMenu::popupAbove(const QPoint &globalAnchor) {
    adjustSize();
    QPoint p(globalAnchor.x() - width() / 2, globalAnchor.y() - height() - 8);
    if (const QScreen *sc = QGuiApplication::screenAt(globalAnchor)) {
        const QRect a = sc->availableGeometry();
        if (p.y() < a.top()) p.setY(globalAnchor.y() + 34);  // no room above: open below
        p.setX(qBound(a.left() + 4, p.x(), a.right() - width() - 4));
    }
    move(p);
    show();
}
