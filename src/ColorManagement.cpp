#include "ColorManagement.h"

#include <QtMath>
#include <algorithm>

QStringList colorViews() { return {"Standard", "Filmic", "Raw"}; }
QStringList colorLooks() { return {"None", "Low Contrast", "Medium Contrast", "High Contrast"}; }

bool colorManagementActive(const QString &view, const QString &look, double exposure, double gamma) {
    return view != "Standard" || look != "None" || qAbs(exposure) > 1e-6 || qAbs(gamma - 1.0) > 1e-6;
}

void applyColorManagement(QImage &img, const QString &view, const QString &look, double exposure, double gamma) {
    if (img.isNull()) return;
    // one lookup table for all three channels: 8-bit in, 8-bit out
    quint8 lut[256];
    const double gain = qPow(2.0, exposure);
    const double contrast = look == "Low Contrast" ? 1.12 : look == "Medium Contrast" ? 1.28 : look == "High Contrast" ? 1.5 : 1.0;
    for (int i = 0; i < 256; ++i) {
        const double lin = qPow(i / 255.0, 2.2) * gain;  // sRGB-ish to linear light, then expose
        double out;
        if (view == "Filmic") {  // a soft shoulder instead of hard clipping (Hejl-Dawson)
            const double x = qMax(0.0, lin - 0.004);
            out = (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
        } else if (view == "Raw") {
            out = qMin(1.0, lin);  // no display transform: the linear numbers go straight to the screen
        } else {
            out = qPow(qMin(1.0, lin), 1 / 2.2);
        }
        out = 0.5 + (out - 0.5) * contrast;
        out = qPow(qBound(0.0, out, 1.0), 1.0 / qMax(0.1, gamma));
        lut[i] = quint8(qBound(0, qRound(out * 255.0), 255));
    }
    QImage::Format f = img.format();
    if (f != QImage::Format_ARGB32_Premultiplied && f != QImage::Format_RGB32 && f != QImage::Format_ARGB32)
        img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const bool premul = img.format() == QImage::Format_ARGB32_Premultiplied;
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = line[x];
            const int a = qAlpha(c);
            if (a == 0) continue;
            if (premul && a < 255) {  // work on the true colour, then premultiply again
                const int r = qMin(255, qRed(c) * 255 / a), g = qMin(255, qGreen(c) * 255 / a), b = qMin(255, qBlue(c) * 255 / a);
                line[x] = qRgba(lut[r] * a / 255, lut[g] * a / 255, lut[b] * a / 255, a);
            } else {
                line[x] = qRgba(lut[qRed(c)], lut[qGreen(c)], lut[qBlue(c)], a);
            }
        }
    }
}
