#pragma once
#include "Data.h"
#include <QObject>
#include <QImage>
#include <QElapsedTimer>
#include <QTimer>
class QWidget;class QMediaPlayer;class QAudioOutput;class QVideoSink;class DesignAudio;class Playout;class NetworkOutput;
class ContinuityOutput:public QObject{
    Q_OBJECT
public:
    explicit ContinuityOutput(Library *library,QObject *parent=nullptr);
    bool active()const{return enabled;}
    void setActive(bool value);
    bool configure(QString mode,QString reference,QString file,QString message,QString *error=nullptr);
    QImage frame(QSize size);
    void setAudio(double volume,bool muted);
    QString mediaError()const;
    QString mode()const{return kind;}
signals:
    void changed();
    void audioSourceCreated(QMediaPlayer *player,QAudioOutput *output);
private:
    Library *lib;bool enabled=false;QString kind="Black",reference,media,message;QImage video;QMediaPlayer *player;QAudioOutput *audio;QVideoSink *sink;DesignAudio *designAudio;QElapsedTimer clock;QTimer timer;double volume=.8;bool muted=false;
    Element selectedElement()const;
    void syncAudio();
};
QWidget *continuityControls(Library *library,Playout *play,NetworkOutput *output);
