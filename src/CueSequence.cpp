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
CuePlan compileCueBoard(const ElementDesign &source,const QJsonArray &board,const QJsonObject &settings,int depth){
    if(depth>8)return CuePlan{{},"Cue nesting exceeds eight levels."};
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
                QJsonArray childBoard=section["board"].toArray();if(!section.contains("board"))for(const auto &child:children)childBoard.append(newCueBoardEntry(child));
                auto nested=compileCueBoard(design,childBoard,section["boardSettings"].toObject(),depth+1);if(!nested.error.isEmpty()){plan.error=nested.error;return plan;}
                const int childFirst=plan.steps.size();for(auto step:nested.steps){step.entryId=entry["id"].toString();step.iteration=n;if(step.loopTo>=0)step.loopTo+=childFirst;plan.steps.append(step);}
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
    if(!plan.steps.isEmpty()){
        plan.steps.last().holdMs+=holdTime(settings);plan.steps.last().manualHold|=settings["hold"].toBool(false)&&manualHold(settings);
        const auto cycle=plan.steps;for(int repeat=1;repeat<repetitions(settings);++repeat){const int offset=plan.steps.size();if(offset+cycle.size()>10000){plan.error="This board exceeds 10,000 playback sections.";return plan;}for(auto step:cycle){if(step.loopTo>=0)step.loopTo+=offset;plan.steps.append(step);}}
        if(settings["loop"].toBool())plan.steps.last().loopTo=0;
    }
    return plan;
}
QString cueTimingLabel(const QJsonObject &cue){
    QString label=QString("%1 s · %2 plays").arg(qMax(1.,cue["end"].toDouble()-cue["start"].toDouble())/1000.,0,'g',4).arg(repetitions(cue));if(cue["loop"].toBool())label+=" · Loop until stopped";
    if(manualHold(cue))label+=" · Manual hold";else if(holdTime(cue)>0)label+=QString(" · Hold %1 s each").arg(holdTime(cue)/1000.,0,'g',4);
    return label;
}

QJsonObject bakeCueBoard(const Element &element){auto design=ElementDesign::fromJson(element.design);for(int i=0;i<design.animationCues.size();++i){auto cue=design.animationCues[i].toObject();cue["trigger"]="Manual";cue.remove("triggerMs");design.animationCues[i]=cue;}auto settings=element.cueBoardSettings;settings.remove("_placementLength");settings.remove("_cycle");return {{"design",design.toJson()},{"cueBoard",element.cueBoard},{"settings",settings}};}
bool canEditCueBoard(const Element &before,const Element &after,qint64 start,qint64 now,bool live,QString *reason){
    if(!live)return true;const auto plan=compileCueBoard(ElementDesign::fromJson(before.design),before.cueBoard,before.cueBoardSettings);
    qint64 earliest=start;const bool global=before.design!=after.design||before.cueBoardSettings!=after.cueBoardSettings;
    int first=0;if(!global){while(first<before.cueBoard.size()&&first<after.cueBoard.size()&&before.cueBoard[first]==after.cueBoard[first])++first;if(first==before.cueBoard.size()&&first==after.cueBoard.size())return true;QSet<QString> prefix;for(int i=0;i<first;++i)prefix.insert(before.cueBoard[i].toObject()["id"].toString());for(const auto &step:plan.steps){if(!prefix.contains(step.entryId))break;if(step.duration()<0||step.loopTo>=0){earliest=now;break;}earliest+=step.duration();}}
    if(earliest-now>=300000)return true;if(reason)*reason="On air, only cues at least five minutes ahead may be changed. Played and near-air cues are protected.";return false;
}

Element cueBoardContext(const Element &root,const QStringList &path){Element current=root;for(int at=0;at+1<path.size();at+=2){QJsonObject entry;for(auto value:current.cueBoard)if(value.toObject()["id"].toString()==path[at])entry=value.toObject();const auto sections=entry["sections"].toArray();const int n=path[at+1].toInt();if(entry.isEmpty()||n<0||n>=sections.size())return {};const auto section=sections[n].toObject();current.design=section["design"].toString();current.name=section["name"].toString(entry["name"].toString()+QString(" · Play %1").arg(n+1));current.cueBoard=section["board"].toArray();current.cueBoardSettings=section["boardSettings"].toObject();if(!section.contains("board")){auto d=ElementDesign::fromJson(current.design);QList<QJsonObject> cues;for(auto c:d.animationCues)cues.append(c.toObject());std::sort(cues.begin(),cues.end(),[](auto a,auto b){return a["start"].toDouble()<b["start"].toDouble();});for(auto c:cues)current.cueBoard.append(newCueBoardEntry(c));}}return current;}
QJsonObject cueBoardSection(const Element &root,const QStringList &path){if(path.size()<2)return {};auto parent=cueBoardContext(root,path.mid(0,path.size()-2));for(auto value:parent.cueBoard){auto e=value.toObject();if(e["id"].toString()!=path[path.size()-2])continue;auto sections=e["sections"].toArray();int n=path.last().toInt();return n>=0&&n<sections.size()?sections[n].toObject():QJsonObject{};}return {};}
bool setCueBoardSection(Element &root,const QStringList &path,const QJsonObject &section){if(path.size()<2||path.size()%2)return false;auto parentPath=path.mid(0,path.size()-2);auto parent=cueBoardContext(root,parentPath);for(int i=0;i<parent.cueBoard.size();++i){auto e=parent.cueBoard[i].toObject();if(e["id"].toString()!=path[path.size()-2])continue;int n=path.last().toInt();if(n<0||n>=100)return false;auto sections=e["sections"].toArray();while(sections.size()<=n)sections.append(QJsonObject{});sections[n]=section;e["sections"]=sections;parent.cueBoard[i]=e;return setCueBoardContext(root,parentPath,parent);}return false;}
bool setCueBoardContext(Element &root,const QStringList &path,const Element &context){if(path.isEmpty()){root=context;return true;}auto section=cueBoardSection(root,path);section["design"]=context.design;section["board"]=context.cueBoard;section["boardSettings"]=context.cueBoardSettings;section["name"]=context.name;return setCueBoardSection(root,path,section);}
