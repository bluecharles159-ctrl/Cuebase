#include "UiControls.h"
#include <QPainterPath>
#include <QToolButton>
#include <QTimer>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>

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
        painter->setPen(QPen(QColor(!enabled?"#444b55":focus?"#bbd5f3":hover?"#91a9c5":checked?"#7897bf":"#657181"),1));
        painter->setBrush(QColor(!enabled?"#303640":checked?(hover?"#6688b3":"#52769f"):(hover?"#374353":"#28313d")));
        painter->drawRoundedRect(box,3,3);
        painter->setPen(QPen(QColor(enabled?"#f0f5fb":"#7a8593"),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if(option->state&State_NoChange)painter->drawLine(QPointF(box.left()+3,box.center().y()),QPointF(box.right()-3,box.center().y()));
        else if(checked){QPainterPath tick;tick.moveTo(box.left()+2.8,box.center().y());tick.lineTo(box.left()+5.1,box.bottom()-3);tick.lineTo(box.right()-2.5,box.top()+3);painter->drawPath(tick);}
        painter->restore();
    }
};

UiGesture *uiGesture(){static UiGesture instance;return &instance;}

QIcon uiIcon(const QString &name,bool active){
    QPixmap image(40,40);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(2,2);
    p.setPen(QPen(active?QColor("#c7d6e8"):QColor("#526174"),1.35,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
    const QString n=name.toLower();
    if(n=="effects"){p.setPen(QPen(QColor("#b79bdb"),1.3));p.drawPolygon(QPolygonF{{10,2},{12,8},{18,10},{12,12},{10,18},{8,12},{2,10},{8,8}});p.end();return QIcon(image);}
    if(n=="crew"){p.drawEllipse(QPointF(7,6),3,3);p.drawArc(QRectF(1,10,12,10),0,180*16);p.drawEllipse(QPointF(15,7),2,2);p.drawArc(QRectF(12,11,7,7),0,150*16);p.end();return QIcon(image);}
    if(n=="stop"){p.drawRoundedRect(QRectF(4,4,12,12),1,1);p.end();return QIcon(image);}
    if(n=="scale"){p.drawRect(3,11,6,6);p.drawLine(8,12,17,3);p.drawPolyline(QPolygonF{{11,3},{17,3},{17,9}});p.end();return QIcon(image);}
    if(n=="step"){p.drawPolyline(QPolygonF{{2,16},{9,16},{9,5},{18,5}});p.end();return QIcon(image);}
    if(n=="linear"){p.drawLine(3,17,17,3);p.drawEllipse(QPointF(3,17),1.5,1.5);p.drawEllipse(QPointF(17,3),1.5,1.5);p.end();return QIcon(image);}
    if(n=="cursor"){p.drawEllipse(QPointF(10,10),5,5);p.drawLine(10,1,10,19);p.drawLine(1,10,19,10);p.end();return QIcon(image);}
    if(n=="audio"){p.setPen(QPen(QColor("#85bd9a"),1.4));p.drawLine(8,4,8,15);p.drawLine(8,4,16,2);p.drawLine(16,2,16,13);p.drawEllipse(QPointF(5,15),3,2);p.drawEllipse(QPointF(13,13),3,2);}
    else if(n=="text"){p.setPen(QPen(QColor("#d7a574"),1.4));p.drawLine(4,4,16,4);p.drawLine(10,4,10,17);p.drawLine(6,17,14,17);}
    else if(n=="camera"){p.setPen(QPen(QColor("#88bd91"),1.4));p.drawRoundedRect(QRectF(2,5,11,11),2,2);p.drawPolygon(QPolygonF{{13,8},{18,5},{18,16},{13,13}});}
    else if(n=="light"){p.setPen(QPen(QColor("#ddc677"),1.4));p.drawEllipse(QPointF(10,10),4,4);for(int i=0;i<8;++i){p.save();p.translate(10,10);p.rotate(i*45);p.drawLine(0,6,0,9);p.restore();}}
    else if(n=="shape"||n=="image"){p.setPen(QPen(QColor(n=="shape"?"#df9c64":"#ae8bd2"),1.4));p.drawRoundedRect(QRectF(3,3,14,14),2,2);if(n=="image"){p.drawEllipse(QPointF(7,7),1.5,1.5);p.drawPolyline(QPolygonF{{3,15},{8,10},{11,13},{14,9},{17,12}});}}
    else if(n=="loop"){p.drawPolyline(QPolygonF{{4,7},{4,4},{16,4},{16,8},{18,6}});p.drawPolyline(QPolygonF{{16,13},{16,16},{4,16},{4,12},{2,14}});}
    else
    if(n=="eye"||n=="hidden"){QPainterPath path;path.moveTo(2,10);path.quadTo(10,1,18,10);path.quadTo(10,19,2,10);p.drawPath(path);p.drawEllipse(QPointF(10,10),2.4,2.4);if(n=="hidden")p.drawLine(3,17,17,3);}
    else if(n=="lock"||n=="unlock"){p.drawRoundedRect(QRectF(4,9,12,8),2,2);p.drawArc(QRectF(n=="lock"?6:9,2,8,11),0,180*16);p.drawLine(10,12,10,14);}
    else if(n=="down"||n=="right"){if(n=="down")p.drawPolyline(QPolygonF{{5,8},{10,13},{15,8}});else p.drawPolyline(QPolygonF{{8,5},{13,10},{8,15}});}
    else if(n.contains("undo")||n.contains("back")||n.contains("studio")){p.drawPolyline(QPolygonF{{7,4},{2,9},{7,14}});p.drawLine(2,9,12,9);p.drawArc(QRectF(8,9,9,9),0,100*16);}
    else if(n.contains("redo")){p.drawPolyline(QPolygonF{{13,4},{18,9},{13,14}});p.drawLine(18,9,8,9);p.drawArc(QRectF(3,9,9,9),80*16,100*16);}
    else if(n.contains("save")){p.drawRoundedRect(QRectF(3,2,14,16),1,1);p.drawRect(6,2,8,6);p.drawRect(6,12,8,6);}
    else if(n.contains("add")){p.drawLine(10,3,10,17);p.drawLine(3,10,17,10);}
    else if(n.contains("world")){p.drawEllipse(QRectF(2,2,16,16));p.drawEllipse(QRectF(6,2,8,16));p.drawLine(2,10,18,10);}
    else if(n.contains("modifier")||n.contains("mesh")){p.drawPolyline(QPolygonF{{3,17},{11,9},{16,9},{18,5},{14,6},{14,2},{10,4},{10,8},{2,16},{3,17}});}
    else if(n.contains("inspector")||n.contains("align")){for(int y:{5,10,15}){p.drawLine(3,y,17,y);p.drawEllipse(QPointF(y==10?7:13,y),2,2);}}
    else if(n.contains("fit")||n.contains("composition")){p.drawRect(3,5,14,10);p.drawLine(1,3,5,3);p.drawLine(1,3,1,7);p.drawLine(19,17,15,17);p.drawLine(19,17,19,13);}
    else if(n=="100%"){p.drawEllipse(QRectF(2,2,11,11));p.drawLine(12,12,18,18);p.drawLine(7,5,7,10);}
    else if(n.contains("render")||n.contains("solid")){p.drawEllipse(QRectF(3,3,14,14));if(n.contains("render")){p.setBrush(QColor("#668cb6"));p.drawPie(QRectF(3,3,14,14),-90*16,180*16);}}
    else if(n.contains("graph")||n.contains("animation")){p.drawLine(3,2,3,17);p.drawLine(3,17,18,17);QPainterPath path;path.moveTo(4,15);path.cubicTo(8,15,9,4,17,4);p.drawPath(path);}
    else if(n.contains("play")){p.drawPolygon(QPolygonF{{6,3},{17,10},{6,17}});}
    else if(n.contains("pause")){p.drawLine(7,4,7,16);p.drawLine(13,4,13,16);}
    else if(n.contains("key")){p.drawPolygon(QPolygonF{{10,3},{17,10},{10,17},{3,10}});}
    else if(n.contains("delete")||n.contains("remove")){p.drawLine(4,5,16,5);p.drawLine(8,2,12,2);p.drawPolyline(QPolygonF{{5,5},{6,18},{14,18},{15,5}});}
    else if(n.contains("reset")){p.drawArc(QRectF(3,3,14,14),30*16,290*16);p.drawPolyline(QPolygonF{{13,2},{17,4},{15,8}});}
    else if(n.contains("vertex")){for(auto q:QPolygonF{{4,4},{16,4},{4,16},{16,16}})p.drawEllipse(q,2,2);}
    else if(n.contains("edge")){p.drawLine(4,16,16,4);p.drawEllipse(QPointF(4,16),2,2);p.drawEllipse(QPointF(16,4),2,2);}
    else if(n.contains("face")){p.setBrush(QColor("#587da5"));p.drawRect(4,4,12,12);}
    else if(n.contains("edit")){p.drawPolyline(QPolygonF{{3,17},{4,12},{14,2},{18,6},{8,16},{3,17}});p.drawLine(12,4,16,8);}
    else {p.drawPolygon(QPolygonF{{3,6},{10,2},{17,6},{17,14},{10,18},{3,14}});p.drawPolyline(QPolygonF{{3,6},{10,10},{17,6}});p.drawLine(10,10,10,18);}
    p.end();return QIcon(image);
}
class UiBehavior:public QObject{
public:using QObject::QObject;
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::KeyPress){auto *key=static_cast<QKeyEvent*>(event);if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){
            if(auto *line=qobject_cast<QLineEdit*>(object))QTimer::singleShot(0,line,[line]{line->deselect();line->clearFocus();});
            if(auto *text=qobject_cast<QPlainTextEdit*>(object);text&&!(key->modifiers()&Qt::ShiftModifier)){text->clearFocus();auto cursor=text->textCursor();cursor.clearSelection();text->setTextCursor(cursor);return true;}
        }}
        if(event->type()==QEvent::Wheel){auto *wheel=static_cast<QWheelEvent*>(event);if(wheel->angleDelta().x()==0&&wheel->pixelDelta().x()==0&&(qobject_cast<QAbstractSpinBox*>(object)||qobject_cast<QComboBox*>(object))){event->ignore();return true;}}
        if(event->type()==QEvent::Polish)if(auto *b=qobject_cast<QAbstractButton*>(object)){b->setCursor(Qt::PointingHandCursor);if(b->toolTip().isEmpty()&&!b->text().isEmpty())b->setToolTip(b->text().remove('&'));}
        return false;
    }
};
void installUiBehavior(QApplication *app){
    if(app->property("cuebaseUiInstalled").toBool())return;
    app->setProperty("cuebaseUiInstalled",true);
    auto *base=QStyleFactory::create(app->style()->objectName());
    app->setStyle(new RoundedControlStyle(base?base:QStyleFactory::create("Fusion")));
    app->installEventFilter(new UiBehavior(app));
}
