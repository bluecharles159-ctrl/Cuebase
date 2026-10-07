#pragma once
#include <QWidget>
#include <QPainter>
#include <QGraphicsBlurEffect>
#include <QLabel>
#include <QTimer>
#include <cmath>

class FileDropOverlay final:public QWidget {
    QTimer timer;int tick=0;QLabel *backdrop;
public:
    explicit FileDropOverlay(QWidget *parent):QWidget(parent){
        setAttribute(Qt::WA_TransparentForMouseEvents);setGeometry(parent->rect());
        backdrop=new QLabel(this);backdrop->setPixmap(parent->grab());backdrop->setGeometry(rect());backdrop->setScaledContents(true);
        auto *blur=new QGraphicsBlurEffect(backdrop);blur->setBlurRadius(9);backdrop->setGraphicsEffect(blur);
        timer.setInterval(30);connect(&timer,&QTimer::timeout,this,[this]{++tick;update();});timer.start();show();raise();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        // The translucent veil and foreground are painted above the blurred snapshot.
    }
    void showEvent(QShowEvent *) override {
        auto *foreground=new QWidget(this);foreground->setAttribute(Qt::WA_TransparentForMouseEvents);foreground->setGeometry(rect());
        foreground->installEventFilter(this);foreground->show();
        connect(&timer,&QTimer::timeout,foreground,qOverload<>(&QWidget::update));
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()!=QEvent::Paint)return QWidget::eventFilter(object,event);
        auto *widget=qobject_cast<QWidget*>(object);QPainter p(widget);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor(20,20,24,145));
        const QPointF c(width()/2.,height()/2.);p.setPen(QPen(QColor("#acc3e6"),2));p.setBrush(QColor("#454a54"));
        for(int i=-1;i<=1;++i){double y=std::sin(qMin(tick,35)*.16+i)*qMax(0,35-tick)*.25;p.save();p.translate(c+QPointF(i*43,-38+y));p.rotate(i*12);p.drawRoundedRect(QRectF(-19,-28,38,50),5,5);p.drawLine(-10,-10,10,-10);p.drawLine(-10,0,10,0);p.restore();}
        p.setPen(Qt::white);QFont font=p.font();font.setPixelSize(19);p.setFont(font);p.drawText(QRectF(c.x()-300,c.y()+20,600,35),Qt::AlignCenter,"Drop files into Cuebase");font.setPixelSize(12);p.setFont(font);p.setPen(QColor("#c0c4cd"));p.drawText(QRectF(c.x()-350,c.y()+56,700,25),Qt::AlignCenter,"Images · SVG · Audio · Video · FBX · OBJ · glTF");return true;
    }
};
