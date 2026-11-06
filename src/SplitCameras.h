#pragma once
#include <QHash>
#include <QImage>
#include <QObject>
#include <QTimer>
class SplitCameras final : public QObject {
public:
    using QObject::QObject;
    QImage frame(const QByteArray &device);
private:
    struct Feed {QObject *owner;QTimer *idle;QImage image;};
    QHash<QByteArray,Feed> feeds;
};
