#include "SurfaceMasks.h"
#include "SurfaceEffects.h"
#include "SurfacePaint.h"
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>

QString SurfaceMasks::fileSignature(const DObject &o) {
    QString signature;
    for (auto entry:o.style.value("mask.stack").toArray()) {
        QFileInfo file(o.style.value("mask."+entry.toString()+".file").toString());
        if (!file.filePath().isEmpty())
            signature+=QString("|%1,%2").arg(file.size()).arg(file.lastModified().toMSecsSinceEpoch());
    }
    return signature;
}

namespace {
QImage raster(const DObject &o,const QString &prefix,QSize size) {
    const auto field=[&](QString name){return o.style.value(prefix+name);};
    const auto kind=field("type").toString("Ellipse");
    QImage coverage(size,QImage::Format_ARGB32_Premultiplied);
    coverage.fill(Qt::transparent);
    QPainter p(&coverage);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.translate(size.width()*o.propertyBase(prefix+"x"),size.height()*o.propertyBase(prefix+"y"));
    p.rotate(o.propertyBase(prefix+"angle"));
    p.scale(qMax(.001,o.propertyBase(prefix+"width")),qMax(.001,o.propertyBase(prefix+"height")));
    const QRectF rect(-size.width()*.5,-size.height()*.5,size.width(),size.height());
    p.setPen(Qt::NoPen);p.setBrush(Qt::white);
    if (kind=="Ellipse") p.drawEllipse(rect);
    else if (kind=="Rectangle") p.drawRect(rect);
    else if (kind=="Gradient") {
        QLinearGradient gradient(rect.topLeft(),rect.topRight());
        gradient.setColorAt(0,Qt::transparent);gradient.setColorAt(1,Qt::white);
        p.fillRect(rect,gradient);
    } else if (kind=="Text") {
        QFont font= p.font();font.setBold(true);font.setPixelSize(100);
        QPainterPath glyphs;glyphs.addText(0,0,font,field("text").toString("TITLE"));
        const auto box=glyphs.boundingRect();
        if (!box.isEmpty()) {
            const double fit=qMin(rect.width()/box.width(),rect.height()/box.height());
            p.scale(fit,fit);p.translate(-box.center());p.drawPath(glyphs);
        }
    } else if (kind=="Brush") {
        QImage brush(size,QImage::Format_ARGB32_Premultiplied);
        brush.fill(Qt::black);
        SurfacePaint::composite(brush,brush.rect(),field("brush").toArray());
        p.drawImage(rect,brush);
    } else {
        QImageReader reader(field("file").toString());
        if (!reader.size().isValid()) return {};
        reader.setScaledSize(size);
        const auto image=reader.read();
        if (image.isNull()) return {};
        p.drawImage(rect,image);
    }
    p.end();
    if (kind=="Luminance" || kind=="Brush") {
        for(int y=0;y<size.height();++y) {
            auto *row=reinterpret_cast<QRgb*>(coverage.scanLine(y));
            for(int x=0;x<size.width();++x) {
                // Premultiplied channels already include source alpha.
                const int alpha=qGray(row[x]);row[x]=qRgba(alpha,alpha,alpha,alpha);
            }
        }
    }
    const int blur=qBound(0,qRound(o.propertyBase(prefix+"feather")),100);
    return blur?backdropImage(coverage,blur,1.):coverage;
}
}

void SurfaceMasks::apply(QImage &surface,const QRect &bounds,const DObject &o) {
    const auto stack=o.style.value("mask.stack").toArray();
    if (stack.isEmpty()) return;
    QImage combined;
    for(auto item:stack) {
        const auto prefix="mask."+item.toString()+".";
        if (o.style.value(prefix+"enabled").toString()=="Off") continue;
        const auto source=raster(o,prefix,bounds.size());
        if (source.isNull()) continue;
        const auto op=o.style.value(prefix+"combine").toString("Intersect");
        const bool inverse=o.style.value(prefix+"invert").toString()=="On";
        const double strength=qBound(0.,o.propertyBase(prefix+"opacity"),1.);
        if (strength==0.) continue;
        const bool first=combined.isNull();
        if (first) combined=QImage(bounds.size(),QImage::Format_ARGB32_Premultiplied);
        for(int y=0;y<bounds.height();++y) {
            const auto *src=reinterpret_cast<const QRgb*>(source.constScanLine(y));
            auto *dst=reinterpret_cast<QRgb*>(combined.scanLine(y));
            for(int x=0;x<bounds.width();++x) {
                const int b=inverse?255-qAlpha(src[x]):qAlpha(src[x]);
                const int a=first?255:qAlpha(dst[x]);
                int result=b;
                if(op=="Subtract") result=a*(255-b)/255;
                else if(!first && op=="Intersect") result=a*b/255;
                else if(!first && op=="Add") result=a+b-a*b/255;
                const int alpha=qRound(a+(result-a)*strength);
                dst[x]=qRgba(alpha,alpha,alpha,alpha);
            }
        }
    }
    if (combined.isNull()) return;
    QPainter p(&surface);p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.drawImage(bounds.topLeft(),combined);
}
