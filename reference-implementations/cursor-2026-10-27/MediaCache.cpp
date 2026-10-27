#include "MediaCache.h"

#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

MediaCache &MediaCache::instance() {
    static MediaCache cache;
    return cache;
}

MediaCache::MediaCache(QObject *parent) : QObject(parent) {}

QString MediaCache::fileKey(const QString &path) const {
    const QFileInfo fi(path);
    return path + "|" + QString::number(fi.size()) + "|" + QString::number(fi.lastModified().toMSecsSinceEpoch());
}

QPixmap MediaCache::waveform(const QString &path) {
    const QString key = path.isEmpty() ? QStringLiteral("empty-wave") : fileKey(path);
    if (m_waves.contains(key)) return m_waves.value(key);
    const QPixmap pm = makeWaveform(path);
    m_waves.insert(key, pm);
    return pm;
}

QPixmap MediaCache::thumbnail(const QString &path, const QColor &fallback) {
    const QString key = (path.isEmpty() ? QStringLiteral("empty-thumb") : fileKey(path)) + fallback.name();
    if (m_thumbs.contains(key)) return m_thumbs.value(key);
    const QPixmap pm = makeThumb(path, fallback);
    m_thumbs.insert(key, pm);
    return pm;
}

QPixmap MediaCache::makeWaveform(const QString &path) {
    QPixmap pm(512, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QByteArray bytes;
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) bytes = f.read(64 * 1024);
    }
    QVector<float> peaks(256, 0.12f);
    if (!bytes.isEmpty()) {
        const int step = qMax(1, bytes.size() / peaks.size());
        for (int i = 0; i < peaks.size(); ++i) {
            int acc = 0, n = 0;
            const int start = i * step;
            for (int j = 0; j < step && start + j < bytes.size(); ++j) {
                acc += qAbs(int(uchar(bytes[start + j])) - 128);
                ++n;
            }
            peaks[i] = n ? qBound(0.06f, acc / (n * 140.0f), 1.0f) : 0.1f;
        }
    } else {
        for (int i = 0; i < peaks.size(); ++i)
            peaks[i] = 0.18f + 0.22f * qAbs(qSin(i * 0.21) * qCos(i * 0.07));
    }
    p.setPen(QPen(QColor(96, 165, 250, 190), 1.4));
    const float mid = 32.f, amp = 26.f;
    for (int i = 0; i < peaks.size(); ++i) {
        const float x = i * (512.f / peaks.size());
        const float h = peaks[i] * amp;
        p.drawLine(QPointF(x, mid - h), QPointF(x, mid + h));
    }
    return pm;
}

QPixmap MediaCache::makeThumb(const QString &path, const QColor &fallback) {
    QImage img;
    if (!path.isEmpty() && img.load(path)) {
        return QPixmap::fromImage(img.scaled(320, 180, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
    QPixmap pm(320, 180);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, 320, 180);
    g.setColorAt(0, fallback.darker(180));
    g.setColorAt(1, QColor("#020617"));
    p.fillRect(pm.rect(), g);
    p.setPen(QColor(255, 255, 255, 40));
    for (int x = 0; x < 320; x += 18) p.drawLine(x, 0, x, 180);
    return pm;
}
