#pragma once
#include "ElementDesign.h"
#include <QUuid>
#include <functional>

namespace FormAnimation {
inline QJsonArray deltasBetween(const Mesh &full,const Mesh &zero){
    QJsonArray deltas;
    for(int i=0;i<full.v.size();++i){const auto delta=full.v[i]-zero.v[i];if(delta.lengthSquared()>1e-12f)deltas.append(QJsonArray{i,delta.x(),delta.y(),delta.z()});}
    return deltas;
}
// Capture after topology creation, before displacement. This gives extrusion
// a collapsed zero pose with the same vertex indices as its completed pose.
inline QString capture(DObject &object,const Mesh &zero,const QString &name){
    if(zero.v.size()!=object.mesh.v.size()||zero.f!=object.mesh.f)return {};
    const QJsonArray deltas=deltasBetween(object.mesh,zero);
    if(deltas.isEmpty())return {};
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);const auto property="form."+id;
    auto forms=object.style["form.poses"].toArray();forms.append(QJsonObject{{"id",id},{"name",name},{"vertices",object.mesh.v.size()},{"faces",object.mesh.toJson()["f"]},{"deltas",deltas}});
    object.style["form.poses"]=forms;object.style[property]=1.;return property;
}
inline Mesh evaluate(const DObject &object,qint64 time){
    Mesh mesh=object.mesh;
    const auto faces=mesh.toJson()["f"];
    for(const auto &entry:object.style["form.poses"].toArray()){
        const auto pose=entry.toObject();if(pose["vertices"].toInt()!=mesh.v.size())continue;
        if(pose["faces"]!=faces)continue;
        const float amount=float(1-qBound(0.,object.propertyValue("form."+pose["id"].toString(),time),1.));
        for(const auto &value:pose["deltas"].toArray()){const auto delta=value.toArray();if(delta.size()!=4)continue;const int index=delta[0].toInt(-1);if(index<0||index>=mesh.v.size())continue;mesh.v[index]-=QVector3D(delta[1].toDouble(),delta[2].toDouble(),delta[3].toDouble())*amount;}
    }return mesh;
}
// Replay a topology operation on every zero pose so an extrusion or subdivision
// cannot silently detach earlier animation from its vertex indices.
inline bool retarget(DObject &after,const DObject &before,const std::function<bool(Mesh &)> &operation){
    QJsonArray updated;
    const auto oldFaces=before.mesh.toJson()["f"],newFaces=after.mesh.toJson()["f"];
    for(const auto &entry:before.style["form.poses"].toArray()){
        auto pose=entry.toObject();
        if(pose["vertices"].toInt()!=before.mesh.v.size()||pose["faces"]!=oldFaces)return false;
        Mesh zero=before.mesh;
        for(const auto &value:pose["deltas"].toArray()){
            const auto delta=value.toArray();if(delta.size()!=4)return false;
            const int index=delta[0].toInt(-1);if(index<0||index>=zero.v.size())return false;
            zero.v[index]-=QVector3D(delta[1].toDouble(),delta[2].toDouble(),delta[3].toDouble());
        }
        if(!operation(zero)||zero.v.size()!=after.mesh.v.size()||zero.toJson()["f"]!=newFaces)return false;
        pose["vertices"]=after.mesh.v.size();pose["faces"]=newFaces;pose["deltas"]=deltasBetween(after.mesh,zero);updated.append(pose);
    }
    auto style=after.style;
    if(!before.style["form.poses"].toArray().isEmpty())style["form.poses"]=updated;
    if(before.hasSurface()&&before.style.contains("surface.restMesh")){
        Mesh rest=Mesh::fromJson(before.style["surface.restMesh"].toObject());
        if(operation(rest)&&rest.v.size()==after.mesh.v.size()&&rest.toJson()["f"]==newFaces)style["surface.restMesh"]=rest.toJson();
    }
    after.style=style;return true;
}
}
