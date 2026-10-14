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
#include <QScreen>
#include <QKeyEvent>
#include <QDialog>
#include <cmath>

QIcon uiIcon(const QString &name,bool active=true);
void installUiBehavior(QApplication *app);
QDialog *roundedModal(QWidget *parent, const QString &title);
void releaseInputFocus(QWidget *input);
class UiGesture:public QObject {
    Q_OBJECT
public:
    quint64 token=0,serial=0;
    void begin(){token=++serial;}
    void finish(bool cancel){auto id=token;token=0;emit finished(id,cancel);}
signals:
    void finished(quint64 token,bool cancel);
};
UiGesture *uiGesture();

// Horizontal scrub input: drag to adjust, double-click to type; vertical wheel passes to scrolling.
template<class Base> class ScrubInput : public Base {
    QPoint press,last;double initial=0,total=0;bool pressed=false,dragged=false,typing=false;
public:
    explicit ScrubInput(QWidget *parent=nullptr):Base(parent){
        this->setButtonSymbols(QAbstractSpinBox::NoButtons);this->setKeyboardTracking(false);
        this->setMaximumWidth(130);this->setMinimumWidth(65);this->setFixedHeight(23);
        this->setCursor(Qt::SizeHorCursor);this->lineEdit()->setCursor(Qt::SizeHorCursor);this->lineEdit()->installEventFilter(this);
        this->setToolTip("Drag horizontally or scroll horizontally to adjust. Shift for precision. Double-click to type.");
    }
protected:
    void keyPressEvent(QKeyEvent *event) override {
        if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter){
            this->interpretText();typing=false;this->lineEdit()->deselect();
            releaseInputFocus(this);event->accept();return;
        }
        Base::keyPressEvent(event);
    }
    void wheelEvent(QWheelEvent *event) override {
        if(event->angleDelta().x()!=0 || event->pixelDelta().x()!=0){double delta=event->angleDelta().x()/120.0;if(!delta)delta=event->pixelDelta().x()/20.0;this->setValue(this->value()+delta*this->singleStep());event->accept();}
        else event->ignore();
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        auto finish=[&](bool cancel){pressed=false;this->setProperty("scrubbing",false);this->lineEdit()->releaseMouse();if(cancel)this->setValue(initial);uiGesture()->finish(cancel);};
        if(event->type()==QEvent::FocusOut)typing=false;
        if(event->type()==QEvent::KeyPress){auto *key=static_cast<QKeyEvent*>(event);if(key->key()==Qt::Key_Escape&&pressed){finish(true);return true;}if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){this->interpretText();typing=false;releaseInputFocus(this);return true;}}
        if(event->type()==QEvent::MouseButtonDblClick){if(pressed)finish(false);typing=true;this->lineEdit()->setFocus();this->lineEdit()->selectAll();return true;}
        if(event->type()==QEvent::MouseButtonPress){auto *e=static_cast<QMouseEvent*>(event);if(e->button()==Qt::RightButton&&pressed){finish(true);return true;}if(e->button()==Qt::LeftButton){if(typing)return Base::eventFilter(object,event);press=last=e->globalPosition().toPoint();initial=this->value();total=0;pressed=true;dragged=false;this->lineEdit()->setFocus();this->lineEdit()->selectAll();uiGesture()->begin();this->setProperty("scrubbing",true);return true;}}
        if(event->type()==QEvent::MouseMove&&pressed){auto *e=static_cast<QMouseEvent*>(event);auto pos=e->globalPosition().toPoint();const int dx=pos.x()-last.x();last=pos;total+=dx*(e->modifiers().testFlag(Qt::ShiftModifier)?.1:1);if(qAbs(pos.x()-press.x())>3||qAbs(total)>3)dragged=true;if(dragged){this->lineEdit()->grabMouse();this->setValue(initial+total*this->singleStep()*.2);if(auto *screen=QGuiApplication::screenAt(pos)){auto bounds=screen->geometry();if(pos.x()<=bounds.left()+1||pos.x()>=bounds.right()-1){last.setX(pos.x()<=bounds.left()+1?bounds.right()-3:bounds.left()+3);QCursor::setPos(last);}}}return true;}
        if(event->type()==QEvent::MouseButtonRelease&&pressed){finish(false);if(dragged){this->lineEdit()->deselect();this->clearFocus();}if(!dragged){typing=true;this->lineEdit()->setFocus();this->lineEdit()->selectAll();}return true;}
        return Base::eventFilter(object,event);
    }
    void paintEvent(QPaintEvent *event) override {
        Base::paintEvent(event);
        if(this->maximum()-this->minimum()<=1000){QPainter p(this);p.setPen(Qt::NoPen);p.setBrush(QColor(86,143,210,95));double f=(this->value()-this->minimum())/qMax(1.0,double(this->maximum()-this->minimum()));p.drawRect(QRectF(2,this->height()-3,(this->width()-4)*f,2));}
    }
};
using ValueInput=ScrubInput<QDoubleSpinBox>;
using FrameInput=ScrubInput<QSpinBox>;
