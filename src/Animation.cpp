#include "ElementDesign.h"
#include <algorithm>
#include <cmath>

QStringList transformChannels() {
    return {"position.x", "position.y", "position.z", "rotation.x", "rotation.y", "rotation.z",
            "scale.x", "scale.y", "scale.z", "opacity"};
}
double channelValue(const DProps &p, const QString &path) {
    if (path == "opacity") return p.opacity;
    const int axis = path.endsWith('x') ? 0 : path.endsWith('y') ? 1 : 2;
    if (path.startsWith("position.")) return p.loc[axis];
    if (path.startsWith("rotation.")) return p.rot[axis];
    if (path.startsWith("scale.")) return p.scl[axis];
    return 0;
}
void setChannelValue(DProps &p, const QString &path, double value) {
    if (path == "opacity") { p.opacity = value; return; }
    const int axis = path.endsWith('x') ? 0 : path.endsWith('y') ? 1 : 2;
    if (path.startsWith("position.")) p.loc[axis] = float(value);
    else if (path.startsWith("rotation.")) p.rot[axis] = float(value);
    else if (path.startsWith("scale.")) p.scl[axis] = float(value);
}
int PropertyTrack::indexAt(qint64 ms) const {
    for (int i = 0; i < keys.size(); ++i) if (keys[i].ms == ms) return i;
    return -1;
}
void PropertyTrack::setKey(int ms, double value, const QString &interpolation) {
    int i = indexAt(ms);
    if (i >= 0) {
        keys[i].value = value;
        if (!interpolation.isEmpty()) keys[i].interpolation = interpolation;
    } else {
        keys.append({ms, value, interpolation.isEmpty() ? "Bezier" : interpolation});
        std::sort(keys.begin(), keys.end(), [](const auto &a, const auto &b) { return a.ms < b.ms; });
    }
}
double PropertyTrack::evaluate(qint64 ms, double fallback) const {
    const double base=evaluateBase(ms,fallback);
    if(noise.isEmpty()||!noise["enabled"].toBool(true))return base;
    auto number=[&](const char *key,double def,double lo,double hi){const double v=noise[key].toDouble(def);return std::isfinite(v)?qBound(lo,v,hi):def;};
    double influence=number("influence",1,0,1);
    const double seconds=double(ms)/1000.;
    if(noise["restricted"].toBool()){
        const double start=number("start",0,0,1e9),end=number("end",1,0,1e9);
        if(seconds<start||seconds>end||end<start)return base;
        const double fadeIn=number("fadeIn",0,0,1e9),fadeOut=number("fadeOut",0,0,1e9);
        if(fadeIn>0)influence*=qBound(0.,(seconds-start)/fadeIn,1.);
        if(fadeOut>0)influence*=qBound(0.,(end-seconds)/fadeOut,1.);
    }
    if(influence==0)return base;
    const double scale=number("scale",1,.001,1e9),offset=number("offset",0,-1e9,1e9);
    const auto seed=quint32(number("seed",1,0,2147483647));
    const int depth=int(number("depth",0,0,8));
    const double roughness=number("roughness",.5,0,1),lacunarity=number("lacunarity",2,1,4);
    auto sample=[&](double x,int octave){
        // Bounded coordinates and unsigned integer mixing give stable random-access playback.
        x=std::fmod(x,1048576.);const auto cell=qint64(std::floor(x));const double t=x-std::floor(x),smooth=t*t*t*(t*(t*6-15)+10);
        auto hash=[&](qint64 i){quint32 h=quint32(i)^seed^(quint32(octave+1)*0x9e3779b9U);h^=h>>16;h*=0x7feb352dU;h^=h>>15;h*=0x846ca68bU;h^=h>>16;return double(h)/4294967295.;};
        return hash(cell)*(1-smooth)+hash(cell+1)*smooth;
    };
    double x=(seconds-offset)/scale,value=0,weight=1,total=0;
    for(int octave=0;octave<=depth;++octave){value+=sample(x,octave)*weight;total+=weight;weight*=roughness;x*=lacunarity;}
    value/=total;
    const double strength=number("strength",1,-1e6,1e6);
    const QString blend=noise["blend"].toString("Replace");
    double result=base+(value-.5)*strength;
    if(blend=="Add")result=base+value*strength;
    else if(blend=="Subtract")result=base-value*strength;
    else if(blend=="Multiply")result=base*value*strength;
    return base+(result-base)*influence;
}
double PropertyTrack::evaluateBase(qint64 ms, double fallback) const {
    if (keys.isEmpty()) return fallback;
    if (ms <= keys.first().ms) return keys.first().value;
    if (ms >= keys.last().ms) return keys.last().value;
    for (int i = 1; i < keys.size(); ++i) {
        if (ms > keys[i].ms) continue;
        if (ms == keys[i].ms) return keys[i].value;
        const auto &a = keys[i - 1], &b = keys[i];
        if (a.interpolation == "Bezier") {
            const QPointF p0(a.ms,a.value),p3(b.ms,b.value);
            QPointF p1=handle(i-1,true),p2=handle(i,false);
            p1.setX(qBound(p0.x(),p1.x(),p3.x()));p2.setX(qBound(p1.x(),p2.x(),p3.x()));
            auto cubic=[&](double t){double s=1-t;return p0*(s*s*s)+p1*(3*s*s*t)+p2*(3*s*t*t)+p3*(t*t*t);};
            double lo=0,hi=1;
            for(int n=0;n<40;++n){double t=(lo+hi)/2;if(cubic(t).x()<ms)lo=t;else hi=t;}
            return cubic((lo+hi)/2).y();
        }
        return a.value + (b.value - a.value) * interpolate(a.interpolation, double(ms - a.ms) / (b.ms - a.ms));
    }
    return keys.last().value;
}
QPointF PropertyTrack::handle(int index, bool right) const {
    const auto &k=keys[index];
    if(k.customHandles)return QPointF(k.ms,k.value)+(right?k.rightHandle:k.leftHandle);
    int a=qMax(0,index-1),b=qMin(int(keys.size())-1,index+1);
    double slope=b==a?0:(keys[b].value-keys[a].value)/qMax(1,keys[b].ms-keys[a].ms);
    if(k.handleType=="Vector"){const int neighbor=right?qMin(int(keys.size())-1,index+1):qMax(0,index-1);if(neighbor!=index)slope=(keys[neighbor].value-k.value)/double(keys[neighbor].ms-k.ms);}
    if(k.handleType=="Auto Clamped"&&a!=index&&b!=index&&(k.value-keys[a].value)*(keys[b].value-k.value)<=0)slope=0;
    double dt=right?(index+1<keys.size()?(keys[index+1].ms-k.ms)/3.0:100.0):(index>0?(keys[index-1].ms-k.ms)/3.0:-100.0);
    double value=k.value+slope*dt;if(k.handleType=="Auto Clamped"){const auto &neighbor=keys[right?b:a];value=qBound(qMin(k.value,neighbor.value),value,qMax(k.value,neighbor.value));}return QPointF(k.ms+dt,value);
}
const PropertyTrack *DObject::track(const QString &property) const {
    for (const auto &t : tracks) if (t.property == property) return &t;
    return nullptr;
}
PropertyTrack *DObject::track(const QString &property) {
    for (auto &t : tracks) if (t.property == property) return &t;
    return nullptr;
}
void DObject::keyChannel(const QString &property, int ms, double value) {
    auto *t = track(property);
    if (!t) { tracks.append({property, {}}); t = &tracks.last(); }
    t->setKey(ms, value);
    overrides.remove(property);
    refreshLegacyKeys();
}
void DObject::migrateKeys() {
    const auto old = keys;
    tracks.clear();
    for (const auto &k : old) {
        const auto channels = k.channels.isEmpty() ? transformChannels() : k.channels;
        for (const auto &path : channels) {
            auto *t = track(path);
            if (!t) { tracks.append({path, {}}); t = &tracks.last(); }
            t->setKey(k.ms, channelValue(k.p, path), k.interp);
        }
    }
    refreshLegacyKeys();
}
void DObject::refreshLegacyKeys() {
    // A read-only summary used by old navigation and mesh-origin operations.
    keys.clear();
    for (const auto &track : tracks) for (const auto &key : track.keys) {
        int i = keyIndexNear(key.ms, 0);
        if (i < 0) { DKey k; k.ms = key.ms; k.p = base; keys.append(k); i = int(keys.size()) - 1; }
        setChannelValue(keys[i].p, track.property, key.value);
        keys[i].channels.append(track.property);
        keys[i].interp = key.interpolation;
    }
    std::sort(keys.begin(), keys.end(), [](const auto &a, const auto &b) { return a.ms < b.ms; });
}

