#include "DesignAssets.h"
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>

QList<DesignAsset> designAssets(const ElementDesign &design){
    QList<DesignAsset> assets;
    for(const auto &object:design.objects){
        if(!object.image.isEmpty())assets.append({object.id,object.name,"image",object.image});
        const auto audio=object.style["audio.file"].toString();
        if(!audio.isEmpty())assets.append({object.id,object.name,"audio",audio});
        const auto texture=object.style["texture.file"].toString();
        if(!texture.isEmpty())assets.append({object.id,object.name,"texture",texture});
    }
    return assets;
}
namespace {
void setAsset(ElementDesign &design,const DesignAsset &asset,const QString &path){
    if(auto *object=design.find(asset.objectId)){
        if(asset.role=="image")object->image=path;
        else if(asset.role=="texture")object->style["texture.file"]=path;
        else if(asset.role=="audio")object->style["audio.file"]=path;
    }
}
}
void resolveDesignAssets(ElementDesign &design,const QString &directory){
    for(const auto &asset:designAssets(design))if(QFileInfo(asset.path).isRelative())
        setAsset(design,asset,QDir(directory).absoluteFilePath(asset.path));
}
QString missingDesignAsset(const ElementDesign &design){
    for(const auto &asset:designAssets(design))if(!QFileInfo(asset.path).isFile())
        return QString("Missing %1 media in %2:\n%3").arg(asset.role,asset.objectName.isEmpty()?asset.objectId:asset.objectName,asset.path);
    return {};
}
bool relinkDesignAsset(ElementDesign &design,const QString &previous,const QString &replacement,QString *error){
    if(!QFileInfo(replacement).isFile()){if(error)*error="The replacement media file does not exist.";return false;}
    bool found=false;
    for(const auto &asset:designAssets(design))if(asset.path==previous){setAsset(design,asset,QFileInfo(replacement).absoluteFilePath());found=true;}
    if(error)*error=found?QString():QString("The original media reference no longer exists.");return found;
}
bool exportPortableDesign(const ElementDesign &source,const QString &file,QString *error,const std::function<bool(int,int)> &progress){
    auto fail=[&](const QString &message){if(error)*error=message;return false;};
    const auto missing=missingDesignAsset(source);if(!missing.isEmpty())return fail(missing);
    const QFileInfo destination(file);const auto parent=destination.absoluteDir();
    if(!parent.exists())return fail("The export directory does not exist.");
    QTemporaryDir staging(parent.filePath(".cuebase-export-XXXXXX"));
    if(!staging.isValid())return fail("Could not create a temporary export directory.");
    const QString folder=destination.completeBaseName()+".assets-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto stagedAssets=QDir(staging.path()).filePath("assets");
    if(!QDir().mkdir(stagedAssets))return fail("Could not create the media directory.");
    auto design=source;const auto assets=designAssets(source);QHash<QString,QString> copied;
    for(int index=0;index<assets.size();++index){
        if(progress&&!progress(index,assets.size()))return fail("Export cancelled; the existing composition was preserved.");
        const auto &asset=assets[index];const QFileInfo original(asset.path);const auto key=original.canonicalFilePath();
        const auto originalSize=original.size();const auto originalModified=original.lastModified();
        auto relative=copied.value(key);
        if(relative.isEmpty()){
            const auto name=QString::number(copied.size()+1)+"."+original.suffix();
            QFile input(asset.path);QSaveFile output(QDir(stagedAssets).filePath(name));
            if(!input.open(QIODevice::ReadOnly))return fail(input.errorString());
            if(!output.open(QIODevice::WriteOnly))return fail(output.errorString());
            while(!input.atEnd()){
                const auto bytes=input.read(1024*1024);
                if(input.error()!=QFile::NoError)return fail(input.errorString());
                if(output.write(bytes)!=bytes.size())return fail(output.errorString());
                if(progress&&!progress(index,assets.size()))return fail("Export cancelled; the existing composition was preserved.");
            }
            const QFileInfo after(asset.path);
            if(after.size()!=originalSize||after.lastModified()!=originalModified)return fail("Media changed during export: "+asset.path);
            if(!output.commit())return fail(output.errorString());
            relative=folder+"/"+name;copied.insert(key,relative);
        }
        setAsset(design,asset,relative);
    }
    if(progress&&!progress(assets.size(),assets.size()))return fail("Export cancelled; the existing composition was preserved.");
    QSaveFile composition(destination.absoluteFilePath());const auto bytes=design.toJson().toUtf8();
    if(!composition.open(QIODevice::WriteOnly)||composition.write(bytes)!=bytes.size())return fail(composition.errorString());
    const auto mediaPath=parent.filePath(folder);
    if(!assets.isEmpty()&&!QDir().rename(stagedAssets,mediaPath))return fail("Could not finalize the media directory.");
    if(!composition.commit()){
        // Only the unique directory created by this export is eligible for cleanup.
        if(!assets.isEmpty())QDir(mediaPath).removeRecursively();
        return fail(composition.errorString());
    }
    if(error)error->clear();return true;
}
