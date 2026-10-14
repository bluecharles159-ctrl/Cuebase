#include "ElementDesign.h"
#include "MediaFrames.h"
#include "SurfaceEffects.h"
#include "RevealEffects.h"
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QAbstractTextDocumentLayout>
#include <QJsonDocument>
#include <QtMath>

namespace {
void prepareText(QTextDocument &doc,const DObject &o,double width){
    QFont font(o.style["text.family"].toString("Segoe UI"));font.setPixelSize(qBound(1,o.fontPx,1000));
    font.setWeight(QFont::Weight(o.style["text.weight"].toInt(600)));font.setItalic(o.style["text.italic"].toBool());
    font.setUnderline(o.style["text.decoration"].toString()=="Underline");font.setStrikeOut(o.style["text.decoration"].toString()=="Line Through");
    font.setLetterSpacing(QFont::AbsoluteSpacing,o.propertyBase("text.tracking"));font.setWordSpacing(o.propertyBase("text.wordSpacing"));
    QString text=o.text,transform=o.style["text.transform"].toString();
    if(transform=="Uppercase")text=text.toUpper();else if(transform=="Lowercase")text=text.toLower();
    else if(transform=="Capitalize")for(int i=0;i<text.size();++i)if(i==0||text[i-1].isSpace())text[i]=text[i].toUpper();
    doc.setDocumentMargin(0);doc.setDefaultFont(font);
    if(o.style["text.format"].toString()=="Markdown")doc.setMarkdown(text,QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub)|QTextDocument::MarkdownNoHTML);else doc.setPlainText(text);
    QTextCursor cursor(&doc);cursor.select(QTextCursor::Document);QTextBlockFormat block;
    const auto align=o.style["text.align"].toString("Center");block.setAlignment(align=="Left"?Qt::AlignLeft:align=="Right"?Qt::AlignRight:align=="Justify"?Qt::AlignJustify:Qt::AlignHCenter);
    block.setLineHeight(o.propertyBase("text.lineHeight")*100,QTextBlockFormat::ProportionalHeight);cursor.mergeBlockFormat(block);doc.setTextWidth(width);
}
QImage blurImage(const QImage &source, int radius) {
    if (radius <= 0) return source;
    QImage a=source.convertToFormat(QImage::Format_ARGB32_Premultiplied), b(a.size(),a.format());
    radius=qBound(1,radius,80);
    for (int pass=0;pass<2;++pass) {
        const int outer=pass ? a.width():a.height(), inner=pass ? a.height():a.width();
        for(int row=0;row<outer;++row) {
            qint64 sums[4]={0,0,0,0};
            auto accumulate=[&](int pos,int sign) {
                if(pos<0||pos>=inner)return;
                QRgb c=pass ? a.pixel(row,pos):a.pixel(pos,row);
                sums[0]+=sign*qRed(c);sums[1]+=sign*qGreen(c);sums[2]+=sign*qBlue(c);sums[3]+=sign*qAlpha(c);
            };
            for(int k=0;k<=radius;++k)accumulate(k,1);
            for(int col=0;col<inner;++col) {
                int count=2*radius+1;
                QRgb c=qRgba(int(sums[0]/count),int(sums[1]/count),int(sums[2]/count),int(sums[3]/count));
                if(pass)b.setPixel(row,col,c);else b.setPixel(col,row,c);
                accumulate(col-radius,-1);accumulate(col+radius+1,1);
            }
        }
        a=b.copy();
    }
    return a;
}
QPainterPath roundedPath(QRectF r,const DObject &o) {
    QPainterPath path;
    if(o.style["shape"].toString()=="Ellipse"){path.addEllipse(r);return path;}
    double all=o.propertyBase("corner.radius"),lim=qMin(r.width(),r.height())/2;
    auto radius=[&](const char *key){return qBound(0.0,o.style["corner.linked"].toBool(true)?all:o.style[key].toDouble(all),lim);};
    double a=radius("corner.tl"),b=radius("corner.tr"),c=radius("corner.br"),d=radius("corner.bl");
    path.moveTo(r.left()+a,r.top());path.lineTo(r.right()-b,r.top());path.quadTo(r.topRight(),r.topRight()+QPointF(0,b));
    path.lineTo(r.right(),r.bottom()-c);path.quadTo(r.bottomRight(),r.bottomRight()-QPointF(c,0));
    path.lineTo(r.left()+d,r.bottom());path.quadTo(r.bottomLeft(),r.bottomLeft()-QPointF(0,d));
    path.lineTo(r.left(),r.top()+a);path.quadTo(r.topLeft(),r.topLeft()+QPointF(a,0));path.closeSubpath();return path;
}
}
void DObject::fitTextLayout(){
    if(type!="Text"||!style["text.autoFit"].toBool())return;
    const double inset=qBound(0.,propertyBase("text.padding"),512.);
    QTextDocument doc;prepareText(doc,*this,style["text.autoFitX"].toBool(true)?-1:qMax(1.,w*kPxPerUnit-2*inset));
    if(style["text.autoFitX"].toBool(true)){w=qBound(1.,qCeil(doc.idealWidth()+2*inset)*1.,4096.)/kPxPerUnit;doc.setTextWidth(qMax(1.,w*kPxPerUnit-2*inset));}
    if(style["text.autoFitY"].toBool(true))h=qBound(1.,qCeil(doc.size().height()+2*inset)*1.,4096.)/kPxPerUnit;
}
QImage ElementDesign::textureFor(const DObject &source) const {
    DObject o=source;o.fitTextLayout();
    const QString signature=QString::fromUtf8(QJsonDocument(o.style).toJson(QJsonDocument::Compact))+
        QString("|%1|%2|%3|%4|%5|%6|%7").arg(o.type,o.text).arg(o.fontPx).arg(o.mat.color.name(QColor::HexArgb)).arg(o.w).arg(o.h).arg(o.image);
    auto cache=m_texCache.constFind(o.id);
    if(cache!=m_texCache.constEnd() && cache->first==signature)return cache->second;
    const int w=qBound(1,qRound(o.w*kPxPerUnit),4096),h=qBound(1,qRound(o.h*kPxPerUnit),4096);
    const bool shadowEnabled=o.style["shadow.enabled"].toBool();
    const bool textShadow=o.type=="Text"&&o.style["text.background"].toBool()&&o.style["textShadow.enabled"].toBool();
    const double textPad=textShadow?qBound(0.,o.propertyBase("textShadow.blur"),80.)*2+qMax(qAbs(o.propertyBase("textShadow.x")),qAbs(o.propertyBase("textShadow.y"))):0;
    const int padding=qBound(0,qCeil(textPad+qBound(0.0,o.propertyBase("effects.blur"),80.0)*2+(shadowEnabled?qBound(0.0,o.propertyBase("shadow.blur"),80.0)*2+qMax(qAbs(o.propertyBase("shadow.x")),qAbs(o.propertyBase("shadow.y"))):0)),512);
    QImage image(w+padding*2,h+padding*2,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.translate(padding,padding);
    const double sw=o.propertyBase("stroke.width");
    const QRectF bounds=QRectF(0,0,w,h).adjusted(sw/2,sw/2,-sw/2,-sw/2);
    const auto outline=surfaceOutline(bounds,o);
    const QString background=o.style["background.type"].toString(o.type=="Image"?"Media":o.style["fill.kind"].toString("Solid")=="Solid"?"Solid":"Gradient");
    QString fill=background=="Solid"?"Solid":o.style["fill.kind"].toString("Linear");
    if(background=="Gradient"&&fill=="Solid")fill="Linear";
    QBrush brush(o.mat.color);
    if(fill!="Solid") {
        QGradientStops stops;
        stops.append({0,o.mat.color});
        stops.append({qBound(.001,o.style["fill.stop"].toDouble(.5),.999),QColor(o.style["fill.middle"].toString("#6366f1"))});
        stops.append({1,QColor(o.style["fill.end"].toString("#a855f7"))});
        const double angle=qDegreesToRadians(o.propertyBase("fill.angle"));
        if(fill=="Linear") {QPointF d(qCos(angle)*w/2,qSin(angle)*h/2);QLinearGradient g(bounds.center()-d,bounds.center()+d);g.setStops(stops);brush=g;}
        if(fill=="Radial") {QRadialGradient g(bounds.center(),qMax(w,h)/2.0);g.setStops(stops);brush=g;}
        if(fill=="Conic") {QConicalGradient g(bounds.center(),o.propertyBase("fill.angle"));g.setStops(stops);brush=g;}
    }
    if((o.type=="Shape"||o.type=="Image")&&background!="Media")p.fillPath(outline,brush);
    if(background=="Media") {
        qint64 time=o.style["media.time"].toDouble()*o.style["media.speed"].toDouble(1)+o.style["media.offset"].toDouble();
        const qint64 duration=o.style["media.duration"].toDouble();if(duration>0)time=o.style["media.loop"].toBool(true)?time%duration:qMin(time,duration-1);
        const QImage source=o.style["media.kind"].toString()=="Video"?MediaFrames::frame(o.image,time,o.id):QImage(o.image);
        if(!source.isNull()) {
            p.save();p.setClipPath(outline);
            QString fit=o.style["image.fit"].toString("Contain");
            QSizeF size=source.size();
            if(fit=="Fill")size=QSizeF(w,h);
            else if(fit=="Contain" || fit=="Cover" || (fit=="Scale Down" && (size.width()>w || size.height()>h)))
                size.scale(QSizeF(w,h),fit=="Cover"?Qt::KeepAspectRatioByExpanding:Qt::KeepAspectRatio);
            const double x=o.style["image.x"].toDouble(.5),y=o.style["image.y"].toDouble(.5);
            p.drawImage(QRectF((w-size.width())*x,(h-size.height())*y,size.width(),size.height()),source);p.restore();
        }
    }
    if(o.type=="Text") {
        if(o.style["text.background"].toBool())p.fillPath(outline,QColor(o.style["text.backgroundColor"].toString("#cc202020")));
        const double inset=qBound(0.,o.propertyBase("text.padding"),512.),contentWidth=qMax(1.,w-2*inset),contentHeight=qMax(1.,h-2*inset);
        QTextDocument doc;prepareText(doc,o,contentWidth);QFont font=doc.defaultFont();
        QTextCursor cursor(&doc);cursor.select(QTextCursor::Document);
        QTextCharFormat chars;chars.setForeground(brush);cursor.mergeCharFormat(chars);
        if(!o.style["text.autoFit"].toBool()&&o.style["studio.editable"].toBool()&&o.style["studio.fit"].toBool(true)){
            int px=font.pixelSize();while(doc.size().height()>contentHeight&&px>6){font.setPixelSize(--px);doc.setDefaultFont(font);doc.setTextWidth(contentWidth);}
        }
        QString vertical=o.style["text.vertical"].toString("Center");double y=inset+(vertical=="Top"?0:vertical=="Bottom"?contentHeight-doc.size().height():(contentHeight-doc.size().height())/2);
        QImage glyphs(image.size(),QImage::Format_ARGB32_Premultiplied);glyphs.fill(Qt::transparent);
        QPainter gp(&glyphs);gp.translate(padding+inset,padding+y);doc.drawContents(&gp);gp.end();
        if(textShadow){
            QImage shadow(glyphs.size(),glyphs.format());shadow.fill(Qt::transparent);QPainter sp(&shadow);
            sp.drawImage(QPointF(o.propertyBase("textShadow.x"),o.propertyBase("textShadow.y")),glyphs);sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            sp.fillRect(shadow.rect(),QColor(o.style["textShadow.color"].toString("#99000000")));sp.end();
            p.drawImage(-padding,-padding,blurImage(shadow,qRound(o.propertyBase("textShadow.blur"))));
        }
        p.drawImage(-padding,-padding,glyphs);
    }
    if(sw>0 && (o.type!="Text"||o.style["text.background"].toBool())) {
        QPen pen(QColor(o.style["stroke.color"].toString("#ffffff")),sw);
        const QString style=o.style["stroke.style"].toString();
        pen.setStyle(style=="Dashed"?Qt::DashLine:style=="Dotted"?Qt::DotLine:Qt::SolidLine);
        const auto join=o.style["stroke.join"].toString("Round");
        pen.setJoinStyle(join=="Miter"?Qt::MiterJoin:join=="Bevel"?Qt::BevelJoin:Qt::RoundJoin);p.setPen(pen);p.setBrush(Qt::NoBrush);p.drawPath(outline);
    }
    p.end();
    // Pixel effects apply to the same texture used by the editor and broadcast renderer.
    QImage pixels=image.convertToFormat(QImage::Format_ARGB32);
    const double saturation=o.propertyBase("effects.saturation")*(1-o.propertyBase("effects.grayscale"));
    const double bright=o.propertyBase("effects.brightness"),contrast=o.propertyBase("effects.contrast"),invert=o.propertyBase("effects.invert"),sepia=o.propertyBase("effects.sepia"),hue=o.propertyBase("effects.hue");
    if(saturation!=1||bright!=1||contrast!=1||invert!=0||sepia!=0||hue!=0)for(int y=0;y<pixels.height();++y){auto *line=reinterpret_cast<QRgb*>(pixels.scanLine(y));for(int x=0;x<pixels.width();++x){
        QColor c=QColor::fromRgba(line[x]);double r=c.redF(),g=c.greenF(),b=c.blueF();
        double lum=.2126*r+.7152*g+.0722*b;
        r=lum+(r-lum)*saturation;g=lum+(g-lum)*saturation;b=lum+(b-lum)*saturation;
        double sr=.393*r+.769*g+.189*b,sg=.349*r+.686*g+.168*b,sb=.272*r+.534*g+.131*b;
        r=r*(1-sepia)+sr*sepia;g=g*(1-sepia)+sg*sepia;b=b*(1-sepia)+sb*sepia;
        auto adjust=[&](double v){v=((v-.5)*contrast+.5)*bright;return qBound(0.0,v*(1-invert)+(1-v)*invert,1.0);};
        c.setRgbF(adjust(r),adjust(g),adjust(b),c.alphaF());
        if(hue!=0){double hv=c.hsvHueF();c.setHsvF(std::fmod(qMax(0.0,hv)+hue/360+2,1.0),c.hsvSaturationF(),c.valueF(),c.alphaF());}
        line[x]=c.rgba();
    }}
    // A stable object-space threshold makes a keyed reveal reversible and deterministic.
    const double reveal=RevealEffects::progress(o);
    const bool revealHandled=RevealEffects::apply(pixels,o,padding,w,h,reveal);
    if(!revealHandled&&reveal<1){
        const double rough=qBound(0.,o.propertyBase("effects.revealRoughness"),1.),soft=qMax(.00001,o.propertyBase("effects.revealSoftness"));
        const double angle=qDegreesToRadians(o.propertyBase("effects.revealAngle")),dx=qCos(angle),dy=qSin(angle),span=qMax(.00001,qAbs(dx)+qAbs(dy));
        const bool radial=o.style["effects.revealMode"].toString()=="Radial";
        const double cx=qBound(0.,o.propertyBase("effects.revealX"),1.),cy=qBound(0.,o.propertyBase("effects.revealY"),1.);
        const double radius=qSqrt(qPow(qMax(cx,1-cx)*w,2)+qPow(qMax(cy,1-cy)*h,2));
        const double threshold=reveal*(1+rough+2*soft)-rough/2-soft;
        for(int y=0;y<pixels.height();++y){auto *row=reinterpret_cast<QRgb*>(pixels.scanLine(y));for(int x=0;x<pixels.width();++x){
            quint32 hash=quint32(x)*374761393u^quint32(y)*668265263u;
            hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;hash*=0x846ca68bu;hash^=hash>>16;
            const double noise=double(hash&65535)/65535.-.5;
            const double u=(x-padding+.5)/w,v=(y-padding+.5)/h;
            const double distance=(radial?qSqrt(qPow((u-cx)*w,2)+qPow((v-cy)*h,2))/radius:((u-.5)*dx+(v-.5)*dy)/span+.5)+noise*rough;
            const double t=reveal<=0?0:qBound(0.,(threshold-distance)/soft+.5,1.);
            const int alpha=qRound(qAlpha(row[x])*t*t*(3-2*t));row[x]=qRgba(qRed(row[x]),qGreen(row[x]),qBlue(row[x]),alpha);
        }}
    }
    image=blurImage(pixels,qRound(o.propertyBase("effects.blur")));
    if(o.style["shadow.enabled"].toBool()) {
        QImage shadow(image.size(),QImage::Format_ARGB32_Premultiplied);shadow.fill(Qt::transparent);
        QPainter sp(&shadow);sp.drawImage(QPointF(o.propertyBase("shadow.x"),o.propertyBase("shadow.y")),image);
        sp.setCompositionMode(QPainter::CompositionMode_SourceIn);sp.fillRect(shadow.rect(),QColor(o.style["shadow.color"].toString("#99000000")));sp.end();
        shadow=blurImage(shadow,qRound(o.propertyBase("shadow.blur")));
        QPainter composite(&shadow);composite.drawImage(0,0,image);composite.end();image=shadow;
    }
    m_texCache[o.id]=qMakePair(signature,image);return image;
}
