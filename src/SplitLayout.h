#pragma once
#include <QJsonArray>
#include <QImage>
#include <QPainter>
#include <functional>
class Library;
class QWidget;
namespace SplitLayout {
QStringList layouts();
QStringList transitions();
int paneCount(const QString &layout);
QString validate(const QJsonArray &shots);
using Source=std::function<QImage(const QString&,QSize)>;
bool draw(QPainter &p,const QRect &rect,const QJsonArray &shots,qint64 time,const Source &source);
void edit(QWidget *parent,Library *library,const QString &programId,qint64 time);
}
