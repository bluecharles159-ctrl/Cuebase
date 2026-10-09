#include "Playout.h"
#include <QJsonDocument>

Playout::GraphicPlayback Playout::graphicPlayback(const Element &element,qint64 timelineTime,qint64 placementStart,qint64 offset) const {
    GraphicPlayback result{element.live||placementStart>=0,false,placementStart>=0?timelineTime-placementStart+offset:timelineTime,{}};
    auto cached=m_cueCache.find(element.id);
    if(cached==m_cueCache.end()||cached->first!=element.design)
        cached=m_cueCache.insert(element.id,{element.design,QJsonDocument::fromJson(element.design.toUtf8()).object()["animationCues"].toArray()});
    const auto &cues=cached->second;if(cues.isEmpty())return result;
    qint64 latest=-1;QJsonObject chosen;
    auto consider=[&](const QJsonObject &cue,qint64 at){if(at>=0&&at<=timelineTime&&at>=latest){latest=at;chosen=cue;}};
    for(const auto &value:cues){auto cue=value.toObject();const auto mode=cue["trigger"].toString("Manual");const qint64 offset=qMax<qint64>(0,cue["triggerMs"].toDouble());
        if(mode=="Program time")consider(cue,offset);
        else if(mode=="Before ad"||mode=="After ad")for(int i=0;i<m_segs.size();++i)if(m_segs[i].type==Segment::Ad)
            consider(cue,mode=="Before ad"?qMax<qint64>(0,segmentStartMs(i)-offset):segmentStartMs(i)+m_segs[i].ms+offset);
    }
    const auto manual=m_manualCues.constFind(element.id);
    if(manual!=m_manualCues.cend()&&manual->second<=timelineTime&&manual->second>=latest){
        if(manual->first.isEmpty())return {false,true,0,{}};
        for(const auto &value:cues)if(value.toObject()["id"].toString()==manual->first)consider(value.toObject(),manual->second);
    }
    if(chosen.isEmpty()&&placementStart>=0){chosen=cues.first().toObject();latest=placementStart-offset;}
    if(chosen.isEmpty())return result;
    const qint64 start=qMax<qint64>(0,chosen["start"].toDouble()),end=qMax(start+1,qint64(chosen["end"].toDouble()));
    const qint64 elapsed=qMax<qint64>(0,timelineTime-latest);
    return {chosen["hold"].toBool(true)||elapsed<end-start,true,qMin(end,start+elapsed),chosen["id"].toString(),elapsed<end-start};
}

void Playout::triggerElementAnimation(const QString &elementId,const QString &cueId){
    const auto *element=m_lib->element(elementId);if(!element)return;
    if(!cueId.isEmpty()){const auto cues=QJsonDocument::fromJson(element->design.toUtf8()).object()["animationCues"].toArray();bool found=false;for(auto value:cues)found|=value.toObject()["id"].toString()==cueId;if(!found)return;}
    m_manualCues[elementId]={cueId,playheadMs()};syncDesignAudio();emit ticked();
}
