#include "UiControls.h"
#include <QPainterPath>
#include <QMap>
#include <QToolButton>
#include <QTimer>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QListView>
#include <QStyledItemDelegate>
#include <QMenu>
#include <QFontComboBox>

class RoundedControlStyle final : public QProxyStyle {
public:
    explicit RoundedControlStyle(QStyle *base):QProxyStyle(base){}
    int pixelMetric(PixelMetric metric,const QStyleOption *option=nullptr,const QWidget *widget=nullptr) const override {
        if(metric==PM_IndicatorWidth||metric==PM_IndicatorHeight)return 14;
        return QProxyStyle::pixelMetric(metric,option,widget);
    }
    void drawPrimitive(PrimitiveElement element,const QStyleOption *option,QPainter *painter,const QWidget *widget=nullptr) const override {
        if(element!=PE_IndicatorCheckBox&&element!=PE_IndicatorItemViewItemCheck){QProxyStyle::drawPrimitive(element,option,painter,widget);return;}
        const bool enabled=option->state&State_Enabled;
        const bool checked=option->state&(State_On|State_NoChange);
        const bool hover=enabled&&(option->state&State_MouseOver);
        const bool focus=enabled&&(option->state&State_HasFocus);
        painter->save();painter->setRenderHint(QPainter::Antialiasing);
        QRectF box(option->rect);const qreal side=qMin(box.width(),box.height())-2;
        box=QRectF(box.center().x()-side/2,box.center().y()-side/2,side,side);
        painter->setPen(QPen(option->palette.color(focus||checked?QPalette::Highlight:hover?QPalette::Light:QPalette::Mid),1));
        painter->setBrush(option->palette.color(!enabled?QPalette::AlternateBase:checked?QPalette::Highlight:hover?QPalette::Button:QPalette::Base));
        painter->drawRoundedRect(box,3,3);
        painter->setPen(QPen(option->palette.color(enabled?QPalette::HighlightedText:QPalette::Mid),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if(option->state&State_NoChange)painter->drawLine(QPointF(box.left()+3,box.center().y()),QPointF(box.right()-3,box.center().y()));
        else if(checked){QPainterPath tick;tick.moveTo(box.left()+2.8,box.center().y());tick.lineTo(box.left()+5.1,box.bottom()-3);tick.lineTo(box.right()-2.5,box.top()+3);painter->drawPath(tick);}
        painter->restore();
    }
};

UiGesture *uiGesture(){static UiGesture instance;return &instance;}

class OptionDelegate final:public QStyledItemDelegate{
public:using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option,const QModelIndex &index)const override{auto size=QStyledItemDelegate::sizeHint(option,index);size.setHeight(qMax(26,size.height()+8));return size;}
    void paint(QPainter *p,const QStyleOptionViewItem &source,const QModelIndex &index)const override{
        QStyleOptionViewItem option(source);initStyleOption(&option,index);p->save();p->setRenderHint(QPainter::Antialiasing);const auto r=option.rect.adjusted(2,1,-2,-1);
        const bool selected=option.state&(QStyle::State_Selected|QStyle::State_MouseOver),enabled=option.state&QStyle::State_Enabled;
        if(selected&&enabled){p->setPen(Qt::NoPen);p->setBrush(option.palette.color(QPalette::Highlight));p->drawRoundedRect(r,5,5);}
        auto textRect=r.adjusted(8,0,-8,0);if(!option.icon.isNull()){option.icon.paint(p,QRect(textRect.left(),r.center().y()-7,14,14));textRect.setLeft(textRect.left()+21);}
        p->setFont(option.font);p->setPen(option.palette.color(enabled?QPalette::Active:QPalette::Disabled,selected&&enabled?QPalette::HighlightedText:QPalette::Text));p->drawText(textRect,Qt::AlignVCenter|Qt::AlignLeft,option.fontMetrics.elidedText(option.text,Qt::ElideRight,textRect.width()));p->restore();
    }
};

void releaseInputFocus(QWidget *input){
    if(auto *line=qobject_cast<QLineEdit*>(input)){line->deselect();line->clearFocus();}
    if(auto *line=input->findChild<QLineEdit*>()){line->deselect();line->clearFocus();}
    input->clearFocus();
    if(auto *window=input->window();window!=input)window->setFocus(Qt::OtherFocusReason);
}
class UiBehavior:public QObject{
public:using QObject::QObject;
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::KeyPress){auto *key=static_cast<QKeyEvent*>(event);if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){
            if(auto *spin=qobject_cast<QAbstractSpinBox*>(object))QTimer::singleShot(0,spin,[spin]{releaseInputFocus(spin);});
            if(auto *line=qobject_cast<QLineEdit*>(object))QTimer::singleShot(0,line,[line]{releaseInputFocus(line);});
            if(auto *text=qobject_cast<QPlainTextEdit*>(object);text&&!(key->modifiers()&Qt::ShiftModifier)){text->clearFocus();auto cursor=text->textCursor();cursor.clearSelection();text->setTextCursor(cursor);return true;}
        }}
        if(event->type()==QEvent::Wheel){auto *wheel=static_cast<QWheelEvent*>(event);if(wheel->angleDelta().x()==0&&wheel->pixelDelta().x()==0&&(qobject_cast<QAbstractSpinBox*>(object)||qobject_cast<QComboBox*>(object))){event->ignore();return true;}}
        if(event->type()==QEvent::Polish){
            if(auto *b=qobject_cast<QAbstractButton*>(object)){b->setCursor(Qt::PointingHandCursor);if(b->iconSize()==QSize(16,16))b->setIconSize({14,14});if(b->toolTip().isEmpty()&&!b->text().isEmpty())b->setToolTip(b->text().remove('&'));}
            if(auto *combo=qobject_cast<QComboBox*>(object);combo&&!combo->property("styledOptions").toBool()){
                combo->setProperty("styledOptions",true);auto *view=combo->view();view->setObjectName("optionPopup");view->setMouseTracking(true);if(auto *list=qobject_cast<QListView*>(view)){list->setSpacing(2);list->setUniformItemSizes(true);}
                if(!qobject_cast<QFontComboBox*>(combo))view->setItemDelegate(new OptionDelegate(view));
            }
            if(auto *menu=qobject_cast<QMenu*>(object))menu->setAttribute(Qt::WA_TranslucentBackground);
        }
        return false;
    }
};
QDialog *roundedModal(QWidget *parent,const QString &title){
    class Modal final:public QDialog {
    public:using QDialog::QDialog;
    protected:void paintEvent(QPaintEvent *)override{QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(palette().color(QPalette::Mid),1));p.setBrush(palette().color(QPalette::Window));p.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),14,14);}
    };
    auto *box=new Modal(parent,Qt::Dialog|Qt::FramelessWindowHint);
    box->setAttribute(Qt::WA_TranslucentBackground);box->setWindowTitle(title);box->setWindowModality(Qt::WindowModal);
    return box;
}
void installUiBehavior(QApplication *app){
    if(app->property("cuebaseUiInstalled").toBool())return;
    app->setProperty("cuebaseUiInstalled",true);
    auto *base=QStyleFactory::create(app->style()->objectName());
    app->setStyle(new RoundedControlStyle(base?base:QStyleFactory::create("Fusion")));
    app->installEventFilter(new UiBehavior(app));
}
