#pragma once
#include "ElementDesign.h"
#include <functional>

struct DesignAsset {QString objectId,objectName,role,path;};
QList<DesignAsset> designAssets(const ElementDesign &design);
void resolveDesignAssets(ElementDesign &design,const QString &directory);
QString missingDesignAsset(const ElementDesign &design);
bool relinkDesignAsset(ElementDesign &design,const QString &previous,const QString &replacement,QString *error);
class BuildModel;
class QDialog;
class QWidget;
QDialog *createDesignMediaDialog(BuildModel *model,QWidget *parent=nullptr);
// Saves a composition and a uniquely named sibling media folder. The original
// composition and existing exports remain intact on cancellation or failure.
bool exportPortableDesign(const ElementDesign &design,const QString &file,QString *error,
                          const std::function<bool(int,int)> &progress={});
