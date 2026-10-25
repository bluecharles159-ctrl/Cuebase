#pragma once
#include <QPair>
#include <QVector>
#include <QString>
class BuildModel;
class QWidget;
void editCurveNoise(BuildModel *model,const QVector<QPair<QString,QString>> &channels,QWidget *parent);
