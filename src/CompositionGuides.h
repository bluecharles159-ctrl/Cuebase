#pragma once
#include <QJsonObject>
#include <QImage>
#include <QPainter>
#include <QVector>
#include <QtMath>

namespace CompositionGuides {
struct Visibility {
    bool grid = true;
    bool axes = true;
    bool titleSafe = true;
    bool actionSafe = true;
};

inline QRectF safeFrame(const QRectF &frame,const QJsonObject &style,bool title){
    const double inset=qBound(0.,title?style["editor.titleSafe"].toDouble(.05):style["editor.actionSafe"].toDouble(.035),.45);
    return frame.adjusted(frame.width()*inset,frame.height()*inset,-frame.width()*inset,-frame.height()*inset);
}

inline double visibleGridStep(const QRectF &frame,const QJsonObject &style,int outputWidth){
    const double spacing=qBound(1.,style["editor.gridSpacing"].toDouble(120),4096.);
    double step=spacing*frame.width()/qMax(1,outputWidth);
    if(step<=0)return 12;
    while(step<12)step*=2;
    return step;
}

inline QRectF compositionReference(const QRectF &frame,const QJsonObject &style){
    const double aspect=style.value("editor.referenceAspect").toDouble();
    if(!(aspect>0) || frame.isEmpty())return {};
    QSizeF size(aspect,1);
    size.scale(frame.size(),Qt::KeepAspectRatio);
    QRectF reference(QPointF(),size);reference.moveCenter(frame.center());
    return reference;
}

inline double gridSubdivision(double major,const QJsonObject &style){
    const double minor=major/qBound(1,style.value("editor.gridDivisions").toInt(1),16);
    return minor>=6 ? minor : major;
}

inline void paint(QPainter &p, const QRectF &r, const QJsonObject &guideStyle, int outputWidth,
                  Visibility visible, const QVector<double> &vertical, const QVector<double> &horizontal,
                  bool cameraView = true) {
    if (r.isEmpty()) return;
    p.save();
    p.setBrush(Qt::NoBrush);
    p.setOpacity(qBound(0.,guideStyle.value("editor.guideOpacity").toDouble(1.),1.));
    const QRectF titleSafe = safeFrame(r,guideStyle,true);
    const QRectF actionSafe = safeFrame(r,guideStyle,false);
    p.setPen(QPen(QColor(96,165,250,45), 1, Qt::DashLine));
    if(visible.actionSafe&&cameraView) p.drawRect(actionSafe);
    p.setPen(QPen(QColor(250,204,21,38), 1, Qt::DashLine));
    if(visible.titleSafe&&cameraView) p.drawRect(titleSafe);
    p.setPen(QPen(QColor(148,163,184,28), 1));
    if(visible.axes) p.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x(), r.bottom()));
    if(visible.axes) p.drawLine(QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y()));

    p.save();p.setClipRect(r);
    if(visible.grid){
        p.setPen(QColor(148,163,184,25));
        const double step=visibleGridStep(r,guideStyle,outputWidth);
        const double minor=gridSubdivision(step,guideStyle);
        if(minor<step){
            p.setPen(QColor(148,163,184,13));
            for(double x=r.left();x<r.right();x+=minor)p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));
            for(double y=r.top();y<r.bottom();y+=minor)p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));
            p.setPen(QColor(148,163,184,25));
        }
        for(double x=r.left();x<r.right();x+=step)p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));
        for(double y=r.top();y<r.bottom();y+=step)p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));
    }
    QColor guideColor(guideStyle["editor.guideColor"].toString("#22d3ee"));if(!guideColor.isValid())guideColor=QColor("#22d3ee");
    p.setPen(QPen(guideColor,1,Qt::DashLine));
    const auto reference=cameraView?compositionReference(r,guideStyle):QRectF();
    if(!reference.isEmpty())p.drawRect(reference);

    for(double guide:vertical){double x=r.left()+guide*r.width();p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));}
    for(double guide:horizontal){double y=r.top()+guide*r.height();p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));}
    p.restore();
    p.restore();
}

struct Proof {
    QJsonObject style;
    Visibility visible;
    QVector<double> vertical;
    QVector<double> horizontal;

    void draw(QImage &frame) const {
        QPainter painter(&frame);
        painter.setRenderHint(QPainter::Antialiasing);
        paint(painter, frame.rect(), style, frame.width(), visible, vertical, horizontal);
    }
};
}
