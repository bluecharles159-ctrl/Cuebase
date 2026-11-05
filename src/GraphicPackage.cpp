#include "GraphicPackage.h"
#include "CueSequence.h"
#include "DesignAssets.h"
#include "ElementLibrary.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {
constexpr auto magic="CUEGRAPHIC1\n";
constexpr int headerBytes=12;
constexpr int maxJson=64*1024*1024;
using Remap=std::function<QString(const QString &)>;

bool mapScene(QString &json,ElementDesign &design,const Remap &remap) {
    const auto parsed=QJsonDocument::fromJson(json.toUtf8());
    if (!parsed.object()["objects"].isArray()) return false;
    design=ElementDesign::fromJson(json);
    for (const auto &asset:designAssets(design)) {
        const auto path=remap(asset.path);
        if (path.isEmpty()) return false;
        setDesignAsset(design,asset,path);
    }
    json=design.toJson();
    return true;
}

bool mapBoard(QJsonArray &board,const ElementDesign &parent,const Remap &remap,int depth) {
    if (depth>8) return false;
    for (int i=0;i<board.size();++i) {
        auto entry=board[i].toObject();
        auto sections=entry["sections"].toArray();
        for (int j=0;j<sections.size();++j) {
            auto section=sections[j].toObject();
            auto design=parent;
            auto json=section["design"].toString();
            if (!json.isEmpty()) {
                if (!mapScene(json,design,remap)) return false;
                section["design"]=json;
            }
            auto fields=section["fields"].toObject();
            for(auto it=fields.begin();it!=fields.end();++it) {
                const auto *o=design.find(it.key());
                if (!o || o->surfaceType()!="Image" || it.value().toString().isEmpty()) continue;
                const auto path=remap(it.value().toString());
                if(path.isEmpty()) return false;
                it.value()=path;
            }
            if(section.contains("fields")) section["fields"]=fields;
            if(section.contains("board")) {
                auto child=section["board"].toArray();
                if(!mapBoard(child,design,remap,depth+1)) return false;
                section["board"]=child;
            }
            sections[j]=section;
        }
        if (entry.contains("sections")) entry["sections"]=sections;
        board[i]=entry;
    }
    return true;
}
bool mapGraphic(Element &element,const Remap &remap) {
    ElementDesign scene;
    return mapScene(element.design,scene,remap) && mapBoard(element.cueBoard,scene,remap,0);
}
// Media runs directly between files, including large program clips.
bool transfer(QIODevice &in,QIODevice &out,qint64 size,QCryptographicHash &hash) {
    while(size>0) {
        const auto bytes=in.read(qMin<qint64>(size,1024*1024));
        if(bytes.isEmpty() || out.write(bytes)!=bytes.size()) return false;
        hash.addData(bytes); size-=bytes.size();
    }
    return true;
}
}

bool exportGraphicPackage(const Element &source,const QString &path,QString *error) {
    auto fail=[&](QString message){if(error)*error=message;return false;};
    const auto checked=compileCueBoard(ElementDesign::fromJson(source.design),source.cueBoard,source.cueBoardSettings);
    if(!checked.error.isEmpty()) return fail(checked.error);
    QStringList files;
    QHash<QString,QString> names;
    QJsonArray assets;
    Element element=source;
    QString missing;
    auto pack=[&](const QString &path)->QString {
        const QFileInfo info(path);
        if(!info.isFile()) {missing=path;return {};}
        const auto key=info.canonicalFilePath();
        if(names.contains(key)) return names[key];
        auto suffix=info.suffix().toLower();
        if(!QRegularExpression("^[a-z0-9]{1,12}$").match(suffix).hasMatch()) suffix="bin";
        const auto name=QString("asset-%1.%2").arg(files.size()).arg(suffix);
        names.insert(key,name); files.append(key);
        assets.append(QJsonObject{{"name",name},{"bytes",QString::number(info.size())}});
        return name;
    };
    if(!mapGraphic(element,pack)) return fail(missing.isEmpty() ? "Invalid graphic or nested board." : "Missing media: "+missing);
    const auto manifest=QJsonDocument(QJsonObject{{"name",source.name},{"type",source.type},{"playlist",source.playlist},
        {"description",source.description},{"seconds",source.seconds},{"previewMs",double(source.previewMs)},{"fx",fxToJson(source.fx)},
        {"crop",QJsonArray{source.previewX,source.previewY,source.previewW,source.previewH}},
        {"design",element.design},{"board",element.cueBoard},{"settings",bakeCueBoard(source)["settings"]},{"assets",assets}}).toJson(QJsonDocument::Compact);
    if(manifest.size()>maxJson || files.size()>8192) return fail("This graphic exceeds the package limits.");
    QSaveFile output(path);
    if(!output.open(QIODevice::WriteOnly)) return fail(output.errorString());
    QDataStream stream(&output);
    stream.writeRawData(magic,headerBytes); stream<<quint32(manifest.size());
    if(stream.status()!=QDataStream::Ok || output.write(manifest)!=manifest.size()) return fail(output.errorString());
    for(int i=0;i<files.size();++i) {
        QFile file(files[i]);
        const auto size=assets[i].toObject()["bytes"].toString().toLongLong();
        if(!file.open(QIODevice::ReadOnly)) return fail(file.errorString());
        const auto modified=QFileInfo(file).lastModified();
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if(file.size()!=size || !transfer(file,output,size,hash) || !file.atEnd() || QFileInfo(file).lastModified()!=modified)
            return fail("Could not collect unchanged media: "+files[i]);
        if(output.write(hash.result())!=32) return fail(output.errorString());
    }
    return output.commit() || fail(output.errorString());
}

