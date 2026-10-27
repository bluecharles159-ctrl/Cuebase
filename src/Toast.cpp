#include "Toast.h"
#include "UiControls.h"
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>

ToastHost::ToastHost(QWidget *parent):QWidget(parent){
    setObjectName("noticeOverlay");setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_TranslucentBackground);
    parent->installEventFilter(this);m_card=new QWidget(this);m_card->setObjectName("noticeCard");
    m_card->setStyleSheet("QWidget#noticeCard{background:palette(window);border:1px solid palette(mid);border-radius:10px;}QLabel{background:transparent;border:0;color:palette(window-text);}");
    auto *shadow=new QGraphicsDropShadowEffect(m_card);shadow->setBlurRadius(24);shadow->setOffset(0,5);shadow->setColor(QColor(0,0,0,140));m_card->setGraphicsEffect(shadow);
    auto *row=new QHBoxLayout(m_card);row->setContentsMargins(12,10,16,10);row->setSpacing(10);m_icon=new QLabel;m_icon->setFixedSize(20,20);m_text=new QLabel;m_text->setTextFormat(Qt::PlainText);m_text->setMinimumWidth(0);row->addWidget(m_icon);row->addWidget(m_text,1);
    m_hide=new QTimer(this);m_hide->setSingleShot(true);connect(m_hide,&QTimer::timeout,this,&QWidget::hide);hide();
}
void ToastHost::layoutToast(){
    setGeometry(parentWidget()->rect());const int available=qMax(1,qMin(460,width()-32));m_card->setFixedWidth(available);m_text->setText(m_text->fontMetrics().elidedText(m_line,Qt::ElideRight,qMax(1,available-58)));m_card->adjustSize();m_card->move((width()-available)/2,qMax(8,height()-m_card->height()-24));
}
bool ToastHost::eventFilter(QObject *object,QEvent *event){if(object==parentWidget()&&event->type()==QEvent::Resize)layoutToast();return QWidget::eventFilter(object,event);}
void ToastHost::showNotice(const QString &kind,const QString &message,const QString &detail){
    m_line=message+(detail.isEmpty()?QString():" {"+detail+"}");m_text->setToolTip(m_line);m_text->setAccessibleName(m_line);
    m_icon->setPixmap(uiIcon(kind).pixmap(20,20));layoutToast();show();raise();m_hide->start(3200);
}
void ToastHost::notify(QWidget *context,const QString &kind,const QString &message,const QString &detail){
    if(!context)return;auto *top=context->window();auto *host=top->findChild<ToastHost*>("noticeOverlay",Qt::FindDirectChildrenOnly);if(!host)host=new ToastHost(top);host->showNotice(kind,message,detail);
}
