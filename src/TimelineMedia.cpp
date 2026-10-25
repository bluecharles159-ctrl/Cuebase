#include "TimelineMedia.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtEndian>
#include <cmath>
#include <cstring>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

TimelineMedia::TimelineMedia(QObject *parent):QObject(parent){
#ifdef Q_OS_WIN
    m_process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args){args->flags|=CREATE_NO_WINDOW|BELOW_NORMAL_PRIORITY_CLASS;});
#endif
    m_timeout.setSingleShot(true);
    connect(&m_timeout,&QTimer::timeout,this,[this]{setProperty("lastError",QString("Media preview timed out."));m_process.kill();});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,&TimelineMedia::read);
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{const auto message=QString::fromUtf8(m_process.readAllStandardError()).trimmed();if(!message.isEmpty())setProperty("lastError",message.right(2048));});
    connect(&m_process,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){read();finish(code==0&&status==QProcess::NormalExit);});
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){setProperty("lastError",m_process.errorString());finish(false);}});
}
TimelineMedia::~TimelineMedia(){m_process.disconnect(this);m_process.kill();m_process.waitForFinished(1000);}
QString TimelineMedia::fileKey(const QString &file)const{
    const QFileInfo info(file);if(!info.isFile())return {};
    return info.absoluteFilePath()+"|"+QString::number(info.size())+"|"+QString::number(info.lastModified().toMSecsSinceEpoch());
}
QImage TimelineMedia::thumbnail(const QString &file,qint64 sourceMs){
    const auto base=fileKey(file);if(base.isEmpty())return {};const auto time=qMax<qint64>(0,sourceMs)/500*500;
    const auto key=base+"|v|"+QString::number(time);if(auto *image=m_images.object(key))return *image;
    enqueue({file,key,time,0,false});return {};
}
TimelineWave TimelineMedia::waveform(const QString &file,qint64 startMs,qint64 durationMs){
    const auto base=fileKey(file);if(base.isEmpty()||durationMs<=0)return {};
    const qint64 start=qMax<qint64>(0,startMs)/1000*1000,end=(qMax<qint64>(0,startMs)+durationMs+999)/1000*1000;
    const auto key=base+"|a|"+QString::number(start)+"|"+QString::number(end);
    if(auto *wave=m_waves.object(key))return *wave;
    enqueue({file,key,start,qMax<qint64>(1000,end-start),true});return {};
}
void TimelineMedia::enqueue(Job job){
    if(m_pending.contains(job.key)||m_failed.contains(job.key)||m_queue.size()>=32)return;
    m_pending.insert(job.key);m_queue.enqueue(job);if(!m_busy)QTimer::singleShot(0,this,&TimelineMedia::next);
}
void TimelineMedia::next(){
    if(m_busy||m_queue.isEmpty())return;
    m_job=m_queue.dequeue();m_busy=true;m_bytes.clear();m_samples=0;m_peaks=QVector<float>(512,0);setProperty("lastError",QString());
    QString executable;
    for(const auto &path:QStringList{QCoreApplication::applicationDirPath()+"/ffmpeg.exe",QStandardPaths::findExecutable("ffmpeg"),"C:/msys64/ucrt64/bin/ffmpeg.exe"})if(QFileInfo(path).isFile()){executable=path;break;}
    if(executable.isEmpty()){finish(false);return;}
    QStringList args{"-hide_banner","-loglevel","error","-nostdin","-threads","1","-ss",QString::number(m_job.start/1000.,'f',3),"-i",m_job.file};
    if(m_job.audio)args<<"-map"<<"0:a:0"<<"-t"<<QString::number(m_job.duration/1000.,'f',3)<<"-vn"<<"-af"<<"aresample=async=1:first_pts=0"<<"-ac"<<"2"<<"-ar"<<"8000"<<"-c:a"<<"pcm_f32le"<<"-f"<<"f32le"<<"pipe:1";
    else args<<"-map"<<"0:v:0"<<"-frames:v"<<"1"<<"-an"<<"-vf"<<"scale=96:54:force_original_aspect_ratio=decrease,pad=96:54:(ow-iw)/2:(oh-ih)/2"<<"-threads"<<"1"<<"-c:v"<<"png"<<"-f"<<"image2pipe"<<"pipe:1";
    // Cold decoder startup can be slow on low-memory machines; keep it off the UI thread.
    m_process.start(executable,args);m_timeout.start(m_job.audio?120000:60000);
}
void TimelineMedia::read(){
    m_bytes+=m_process.readAllStandardOutput();
    if(!m_job.audio){if(m_bytes.size()>1024*1024)m_process.kill();return;}
    const qsizetype complete=m_bytes.size()/4*4;
    for(qsizetype i=0;i<complete;i+=4){const quint32 bits=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(m_bytes.constData()+i));float sample;std::memcpy(&sample,&bits,4);
        const auto bin=qMin<qint64>(511,(m_samples/2)*512/qMax<qint64>(1,m_job.duration*8));
        if(std::isfinite(sample))m_peaks[int(bin)]=qMax(m_peaks[int(bin)],qMin(1.f,std::abs(sample)));++m_samples;
    }
    m_bytes.remove(0,complete);
}
void TimelineMedia::finish(bool success){
    if(!m_busy)return;m_timeout.stop();const auto key=m_job.key;
    if(qEnvironmentVariableIsSet("CUEBASE_PREVIEW_TRACE"))qInfo()<<"Timeline preview"<<m_job.start<<m_job.duration<<m_job.audio<<success<<m_bytes.size()<<m_samples<<property("lastError");
    if(key.isEmpty()){m_busy=false;m_bytes.clear();emit updated();QTimer::singleShot(0,this,&TimelineMedia::next);return;}
    if(success&&m_job.audio&&m_samples>0)m_waves.insert(key,new TimelineWave{m_job.start,m_job.duration,m_peaks},3);
    else if(success&&!m_job.audio){auto image=QImage::fromData(m_bytes,"PNG");if(image.isNull())success=false;else m_images.insert(key,new QImage(image),qMax(1,int(image.sizeInBytes()/1024)));}
    else success=false;
    if(!success){if(m_failed.size()>1024)m_failed.clear();m_failed.insert(key);
        if(property("lastError").toString()=="Media preview timed out.")QTimer::singleShot(30000,this,[this,key]{m_failed.remove(key);emit updated();});}
    m_pending.remove(key);m_busy=false;m_bytes.clear();emit updated();QTimer::singleShot(0,this,&TimelineMedia::next);
}
void TimelineMedia::clear(){m_queue.clear();m_pending.clear();m_images.clear();m_waves.clear();m_failed.clear();if(m_busy){m_job.key.clear();m_process.kill();}emit updated();}