QString importGraphicPackage(Library &library,const QString &path,QString *error) {
    auto fail=[&](QString message)->QString{if(error)*error=message;return {};};
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return fail(file.errorString());
    if(file.read(headerBytes)!=magic) return fail("Unrecognized graphic package.");
    QDataStream stream(&file); quint32 length=0; stream>>length;
    if(stream.status()!=QDataStream::Ok || length>maxJson || length>file.bytesAvailable()) return fail("Invalid graphic manifest length.");
    const auto manifest=QJsonDocument::fromJson(file.read(length)).object();
    if(!manifest["assets"].isArray() || !manifest["design"].isString() || manifest["assets"].toArray().size()>8192)
        return fail("Invalid graphic manifest.");
    const auto root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/graphic-media";
    if(!QDir().mkpath(root)) return fail("Could not create the graphic media library.");
    QTemporaryDir imported(root+"/graphic-XXXXXX");
    if(!imported.isValid()) return fail(imported.errorString());
    QHash<QString,QString> paths;
    static const QRegularExpression assetName("^asset-[0-9]+\\.[a-z0-9]{1,12}$");
    for(const auto &value:manifest["assets"].toArray()) {
        const auto asset=value.toObject(); const auto name=asset["name"].toString();
        bool ok=false;const auto size=asset["bytes"].toString().toLongLong(&ok);
        if(!ok || size<0 || size>file.bytesAvailable()-32 || !assetName.match(name).hasMatch() || paths.contains(name))
            return fail("Invalid media entry in graphic package.");
        const auto destination=imported.filePath(name);
        QFile output(destination);QCryptographicHash hash(QCryptographicHash::Sha256);
        if(!output.open(QIODevice::WriteOnly|QIODevice::NewOnly) || !transfer(file,output,size,hash) || !output.flush())
            return fail("Could not unpack graphic media.");
        if(file.read(32)!=hash.result()) return fail("Graphic media failed its checksum.");
        paths.insert(name,destination);
    }
    if(!file.atEnd()) return fail("Unexpected data after graphic media.");
    Element e;e.id=Library::newId();e.name=manifest["name"].toString();e.type=manifest["type"].toString();
    if(e.type!="Custom" && e.type!="Effect") return fail("Unsupported graphic type.");
    e.playlist=manifest["playlist"].toString("Overlays");e.description=manifest["description"].toString();
    e.seconds=qMax(1,manifest["seconds"].toInt());e.previewMs=qMax<qint64>(0,manifest["previewMs"].toDouble());
    e.fx=fxFromJson(manifest["fx"].toObject());
    const auto crop=manifest["crop"].toArray();
    if(crop.size()==4){e.previewX=crop[0].toDouble();e.previewY=crop[1].toDouble();e.previewW=crop[2].toDouble();e.previewH=crop[3].toDouble();}
    e.design=manifest["design"].toString();e.cueBoard=manifest["board"].toArray();e.cueBoardSettings=manifest["settings"].toObject();
    if(!mapGraphic(e,[&](const QString &name){return paths.value(name);})) return fail("The graphic references missing media or an invalid scene.");
    const auto plan=compileCueBoard(ElementDesign::fromJson(e.design),e.cueBoard,e.cueBoardSettings);
    if(!plan.error.isEmpty()) return fail(plan.error);
    refreshElementThumbnail(e);
    const auto playlists=library.elementPlaylists;
    if(!library.elementPlaylists.contains(e.playlist)) library.elementPlaylists.append(e.playlist);
    library.elements.append(e);
    if(!library.saveChecked(error)){library.elements.removeLast();library.elementPlaylists=playlists;return {};}
    if(!paths.isEmpty()) imported.setAutoRemove(false);
    emit library.elementsChanged();return e.id;
}
