#include "PlanHandoff.h"
#include "Playout.h"
#include "DesignAssets.h"
#include <QFileInfo>
#include <QJsonDocument>

namespace {
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
bool available(const QString &path){return path.isEmpty()||QFileInfo(path).isFile();}
bool designMediaAvailable(const QString &json,const QString &context,QString *error){
    if(!QJsonDocument::fromJson(json.toUtf8()).isObject())
        return fail(error,"The design cannot be read: "+context);
    const auto design=ElementDesign::fromJson(json);
    const auto missing=missingDesignAsset(design);if(!missing.isEmpty())return fail(error,context+" / "+missing);
    return true;
}
}
PlanHandoff::PlanHandoff(Library *library,Playout *broadcast,QObject *parent)
    :QObject(parent),m_library(library),m_broadcast(broadcast){
    auto invalidate=[this]{if(!m_writing){m_hasUndo=false;m_before.clear();emit changed();}};
    connect(library,&Library::programsChanged,this,invalidate);
    connect(library,&Library::placementsChanged,this,invalidate);
    connect(broadcast,&Playout::stateChanged,this,[this]{emit changed();});
}
bool PlanHandoff::canUndo()const{return m_hasUndo&&m_broadcast->state()==Playout::Stopped;}
bool PlanHandoff::validate(const ProductionPlan &plan,QString *error)const{
    if(m_broadcast->state()!=Playout::Stopped)return fail(error,"Stop broadcast playback before sending a plan to the rundown.");
    if(!plan.validate(error))return false;
    if(plan.entries.isEmpty())return fail(error,"Add at least one item to the plan first.");
    for(const auto &entry:plan.entries){
        if(entry.kind=="Program"){
            const auto *p=m_library->program(entry.sourceId);
            if(!p||p->isAdSpot()||!p->elementId.isEmpty())return fail(error,"A program source is missing or is no longer a program.");
            if(!available(p->file))return fail(error,"Program media is missing: "+p->title);
        }else if(entry.kind=="Ad"){
            const auto *ad=m_library->ad(entry.sourceId);
            if(!ad)return fail(error,"An advertisement source is missing.");
            if(!available(ad->file))return fail(error,"Advertisement media is missing: "+ad->name);
        }else{
            const auto *e=m_library->element(entry.sourceId);
            if(!e)return fail(error,"A screen-element source is missing.");
            if(!available(e->image))return fail(error,"Screen-element image is missing: "+e->name);
            if(e->type=="Custom"){
                if(!designMediaAvailable(e->design,e->name,error))return false;
                const auto cues=compileCueBoard(ElementDesign::fromJson(e->design),e->cueBoard,e->cueBoardSettings);
                if(!cues.error.isEmpty())return fail(error,"The cue board cannot play: "+e->name+"\n"+cues.error);
                for(const auto &step:cues.steps)
                    if(!step.design.isEmpty()&&!designMediaAvailable(step.design,e->name+" / cue "+step.cueId,error))return false;
            }
        }
    }
    if(error)error->clear();return true;
}
bool PlanHandoff::append(const ProductionPlan &plan,QString *error){
    if(!validate(plan,error))return false;
    const auto before=m_library->programs;
    auto after=before;
    for(const auto &entry:plan.entries){
        ProgramItem p;
        if(entry.kind=="Program")p=*m_library->program(entry.sourceId);
        else if(entry.kind=="Ad"){const auto *ad=m_library->ad(entry.sourceId);p.title=ad->name;p.kind="Ad";p.adId=ad->id;}
        else{const auto *e=m_library->element(entry.sourceId);p.title=e->name;p.kind="Screen element";p.elementId=e->id;p.fx=e->fx;}
        p.id=Library::newId();p.durationMs=entry.durationMs;p.seconds=int((entry.durationMs+999)/1000);
        p.breakIn.clear();p.breakOut.clear();p.slotAfter=false;
        if(entry.kind!="Ad")p.adId.clear();
        after.append(p);
    }
    m_library->programs=after;
    if(!m_library->saveChecked(error)){m_library->programs=before;return false;}
    m_before=before;m_hasUndo=true;m_writing=true;
    emit m_library->programsChanged();m_writing=false;emit changed();return true;
}
bool PlanHandoff::undo(QString *error){
    if(!canUndo())return fail(error,"Undo is unavailable after another timeline edit or while playback is active.");
    const auto after=m_library->programs;m_library->programs=m_before;
    if(!m_library->saveChecked(error)){m_library->programs=after;return false;}
    m_before.clear();m_hasUndo=false;m_writing=true;
    emit m_library->programsChanged();m_writing=false;emit changed();return true;
}
