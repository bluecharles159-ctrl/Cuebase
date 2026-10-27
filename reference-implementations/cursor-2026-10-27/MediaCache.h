#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QString>

class MediaCache : public QObject {
    Q_OBJECT
public:
    static MediaCache &instance();
    QPixmap waveform(const QString &path);
    QPixmap thumbnail(const QString &path, const QColor &fallback);

signals:
    void ready(const QString &path);

private:
    explicit MediaCache(QObject *parent = nullptr);
    QString fileKey(const QString &path) const;
    QPixmap makeWaveform(const QString &path);
    QPixmap makeThumb(const QString &path, const QColor &fallback);

    QHash<QString, QPixmap> m_waves;
    QHash<QString, QPixmap> m_thumbs;
};
