#include "ElementDesign.h"
#include "SurfaceEffects.h"
#include <QPolygonF>
#include <algorithm>

QMatrix4x4 ElementDesign::worldMatrix(const DObject &o,qint64 ms) const {
    if(o.phys.enabled&&o.phys.active&&o.isRenderable())return physicsWorldMatrix(o,ms);
    QMatrix4x4 result=o.parentInverse*matrixOf(o.at(ms));
    QSet<QString> visited{o.id};const DObject *p=find(o.parentId);
    while(p&&!visited.contains(p->id)){visited.insert(p->id);if(p->phys.enabled&&p->phys.active&&p->isRenderable()){result=physicsWorldMatrix(*p,ms)*result;break;}result=p->parentInverse*matrixOf(p->at(ms))*result;p=find(p->parentId);}
    return result;
}
QMatrix4x4 ElementDesign::parentMatrix(const DObject &o,qint64 ms) const {
    const auto *p=find(o.parentId);return p?worldMatrix(*p,ms)*o.parentInverse:o.parentInverse;
}
bool ElementDesign::effectiveVisible(const DObject &o) const {
    if(!o.visible)return false;QSet<QString> seen{o.id};const DObject *p=find(o.parentId);
    while(p&&!seen.contains(p->id)){if(!p->visible)return false;seen.insert(p->id);p=find(p->parentId);}return true;
}
QPainterPath ElementDesign::overflowClip(const DObject &o,qint64 ms,const ViewCam &cam,const QRectF &rect,const QRectF &viewport) const {
    const QRectF bounds=viewport.isValid()?viewport:rect;
    QPainterPath result;result.addRect(bounds);
    QSet<QString> seen{o.id};const DObject *source=find(o.parentId);
    while(source&&!seen.contains(source->id)){
        seen.insert(source->id);const DObject p=source->evaluated(ms);
        const bool x=p.style["overflow.x"].toString("Visible")=="Hidden",y=p.style["overflow.y"].toString("Visible")=="Hidden";
        if((x||y)&&p.isRenderable()){
            const auto world=worldMatrix(p,ms);QPolygonF quad;
            for(const auto &v:QVector<QVector3D>{{float(-p.w/2),float(p.h/2),0},{float(p.w/2),float(p.h/2),0},{float(p.w/2),float(-p.h/2),0},{float(-p.w/2),float(-p.h/2),0}}){QPointF q;if(!cam.toScreen(world.map(v),rect,&q))return {};quad<<q;}
            QPainterPath clip;
            if(x&&y){
                QPolygonF local{QPointF(0,0),QPointF(p.w*kPxPerUnit,0),QPointF(p.w*kPxPerUnit,p.h*kPxPerUnit),QPointF(0,p.h*kPxPerUnit)};
                QTransform transform;if(!QTransform::quadToQuad(local,quad,transform))return {};
                QPainterPath shape=surfaceOutline(QRectF(0,0,p.w*kPxPerUnit,p.h*kPxPerUnit),p);
                clip=transform.map(shape);
            }else{
                // Clip the output rectangle against just the requested pair of projected edges.
                QPolygonF polygon(bounds);QPointF center=(quad[0]+quad[1]+quad[2]+quad[3])/4;
                auto side=[](QPointF a,QPointF b,QPointF q){return (b.x()-a.x())*(q.y()-a.y())-(b.y()-a.y())*(q.x()-a.x());};
                for(int edge:(x?QVector<int>{1,3}:QVector<int>{0,2})){
                    auto a=quad[edge],b=quad[(edge+1)%4];double sign=side(a,b,center)>=0?1:-1;QPolygonF next;
                    for(int i=0;i<polygon.size();++i){auto c=polygon[i],d=polygon[(i+1)%polygon.size()];double sc=side(a,b,c)*sign,sd=side(a,b,d)*sign;if(sc>=0)next<<c;if((sc>=0)!=(sd>=0))next<<c+(d-c)*(sc/(sc-sd));}polygon=next;
                }
                clip.addPolygon(polygon);clip.closeSubpath();
            }
            result=result.intersected(clip);
        }
        source=find(source->parentId);
    }
    return result;
}
