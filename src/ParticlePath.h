#pragma once
#include <QJsonArray>
#include <QPainterPath>
#include <QVector>
#include <cmath>

namespace ParticlePath {
struct Knot { QPointF point,incoming,outgoing; };

inline QVector<Knot> read(const QJsonArray &array){
    QVector<Knot> points;
    if(array.size()>128)return points;
    for(const auto &value:array){
        const auto coords=value.toArray();
        if(coords.size()!=6)return {};
        for(const auto &v:coords)
            if(!v.isDouble() || !std::isfinite(v.toDouble()) || std::abs(v.toDouble())>16)return {};
        points.append(Knot{{coords[0].toDouble(),coords[1].toDouble()},
                           {coords[2].toDouble(),coords[3].toDouble()},
                           {coords[4].toDouble(),coords[5].toDouble()}});
    }
    return points;
}
inline QJsonArray write(const QVector<Knot> &points){
    QJsonArray array;
    for(const auto &k:points)array.append(QJsonArray{k.point.x(),k.point.y(),k.incoming.x(),k.incoming.y(),k.outgoing.x(),k.outgoing.y()});
    return array;
}
inline QVector<Knot> smooth(const QVector<QPointF> &stroke){
    QVector<Knot> result;
    for(qsizetype i=0;i<stroke.size();++i){
        const auto before=i>0?stroke[i-1]:stroke[i];
        const auto after=i+1<stroke.size()?stroke[i+1]:stroke[i];
        const auto tangent=(after-before)/6.;
        result.append(Knot{stroke[i],stroke[i]-tangent,stroke[i]+tangent});
    }
    return result;
}
inline QPainterPath curve(const QVector<Knot> &knots){
    QPainterPath path;
    if(knots.isEmpty())return path;
    path.moveTo(knots.first().point);
    for(qsizetype i=1;i<knots.size();++i)
        path.cubicTo(knots[i-1].outgoing,knots[i].incoming,knots[i].point);
    return path;
}
}
