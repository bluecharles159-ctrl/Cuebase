#pragma once
#include "ElementDesign.h"
#include <QWidget>
class BuildModel;
struct SceneEffectControl {
    QString group, label, key;
    double minimum, maximum, initial, step;
};
const QVector<SceneEffectControl> &sceneEffectControls();
QStringList sceneEffectPresets();
ElementDesign sceneEffectPreset(const QString &name);
DObject *sceneEffect(ElementDesign &design);
const DObject *sceneEffect(const ElementDesign &design);
double sceneEffectValue(const ElementDesign &design,const QString &key,qint64 time,double fallback);
QImage sceneEffectSample(QSize size,bool alternate=false);
QImage sceneEffectPreview(const ElementDesign &design,QSize size,qint64 time);
QImage applySceneEffect(const QImage &source,const ElementDesign &design,qint64 time,bool layers=true);
QImage blendSceneEffect(const QImage &from,const QImage &to,const ElementDesign &design,qint64 time);
QWidget *sceneEffectEditor(BuildModel *model);
