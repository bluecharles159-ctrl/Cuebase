#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <functional>
#include <cmath>
#include <algorithm>

// Ordered, bypassable offline processing. Only generated numeric filter arguments
// are accepted; project documents cannot inject arbitrary FFmpeg filter graphs.
namespace AudioDsp {
inline QStringList types(){
    return {"Echo","Equalizer","High pass","Low pass","Compressor","Limiter","Gain",
            "Stereo width","Tremolo","Vibrato","Flanger","Pitch","Tempo","Balance","Sidechain compressor"};
}
struct Command {QString key,label,filter,command;double min=0,max=0;};
inline QList<Command> commands(const QString &type){
    if(type=="Gain")return {{"gainDb","Gain (dB)","volume","volume",-60,24}};
    if(type=="Equalizer")return {{"gainDb","Gain (dB)","equalizer","g",-24,24},{"frequency","Frequency (Hz)","equalizer","f",20,20000}};
    if(type=="High pass"||type=="Low pass")return {{"frequency","Frequency (Hz)",type=="High pass"?"highpass":"lowpass","f",20,20000}};
    if(type=="Compressor")return {{"threshold","Threshold","acompressor","threshold",.001,1},{"ratio","Ratio","acompressor","ratio",1,20}};
    if(type=="Stereo width")return {{"crossfeed","Crossfeed","stereowiden","crossfeed",0,.8},{"dryMix","Dry mix","stereowiden","drymix",0,1}};
    return {};
}
inline QString tempoFilters(double ratio){
    QStringList filters;
    while(ratio<.5){filters.append("atempo=0.5");ratio*=2;}
    while(ratio>2){filters.append("atempo=2");ratio/=2;}
    filters.append("atempo="+QString::number(ratio,'g',12));return filters.join(',');
}
inline QString filterGraph(const QJsonArray &chain,QString *error=nullptr,int firstSlot=0){
    if(error)error->clear();
    QStringList filters;
    auto fail=[&](const QString &message){if(error)*error=message;return QString();};
    if(chain.size()>32)return fail("Use at most 32 audio effects per chain.");
    int slot=firstSlot;
    for(const auto &entry:chain){
        ++slot;
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
        }else if(type=="Compressor"||type=="Sidechain compressor"){
            const auto threshold=number("threshold",.125,.001,1),ratio=number("ratio",4,1,20),attack=number("attackMs",20,.01,2000),release=number("releaseMs",250,.01,9000);
            filter=(type=="Compressor"?QStringLiteral("acompressor"):QStringLiteral("sidechaincompress"))+"=threshold="+threshold+":ratio="+ratio+":attack="+attack+":release="+release;
            if(type=="Sidechain compressor" && effect.value("trigger").toString().isEmpty())return fail("Choose the audio that triggers the sidechain compressor.");
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
        }else if(type=="Pitch"){
            const auto semitones=number("semitones",0,-24,24);if(!valid)return fail("Pitch must be between −24 and +24 semitones.");
            const double ratio=std::exp2(semitones.toDouble()/12.);
            filter="aresample=48000,asetrate="+QString::number(qRound(48000*ratio))+",aresample=48000,"+tempoFilters(1./ratio);
        }else if(type=="Tempo"){
            const auto ratio=number("ratio",1,.25,4);if(!valid)return fail("Tempo must be between 0.25 and 4.");
            filter=tempoFilters(ratio.toDouble());
        }else if(type=="Balance"){
            const double balance=number("balance",0,-1,1).toDouble();
            filter="aformat=channel_layouts=stereo,pan=stereo|c0="+QString::number(1.-qMax(0.,balance))+"*c0|c1="+QString::number(1.+qMin(0.,balance))+"*c1";
        }else return fail("Unknown audio effect: "+type);
        if(!valid)return fail("Invalid or out-of-range parameters for "+type+".");
        const auto events=effect.value("automation").toArray();
        if(!events.isEmpty()){
            if(events.size()>128)return fail("Use up to 128 automation keys per processor.");
            const auto options=commands(type);QMap<qint64,QStringList> ordered;QSet<QString> used;
            QString target;
            for(auto item:events){
                const auto e=item.toObject();const auto time=e.value("ms").toInteger(-1);
                const QString field=e.value("field").toString();const double value=e.value("value").toDouble(NAN);
                const auto signature=QString::number(time)+"/"+field;
                if(time<0||time>86400000||used.contains(signature))return fail("Automation times must be unique per parameter within 24 hours.");
                used.insert(signature);
                auto cmd=std::find_if(options.cbegin(),options.cend(),[&](const Command &c){return c.key==field;});
                if(cmd==options.cend() || !std::isfinite(value) || value<cmd->min || value>cmd->max)return fail("Invalid automation value for "+type+".");
                target=cmd->filter+"@cb"+QString::number(slot);
                ordered[time].append(target+" "+cmd->command+" "+QString::number(value,'g',12)+(cmd->filter=="volume"?"dB":""));
            }
            QStringList instructions;
            for(auto it=ordered.cbegin();it!=ordered.cend();++it)instructions.append(QString::number(it.key()/1000.,'f',3)+" "+it.value().join(","));
            filter.replace(options.first().filter+"=",target+"=");
            filter="asendcmd=c='"+instructions.join(";")+"',"+filter;
        }
        filters.append(filter);
    }
    return filters.isEmpty()?QString("anull"):filters.join(',');
}

// Keep file paths in process arguments. Only generated labels and validated
// numeric settings enter the FFmpeg graph.
inline QStringList processingArgs(const QString &source,const QJsonArray &chain,
                                  const std::function<QString(QString)> &fileFor,QString *error){
    if(filterGraph(chain,error).isEmpty())return {};
    QStringList args{"-i",source},parts;int input=0,step=0;QString current="[0:a:0]";
    for(auto value:chain){
        const auto effect=value.toObject();if(!effect.value("enabled").toBool(true))continue;
        const auto filter=filterGraph({effect},error,step);
        if(effect.value("type").toString()=="Sidechain compressor"){
            const auto path=fileFor(effect.value("trigger").toString());
            if(!QFileInfo(path).isFile()){if(error)*error="The sidechain trigger is unavailable.";return {};}
            args<<"-i"<<path;++input;
            const auto tag="[trigger"+QString::number(input)+"]";
            parts.append("["+QString::number(input)+":a:0]asetpts=PTS-STARTPTS,apad"+tag);current+=tag;
        }
        const auto next="[stage"+QString::number(++step)+"]";parts.append(current+filter+next);current=next;
    }
    if(!step)return args<< "-map"<<"0:a:0"<<"-af"<<"anull";
    return args<<"-filter_complex"<<parts.join(';')<<"-map"<<current;
}
}
