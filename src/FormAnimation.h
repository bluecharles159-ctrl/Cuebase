#pragma once
#include "ElementDesign.h"
#include <QUuid>

namespace FormAnimation {
// Capture after topology creation, before displacement. This gives extrusion
// a collapsed zero pose with the same vertex indices as its completed pose.
inline QString capture(DObject &object,const Mesh &zero,const QString &name){
    if(zero.v.size()!=object.mesh.v.size()||zero.f!=object.mesh.f)return {};
    QJsonArray deltas;for(int i=0;i<zero.v.size();++i){const auto delta=object.mesh.v[i]-zero.v[i];if(delta.lengthSquared()>1e-12f)deltas.append(QJsonArray{i,delta.x(),delta.y(),delta.z()});}
    if(deltas.isEmpty())return {};
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);const auto property="form."+id;
    auto forms=object.style["form.poses"].toArray();forms.append(QJsonObject{{"id",id},{"name",name},{"vertices",object.mesh.v.size()},{"faces",object.mesh.toJson()["f"]},{"deltas",deltas}});
    object.style["form.poses"]=forms;object.style[property]=1.;return property;
}
inline Mesh evaluate(const DObject &object,qint64 time){
    Mesh mesh=object.mesh;
    for(const auto &entry:object.style["form.poses"].toArray()){
        const auto pose=entry.toObject();if(pose["vertices"].toInt()!=mesh.v.size())continue;
        if(pose["faces"]!=mesh.toJson()["f"])continue;
        const float amount=float(1-qBound(0.,object.propertyValue("form."+pose["id"].toString(),time),1.));
        for(const auto &value:pose["deltas"].toArray()){const auto delta=value.toArray();if(delta.size()!=4)continue;const int index=delta[0].toInt(-1);if(index<0||index>=mesh.v.size())continue;mesh.v[index]-=QVector3D(delta[1].toDouble(),delta[2].toDouble(),delta[3].toDouble())*amount;}
    }return mesh;
}
}
