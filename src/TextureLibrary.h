#pragma once
#include <QJsonArray>
#include <QImage>
#include "ElementDesign.h"
class Library;
class BuildModel;
class QWidget;

namespace TextureLibrary {
inline constexpr auto mimeType="application/x-cuebase-texture-id";
QJsonArray entries(const Library &library);
QJsonObject entry(const Library &library,const QString &id);
QString file(const QJsonObject &entry);
QImage thumbnail(const QJsonObject &entry);
// Imported files are copied into this profile; callers keep their originals.
QString importFile(Library &library,const QString &path,QString *error);
QString savePreset(Library &library,const DObject &source,const QString &name,QString *error);
bool update(Library &library,const QString &id,const QString &name,const QString &collection,bool favorite,QString *error);
bool relink(Library &library,const QString &id,const QString &path,QString *error);
bool remove(Library &library,const QString &id,QString *error);
bool exportEntry(const Library &library,const QString &id,const QString &path,QString *error);
bool compatible(const DObject &object);
bool apply(BuildModel &model,const Library &library,const QString &id,const QString &target={},QString *error=nullptr);
QWidget *createPanel(BuildModel *model,Library *library,QWidget *parent=nullptr);
}
