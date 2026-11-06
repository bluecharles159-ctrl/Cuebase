#pragma once
#include <QColor>
#include <QString>
#include <QStringList>
#include <cmath>
namespace OverlapLayers {
inline int layer(int z) {return int(std::floor((double(z)+1)/3));}
inline QString label(int z) {
    const int group=layer(z);
    const QString position=QStringList{"Back","Middle","Front"}.at(z-3*group+1);
    return QString("Layer %1 · %2").arg(group).arg(position);
}
inline QColor color(int group) {return QColor::fromHsv(((205+(group%360)*67)%360+360)%360,130,235);}
}
