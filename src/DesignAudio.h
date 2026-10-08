#pragma once
#include "ElementDesign.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>
#include <functional>

struct DesignAudioCue {QString id,file;qint64 position=0;double volume=1;bool playing=false;double rate=1;};
inline QList<DesignAudioCue> designAudioCues(const ElementDesign &design,qint64 time,bool playing,const QString &prefix={},double gain=1,bool loopPlayback=true){
    QList<DesignAudioCue> cues;
    if(loopPlayback&&design.loop&&design.durationMs>design.startMs&&time>=design.durationMs)time=design.startMs+(time-design.startMs)%(design.durationMs-design.startMs);
    for(const auto &object:design.objects){if(!design.effectiveVisible(object))continue;
        if(object.style["media.kind"].toString()=="Video"&&object.style["background.type"].toString(object.type=="Image"?"Media":"Solid")=="Media"){
            const auto length=qint64(object.style["media.duration"].toDouble());const auto rate=object.propertyValue("media.speed",time);qint64 position=time*rate+object.propertyValue("media.offset",time);
            if(length>0&&!object.image.isEmpty()&&(object.style["media.loop"].toBool(true)||position<length)){position%=length;cues.append({prefix+object.id,object.image,position,object.propertyValue("media.volume",time)*gain,playing,rate});}continue;
        }
        if(object.type!="Audio")continue;
        const qint64 start=object.propertyValue("audio.start",time),sourceLength=object.style["audio.duration"].toDouble(),offset=object.propertyValue("audio.offset",time);
        const double rate=qBound(.1,object.propertyValue("audio.speed",time),4.);const qint64 available=qMax<qint64>(0,sourceLength-offset);qint64 length=object.propertyValue("audio.length",time);if(length<=0)length=qRound64(available/rate);
        const auto file=object.style["audio.file"].toString();if(file.isEmpty()||available<=0||time<start||time>=start+length||object.style["audio.mute"].toString()=="On")continue;
        const qint64 elapsed=time-start;const bool repeat=object.style["audio.repeat"].toString()=="Loop";qint64 position=qRound64(elapsed*rate);if(!repeat&&position>=available)continue;if(repeat)position%=available;
        double level=object.propertyValue("audio.volume",time)*gain,fadeIn=object.propertyValue("audio.fadeIn",time),fadeOut=object.propertyValue("audio.fadeOut",time);
        if(fadeIn>0)level*=qBound(0.,elapsed/fadeIn,1.);if(fadeOut>0)level*=qBound(0.,(length-elapsed)/fadeOut,1.);
        cues.append({prefix+object.id,file,offset+position,qBound(0.,level,1.),playing,rate});
    }return cues;
}
class DesignAudio final:public QObject {
    struct Source {QMediaPlayer *player;QAudioOutput *output;};
    QHash<QString,Source> sources;
public:
    explicit DesignAudio(QObject *parent):QObject(parent){}
    std::function<void(QMediaPlayer*,QAudioOutput*)> created;
    std::function<void(QString)> error;
    void sync(const QList<DesignAudioCue> &cues){
        QSet<QString> active;
        for(const auto &cue:cues){active.insert(cue.id);
            if(!sources.contains(cue.id)){auto *player=new QMediaPlayer(this);auto *output=new QAudioOutput(this);player->setAudioOutput(output);sources.insert(cue.id,{player,output});
                connect(player,&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString &text){if(error)error(text);});if(created)created(player,output);
            }
            auto source=sources.value(cue.id);auto *player=source.player;source.output->setVolume(cue.volume);
            player->setPlaybackRate(cue.rate);
            const auto url=QUrl::fromLocalFile(cue.file);bool changed=player->source()!=url;
            if(changed)player->setSource(url);
            if(changed||qAbs(player->position()-cue.position)>150||!cue.playing)player->setPosition(cue.position);
            if(cue.playing){if(player->playbackState()!=QMediaPlayer::PlayingState)player->play();}else player->pause();
        }
        for(auto it=sources.begin();it!=sources.end();){if(!active.contains(it.key())){it->player->stop();it->player->deleteLater();it->output->deleteLater();it=sources.erase(it);}else ++it;}
    }
};
