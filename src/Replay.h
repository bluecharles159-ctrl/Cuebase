#pragma once
#include "Playout.h"
#include <QImage>
class QWidget;

class Replay final : public QObject {
    Q_OBJECT
public:
    enum Phase { Off, Enter, Running, Leave };
    explicit Replay(Playout *play,Library *library);
    bool start(qint64 from,qint64 to,const QString &entry,const QString &exit,bool pause,QString *error=nullptr);
    void returnToProgram();
    void cancel();
    bool active()const{return phase!=Off;}
    // Independent replay controls: they act on the replay only and never touch
    // the program's own transport.
    void setPaused(bool value);
    bool paused()const{return m_paused;}
    void setSpeed(double value);
    double speed()const{return m_speed;}
    void seekTo(qint64 positionMs);
    qint64 rangeStart()const{return rangeFrom;}
    qint64 rangeEnd()const{return end;}
    qint64 segmentElapsed()const{return index>=0?position-starts[index]:0;}
    Phase phase=Off;
    qint64 position=0,elapsed=0,phaseTime=0;
    Segment segment;
    QImage frame;
    ElementDesign entryEffect,exitEffect;
    QImage composite(const QImage &program,const QImage &replay)const;
signals:
    void changed();
    void failed(const QString &reason);
private:
    void tick();
    void sync();
    Playout *play;Library *library;
    QList<Segment> segments;QVector<qint64> starts;
    qint64 end=0,last=0,pending=-1,lastReady=0,rangeFrom=0;
    int index=-1;bool resume=false;
    bool m_paused=false;double m_speed=1.;
    QElapsedTimer clock;QTimer timer;
    QMediaPlayer *player;QAudioOutput *audio;QVideoSink *sink;
    DesignAudio *layers;
    QHash<QString,QPair<QString,ElementDesign>> audioDesigns;
};

bool showReplayDialog(Playout *play,Library *library,qint64 from,qint64 to,QWidget *parent);
