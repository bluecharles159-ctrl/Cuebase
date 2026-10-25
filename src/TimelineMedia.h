#pragma once
#include <QCache>
#include <QImage>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QVector>

struct TimelineWave {
    qint64 startMs=0,durationMs=0;
    QVector<float> peaks;
};

// Bounded asynchronous media previews. No decoder runs in a paint event.
class TimelineMedia : public QObject {
    Q_OBJECT
public:
    explicit TimelineMedia(QObject *parent=nullptr);
    ~TimelineMedia() override;
    QImage thumbnail(const QString &file,qint64 sourceMs);
    TimelineWave waveform(const QString &file,qint64 startMs,qint64 durationMs);
    void clear();
signals:
    void updated();
private:
    struct Job {QString file,key; qint64 start=0,duration=0;bool audio=false;};
    QString fileKey(const QString &file) const;
    void enqueue(Job job);
    void next();
    void read();
    void finish(bool success);
    QCache<QString,QImage> m_images{8192}; // KiB
    QCache<QString,TimelineWave> m_waves{2048};
    QSet<QString> m_pending,m_failed;
    QQueue<Job> m_queue;
    Job m_job;
    QProcess m_process;
    QTimer m_timeout;
    QByteArray m_bytes;
    QVector<float> m_peaks;
    qint64 m_samples=0;
    bool m_busy=false;
};
