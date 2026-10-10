#include "CueSequence.h"
#include "ElementLibrary.h"
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

namespace {
bool manualHold(const QJsonObject &cue){return cue["hold"].toBool(true)&&cue["holdManual"].toBool(!cue.contains("holdMs"));}
qint64 holdTime(const QJsonObject &cue){return cue["hold"].toBool(false)?qBound(qint64(0), qint64(cue["holdMs"].toDouble()),qint64(86400000)):0;}
int repetitions(const QJsonObject &cue){return qBound(1,cue["repeat"].toInt(1),100);}
void appendCue(CuePlan &plan,const ElementDesign &source,const QJsonObject &cue,const QString &entry,int iteration,const QString &json){
    if(cue["disabled"].toBool())return;
    const int first=plan.steps.size();
    const qint64 start=qBound(qint64(0),qint64(cue["start"].toDouble()),qint64(qMax(0,source.durationMs-1)));
    const qint64 end=qBound<qint64>(start+1,qint64(cue["end"].toDouble(source.durationMs)),qMax<qint64>(start+1,source.durationMs));
    for(int n=0;n<repetitions(cue);++n){
        if(plan.steps.size()>=10000){plan.error="This board exceeds 10,000 playback sections. Reduce its repeat counts.";return;}
        plan.steps.append({entry,cue["id"].toString(),cue["name"].toString("Animation"),json,iteration,start,end,holdTime(cue),manualHold(cue),-1});
    }
    if(cue["loop"].toBool()&&plan.steps.size()>first)plan.steps.last().loopTo=first;
}
}
qint64 CuePlan::duration()const{
    if(!error.isEmpty())return 0;
    qint64 result=0;for(const auto &s:steps){if(s.duration()<0||s.loopTo>=0)return -1;result+=s.duration();}return result;
}
int CuePlan::next(int index)const{
    if(index<0||index>=steps.size())return -1;
    return steps[index].loopTo>=0?steps[index].loopTo:index+1<int(steps.size())?index+1:-1;
}
CueSample CuePlan::sample(qint64 elapsed,int firstStep)const{
    if(!error.isEmpty()||firstStep<0||firstStep>=steps.size())return {};
    elapsed=qMax<qint64>(0,elapsed);int index=firstStep;
    for(int guard=0;guard<=steps.size()*2;++guard){
        const auto &s=steps[index];const auto length=s.end-s.start,duration=s.duration();
        if(duration<0||elapsed<duration)return {true,elapsed<length,s.start+qMin(elapsed,length),index};
        elapsed-=duration;
        if(s.loopTo>=0){
            qint64 cycle=0;for(int n=s.loopTo;n<=index;++n){if(steps[n].duration()<0){cycle=-1;break;}cycle+=steps[n].duration();}
            if(cycle>0)elapsed%=cycle;
        }
        index=next(index);if(index<0)return {};
    }
    return {};
}
QJsonObject newCueBoardEntry(const QJsonObject &cue){
    QJsonObject entry=cue;entry["id"]=Library::newId();entry["cue"]=cue["id"];entry["repeat"]=repetitions(cue);entry["hold"]=cue["hold"].toBool(true);entry["holdManual"]=manualHold(cue);entry["holdMs"]=double(holdTime(cue));entry.remove("trigger");entry.remove("triggerMs");entry["sections"]=QJsonArray{};return entry;
}
CuePlan compileAnimationCue(const ElementDesign &source,const QJsonObject &cue){CuePlan plan;appendCue(plan,source,cue,{},0,source.toJson());return plan;}
CuePlan compileCueBoard(const ElementDesign &source,const QJsonArray &board){
    CuePlan plan;
    for(const auto &value:board){
        const auto entry=value.toObject();if(entry["disabled"].toBool())continue;
        QJsonObject cue;for(const auto &c:source.animationCues)if(c.toObject()["id"]==entry["cue"]){cue=c.toObject();break;}
        if(cue.isEmpty()){plan.error="A cue used by this board no longer exists. Remove or replace that board entry.";return plan;}
        for(const auto &key:QStringList{"repeat","loop","hold","holdMs","holdManual"})if(entry.contains(key))cue[key]=entry[key];
        const auto sections=entry["sections"].toArray();const int first=plan.steps.size();
        for(int n=0;n<repetitions(cue);++n){
            const auto section=n<sections.size()?sections[n].toObject():QJsonObject{};if(section["disabled"].toBool())continue;
            auto design=section["design"].toString().isEmpty()?source:ElementDesign::fromJson(section["design"].toString());
            const auto fields=section["fields"].toObject();for(auto it=fields.begin();it!=fields.end();++it){QString reason;if(!setStudioField(design,it.key(),it.value().toString(),&reason)){plan.error=reason;return plan;}}
            const auto json=design.toJson();
            QList<QJsonObject> children;for(const auto &child:design.animationCues)children.append(child.toObject());
            if(!section["design"].toString().isEmpty()&&!children.isEmpty()){
                std::sort(children.begin(),children.end(),[](const auto &a,const auto &b){return a["start"].toDouble()<b["start"].toDouble();});
                const int childFirst=plan.steps.size();
                for(const auto &child:children)appendCue(plan,design,child,entry["id"].toString(),n,json);
                if(plan.steps.size()>childFirst){auto &last=plan.steps.last();last.holdMs+=holdTime(cue);last.manualHold=last.manualHold||manualHold(cue);}
            }else{
                auto once=cue;once["repeat"]=1;once["loop"]=false;
                if(!section["design"].toString().isEmpty()){once["start"]=design.startMs;once["end"]=design.durationMs;}
                appendCue(plan,design,once,entry["id"].toString(),n,json);
            }
            if(!plan.error.isEmpty())return plan;
        }
        if(cue["loop"].toBool()&&plan.steps.size()>first)plan.steps.last().loopTo=first;
    }
    return plan;
}
QString cueTimingLabel(const QJsonObject &cue){
    QString label=QString("%1 s · %2 plays").arg(qMax(1.,cue["end"].toDouble()-cue["start"].toDouble())/1000.,0,'g',4).arg(repetitions(cue));if(cue["loop"].toBool())label+=" · Loop until stopped";
    if(manualHold(cue))label+=" · Manual hold";else if(holdTime(cue)>0)label+=QString(" · Hold %1 s each").arg(holdTime(cue)/1000.,0,'g',4);
    return label;
}
