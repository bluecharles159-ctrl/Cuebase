#include "SurfaceTreatments.h"
#include <QPainter>
#include <QtMath>
#include <cmath>

namespace {
bool enabled(const DObject &o,const QString &effect) {
    return o.style.value("finish."+effect).toString()=="On";
}
int amount(const DObject &o,const QString &field,int maximum) {
    return qBound(0,qRound(o.propertyBase("finish."+field)),maximum);
}
}
int SurfaceTreatments::padding(const DObject &o) {
    const int depth=enabled(o,"extrude")?amount(o,"depth",64):0;
    const int reflected=enabled(o,"reflection")?amount(o,"height",256)+amount(o,"gap",100):0;
    return depth+reflected;
}

void SurfaceTreatments::apply(QImage &surface,const QRect &bounds,const DObject &o) {
    const bool bevel=enabled(o,"bevel"),sweep=enabled(o,"sweep");
    const int depth=enabled(o,"extrude")?amount(o,"depth",64):0;
    const int reflection=enabled(o,"reflection")?amount(o,"height",256):0;
    if (!bevel && !sweep && !depth && !reflection) return;
    const QImage original=surface;
    if (bevel || sweep) {
        const int edge=qBound(1,amount(o,"edge",24),24);
        const double radians=qDegreesToRadians(o.propertyBase("finish.angle"));
        const QPointF direction(std::cos(radians),std::sin(radians));
        const double span=qAbs(direction.x())*bounds.width()+qAbs(direction.y())*bounds.height();
        const double band=qMax(1.,o.propertyBase("finish.band")*span);
        const double peak=(o.propertyBase("finish.position")-.5)*span;
        const double strength=qBound(0.,o.propertyBase("finish.intensity"),1.);
        const QColor glow(o.style.value("finish.tint").toString("#ffffff"));
        const auto center=QRectF(bounds).center();
        auto alpha=[&](int x,int y) {
            return original.rect().contains(x,y)?qAlpha(original.pixel(x,y)):0;
        };
        for (int y=bounds.top();y<=bounds.bottom();++y) {
            auto *row=reinterpret_cast<QRgb*>(surface.scanLine(y));
            for (int x=bounds.left();x<=bounds.right();++x) {
                const auto pixel=original.pixel(x,y);
                if (!qAlpha(pixel)) continue;
                const auto straight=qUnpremultiply(pixel);
                double edgeLight=0,glint=0;
                if (bevel) edgeLight=(alpha(x+edge,y+edge)-alpha(x-edge,y-edge))/510.;
                if (sweep) {
                    const double projected=(x+.5-center.x())*direction.x()+(y+.5-center.y())*direction.y();
                    const double t=qBound(0.,1.-qAbs(projected-peak)/band,1.);
                    glint=strength*t*t*(3-2*t);
                }
                auto channel=[&](int c,int light) {
                    const double shaded=edgeLight>0?c+(255-c)*edgeLight:c*(1.+edgeLight);
                    return qRound(shaded+(light-shaded)*glint);
                };
                row[x]=qPremultiply(qRgba(channel(qRed(straight),glow.red()),channel(qGreen(straight),glow.green()),
                                          channel(qBlue(straight),glow.blue()),qAlpha(pixel)));
            }
        }
    }
    if (depth) {
        QImage side=surface.copy(bounds);
        QPainter tint(&side);tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(side.rect(),QColor(o.style.value("finish.sideColor").toString("#293444")));tint.end();
        QPainter p(&surface);p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        for (int step=1;step<=depth;++step) p.drawImage(bounds.topLeft()+QPoint(step,step),side);
    }
    if (reflection) {
        QRect ink=bounds.adjusted(0,0,depth,depth);
        int top=ink.bottom()+1,bottom=ink.top()-1;
        for(int y=ink.top();y<=ink.bottom();++y) {
            const auto *row=reinterpret_cast<const QRgb*>(surface.constScanLine(y));
            for(int x=ink.left();x<=ink.right();++x)if(qAlpha(row[x])){top=qMin(top,y);bottom=y;break;}
        }
        if (bottom<top) return;
        const int start=bottom+1+amount(o,"gap",100);
        const double opacity=qBound(0.,o.propertyBase("finish.reflectOpacity"),1.);
        for(int j=0;j<reflection && start+j<surface.height();++j) {
            const int sample=bottom-qMin(bottom-top,int(double(j)*(bottom-top+1)/reflection));
            const auto *src=reinterpret_cast<const QRgb*>(surface.constScanLine(sample));
            auto *dst=reinterpret_cast<QRgb*>(surface.scanLine(start+j));
            const double fade=opacity*(1.-double(j)/reflection);
            for(int x=ink.left();x<=ink.right();++x) {
                const auto c=src[x];
                dst[x]=qRgba(qRound(qRed(c)*fade),qRound(qGreen(c)*fade),qRound(qBlue(c)*fade),qRound(qAlpha(c)*fade));
            }
        }
    }
}
