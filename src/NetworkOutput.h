#pragma once
#include <QObject>
#include <QImage>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <functional>

struct StreamSettings {
    QString executable,endpoint;
    QSize size{1920,1080};
    double fps=25;
    int videoKbps=6000,audioKbps=160,reconnects=3;
};

class NetworkOutput:public QObject{
    Q_OBJECT
public:
    explicit NetworkOutput(QObject *parent=nullptr);
    ~NetworkOutput() override;
    void setFrameProvider(std::function<QImage(QSize)> provider){m_provider=std::move(provider);}
    void attachPlayer(QMediaPlayer *player,QAudioOutput *audio=nullptr);
    bool start(const StreamSettings &settings);
    void stop();
    bool active()const{return m_active;}
    bool streaming()const{return m_streaming;}
    QString status()const{return m_status;}
    static QString findEncoder();
    static QString validate(const StreamSettings &settings);
signals:
    void statusChanged(const QString &status,bool streaming);
private:
    struct Source{QPointer<QMediaPlayer> player;QPointer<QAudioOutput> output;QByteArray data;};
    QVector<Source> m_sources;
    StreamSettings m_settings;
    QProcess m_process;
    QTcpServer m_videoServer,m_audioServer;
    QPointer<QTcpSocket> m_video,m_audio;
    QTimer m_tick;
    QElapsedTimer m_clock,m_connectClock,m_progressClock,m_backpressureClock,m_serviceClock;
    qint64 m_frames=0,m_samples=0,m_epochFrames=1;
    bool m_active=false,m_streaming=false,m_clockStarted=false,m_stopping=false;
    int m_attempt=0,m_session=0;
    QString m_status,m_errors;
    QByteArray m_progress;
    std::function<QImage(QSize)> m_provider;
    void launch();
    void pump();
    void release();
    void fail(const QString &reason);
    void state(const QString &text,bool live=false);
    QByteArray mix(int frames);
    bool sendFrame();
};
