#include "SplitCameras.h"
#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoSink>
#include <QVideoFrame>

QImage SplitCameras::frame(const QByteArray &device){
    auto it=feeds.find(device);
    if(it==feeds.end()){
        auto *owner=new QObject(this);
        auto *timeout=new QTimer(owner);timeout->setSingleShot(true);
        it=feeds.insert(device,{owner,timeout,{}});
        connect(timeout,&QTimer::timeout,this,[this,device,owner]{feeds.remove(device);owner->deleteLater();});
        const auto devices=QMediaDevices::videoInputs();
        for(const auto &input:devices)if(input.id()==device){
            auto *camera=new QCamera(input,owner);auto *session=new QMediaCaptureSession(owner);
            auto *sink=new QVideoSink(owner);session->setCamera(camera);session->setVideoSink(sink);
            connect(sink,&QVideoSink::videoFrameChanged,this,[this,device,owner](const QVideoFrame &frame){
                auto feed=feeds.find(device);if(feed!=feeds.end() && feed->owner==owner)feed->image=frame.toImage();
            });
            camera->start();break;
        }
    }
    it->idle->start(1500);
    return it->image;
}
