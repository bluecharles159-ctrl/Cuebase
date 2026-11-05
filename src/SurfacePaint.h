#pragma once
#include <QImage>
#include <QJsonArray>

class BuildModel;
class QWidget;
namespace SurfacePaint {
void composite(QImage &surface, const QRect &bounds, const QJsonArray &layers);
void edit(QWidget *parent, BuildModel *model);
}
