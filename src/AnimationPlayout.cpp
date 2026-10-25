#include "Playout.h"
#include <QJsonDocument>
#include <optional>

namespace {
struct CueInstance {Element element;qint64 start=-1,offset=0;};
std::optional<CueInstance> resolveCueInstance(Library *library,const Playout &play,const QString &id){
    if(const auto *element=library->element(id))return CueInstance{*element};
    if(id.startsWith("placement:")){
        const auto *placement=library->placement(id.mid(10));
        if(placement&&placement->type=="Element")return CueInstance{library->placementElement(*placement),placement->startMs,placement->offsetMs};
    }else if(id.startsWith("program:")){
        const auto *program=library->program(id.mid(8));
        const int index=play.segmentAtMs(play.playheadMs());
        if(!program||index<0||index>=play.segments().size())return {};
        const auto &segment=play.segments()[index];
        if(segment.programId!=program->id||segment.type!=Segment::Program)return {};
        if(const auto *source=library->element(program->elementId)){
            auto instance=*source;instance.id=id;
            return CueInstance{instance,play.segmentStartMs(index),program->offsetMs};
        }
    }
    return {};
}
}

Playout::GraphicPlayback Playout::graphicPlayback(const Element &element,qint64 timelineTime,qint64 placementStart,qint64 offset) const {
    GraphicPlayback result{element.live||placementStart>=0,false,placementStart>=0?timelineTime-placementStart+offset:timelineTime,{}};
    const auto command=m_manualCues.constFind(element.id);
    const bool manual=command!=m_manualCues.cend()&&command->second<=timelineTime;
    auto cached=m_cueCache.find(element.id);
    if(cached==m_cueCache.end()||cached->first!=element.design)
        cached=m_cueCache.insert(element.id,{element.design,QJsonDocument::fromJson(element.design.toUtf8()).object()["animationCues"].toArray()});
    const auto &cues=cached->second;
    qint64 latest=-1;QJsonObject chosen;
    auto consider=[&](const QJsonObject &cue,qint64 at){if(at>=0&&at<=timelineTime&&at>=latest){latest=at;chosen=cue;}};
    for(const auto &value:cues){auto cue=value.toObject();const auto mode=cue["trigger"].toString("Manual");const qint64 offset=qMax<qint64>(0,cue["triggerMs"].toDouble());
        if(mode=="Program time")consider(cue,offset);
        else if(mode=="Before ad"||mode=="After ad")for(int i=0;i<m_segs.size();++i)if(m_segs[i].type==Segment::Ad)
            consider(cue,mode=="Before ad"?qMax<qint64>(0,segmentStartMs(i)-offset):segmentStartMs(i)+m_segs[i].ms+offset);
    }
    if(manual&&command->first.isEmpty()&&command->second>=latest)return {false,true,0,{}};
    if(!element.cueBoard.isEmpty()&&((manual&&command->first.startsWith("@board:")&&command->second>=latest)||(!manual&&latest<0&&(placementStart>=0||element.live)))){
        const auto signature=element.design+QString::fromUtf8(QJsonDocument(element.cueBoard).toJson(QJsonDocument::Compact))+QString::fromUtf8(QJsonDocument(element.cueBoardSettings).toJson(QJsonDocument::Compact));
        auto plan=m_boardCache.find(element.id);
        if(plan==m_boardCache.end()||plan->first!=signature)plan=m_boardCache.insert(element.id,{signature,compileCueBoard(ElementDesign::fromJson(element.design),element.cueBoard,element.cueBoardSettings)});
        const int first=manual?command->first.mid(7).toInt():0;
        auto elapsed=manual?timelineTime-command->second:placementStart>=0?timelineTime-placementStart+offset:timelineTime;
        const qint64 cycle=element.cueBoardSettings["_cycle"].toDouble(),length=element.cueBoardSettings["_placementLength"].toDouble();if(cycle>0&&length>cycle&&elapsed>=0)elapsed%=cycle;
        const auto sample=plan->second.sample(elapsed,first);
        if(!sample.visible)return {false,true,0,{}};
        const auto &step=plan->second.steps[sample.step];return {true,true,sample.frame,step.cueId,sample.advancing,step.design,sample.step};
    }
    if(manual&&command->second>=latest){
        if(command->first.isEmpty())return {false,true,0,{}};
        for(const auto &value:cues)if(value.toObject()["id"].toString()==command->first)consider(value.toObject(),command->second);
    }
    if(chosen.isEmpty()&&placementStart>=0&&!cues.isEmpty()){chosen=cues.first().toObject();latest=placementStart-offset;}
    if(chosen.isEmpty())return result;
    const qint64 elapsed=qMax<qint64>(0,timelineTime-latest);
    const auto key=element.id+"/"+chosen["id"].toString();auto plan=m_boardCache.find(key);
    if(plan==m_boardCache.end()||plan->first!=element.design)plan=m_boardCache.insert(key,{element.design,compileAnimationCue(ElementDesign::fromJson(element.design),chosen)});
    const auto sample=plan->second.sample(elapsed);return {sample.visible,true,sample.frame,chosen["id"].toString(),sample.advancing};
}

void Playout::triggerElementAnimation(const QString &elementId,const QString &cueId){
    const auto instance=resolveCueInstance(m_lib,*this,elementId);if(!instance)return;const auto *element=&instance->element;
    if(cueId.startsWith("@board:")){const auto plan=compileCueBoard(ElementDesign::fromJson(element->design),element->cueBoard,element->cueBoardSettings);bool ok=false;const int first=cueId.mid(7).toInt(&ok);if(!ok||!plan.error.isEmpty()||first<0||first>=plan.steps.size())return;}
    else if(!cueId.isEmpty()){const auto cues=QJsonDocument::fromJson(element->design.toUtf8()).object()["animationCues"].toArray();bool found=false;for(auto value:cues)found|=value.toObject()["id"].toString()==cueId;if(!found)return;}
    m_manualCues[elementId]={cueId,playheadMs()};syncDesignAudio();emit ticked();
}
void Playout::advanceElementCue(const QString &elementId){
    const auto instance=resolveCueInstance(m_lib,*this,elementId);if(!instance)return;const auto *element=&instance->element;const auto current=graphicPlayback(*element,playheadMs(),instance->start,instance->offset);
    if(current.boardStep<0)return;const auto plan=compileCueBoard(ElementDesign::fromJson(element->design),element->cueBoard,element->cueBoardSettings);const int next=current.boardStep+1<int(plan.steps.size())?current.boardStep+1:-1;
    triggerElementAnimation(elementId,next>=0?"@board:"+QString::number(next):QString());
}
