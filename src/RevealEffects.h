#pragma once
#include "ElementDesign.h"
#include <QPainter>
#include <QtMath>

namespace RevealEffects {
inline double noise(int x,int y,int seed=0){
    quint32 h=quint32(x)*374761393u^quint32(y)*668265263u^quint32(seed)*2246822519u;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return double(h&65535)/65535.;
}
inline double progress(const DObject &o){
    double p=qBound(0.,o.propertyBase("effects.reveal"),1.);const auto timing=o.style["effects.revealTiming"].toString("Linear");
    if(timing=="Smooth")return p*p*(3-2*p);
    if(timing=="Constant")return p>=1?1:0;
    if(timing=="Steps"){const int steps=qBound(2,o.style["effects.revealSteps"].toInt(8),100);return qFloor(p*steps)/double(steps);}
    if(timing=="Flicker"&&p>0&&p<1){const int phase=qFloor(p*80);return noise(phase,0,o.style["effects.revealSeed"].toInt())>.35?p:qMax(0.,p-.2);}
    return p;
}
// Object-space sampling keeps exports, backward scrubbing and repeated plays identical.
inline bool apply(QImage &image,const DObject &o,int padding,int width,int height,double p){
    const QString style=o.style["effects.revealStyle"].toString("Wipe");
    if(style=="Wipe")return false;
    if(p>=1)return true;
    if(p<=0){image.fill(Qt::transparent);return true;}
    const int seed=o.style["effects.revealSeed"].toInt();
    const int size=style=="Pixels"?1:qBound(2,o.style["effects.revealBlock"].toInt(32),512);
    if(style=="Assemble"){
        const QImage original=image;image.fill(Qt::transparent);QPainter painter(&image);painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const double spread=o.style["effects.revealSpread"].toDouble(1.);
        for(int y=0;y<image.height();y+=size)for(int x=0;x<image.width();x+=size){
            const QRect tile(x,y,qMin(size,image.width()-x),qMin(size,image.height()-y));
            const double delay=noise(x/size,y/size,seed)*.4,local=qBound(0.,(p-delay)/(1-delay),1.);
            if(local<=0)continue;const double remain=1-local;
            painter.save();painter.setOpacity(local);painter.translate(tile.center()+QPointF((noise(x,y,seed+1)-.5)*width*spread*remain,(noise(x,y,seed+2)-.5)*height*spread*remain));
            painter.rotate((noise(x,y,seed+3)-.5)*180*remain);painter.drawImage(QRectF(-tile.width()/2.,-tile.height()/2.,tile.width(),tile.height()),original,tile);painter.restore();
        }
        return true;
    }
    if(style!="Pixels"&&style!="Blocks"&&style!="Liquid")return false;
    for(int y=0;y<image.height();++y){auto *row=reinterpret_cast<QRgb*>(image.scanLine(y));for(int x=0;x<image.width();++x){
        double alpha=1.;
        if(style=="Liquid"){
            const double u=double(x-padding)/width,v=double(y-padding)/height;
            const double edge=(1-p)*1.3-.15+(.06*qSin(u*18+p*8)+.03*qSin(u*37-p*13))*qSin(p*M_PI);
            alpha=qBound(0.,(v-edge)*height/3.+.5,1.);
        }else alpha=noise(qFloor(double(x-padding)/size),qFloor(double(y-padding)/size),seed)<p?1:0;
        row[x]=qRgba(qRed(row[x]),qGreen(row[x]),qBlue(row[x]),qRound(qAlpha(row[x])*alpha));
    }}return true;
}
}
