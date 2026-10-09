#include "NetworkOutput.h"
#include <QAudioBufferOutput>
#include <QAudioBuffer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QRegularExpression>
#include <QtEndian>
#include <cmath>
#include <algorithm>

QString NetworkOutput::findEncoder(){
    for(const QString &path:QStringList{QCoreApplication::applicationDirPath()+"/ffmpeg.exe",QStandardPaths::findExecutable("ffmpeg"),"C:/msys64/ucrt64/bin/ffmpeg.exe"})if(QFileInfo(path).isFile())return path;
    return {};
}
QString NetworkOutput::validate(const StreamSettings &s){
    if(!QFileInfo(s.executable).isFile())return "Choose an installed FFmpeg executable.";
    QUrl url(s.endpoint);if(!url.isValid()||url.host().isEmpty()||!QStringList{"rtmp","rtmps","srt"}.contains(url.scheme().toLower()))return "Enter a valid RTMP, RTMPS or SRT destination URL.";
    if(s.size.width()<16||s.size.height()<16||s.size.width()>7680||s.size.height()>4320||s.size.width()%2||s.size.height()%2)return "Output dimensions must be even and between 16×16 and 7680×4320.";
    if(!std::isfinite(s.fps)||s.fps<1||s.fps>120||s.videoKbps<100||s.videoKbps>100000||s.audioKbps<32||s.audioKbps>512)return "Check the output frame rate and bitrates.";
    return {};
}
NetworkOutput::NetworkOutput(QObject *parent):QObject(parent){
    m_tick.setTimerType(Qt::PreciseTimer);m_tick.setInterval(5);connect(&m_tick,&QTimer::timeout,this,&NetworkOutput::pump);
    connect(&m_videoServer,&QTcpServer::newConnection,this,[this]{if(m_video){m_videoServer.nextPendingConnection()->deleteLater();return;}m_video=m_videoServer.nextPendingConnection();m_video->setSocketOption(QAbstractSocket::LowDelayOption,1);if(sendFrame())m_frames=1;});
    connect(&m_audioServer,&QTcpServer::newConnection,this,[this]{m_audio=m_audioServer.nextPendingConnection();m_audio->setSocketOption(QAbstractSocket::LowDelayOption,1);if(m_video){m_clock.start();m_progressClock.restart();m_clockStarted=true;m_epochFrames=m_frames;m_samples=qRound64(m_frames*48000.0/m_settings.fps);m_audio->write(mix(int(m_samples)));}});
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{QString message=QString::fromUtf8(m_process.readAllStandardError());message.replace(m_settings.endpoint,"[destination]");message.replace(QRegularExpression("(?:rtmps?|srt)://[^\\s]+"),"[destination]");m_errors=(m_errors+message).right(6000);});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this]{m_progress+=m_process.readAllStandardOutput();int newline;
        while((newline=m_progress.indexOf('\n'))>=0){auto line=m_progress.left(newline).trimmed();m_progress.remove(0,newline+1);if(line.startsWith("frame=")&&line.mid(6).toLongLong()>0){m_progressClock.restart();if(!m_streaming){m_epochFrames=m_frames;m_clock.restart();state("Streaming · H.264 + AAC",true);}}}
    });
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart&&m_active&&!m_stopping)fail("Encoder could not start: "+m_process.errorString());});
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){if(m_active&&!m_stopping)fail(QString("Encoder stopped (%1). %2").arg(code).arg(m_errors.trimmed().right(1200)));});
    state("Network output stopped");
}
NetworkOutput::~NetworkOutput(){stop();}
void NetworkOutput::state(const QString &text,bool live){m_status=text;m_streaming=live;emit statusChanged(text,live);}
void NetworkOutput::attachPlayer(QMediaPlayer *player,QAudioOutput *audio){
    if(!player)return;
    m_sources.removeIf([](const auto &source){return source.player.isNull();});
    for(const auto &s:m_sources)if(s.player==player)return;
    m_sources.append({player,audio,{}});QAudioFormat format;format.setSampleRate(48000);format.setChannelCount(2);format.setSampleFormat(QAudioFormat::Int16);
    auto *tap=new QAudioBufferOutput(format,player);player->setAudioBufferOutput(tap);
    connect(tap,&QAudioBufferOutput::audioBufferReceived,this,[this,player](const QAudioBuffer &buffer){if(!m_active)return;
        for(auto &source:m_sources)if(source.player==player){if(!buffer.isValid()){return;}const auto format=buffer.format();
            if(format.sampleRate()!=48000||format.channelCount()!=2||format.sampleFormat()!=QAudioFormat::Int16){fail("The audio decoder did not supply 48 kHz stereo PCM. Check the selected Qt multimedia backend.");return;}
            source.data.append(buffer.constData<char>(),buffer.byteCount());if(source.data.size()>192000)source.data.remove(0,source.data.size()-192000);return;}
    });
    auto clear=[this,player]{for(auto &source:m_sources)if(source.player==player)source.data.clear();};
    connect(player,&QMediaPlayer::sourceChanged,this,[clear](const QUrl &){clear();});
    connect(player,&QMediaPlayer::playbackStateChanged,this,[clear](QMediaPlayer::PlaybackState state){if(state!=QMediaPlayer::PlayingState)clear();});
    connect(player,&QObject::destroyed,this,[this]{m_sources.removeIf([](const auto &source){return source.player.isNull();});});
}
bool NetworkOutput::start(const StreamSettings &settings){
    const auto error=validate(settings);if(!error.isEmpty()){state(error);return false;}if(!m_provider){state("No program picture is connected to network output.");return false;}
    stop();m_settings=settings;m_active=true;m_attempt=0;++m_session;launch();return true;
}
void NetworkOutput::launch(){
    release();if(!m_active)return;
    if(!m_videoServer.listen(QHostAddress::LocalHost,0)||!m_audioServer.listen(QHostAddress::LocalHost,0)){fail("Could not open local encoder transport.");return;}
    QStringList args{"-hide_banner","-loglevel","warning","-nostdin","-thread_queue_size","8","-analyzeduration","0","-probesize","32","-f","rawvideo","-pixel_format","bgra","-video_size",QString("%1x%2").arg(m_settings.size.width()).arg(m_settings.size.height()),"-framerate",QString::number(m_settings.fps,'g',12),"-i",QString("tcp://127.0.0.1:%1").arg(m_videoServer.serverPort()),
        "-thread_queue_size","32","-analyzeduration","0","-probesize","32","-f","s16le","-ar","48000","-ac","2","-i",QString("tcp://127.0.0.1:%1").arg(m_audioServer.serverPort()),"-map","0:v:0","-map","1:a:0","-c:v","libx264","-preset","veryfast","-tune","zerolatency","-pix_fmt","yuv420p","-b:v",QString::number(m_settings.videoKbps)+"k","-maxrate",QString::number(m_settings.videoKbps)+"k","-bufsize",QString::number(m_settings.videoKbps*2)+"k","-g",QString::number(qRound(m_settings.fps*2)),"-c:a","aac","-b:a",QString::number(m_settings.audioKbps)+"k","-ar","48000","-ac","2","-progress","pipe:1","-stats_period","0.5","-rw_timeout","15000000"};
    if(QUrl(m_settings.endpoint).scheme().startsWith("rtmp"))args<<"-flvflags"<<"no_duration_filesize"<<"-f"<<"flv";
    else args<<"-mpegts_flags"<<"resend_headers"<<"-f"<<"mpegts";
    args<<m_settings.endpoint;m_errors.clear();m_progress.clear();m_frames=0;m_samples=0;m_epochFrames=1;m_backpressureClock.invalidate();m_clockStarted=false;
    for(auto &source:m_sources)source.data.clear();
    state(m_attempt?QString("Reconnecting · attempt %1").arg(m_attempt):"Connecting to destination…");m_connectClock.start();m_progressClock.start();m_serviceClock.start();m_process.start(m_settings.executable,args);m_tick.start();
}
void NetworkOutput::release(){
    m_stopping=true;m_tick.stop();if(m_video){m_video->abort();m_video->deleteLater();m_video=nullptr;}if(m_audio){m_audio->abort();m_audio->deleteLater();m_audio=nullptr;}
    m_videoServer.close();m_audioServer.close();if(m_process.state()!=QProcess::NotRunning){m_process.terminate();if(!m_process.waitForFinished(1000)){m_process.kill();m_process.waitForFinished(1000);}}m_stopping=false;m_clockStarted=false;
}
void NetworkOutput::stop(){m_active=false;++m_session;release();for(auto &source:m_sources)source.data.clear();state("Network output stopped");}
void NetworkOutput::fail(const QString &reason){
    if(!m_active)return;release();
    if(m_attempt<m_settings.reconnects){++m_attempt;state(reason+QString(" · retry %1/%2 in 2 seconds").arg(m_attempt).arg(m_settings.reconnects));const int session=m_session;QTimer::singleShot(2000,this,[this,session]{if(m_active&&session==m_session)launch();});}
    else{m_active=false;state(reason+" · output stopped");}
}
QByteArray NetworkOutput::mix(int frames){
    QByteArray result(frames*4,0);auto *out=reinterpret_cast<qint16*>(result.data());QVector<float> sum(frames*2,0);
    for(auto &source:m_sources){if(!source.player||source.player->playbackState()!=QMediaPlayer::PlayingState){source.data.clear();continue;}
        double gain=source.output?(source.output->isMuted()?0:source.output->volume()):1;int samples=qMin(frames*2,int(source.data.size()/2));const auto *data=reinterpret_cast<const uchar*>(source.data.constData());
        for(int i=0;i<samples;++i)sum[i]+=qFromLittleEndian<qint16>(data+i*2)*gain;source.data.remove(0,samples*2);
    }
    for(int i=0;i<sum.size();++i)out[i]=qToLittleEndian(qint16(qBound(-32768,qRound(sum[i]),32767)));return result;
}
bool NetworkOutput::sendFrame(){
    if(!m_video)return false;
    const qint64 frameBytes=qint64(m_settings.size.width())*m_settings.size.height()*4;
    if(m_video->bytesToWrite()>frameBytes*2){
        if(!m_backpressureClock.isValid())m_backpressureClock.start();
        if(m_streaming&&m_backpressureClock.elapsed()>2000)fail("Encoder or connection cannot keep up with the output. Lower resolution, frame rate or bitrate.");
        return false;
    }
    m_backpressureClock.invalidate();QImage frame=m_provider(m_settings.size);if(frame.isNull()){fail("The program renderer returned an empty frame.");return false;}
    if(frame.size()!=m_settings.size)frame=frame.scaled(m_settings.size);frame=frame.convertToFormat(QImage::Format_ARGB32);
    const qint64 bytes=qint64(frame.bytesPerLine())*frame.height();
    if(m_video->write(reinterpret_cast<const char*>(frame.constBits()),bytes)<0){fail("The encoder video connection closed. "+m_errors.right(1600));return false;}return true;
}
void NetworkOutput::pump(){
    if(!m_active)return;
    const auto serviceGap=m_serviceClock.restart();
    // First-use codec discovery can block Qt's event loop. Allow socket callbacks
    // to run after that pause instead of declaring a connection failure immediately.
    if(!m_streaming&&serviceGap>2000){m_connectClock.restart();m_progressClock.restart();if(m_clockStarted){m_clock.restart();m_epochFrames=m_frames;}}
    if(!m_clockStarted){
        // FFmpeg can probe several raw frames before opening its second input.
        // Keep that handshake supplied, with a bounded number of queued frames.
        if(m_video&&!m_audio&&m_frames<8&&m_connectClock.elapsed()>=m_frames*100){if(sendFrame())++m_frames;}
        if(m_connectClock.elapsed()>30000)fail(QString("Encoder input initialization timed out (video %1, audio %2). ").arg(m_video?"connected":"not connected",m_audio?"connected":"not connected")+m_errors.right(800));return;}
    if(m_progressClock.elapsed()>(m_streaming?20000:30000)){fail("Encoder stopped reporting output progress. "+m_errors.right(800));return;}
    qint64 due=qint64(m_clock.elapsed()*m_settings.fps/1000.0)+m_epochFrames;
    if(m_streaming&&due-m_frames>qMax(5.0,m_settings.fps)){fail("The program renderer cannot maintain the configured frame rate. Reduce the stream resolution or frame rate.");return;}
    // Audio and video are separate FFmpeg inputs. Probing one must not wait for
    // the other socket to drain, or startup can deadlock under backpressure.
    const qint64 targetSamples=qRound64(due*48000.0/m_settings.fps);
    if(m_audio&&targetSamples>m_samples){
        if(m_audio->bytesToWrite()>384000){fail("Encoder audio transport stalled.");return;}
        const int count=int(qMin<qint64>(targetSamples-m_samples,4800));
        if(m_audio->write(mix(count))<0){fail("The encoder audio connection closed.");return;}
        m_samples+=count;
    }
    for(int n=0;n<3&&m_frames<due;++n){if(!sendFrame())return;++m_frames;}
}
