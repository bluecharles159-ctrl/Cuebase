#pragma once
#include "ElementDesign.h"
#include "ParticlePath.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace MeshEmitter {
inline int capacity(const Mesh &mesh){
    // Bound the geometry passed to the CPU renderer per emitter and frame.
    constexpr int budget=24000;
    return qMin(256,int(budget/qMax(qsizetype(1),qMax(mesh.v.size(),mesh.f.size()))));
}
inline double sample(int particle,int channel,int seed){
    quint32 hash=quint32(particle)*374761393u^quint32(channel)*668265263u^quint32(seed)*2246822519u;
    hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;hash*=0x846ca68bu;hash^=hash>>16;
    return double(hash&65535)/65535.;
}
inline Mesh instances(const Mesh &prototype,const DObject &settings,qint64 ms){
    Mesh result;
    const double elapsed=ms/1000.-settings.propertyBase("meshEmitter.start");
    QVector3D low,high;
    if(elapsed<0 || !prototype.bounds(low,high) || prototype.isEmpty())return result;
    const auto dimensions=high-low;
    const float longest=qMax(dimensions.x(),qMax(dimensions.y(),dimensions.z()));
    if(longest<=0)return result;
    const int limit=capacity(prototype);
    if(!limit)return result;
    const int count=qBound(1,qRound(settings.propertyBase("meshEmitter.count")),limit);
    const double lifetime=qBound(.05,settings.propertyBase("meshEmitter.life"),120.);
    const double speed=settings.propertyBase("meshEmitter.speed"),depth=settings.propertyBase("meshEmitter.depth");
    const double spread=settings.propertyBase("meshEmitter.spread"),gravity=settings.propertyBase("meshEmitter.gravity");
    const double size=settings.propertyBase("meshEmitter.size"),spin=settings.propertyBase("meshEmitter.spin");
    const int seed=qRound(settings.propertyBase("meshEmitter.seed"));
    const ParticlePath::Motion path(settings.style);
    const auto center=(low+high)*.5f;
    result.v.reserve(count*prototype.v.size());result.f.reserve(count*prototype.f.size());
    for(int i=0;i<count;++i){
        const double cycle=elapsed/lifetime-sample(i,1,seed);
        if(cycle<0)continue;
        const double fraction=cycle-std::floor(cycle),age=fraction*lifetime;
        const float scale=float(size/longest*std::min({1.,fraction*12.,(1.-fraction)*8.}));
        if(scale<=.00001f)continue;
        QMatrix4x4 transform;
        if(!path.isEmpty()){
            const auto uv=path.at(fraction);
            transform.translate((uv.x()-.5)*settings.w,0,(.5-uv.y())*settings.h);
        }
        transform.translate((sample(i,2,seed)-.5)*2.*spread*age,(sample(i,3,seed)-.5)*2.*depth*age,
                            speed*(.6+.8*sample(i,4,seed))*age-.5*gravity*age*age);
        transform.rotate(float(360.*sample(i,5,seed)+spin*age),1,.6f,.3f);
        transform.scale(scale);transform.translate(-center);
        const int offset=int(result.v.size());
        for(const auto &v:prototype.v)result.v.append(transform.map(v));
        for(auto face:prototype.f){
            for(auto &index:face)index+=offset;
            result.f.append(std::move(face));
        }
    }
    return result;
}
}
