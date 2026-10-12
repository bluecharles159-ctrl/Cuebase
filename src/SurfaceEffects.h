#pragma once
#include "ElementDesign.h"

inline QPainterPath surfaceOutline(QRectF r,const DObject &o){
    QPainterPath path;if(o.style["shape"].toString()=="Ellipse"){path.addEllipse(r);return path;}
    if(o.style["shape"].toString()=="Triangle"){path.moveTo(r.center().x(),r.top());path.lineTo(r.bottomRight());path.lineTo(r.bottomLeft());path.closeSubpath();return path;}
    double unit=o.style["corner.unit"].toString()=="%"?qMin(r.width(),r.height())/100.:1.;
    auto radius=[&](const char *key){return qBound(0.,o.propertyBase(o.style["corner.linked"].toBool(true)?"corner.radius":key)*unit,qMin(r.width(),r.height())/2);};
    double a=radius("corner.tl"),b=radius("corner.tr"),c=radius("corner.br"),d=radius("corner.bl");
    path.moveTo(r.left()+a,r.top());path.lineTo(r.right()-b,r.top());path.quadTo(r.topRight(),r.topRight()+QPointF(0,b));
    path.lineTo(r.right(),r.bottom()-c);path.quadTo(r.bottomRight(),r.bottomRight()-QPointF(c,0));
    path.lineTo(r.left()+d,r.bottom());path.quadTo(r.bottomLeft(),r.bottomLeft()-QPointF(0,d));
    path.lineTo(r.left(),r.top()+a);path.quadTo(r.topLeft(),r.topLeft()+QPointF(a,0));path.closeSubpath();return path;
}
inline QImage backdropImage(const QImage &source,int radius,double brightness){
    QImage a=source.convertToFormat(QImage::Format_ARGB32_Premultiplied),b(a.size(),a.format());radius=qBound(0,radius,100);
    if(radius)for(int pass=0;pass<2;++pass){int rows=pass?a.width():a.height(),cols=pass?a.height():a.width();
        for(int row=0;row<rows;++row){qint64 sums[4]={};auto add=[&](int column,int sign){column=qBound(0,column,cols-1);QRgb c=pass?a.pixel(row,column):a.pixel(column,row);sums[0]+=sign*qRed(c);sums[1]+=sign*qGreen(c);sums[2]+=sign*qBlue(c);sums[3]+=sign*qAlpha(c);};
            for(int i=-radius;i<=radius;++i)add(i,1);for(int col=0;col<cols;++col){int n=radius*2+1;auto c=qRgba(sums[0]/n,sums[1]/n,sums[2]/n,sums[3]/n);if(pass)b.setPixel(row,col,c);else b.setPixel(col,row,c);add(col-radius,-1);add(col+radius+1,1);}}
        a=b.copy();
    }
    if(brightness!=1)for(int y=0;y<a.height();++y){auto *row=reinterpret_cast<QRgb*>(a.scanLine(y));for(int x=0;x<a.width();++x){auto c=row[x];const int alpha=qAlpha(c);row[x]=qRgba(qBound(0,int(qRed(c)*brightness),alpha),qBound(0,int(qGreen(c)*brightness),alpha),qBound(0,int(qBlue(c)*brightness),alpha),alpha);}}
    return a;
}
