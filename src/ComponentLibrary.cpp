#include "ComponentLibrary.h"
#include "BuildModel.h"
#include "Data.h"
#include "DesignAssets.h"
#include <QCryptographicHash>
#include <QCache>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPainter>
#include <QUuid>

namespace Components {
namespace {
QString uuid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
QJsonObject raw(const DObject &object){ElementDesign scene;scene.objects.append(object);return QJsonDocument::fromJson(scene.toJson().toUtf8()).object()["objects"].toArray().first().toObject();}
DObject objectFrom(const QJsonObject &item){
    auto data=item["object"].toObject();const auto id=data["id"].toString();
    const auto scene=ElementDesign::fromJson(QString::fromUtf8(QJsonDocument(QJsonObject{{"version",6},{"objects",QJsonArray{data}}}).toJson()));
    if(!id.isEmpty())if(const auto *object=scene.find(id))return *object;DObject invalid;invalid.type="Invalid";return invalid;
}
QString styleGroup(const QString &key){
    if(key.startsWith("component.")||key.startsWith("studio.")||key.startsWith("media.")||key=="audio.file")return {};
    if(key.startsWith("fill.")||key.startsWith("texture."))return "Materials";
    if(key.startsWith("effects.")||key.startsWith("backdrop.")||key.startsWith("shadow.")||key.startsWith("textShadow.")||key.startsWith("surface."))return "Effects";
    return "Appearance";
}
QJsonObject groupData(const DObject &object,const QString &group){
    auto normalized=object;for(auto &modifier:normalized.mods)if(modifier.target==object.id)modifier.target="@self";
    const auto data=raw(normalized);QJsonObject result;
    QStringList fields;
    if(group=="Geometry")fields={"type","mesh","mods","w","h"};
    if(group=="Materials")fields={"mat"};
    if(group=="Animation")fields={"tracks"};
    if(group=="Appearance")fields={"fontPx","layout"};
    if(group=="Appearance")result["opacity"]=object.base.opacity;
    for(const auto &field:fields)if(data.contains(field))result[field]=data[field];
    QJsonObject style;for(auto it=object.style.begin();it!=object.style.end();++it)if(styleGroup(it.key())==group)style[it.key()]=it.value();
    if(!style.isEmpty())result["style"]=style;return result;
}
QString hash(const QJsonValue &value){QJsonDocument doc(value.isArray()?QJsonDocument(value.toArray()):QJsonDocument(value.toObject()));return QString::fromLatin1(QCryptographicHash::hash(doc.toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
QJsonArray sourceCues(const ElementDesign &scene){QJsonArray result;for(auto value:scene.animationCues)if(value.toObject()["component.instance"].toString().isEmpty())result.append(value);return result;}
QJsonArray instanceCues(const ElementDesign &scene,const QString &id){
    QJsonArray result;for(auto value:scene.animationCues){auto cue=value.toObject();if(cue["component.instance"].toString()!=id)continue;
        cue["id"]=cue["component.sourceCue"];cue.remove("component.instance");cue.remove("component.sourceCue");result.append(cue);}return result;
}
QString fingerprint(const ElementDesign &scene,const DObject &object,const QString &group){return group=="Cues"?hash(object.style["component.role"].toString()=="Source"?sourceCues(scene):instanceCues(scene,object.id)):hash(groupData(object,group));}
void recordBaseline(const ElementDesign &scene,DObject &object,int revision){
    QJsonObject baseline,revisions;for(const auto &group:groups()){baseline[group]=fingerprint(scene,object,group);revisions[group]=revision;}
    object.style["component.baseline"]=baseline;object.style["component.groupRevisions"]=revisions;
}
void copyGroup(DObject &target,const DObject &source,const QString &group){
    if(group=="Geometry"){target.type=source.type;target.mesh=source.mesh;target.mods=source.mods;target.w=source.w;target.h=source.h;for(auto &modifier:target.mods)if(modifier.target==source.id)modifier.target=target.id;}
    if(group=="Materials")target.mat=source.mat;
    if(group=="Animation"){target.tracks=source.tracks;target.refreshLegacyKeys();for(auto it=target.overrides.begin();it!=target.overrides.end();)if(target.track(it.key()))it=target.overrides.erase(it);else ++it;}
    if(group=="Appearance"){target.base.opacity=source.base.opacity;target.fontPx=source.fontPx;target.insetLeft=source.insetLeft;target.insetRight=source.insetRight;target.insetTop=source.insetTop;target.insetBottom=source.insetBottom;target.borderRadius=source.borderRadius;}
    for(const auto &key:target.style.keys())if(styleGroup(key)==group)target.style.remove(key);
    for(auto it=source.style.begin();it!=source.style.end();++it)if(styleGroup(it.key())==group)target.style[it.key()]=it.value();
}
void replaceCues(ElementDesign &scene,const QString &instance,const QJsonArray &cues){
    QHash<QString,QString> previous;QJsonArray next;
    for(auto value:scene.animationCues){const auto cue=value.toObject();if(cue["component.instance"].toString()==instance)previous[cue["component.sourceCue"].toString()]=cue["id"].toString();else next.append(value);}
    for(auto value:cues){auto cue=value.toObject();const auto sourceId=cue["id"].toString();cue["component.sourceCue"]=sourceId;cue["component.instance"]=instance;cue["id"]=previous.value(sourceId,uuid());next.append(cue);}scene.animationCues=next;
}
bool commit(Library &library,const QJsonArray &items,QString *error){const auto before=library.production;library.production["components"]=QJsonObject{{"version",1},{"entries",items}};if(!library.saveChecked(error)){library.production=before;return false;}emit library.productionChanged();if(error)error->clear();return true;}
bool usable(const DObject *object,QString *error){
    if(!object||!object->isRenderable()||object->locked)return fail(error,"Select an unlocked mesh, shape, text or image object.");
    for(const auto &modifier:object->mods)if(!modifier.target.isEmpty()&&modifier.target!=object->id)return fail(error,"Apply modifiers that reference other objects before registering this reusable object.");
    return true;
}
QJsonObject definition(const BuildModel &model,const DObject &object){
    auto source=object;for(const auto &key:source.style.keys())if(key.startsWith("component."))source.style.remove(key);source.parentId.clear();source.parentInverse.setToIdentity();
    QJsonObject links;for(const auto &group:groups())links[group]=object.style["component.links"].toObject()[group].toBool(true);
    return {{"object",raw(source)},{"cues",sourceCues(model.design)},{"duration",model.design.durationMs},{"links",links}};
}
}
QStringList groups(){return {"Geometry","Materials","Animation","Cues","Effects","Appearance"};}
QJsonObject defaultLinks(){QJsonObject result;for(const auto &group:groups())result[group]=true;return result;}
QJsonArray entries(const Library &library){return library.production["components"].toObject()["entries"].toArray();}
QJsonObject entry(const Library &library,const QString &id){for(auto value:entries(library)){const auto item=value.toObject();if(item["id"].toString()==id)return item;}return {};}
QString registerSource(BuildModel &model,Library &library,const QString &objectId,const QString &name,QString *error){
    if(model.editMode||name.trimmed().isEmpty()){fail(error,"Enter an object name in Object Mode.");return {};}
    auto *object=model.byId(objectId);if(!usable(object,error))return {};
    if(!object->style.value("component.id").toString().isEmpty()){fail(error,"This object is already linked. Publish its source or insert another instance from Components.");return {};}
    auto item=definition(model,*object);const auto id=uuid();item["id"]=id;item["name"]=name.trimmed();item["category"]="Unsorted";item["revision"]=1;item["sourceObject"]=objectId;item["lastUsed"]=0.;
    auto source=item["object"].toObject();source["name"]=name.trimmed();item["object"]=source;auto items=entries(library);items.append(item);if(!commit(library,items,error))return {};
    model.pushUndo();object=model.byId(objectId);object->name=name.trimmed();object->style["component.id"]=id;object->style["component.role"]="Source";object->style["component.revision"]=1;object->style["component.links"]=defaultLinks();recordBaseline(model.design,*object,1);model.notify();return id;
}
bool publish(BuildModel &model,Library &library,QString *error){
    if(model.editMode)return fail(error,"Exit Edit Mode before publishing a source.");
    const auto *object=model.active();if(!usable(object,error))return false;
    const auto id=object->style["component.id"].toString();auto items=entries(library);
    for(int index=0;index<items.size();++index){auto item=items[index].toObject();if(item["id"].toString()!=id)continue;
        if(object->style["component.role"].toString()!="Source"||item["sourceObject"].toString()!=object->id)return fail(error,"Only the registered source can publish a revision. Instance edits stay local.");
        const auto next=definition(model,*object);for(auto it=next.begin();it!=next.end();++it)item[it.key()]=it.value();const int revision=item["revision"].toInt()+1;item["revision"]=revision;items[index]=item;
        if(!commit(library,items,error))return false;model.pushUndo();model.active()->style["component.revision"]=revision;recordBaseline(model.design,*model.active(),revision);model.notify();return true;
    }return fail(error,"This component source is not available in this library.");
}
QString insert(BuildModel &model,Library &library,const QString &id,QString *error){
    if(model.editMode){fail(error,"Exit Edit Mode before inserting a component.");return {};}
    auto item=entry(library,id);if(item.isEmpty()){fail(error,"This component is not available in this library.");return {};}
    auto object=objectFrom(item);if(!object.isRenderable()){fail(error,"The component definition is invalid.");return {};}
    ElementDesign check;check.objects.append(object);const auto missing=missingDesignAsset(check);if(!missing.isEmpty()){fail(error,missing);return {};}
    const auto sourceId=object.id;object.id=uuid();for(auto &modifier:object.mods)if(modifier.target==sourceId)modifier.target=object.id;
    object.name=model.uniqueName(item["name"].toString());object.locked=false;object.parentId.clear();object.parentInverse.setToIdentity();
    object.style["component.id"]=id;object.style["component.role"]="Instance";object.style["component.revision"]=item["revision"];object.style["component.links"]=item.contains("links")?item["links"].toObject():defaultLinks();
    auto next=model.design;next.durationMs=qMax(next.durationMs,item["duration"].toInt());replaceCues(next,object.id,item["cues"].toArray());
    QJsonObject baseline,revisions;for(const auto &group:groups()){baseline[group]=fingerprint(next,object,group);revisions[group]=item["revision"];}object.style["component.baseline"]=baseline;object.style["component.groupRevisions"]=revisions;next.objects.append(object);
    // Persist recency before editing the scene, so failed writes leave it untouched.
    auto items=entries(library);for(int i=0;i<items.size();++i)if(items[i].toObject()["id"].toString()==id){item["lastUsed"]=double(QDateTime::currentMSecsSinceEpoch());items[i]=item;break;}
    if(!commit(library,items,error))return {};model.pushUndo();model.design=next;model.setSelection({object.id},object.id);model.notify();return object.id;
}
bool metadata(Library &library,const QString &id,const QString &name,const QString &category,QString *error){
    if(name.trimmed().isEmpty())return fail(error,"Enter a component name.");auto items=entries(library);for(int i=0;i<items.size();++i){auto item=items[i].toObject();if(item["id"].toString()!=id)continue;item["name"]=name.trimmed();item["category"]=category.trimmed().isEmpty()?"Unsorted":category.trimmed();items[i]=item;return commit(library,items,error);}return fail(error,"This component is no longer available.");
}
bool setLinks(BuildModel &model,const QJsonObject &links,QString *error){
    if(model.editMode)return fail(error,"Exit Edit Mode before changing component links.");
    auto *object=model.active();if(!object||object->locked||object->style.value("component.id").toString().isEmpty())return fail(error,"Select an unlocked linked object.");
    QJsonObject safe;for(const auto &group:groups())safe[group]=links[group].toBool(true);model.pushUndo();object->style["component.links"]=safe;model.notify();if(error)error->clear();return true;
}
QStringList overrides(const ElementDesign &scene,const DObject &object){QStringList result;const auto baseline=object.style["component.baseline"].toObject();for(const auto &group:groups())if(baseline.contains(group)&&baseline[group].toString()!=fingerprint(scene,object,group))result.append(group);return result;}
void duplicateLinks(ElementDesign &scene,const QString &originalId,const QString &copyId){
    const auto *original=scene.find(originalId);auto *copy=scene.find(copyId);if(!original||!copy||original->style["component.id"].toString().isEmpty())return;
    const bool source=original->style["component.role"].toString()=="Source";
    const auto cues=source?sourceCues(scene):instanceCues(scene,originalId);replaceCues(scene,copyId,cues);copy->style["component.role"]="Instance";
    // Copies retain the last published baseline, even when the source has unpublished edits.
    if(copy->style["component.baseline"].toObject().isEmpty())recordBaseline(scene,*copy,copy->style["component.revision"].toInt());
}
void pruneInstanceCues(ElementDesign &scene){QJsonArray next;for(auto value:scene.animationCues){const auto id=value.toObject()["component.instance"].toString();if(id.isEmpty()||scene.find(id))next.append(value);}scene.animationCues=next;}
int updateSelected(BuildModel &model,const Library &library,const QSet<QString> &chosen,bool preserve,QString *error){
    if(model.editMode){fail(error,"Exit Edit Mode before updating instances.");return -1;}
    auto next=model.design;int changed=0;
    for(const auto &id:model.sel){auto *object=next.find(id);if(!object||object->locked||object->style.value("component.role").toString()!="Instance")continue;
        const auto item=entry(library,object->style["component.id"].toString());if(item.isEmpty())continue;const auto source=objectFrom(item);const auto local=overrides(next,*object);const auto links=object->style["component.links"].toObject();auto baseline=object->style["component.baseline"].toObject(),revisions=object->style["component.groupRevisions"].toObject();bool applied=false;
        for(const auto &group:groups())if(chosen.contains(group)&&links[group].toBool(true)&&(!preserve||!local.contains(group))){
            if(group=="Cues"){replaceCues(next,id,item["cues"].toArray());next.durationMs=qMax(next.durationMs,item["duration"].toInt());}else copyGroup(*object,source,group);
            if(group=="Animation")for(const auto &track:object->tracks)for(const auto &key:track.keys)next.durationMs=qMax(next.durationMs,key.ms);
            baseline[group]=fingerprint(next,*object,group);revisions[group]=item["revision"];applied=true;
        }
        if(applied){object->style["component.baseline"]=baseline;object->style["component.groupRevisions"]=revisions;int oldest=item["revision"].toInt();for(const auto &group:groups())if(links[group].toBool(true))oldest=qMin(oldest,revisions[group].toInt());object->style["component.revision"]=oldest;++changed;}
    }
    const auto missing=missingDesignAsset(next);if(!missing.isEmpty()){fail(error,missing);return -1;}
    if(!changed){fail(error,"No linked instance can be updated. Select an unlocked instance and check its link settings or local overrides.");return 0;}
    if(next.toJson()==model.design.toJson()){fail(error,"Selected linked groups are already up to date.");return 0;}
    model.pushUndo();model.design=next;model.notify();if(error)error->clear();return changed;
}
QImage thumbnail(const QJsonObject &item){
    auto object=objectFrom(item);if(!object.isRenderable())return {};auto scene=ElementDesign::defaultScene();object.base.loc={0,0,0};object.tracks.clear();object.parentId.clear();object.parentInverse.setToIdentity();object.style.remove("component.baseline");scene.objects.append(object);
    QString signature=hash(item["object"]);for(const auto &asset:designAssets(scene)){const QFileInfo file(asset.path);signature+=asset.path+QString::number(file.size())+QString::number(file.lastModified().toMSecsSinceEpoch())+QString::number(file.exists());}
    static thread_local QCache<QString,QImage> cache(16*1024*1024);if(const auto *cached=cache.object(signature))return *cached;
    if(auto *camera=scene.camera())camera->cam.orthoScale=qMax(4.,qMax(object.w,object.h*1.6)*1.3);
    QImage image(160,100,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);scene.render(painter,image.rect(),0);painter.end();cache.insert(signature,new QImage(image),int(image.sizeInBytes()));return image;
}
}
