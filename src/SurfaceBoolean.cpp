#include "ElementDesign.h"
#include <QCache>
#include <QDataStream>
#include <QIODevice>
#include <QPainter>
#include <QtMath>

QImage ElementDesign::textureFor(const DObject &o,qint64 ms) const {
    QSet<QString> visiting;
    return booleanTexture(o,ms,visiting);
}

QImage ElementDesign::booleanTexture(const DObject &object,qint64 ms,QSet<QString> &visiting) const {
    auto base=baseTexture(object);
    const auto op=object.style.value("boolean.operation").toString();
    if(op!="Difference" && op!="Intersect" && op!="Union")return base;
    const auto *cutter=find(object.style.value("boolean.source").toString());
    if(!cutter || !cutter->hasSurface() || cutter->id==object.id || visiting.contains(cutter->id) || visiting.size()>=32)return base;
    auto world=worldMatrix(object,ms);world.translate(object.surfaceOffset());
    bool invertible=false;const auto inverse=world.inverted(&invertible);
    if(!invertible)return base;
    const auto evaluated=cutter->evaluated(ms);
    auto otherWorld=worldMatrix(*cutter,ms);otherWorld.translate(evaluated.surfaceOffset());
    const auto local=inverse*otherWorld;
    visiting.insert(object.id);
    const auto other=booleanTexture(evaluated,ms,visiting);
    visiting.remove(object.id);
    if(other.isNull() || base.isNull())return base;
    // Both textures are centered on their surface. Project the cutter onto the
    // target's local XY plane, including its pivot, parent transform and padding.
    auto units=[](double size,int pixels){
        const int content=qBound(1,qRound(size*kPxPerUnit),4096);
        return (size+(pixels-content)/kPxPerUnit)/pixels;
    };
    const double tx=units(object.w,base.width()),ty=units(object.h,base.height());
    const double cx=units(evaluated.w,other.width()),cy=units(evaluated.h,other.height());
    const auto pos=local.map(QVector3D(float(-other.width()*.5*cx),float(other.height()*.5*cy),0));
    const auto vx=local.mapVector(QVector3D(float(cx),0,0)),vy=local.mapVector(QVector3D(0,float(-cy),0));
    const QPointF origin(base.width()*.5+pos.x()/tx,base.height()*.5-pos.y()/ty);
    const QPointF dx(vx.x()/tx,-vx.y()/ty),dy(vy.x()/tx,-vy.y()/ty);
    QTransform mapping(dx.x(),dx.y(),dy.x(),dy.y(),origin.x(),origin.y());
    if(!mapping.isInvertible())return base;
    const auto opacity=qBound(0.,double(evaluated.mat.alpha)*cutter->at(ms).opacity,1.);
    QByteArray key;QDataStream data(&key,QIODevice::WriteOnly);
    data<<op<<base.cacheKey()<<other.cacheKey()<<mapping<<opacity;
    static thread_local QCache<QByteArray,QImage> cache(32768);
    if(const auto *cached=cache.object(key))return *cached;
    const QRectF bounds=mapping.mapRect(QRectF(other.rect()));
    int px=0,py=0;
    if(op=="Union"){
        auto pad=[](double value,int extent){return qCeil(qBound(0.,value,qMax(0.,(8192.-extent)/2.)));};
        px=pad(qMax(-bounds.left(),bounds.right()-base.width()),base.width());
        py=pad(qMax(-bounds.top(),bounds.bottom()-base.height()),base.height());
    }
    QImage operand(base.width()+2*px,base.height()+2*py,QImage::Format_ARGB32_Premultiplied);
    if(operand.isNull())return base;
    operand.fill(Qt::transparent);
    QPainter p(&operand);p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.translate(px,py);p.setTransform(mapping,true);
    p.setOpacity(opacity);
    p.drawImage(QPointF(),other);p.end();
    if(op=="Union"){
        QPainter combine(&operand);combine.drawImage(px,py,base);combine.end();base=operand;
    }else{
        QPainter combine(&base);combine.setCompositionMode(op=="Difference"?
            QPainter::CompositionMode_DestinationOut:QPainter::CompositionMode_DestinationIn);
        combine.drawImage(0,0,operand);
    }
    cache.insert(key,new QImage(base),qMax(1,int(base.sizeInBytes()/1024)));return base;
}
