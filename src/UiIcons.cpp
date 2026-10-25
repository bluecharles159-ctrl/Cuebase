#include "UiControls.h"
#include <QIconEngine>
#include <QSvgRenderer>
#include <QFile>
#include <QHash>
#include <QPalette>
#include <QPainterPath>

static void initializeIcons(){Q_INIT_RESOURCE(icons);}
namespace {
QString symbol(QString n){
    n=n.toLower();
    static const QHash<QString,QString> names{
        {"inspector","sliders-horizontal"},{"align","align-horizontal-justify-start"},{"view / align","align-horizontal-justify-start"},
        {"graph","chart-spline"},{"animation","film"},{"cue","list-video"},{"modifiers","boxes"},{"modifier","boxes"},
        {"world","globe"},{"material","image"},{"component","layers"},{"effects","sparkles"},{"effect","sparkles"},{"apply","square-check"},{"picker","pipette"},
        {"down","chevron-down"},{"right","chevron-right"},{"back","arrow-left"},{"undo","undo-2"},{"redo","redo-2"},
        {"add","plus"},{"remove","trash-2"},{"delete","trash-2"},{"hidden","eye-off"},{"visible","eye"},
        {"lock","lock-keyhole"},{"unlock","lock-keyhole-open"},{"fit","maximize"},{"100%","search"},
        {"audio","audio-lines"},{"video","film"},{"broadcast","radio"},{"studio","monitor-play"},{"mesh","box"},
        {"text","type"},{"rotate","rotate-3d"},{"scale","scaling"},{"next","skip-forward"},{"step","skip-forward"},
        {"start","skip-back"},{"loop","repeat"},{"key","diamond"},{"reset","rotate-ccw"},{"edit","pencil"},
        {"cursor","crosshair"},{"composition","panels-top-left"},{"rendered","sun"},{"solid","box"},{"wire","boxes"},
        {"vertex","circle"},{"edge","ruler"},{"face","square"},{"build","paintbrush"},{"crew","layers"},
        {"clock","repeat"},{"open","folder-open"},{"import","download"},{"export","upload"},{"cut","scissors"},
        {"help","circle-help"},{"about","info"},{"close","x"},{"volume","volume-2"}
    };
    if(names.contains(n))return names[n];
    if(QFile::exists(":/icons/"+n+".svg"))return n;
    for(const auto &key:QStringList{"modifier","inspector","align","graph","animation","effect","render","camera","image","mesh","text"})if(n.contains(key))return names.value(key,key=="render"?"sun":key);
    return "box";
}
class LibraryIcon final:public QIconEngine {
    QString name;bool active;
public:
    LibraryIcon(QString source,bool enabled):name(std::move(source)),active(enabled){}
    QIconEngine *clone()const override{return new LibraryIcon(name,active);}
    void paint(QPainter *p,const QRect &rect,QIcon::Mode mode,QIcon::State)override{
        static QHash<QString,QByteArray> sources;
        const QString file=symbol(name);if(!sources.contains(file)){QFile input(":/icons/"+file+".svg");if(input.open(QIODevice::ReadOnly))sources[file]=input.readAll();}
        auto svg=sources.value(file);const auto palette=qApp->palette();QColor color=palette.color(active&&mode!=QIcon::Disabled?QPalette::Active:QPalette::Disabled,QPalette::WindowText);
        if(active&&mode!=QIcon::Disabled){const bool light=palette.color(QPalette::Window).lightness()>128;
            if(name=="cue")color=QColor(light?"#287557":"#8bcdb1");
            else if(name=="component")color=QColor(light?"#7653ab":"#bca0f2");
            else if(name=="effects"||name=="effect")color=QColor(light?"#8056a5":"#c49bdc");
            else if(name=="world")color=QColor(light?"#24758c":"#80bdcf");
            else if(name=="graph")color=QColor(light?"#a06d27":"#ddba7f");
        }
        if(name=="graph"||name=="down"||name=="right"){
            p->save();p->setRenderHint(QPainter::Antialiasing);p->translate(rect.topLeft());p->scale(rect.width()/20.,rect.height()/20.);p->setPen(QPen(color,1.4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p->setBrush(Qt::NoBrush);
            if(name=="graph"){p->drawLine(3,2,3,17);p->drawLine(3,17,18,17);QPainterPath path;path.moveTo(4,15);path.cubicTo(8,15,9,4,17,4);p->drawPath(path);}
            else if(name=="down")p->drawPolyline(QPolygonF{{5,8},{10,13},{15,8}});else p->drawPolyline(QPolygonF{{8,5},{13,10},{8,15}});
            p->restore();return;
        }
        svg.replace("currentColor",color.name().toUtf8());QSvgRenderer renderer(svg);p->save();p->setRenderHint(QPainter::Antialiasing);renderer.render(p,QRectF(rect));p->restore();
    }
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state)override{QPixmap image(size);image.fill(Qt::transparent);QPainter painter(&image);paint(&painter,QRect(QPoint(),size),mode,state);return image;}
};
}
QIcon uiIcon(const QString &name,bool active){static const bool initialized=[] {initializeIcons();return true;}();Q_UNUSED(initialized);return QIcon(new LibraryIcon(name,active));}