double DObject::propertyBase(const QString &path) const {
    if(path=="texture.scaleX"||path=="texture.scaleY"||path=="texture.opacity")return style[path].toDouble(1);
    if(path=="effects.revealBlock")return style[path].toDouble(32);
    if(path=="effects.revealSpread")return style[path].toDouble(1);
    if(path=="effects.revealSteps")return style[path].toDouble(8);
    if(path=="media.speed"||path=="media.volume"||path=="audio.volume"||path=="audio.speed")return style[path].toDouble(1);
    if (transformChannels().contains(path)) return channelValue(base, path);
    if (path == "layout.width") return w * kPxPerUnit;
    if (path == "layout.height") return h * kPxPerUnit;
    if (path == "text.size") return fontPx;
    if (path == "material.alpha") return mat.alpha;
    if (path == "material.roughness") return mat.roughness;
    if (path == "material.emission") return mat.emission;
    if (path == "light.power") return light.energy;
    if (path == "light.angle") return light.spotSize;
    if (path == "light.blend") return light.spotBlend;
    if (path == "camera.fov") return cam.fov;
    if (path == "camera.scale") return cam.orthoScale;
    if (path == "text.weight") return style[path].toDouble(600);
    if (path == "fill.stop" || path == "image.x" || path == "image.y" || path == "effects.revealX" || path == "effects.revealY") return style[path].toDouble(.5);
    if (path.startsWith("corner.") && path != "corner.radius") return style[path].toDouble(style["corner.radius"].toDouble());
    if (path == "effects.revealRoughness") return style[path].toDouble(.15);
    if (path == "effects.revealSoftness") return style[path].toDouble(.015);
    if (path == "effects.reveal" || path == "backdrop.brightness" || path == "effects.brightness" || path == "effects.contrast" || path == "effects.saturation" || path == "text.lineHeight") return style[path].toDouble(1);
    return style[path].toDouble();
}
double DObject::propertyValue(const QString &path, qint64 ms) const {
    if(QStringList{"corner.tl","corner.tr","corner.br","corner.bl"}.contains(path)&&style["corner.linked"].toBool(true))return propertyValue("corner.radius",ms);
    if (overrides.contains(path)) return overrides.value(path);
    const auto *tr = track(path);
    return tr ? tr->evaluate(ms, propertyBase(path)) : propertyBase(path);
}
void DObject::setPropertyBase(const QString &path, double value) {
    if (transformChannels().contains(path)) setChannelValue(base, path, value);
    else if (path == "layout.width") w = qMax(1.0, value) / kPxPerUnit;
    else if (path == "layout.height") h = qMax(1.0, value) / kPxPerUnit;
    else if (path == "text.size") fontPx = qMax(1, qRound(value));
    else if (path == "material.alpha") mat.alpha = value;
    else if (path == "material.roughness") mat.roughness = value;
    else if (path == "material.emission") mat.emission = value;
    else if (path == "light.power") light.energy = value;
    else if (path == "light.angle") light.spotSize = value;
    else if (path == "light.blend") light.spotBlend = value;
    else if (path == "camera.fov") cam.fov = value;
    else if (path == "camera.scale") cam.orthoScale = value;
    else style[path] = value;
}
DObject DObject::evaluated(qint64 ms) const {
    DObject result = *this;
    if(style["media.kind"].toString()=="Video")result.style["media.time"]=double(ms);
    for (const auto &tr : tracks) result.setPropertyBase(tr.property, propertyValue(tr.property, ms));
    for (auto it = overrides.begin(); it != overrides.end(); ++it) result.setPropertyBase(it.key(), it.value());
    result.fitTextLayout();
    return result;
}

double DObject::propertyDefault(const QString &path) const {
    if(defaults.contains(path))return defaults[path].toDouble();
    DObject standard;standard.type=type;
    if(type=="Text"){standard.base.rot={90,0,0};standard.fontPx=150;}
    if(type=="Image"||type=="Shape"){standard.base.rot={90,0,0};standard.w=4;standard.h=3;}
    if(type=="Camera"){standard.base.loc={0,-10,0};standard.base.rot={90,0,0};standard.cam.fov=77.3196165;}
    return standard.propertyBase(path);
}
void DObject::captureDefaults(){
    QStringList paths=transformChannels();
    paths<<"layout.width"<<"layout.height"<<"text.size"<<"material.alpha"<<"material.roughness"<<"material.emission"<<"light.power"<<"light.angle"<<"light.blend"<<"camera.fov"<<"camera.scale";
    for(const auto &path:paths)defaults[path]=propertyBase(path);
    for(auto i=style.begin();i!=style.end();++i)if(i.value().isDouble())defaults[i.key()]=i.value();
}
