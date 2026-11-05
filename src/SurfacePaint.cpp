#include "SurfacePaint.h"
#include <QJsonObject>
#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>
#include <cmath>

namespace {
void renderStroke(QPainter &p, QSize size, const QJsonObject &stroke, bool mask) {
    const auto points = stroke.value("points").toArray();
    if (points.isEmpty()) return;
    const double radius = qMax(.5, qBound(.002,stroke.value("size").toDouble(.08),1.) *
                                     qMin(size.width(),size.height()) * .5);
    const double jitter = qBound(0.,stroke.value("jitter").toDouble(),1.) * radius;
    const double spacing = qMax(1., radius * .35);
    QRandomGenerator random(quint32(stroke.value("seed").toInt()));
    QColor ink(mask ? QStringLiteral("#ffffff") : stroke.value("color").toString("#ffffff"));
    if (!ink.isValid()) return;
    ink.setAlphaF(ink.alphaF() * qBound(0.,stroke.value("flow").toDouble(1.),1.));
    p.setCompositionMode(stroke.value("erase").toBool() ? QPainter::CompositionMode_DestinationOut :
                                                          QPainter::CompositionMode_SourceOver);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    auto dab = [&](QPointF point) {
        point += QPointF((random.generateDouble()*2-1)*jitter,(random.generateDouble()*2-1)*jitter);
        p.drawEllipse(point,radius,radius);
    };
    auto position = [&](QJsonValue value) {
        const auto xy=value.toArray();
        return QPointF(qBound(0.,xy.at(0).toDouble(),1.)*size.width(),
                       qBound(0.,xy.at(1).toDouble(),1.)*size.height());
    };
    QPointF previous = position(points.first());
    dab(previous);
    double next = spacing;
    for (qsizetype i=1;i<points.size();++i) {
        QLineF line(previous,position(points.at(i)));
        const double length=line.length();
        while (next<=length) {
            dab(line.pointAt(next/length));
            next+=spacing;
        }
        next-=length;
        previous=line.p2();
    }
}
}

void SurfacePaint::composite(QImage &surface,const QRect &bounds,const QJsonArray &layers) {
    if (layers.isEmpty() || bounds.isEmpty()) return;
    QPainter surfacePainter(&surface);
    for (const auto value:layers) {
        const auto layer=value.toObject();
        const auto strokes=layer.value("strokes").toArray();
        const double opacity=qBound(0.,layer.value("opacity").toDouble(1.),1.);
        if (!layer.value("visible").toBool(true) || strokes.isEmpty() || opacity==0) continue;
        const bool mask=layer.value("target").toString()=="mask";
        QImage coverage(bounds.size(),QImage::Format_ARGB32_Premultiplied);
        coverage.fill(Qt::transparent);
        QPainter brush(&coverage);
        brush.setRenderHint(QPainter::Antialiasing);
        for (auto stroke:strokes) renderStroke(brush,bounds.size(),stroke.toObject(),mask);
        brush.end();
        surfacePainter.setOpacity(opacity);
        surfacePainter.setCompositionMode(mask ? QPainter::CompositionMode_DestinationOut :
                                                  QPainter::CompositionMode_SourceAtop);
        surfacePainter.drawImage(bounds.topLeft(),coverage);
    }
}
