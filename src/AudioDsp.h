#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <cmath>

// Ordered, bypassable offline processing. Only generated numeric filter arguments
// are accepted; project documents cannot inject arbitrary FFmpeg filter graphs.
namespace AudioDsp {
inline QStringList types(){
    return {"Echo","Equalizer","High pass","Low pass","Compressor","Limiter","Gain",
            "Stereo width","Tremolo","Vibrato","Flanger"};
}
inline QString filterGraph(const QJsonArray &chain,QString *error=nullptr){
    if(error)error->clear();
    QStringList filters;
    auto fail=[&](const QString &message){if(error)*error=message;return QString();};
    if(chain.size()>32)return fail("Use at most 32 audio effects per chain.");
    for(const auto &entry:chain){
        if(!entry.isObject())return fail("Invalid audio effect record.");
        const auto effect=entry.toObject();if(!effect["enabled"].toBool(true))continue;
        const auto type=effect["type"].toString();bool valid=true;
        auto number=[&](const char *key,double fallback,double low,double high){
            const auto field=effect[key];const double value=field.isUndefined()?fallback:field.toDouble(NAN);
            if(!std::isfinite(value)||value<low||value>high)valid=false;
            return QString::number(value,'g',12);
        };
        QString filter;
        if(type=="Echo"){
            const auto delay=number("delayMs",250,1,90000),decay=number("decay",.35,0,.95);
            filter="aecho=in_gain=0.8:out_gain=0.8:delays="+delay+":decays="+decay;
        }else if(type=="Equalizer"){
            const auto frequency=number("frequency",1000,20,20000),width=number("q",1,.1,10),gain=number("gainDb",0,-24,24);
            filter="equalizer=f="+frequency+":width_type=q:w="+width+":g="+gain;
        }else if(type=="High pass"||type=="Low pass"){
            filter=(type=="High pass"?"highpass=f=":"lowpass=f=")+number("frequency",type=="High pass"?80:12000,20,20000);
        }else if(type=="Compressor"){
            const auto threshold=number("threshold",.125,.001,1),ratio=number("ratio",4,1,20),attack=number("attackMs",20,.01,2000),release=number("releaseMs",250,.01,9000);
            filter="acompressor=threshold="+threshold+":ratio="+ratio+":attack="+attack+":release="+release;
        }else if(type=="Limiter"){
            filter="alimiter=level=false:limit="+number("limit",.95,.0625,1);
        }else if(type=="Gain"){
            filter="volume="+number("gainDb",0,-60,24)+"dB";
        }else if(type=="Stereo width"){
            filter="aformat=channel_layouts=stereo,stereowiden=delay="+number("delayMs",20,1,100)
                +":feedback="+number("feedback",.3,0,.9)+":crossfeed="+number("crossfeed",.3,0,.8)
                +":drymix="+number("dryMix",.8,0,1);
        }else if(type=="Tremolo" || type=="Vibrato"){
            filter=type.toLower()+"=f="+number("rate",5,.1,20000)+":d="+number("depth",.5,0,1);
        }else if(type=="Flanger"){
            filter="flanger=delay="+number("delayMs",0,0,30)+":depth="+number("sweepMs",2,0,10)
                +":regen="+number("feedback",0,-95,95)+":width="+number("wet",71,0,100)
                +":speed="+number("rate",.5,.1,10);
        }else return fail("Unknown audio effect: "+type);
        if(!valid)return fail("Invalid or out-of-range parameters for "+type+".");
        filters.append(filter);
    }
    return filters.isEmpty()?QString("anull"):filters.join(',');
}
}
