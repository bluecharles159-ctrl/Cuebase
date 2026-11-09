#pragma once
#include "ElementDesign.h"
#include "ParticlePath.h"
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QtMath>
#include <cmath>

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
// Capture the evaluated mesh in the scene file so a portable cue can reuse its silhouette.
inline QString meshStamp(const Mesh &mesh,const QColor &color){
    if(mesh.isEmpty())return {};
    QPolygonF points;
    for(const auto &v:mesh.v)points.append(QPointF(v.x()+.42*v.y(),-v.z()+.32*v.y()));
    const QRectF bounds=points.boundingRect();
    const double extent=qMax(bounds.width(),bounds.height());
    if(extent<=0)return {};
    QImage stamp(96,96,QImage::Format_ARGB32_Premultiplied);
    stamp.fill(Qt::transparent);
    QPainter painter(&stamp);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(48,48);painter.scale(90/extent,90/extent);
    painter.translate(-bounds.center());
    painter.setPen(Qt::NoPen);painter.setBrush(color);
    for(const auto &face:mesh.f){
        QPolygonF polygon;
        for(int i:face)if(i>=0 && i<points.size())polygon.append(points[i]);
        if(polygon.size()>=3)painter.drawPolygon(polygon);
    }
    painter.end();
    QByteArray png;QBuffer sink(&png);sink.open(QIODevice::WriteOnly);
    if(!stamp.save(&sink,"PNG"))return {};
    return QString::fromLatin1(png.toBase64());
}

