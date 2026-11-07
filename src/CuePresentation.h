#pragma once

#include "UiControls.h"
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>

namespace CuePresentation {
inline QHBoxLayout *heading(QDialog *dialog) {
    auto *row = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(uiIcon("cue").pixmap(18,18));
    row->addWidget(icon);
    auto *title = new QLabel(dialog->windowTitle());
    auto font = title->font();
    font.setBold(true);
    title->setFont(font);
    row->addWidget(title,1);
    auto *close = new QToolButton;
    close->setIcon(uiIcon("close"));
    close->setAccessibleName("Close");
    close->setToolTip("Close");
    row->addWidget(close);
    QObject::connect(close,&QToolButton::clicked,dialog,&QDialog::reject);
    return row;
}

class CardDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option,const QModelIndex &) const override {
        return {80,2*option.fontMetrics.lineSpacing()+16};
    }
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const auto &palette = option.palette;
        const QColor ink = palette.color(selected?QPalette::HighlightedText:QPalette::Text);
        QRect card = option.rect.adjusted(1,1,-1,-1);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(palette.color(selected||hovered?QPalette::Highlight:QPalette::Mid));
        p->setBrush(palette.color(selected?QPalette::Highlight:QPalette::Base));
        p->drawRoundedRect(card,6,6);
        QRect iconBox(card.left()+10,card.center().y()-8,16,16);
        qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).paint(p,iconBox);
        const auto label = index.data(Qt::DisplayRole).toString();
        const int split = label.indexOf('\n');
        const auto name = split<0?label:label.left(split);
        const auto detail = split<0?QString():label.mid(split+1).simplified();
        QRect text = card.adjusted(36,5,-10,-5);
        QFont font = option.font;
        font.setBold(true);
        p->setFont(font);
        p->setPen(ink);
        const int half = text.height()/2;
        QRect first = text;
        if (!detail.isEmpty()) first.setHeight(half);
        p->drawText(first,Qt::AlignVCenter|Qt::AlignLeft,p->fontMetrics().elidedText(name,Qt::ElideRight,text.width()));
        if (!detail.isEmpty()) {
            p->setFont(option.font);
            text.setTop(first.bottom()+1);
            p->drawText(text,Qt::AlignVCenter|Qt::AlignLeft,p->fontMetrics().elidedText(detail,Qt::ElideRight,text.width()));
        }
        if (index.data(Qt::ForegroundRole).value<QBrush>().color()==palette.color(QPalette::Link)) {
            p->setPen(Qt::NoPen);
            p->setBrush(selected?ink:palette.color(QPalette::Link));
            p->drawRoundedRect(QRect(card.left()+2,card.top()+6,3,card.height()-12),1,1);
        }
        p->restore();
    }
};
inline void styleCards(QListWidget *list) {
    list->setItemDelegate(new CardDelegate(list));
    list->setSpacing(3);
    list->setMouseTracking(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}
}
