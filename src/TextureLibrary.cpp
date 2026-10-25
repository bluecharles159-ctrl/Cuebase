#include "TextureLibrary.h"
#include "BuildModel.h"
#include "Data.h"
#include "DesignAssets.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace TextureLibrary {
namespace {
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
QImage readImage(const QString &path,int limit){
    QImageReader reader(path);reader.setAutoTransform(true);
    const auto size=reader.size();if(size.isValid()&&(size.width()>limit||size.height()>limit))reader.setScaledSize(size.scaled(limit,limit,Qt::KeepAspectRatio));
    return reader.read();
}
bool commit(Library &library,const QJsonArray &items,QString *error){
    const auto before=library.production;
    library.production["textureLibrary"]=QJsonObject{{"version",1},{"entries",items}};
    if(!library.saveChecked(error)){library.production=before;return false;}
    emit library.productionChanged();if(error)error->clear();return true;
}
QString managedCopy(const QString &path,QString *error){
    if(readImage(path,128).isNull()){fail(error,"This file could not be decoded as an image.");return {};}
    const QString folder=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/textures";
    if(!QDir().mkpath(folder)){fail(error,"Could not create the texture storage folder.");return {};}
    const auto destination=folder+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"."+QFileInfo(path).suffix();
    QFile input(path);if(!input.copy(destination)){fail(error,input.errorString());return {};}
    if(readImage(destination,128).isNull()){QFile::remove(destination);fail(error,"The copied texture could not be decoded; import was cancelled.");return {};}
    return destination;
}
bool appearanceKey(const QString &key){return key.startsWith("texture.")||key.startsWith("fill.");}
DObject appearance(const DObject &source){
    DObject result;result.id="surface";result.type="Shape";result.name=source.name;result.w=4;result.h=3;result.mat=source.mat;
    for(auto it=source.style.begin();it!=source.style.end();++it)if(appearanceKey(it.key()))result.style[it.key()]=it.value();
    const auto background=source.style["background.type"].toString(source.style["fill.kind"].toString("Solid")=="Solid"?"Solid":"Gradient");
    result.style["background.type"]=background=="Gradient"?"Gradient":"Solid";return result;
}
QString serialize(const DObject &object){ElementDesign design;design.objects.append(object);return design.toJson();}
DObject surface(const QJsonObject &item){const auto design=ElementDesign::fromJson(item["surface"].toString());for(const auto &object:design.objects)if(compatible(object))return appearance(object);return appearance(DObject());}
QString append(Library &library,QJsonObject item,const QString &createdFile,QString *error){
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);item["id"]=id;item["collection"]="Unsorted";item["favorite"]=false;
    auto items=entries(library);items.append(item);
    if(!commit(library,items,error)){if(!createdFile.isEmpty())QFile::remove(createdFile);return {};}
    return id;
}
}
QJsonArray entries(const Library &library){return library.production["textureLibrary"].toObject()["entries"].toArray();}
QJsonObject entry(const Library &library,const QString &id){for(auto value:entries(library)){const auto item=value.toObject();if(item["id"].toString()==id)return item;}return {};}
QString file(const QJsonObject &item){return surface(item).style["texture.file"].toString();}
QImage thumbnail(const QJsonObject &item){
    const auto path=file(item);if(!path.isEmpty()&&(item["kind"].toString()=="Texture"||!QFileInfo::exists(path)))return readImage(path,160);
    ElementDesign design;return design.textureFor(surface(item)).scaled(160,120,Qt::KeepAspectRatio,Qt::SmoothTransformation);
}
QString importFile(Library &library,const QString &path,QString *error){
    DObject object;QString kind="Texture",name=QFileInfo(path).completeBaseName(),copied;
    if(QFileInfo(path).suffix().compare("cuemat",Qt::CaseInsensitive)==0){
        QFile input(path);if(!input.open(QIODevice::ReadOnly)){fail(error,input.errorString());return {};}
        if(input.size()>4*1024*1024){fail(error,"This material preset is too large.");return {};}
        auto design=ElementDesign::fromJson(QString::fromUtf8(input.readAll()));
        design.objects.erase(std::remove_if(design.objects.begin(),design.objects.end(),[](const DObject &object){return object.type=="Camera";}),design.objects.end());
        if(design.objects.size()!=1||!compatible(design.objects.first())){fail(error,"This is not a Cuebase surface material preset.");return {};}
        resolveDesignAssets(design,QFileInfo(path).absolutePath());object=appearance(design.objects.first());kind="Preset";
        const auto texture=object.style["texture.file"].toString();
        if(!texture.isEmpty()){copied=managedCopy(texture,error);if(copied.isEmpty())return {};object.style["texture.file"]=copied;}
    }else{
        copied=managedCopy(path,error);if(copied.isEmpty())return {};
        object=appearance(object);object.style["texture.file"]=copied;
    }
    object.name=name;return append(library,{{"name",name},{"kind",kind},{"surface",serialize(object)}},copied,error);
}
QString savePreset(Library &library,const DObject &source,const QString &name,QString *error){
    if(!compatible(source)||name.trimmed().isEmpty()){fail(error,"Select a text, shape or image and enter a preset name.");return {};}
    auto object=appearance(source);QString copied;const auto path=object.style["texture.file"].toString();
    if(!path.isEmpty()){copied=managedCopy(path,error);if(copied.isEmpty())return {};object.style["texture.file"]=copied;}
    object.name=name.trimmed();return append(library,{{"name",object.name},{"kind","Preset"},{"surface",serialize(object)}},copied,error);
}
bool update(Library &library,const QString &id,const QString &name,const QString &collection,bool favorite,QString *error){
    if(name.trimmed().isEmpty())return fail(error,"Enter a name for this library item.");
    auto items=entries(library);for(int i=0;i<items.size();++i){auto item=items[i].toObject();if(item["id"].toString()!=id)continue;
        item["name"]=name.trimmed();item["collection"]=collection.trimmed().isEmpty()?"Unsorted":collection.trimmed();item["favorite"]=favorite;items[i]=item;return commit(library,items,error);}
    return fail(error,"This library item no longer exists.");
}
bool relink(Library &library,const QString &id,const QString &path,QString *error){
    auto items=entries(library);for(int i=0;i<items.size();++i){auto item=items[i].toObject();if(item["id"].toString()!=id)continue;
        const auto copied=managedCopy(path,error);if(copied.isEmpty())return false;
        auto object=surface(item);object.style["texture.file"]=copied;item["surface"]=serialize(object);items[i]=item;
        if(commit(library,items,error))return true;QFile::remove(copied);return false;}
    return fail(error,"This library item no longer exists.");
}
bool remove(Library &library,const QString &id,QString *error){
    auto items=entries(library);for(int i=0;i<items.size();++i)if(items[i].toObject()["id"].toString()==id){items.removeAt(i);return commit(library,items,error);}
    return fail(error,"This library item no longer exists.");
}
bool exportEntry(const Library &library,const QString &id,const QString &path,QString *error){
    const auto item=entry(library,id);if(item.isEmpty())return fail(error,"This library item no longer exists.");
    ElementDesign design;design.objects.append(surface(item));return exportPortableDesign(design,path,error);
}
bool compatible(const DObject &object){return object.type=="Shape"||object.type=="Text"||object.type=="Image";}
bool apply(BuildModel &model,const Library &library,const QString &id,const QString &target,QString *error){
    const auto item=entry(library,id);if(item.isEmpty())return fail(error,"This library item no longer exists.");
    const auto source=surface(item);const auto path=file(item);
    if(!path.isEmpty()&&readImage(path,32).isNull())return fail(error,"The texture is missing or unreadable. Relink it in Materials first.");
    QVector<DObject*> targets=target.isEmpty()?model.selected():QVector<DObject*>{model.byId(target)};
    targets.erase(std::remove_if(targets.begin(),targets.end(),[](const DObject *object){return !object||object->locked||!compatible(*object);}),targets.end());
    if(targets.isEmpty())return fail(error,"Select an unlocked text, shape or image to apply this material.");
    if(model.editMode)return fail(error,"Exit Edit Mode before applying a material.");
    model.pushUndo();for(auto *object:targets){
        if(item["kind"].toString()=="Preset"){
            for(const auto &key:object->style.keys())if(appearanceKey(key))object->style.remove(key);
            for(auto it=source.style.begin();it!=source.style.end();++it)if(appearanceKey(it.key()))object->style[it.key()]=it.value();
            object->mat=source.mat;
        }else {object->style["texture.file"]=path;object->style["texture.enabled"]=true;}
        if(object->type!="Text")object->style["background.type"]=item["kind"].toString()=="Preset"?source.style["background.type"]:QJsonValue("Solid");
    }
    model.notify();if(error)error->clear();return true;
}
}