inline void paintParticles(QImage &image,const QImage &surface,const DObject &o,const QRect &area,double p){
    const auto kind=o.style.value("effects.revealParticles").toString("None");
    const bool emitter=o.style.value("effects.emitter").toBool();
    const double seconds=o.style.value("effects.emitterTimeMs").toDouble()/1000.-o.propertyBase("effects.emitterStart");
    if(kind=="None" || (emitter?seconds<0:(p<=0 || p>=1)))return;
    QImage stamp;
    if(kind=="Mesh stamp"){
        const auto encoded=o.style.value("effects.revealStamp").toString();
        if(encoded.size()>65536)return;
        QBuffer buffer;buffer.setData(QByteArray::fromBase64(encoded.toLatin1()));buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer,"PNG");
        if(reader.size()!=QSize(96,96) || !reader.read(&stamp))return;
    }
    const int count=qBound(1,qRound(o.propertyBase("effects.particleCount")),1024);
    const double size=qBound(.5,o.propertyBase("effects.particleSize"),128.);
    const int seed=o.style.value("effects.revealSeed").toInt();
    const auto style=o.style.value("effects.revealStyle").toString("Wipe");
    const int block=style=="Pixels"?1:qBound(2,qRound(o.propertyBase("effects.revealBlock")),512);
    const double angle=qDegreesToRadians(o.propertyBase("effects.revealAngle"));
    const QPointF direction(qCos(angle),qSin(angle));
    const bool radial=o.style.value("effects.revealMode").toString()=="Radial";
    const QPointF center(o.propertyBase("effects.revealX"),o.propertyBase("effects.revealY"));
    const double radiusMax=qMax(1.,std::hypot(qMax(center.x(),1-center.x())*area.width(),qMax(center.y(),1-center.y())*area.height()));
    const bool smoke=kind=="Smoke",fog=kind=="Fog",fire=kind=="Fire",sparks=kind=="Sparks";
    const double travel=o.propertyBase("effects.particleTravel");
    const double speed=qBound(.05,o.propertyBase("effects.particleSpeed"),8.);
    const double lifetime=emitter?qBound(.05,o.propertyBase("effects.emitterLifetime"),120.):qBound(1.,o.propertyBase("effects.particleLifetime"),100.)/100.;
    const double randomness=qBound(0.,o.propertyBase("effects.particleRandomness"),1.);
    const double turbulence=qBound(0.,o.propertyBase("effects.particleTurbulence"),8.);
    const double gravity=qBound(-8.,o.propertyBase("effects.particleGravity"),8.);
    QTransform emission;emission.rotate(o.propertyBase("effects.particleAngle"));
    const auto path=ParticlePath::curve(ParticlePath::read(o.style.value("effects.particlePath").toArray()));
    const auto pathMix=qBound(0.,o.propertyBase("effects.pathMix"),1.);
    const auto endPath=pathMix>0?ParticlePath::curve(ParticlePath::read(o.style.value("effects.particleEndPath").toArray())):QPainterPath{};
    const bool onPath=!path.isEmpty();
    const bool reverse=o.style.value("effects.pathDirection").toString()=="Reverse";
    QPainter painter(&image);painter.setPen(Qt::NoPen);painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for(int i=0;i<count;++i){
        const double x=noise(i,1,seed),y=noise(i,3,seed);
        const QPoint offset(qRound(x*(area.width()-1)),qRound(y*(area.height()-1)));
        QPointF origin=area.topLeft()+offset;
        double birth;
        if(onPath)birth=x;
        else if(style=="Blocks" || style=="Pixels")birth=noise(offset.x()/block,offset.y()/block,seed);
        else if(style=="Assemble")birth=noise((offset.x()+area.left())/block,(offset.y()+area.top())/block,seed)*.4;
        else if(style=="Liquid")birth=(1.15-y)/1.3;
        else if(radial)birth=std::hypot((x-center.x())*area.width(),(y-center.y())*area.height())/radiusMax;
        else birth=.5+QPointF::dotProduct(QPointF(x-.5,y-.5),direction)/(qAbs(direction.x())+qAbs(direction.y()));
        const double life=lifetime*(1+randomness*(.65*noise(i,2,seed)-.35));
        const double phaseAge=emitter?seconds/life-x:(p-birth)/life;
        if(phaseAge<0)continue;
        const double age=emitter?phaseAge-std::floor(phaseAge):phaseAge;
        if(age<=0 || age>=1)continue;
        if(onPath){
            const double along=qMin(1.,age*speed);
            const double fraction=reverse?1-along:along;
            auto uv=path.pointAtPercent(fraction);
            if(!endPath.isEmpty())uv+=(endPath.pointAtPercent(fraction)-uv)*pathMix;
            origin=area.topLeft()+QPointF(uv.x()*(area.width()-1),uv.y()*(area.height()-1));
        }else if(emitter && o.style.value("effects.emitterSource").toString()=="Point"){
            origin=QPointF(area.center())+QPointF((x-.5)*area.width()*.06,(y-.5)*area.height()*.06);
        }
        const QColor sample=surface.rect().contains(origin.toPoint())?surface.pixelColor(origin.toPoint()):QColor(Qt::transparent);
        const double coverage=onPath || emitter?1.:sample.alphaF();
        if(coverage==0)continue;
        const double radius=size*(1+randomness*(noise(i,4,seed)-.5))*(smoke||fog?1+2*age:1);
        QPointF drift((noise(i,5,seed)-.5)*randomness*size*9*age,-size*(4.5+5*randomness*(noise(i,6,seed)-.5))*age);
        if(fog)drift=QPointF(size*age*6,drift.y()*.2);
        if(kind=="Debris")drift.ry()+=size*11*age*age;
        drift=emission.map(drift*travel*speed);
        const double phase=noise(i,8,seed)*2*M_PI;
        drift+=size*age*turbulence*QPointF(qSin(phase+age*9),qCos(phase-age*7));
        drift.ry()+=size*8*gravity*age*age;
        painter.save();painter.translate(origin+drift);painter.setOpacity(coverage*qSin(age*M_PI)*(emitter?1.:qMin(1.,(1-p)*10)));
        if(!stamp.isNull()){
            painter.rotate(noise(i,7,seed)*360+age*90);
            painter.drawImage(QRectF(-radius,-radius,2*radius,2*radius),stamp);
        }else if(smoke || fog || fire){
            if(fog)painter.scale(2.8,.75);
            if(fire)painter.scale(1-age*.55,1.55);
            QRadialGradient gradient(0,0,radius);
            gradient.setColorAt(0,fire?QColor(255,232,146):QColor(185,190,196,fog?70:120));
            if(fire)gradient.setColorAt(.5,QColor(255,88,23,160));
            gradient.setColorAt(1,Qt::transparent);painter.setBrush(gradient);
            painter.drawEllipse(QPointF(),radius,radius);
        }else if(sparks){
            painter.rotate(-25+50*noise(i,7,seed));painter.setBrush(QColor(255,177,68));
            painter.drawEllipse(QPointF(),radius*.27,radius*1.7);
        }else if(kind=="Energy trails"){
            painter.rotate(30*noise(i,7,seed)-15);
            painter.setPen(QPen(QColor(49,171,255,95),radius,Qt::SolidLine,Qt::RoundCap));
            painter.drawLine(QPointF(),QPointF(0,3*radius));
            painter.setPen(QPen(QColor(193,246,255),radius*.25,Qt::SolidLine,Qt::RoundCap));
            painter.drawLine(QPointF(),QPointF(0,3*radius));
        }else if(kind=="Water droplets" || kind=="Embers"){
            const bool water=kind=="Water droplets";
            painter.setBrush(water?QColor(100,180,235,180):QColor(255,110,33));
            painter.drawEllipse(QPointF(),radius*.55,radius*(water?.9:.58));
            painter.setBrush(water?QColor(226,251,255,210):QColor(255,232,137));
            painter.drawEllipse(QPointF(-radius*.14,-radius*.23),radius*.15,radius*.2);
        }else if(kind=="Debris"){
            painter.rotate(noise(i,7,seed)*360+age*150);
            painter.setBrush(sample.alpha()>0?sample:o.mat.color);
            painter.drawRect(QRectF(-radius,-radius*.5,radius*2,radius));
        }else{
            painter.setBrush(QColor(223,211,183));painter.drawEllipse(QPointF(),radius*.45,radius*.45);
        }
        painter.restore();
    }
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
