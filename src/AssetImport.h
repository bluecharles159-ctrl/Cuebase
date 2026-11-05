#pragma once
#include "ElementDesign.h"
namespace AssetImport {
QVector<DObject> meshes(const QString &path,QString *error);
bool exportObj(const ElementDesign &scene,qint64 time,const QString &path,QString *error);
}
