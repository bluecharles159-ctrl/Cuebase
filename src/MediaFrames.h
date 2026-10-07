#pragma once
#include <QImage>
#include <QString>
namespace MediaFrames {
QImage frame(const QString &file,qint64 milliseconds,const QString &instance);
qint64 duration(const QString &file,QString *error=nullptr);
}
