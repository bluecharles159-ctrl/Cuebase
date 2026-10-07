#pragma once
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QIcon>
#include <cmath>

QIcon uiIcon(const QString &name,bool active=true);
void installUiBehavior(QApplication *app);

// Horizontal scrub input: drag to adjust, double-click to type; vertical wheel passes to scrolling.
template<class Base> class ScrubInput : public Base {
    QPoint press;double initial=0;bool pressed=false,dragged=false;
public:
    explicit ScrubInput(QWidget *parent=nullptr):Base(parent){
        this->setButtonSymbols(QAbstractSpinBox::NoButtons);this->setKeyboardTracking(false);
        this->setMaximumWidth(130);this->setMinimumWidth(65);this->setFixedHeight(23);
        this->setCursor(Qt::SizeHorCursor);this->lineEdit()->setCursor(Qt::SizeHorCursor);this->lineEdit()->installEventFilter(this);
        this->setToolTip("Drag horizontally or scroll horizontally to adjust. Shift for precision. Double-click to type.");
    }
protected:
    void wheelEvent(QWheelEvent *event) override {
        if(event->angleDelta().x()!=0 || event->pixelDelta().x()!=0){double delta=event->angleDelta().x()/120.0;if(!delta)delta=event->pixelDelta().x()/20.0;this->setValue(this->value()+delta*this->singleStep());event->accept();}
        else event->ignore();
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::MouseButtonDblClick){pressed=false;this->lineEdit()->setFocus();this->lineEdit()->selectAll();return true;}
        if(event->type()==QEvent::MouseButtonPress){auto *e=static_cast<QMouseEvent*>(event);if(e->button()==Qt::LeftButton){press=e->globalPosition().toPoint();initial=this->value();pressed=true;dragged=false;this->setProperty("scrubbing",true);this->setProperty("undoCaptured",false);return true;}}
        if(event->type()==QEvent::MouseMove&&pressed){auto *e=static_cast<QMouseEvent*>(event);int dx=e->globalPosition().toPoint().x()-press.x();if(qAbs(dx)>3)dragged=true;if(dragged){double step=this->singleStep()*(e->modifiers().testFlag(Qt::ShiftModifier)?.1:1);this->setValue(initial+dx*step*.2);}return true;}
        if(event->type()==QEvent::MouseButtonRelease&&pressed){pressed=false;this->setProperty("scrubbing",false);if(!dragged){this->lineEdit()->setFocus();this->lineEdit()->selectAll();}return true;}
        return Base::eventFilter(object,event);
    }
    void paintEvent(QPaintEvent *event) override {
        Base::paintEvent(event);
        if(this->maximum()-this->minimum()<=1000){QPainter p(this);p.setPen(Qt::NoPen);p.setBrush(QColor(86,143,210,95));double f=(this->value()-this->minimum())/qMax(1.0,double(this->maximum()-this->minimum()));p.drawRect(QRectF(2,this->height()-3,(this->width()-4)*f,2));}
    }
};
using ValueInput=ScrubInput<QDoubleSpinBox>;
using FrameInput=ScrubInput<QSpinBox>;
