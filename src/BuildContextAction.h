#pragma once
#include <QWidgetAction>
#include <QPushButton>
#include <QPainter>
#include <QElapsedTimer>
#include <QTimer>
#include <cmath>

class BuildContextButton final:public QPushButton {
    QElapsedTimer clock;QTimer timer;
public:
    explicit BuildContextButton(QWidget *parent=nullptr):QPushButton(parent){setText("Build");setAccessibleName("Open in Build");setToolTip("Open this saved screen element in Build");setFixedSize(205,32);clock.start();timer.setInterval(16);connect(&timer,&QTimer::timeout,this,qOverload<>(&QWidget::update));timer.start();}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);p.setBrush(underMouse()?QColor("#4b5260"):QColor("#363636"));p.drawRoundedRect(rect().adjusted(1,1,-1,-1),5,5);
        QFont f=font();f.setPixelSize(11);p.setFont(f);p.setPen(QColor("#eeeeee"));p.drawText(QRect(12,0,125,height()),Qt::AlignVCenter,"Build");
        double time=clock.elapsed(),width=35,offset=0;
        const double times[]={0,160,370,540,700},sizes[]={35,20,50,30,35};
        if(time<700){for(int i=0;i<4;++i)if(time>=times[i]&&time<times[i+1]){double t=(time-times[i])/(times[i+1]-times[i]);t=t*t*(3-2*t);width=sizes[i]+(sizes[i+1]-sizes[i])*t;break;}}
        else offset=3*std::sin((time-700)*.004);
        p.setPen(QPen(QColor("#aec7e7"),1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));double end=184+offset,start=end-width*.55;
        p.drawLine(QPointF(start,16),QPointF(end,16));p.drawPolyline(QPolygonF{{end-5,11},{end,16},{end-5,21}});
    }
};
