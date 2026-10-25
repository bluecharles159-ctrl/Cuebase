#pragma once
#include "ElementDesign.h"
#include <QJsonObject>
#include <QSet>
class Library;
class BuildModel;
class QWidget;
class QDialog;
namespace Components {
QStringList groups();
QJsonObject defaultLinks();
QJsonArray entries(const Library &library);
QJsonObject entry(const Library &library,const QString &id);
QString registerSource(BuildModel &model,Library &library,const QString &objectId,const QString &name,QString *error);
bool publish(BuildModel &model,Library &library,QString *error);
QString insert(BuildModel &model,Library &library,const QString &id,QString *error);
bool metadata(Library &library,const QString &id,const QString &name,const QString &category,QString *error);
bool setLinks(BuildModel &model,const QJsonObject &links,QString *error);
QStringList overrides(const ElementDesign &scene,const DObject &object);
void duplicateLinks(ElementDesign &scene,const QString &originalId,const QString &copyId);
void pruneInstanceCues(ElementDesign &scene);
// Updates selected instances in the current editable scene, never saved/on-air copies.
int updateSelected(BuildModel &model,const Library &library,const QSet<QString> &groups,bool preserveOverrides,QString *error);
QImage thumbnail(const QJsonObject &entry);
QDialog *createBrowser(BuildModel *model,Library *library,QWidget *parent=nullptr);
}
