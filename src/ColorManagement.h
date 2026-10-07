#pragma once

#include <QImage>
#include <QStringList>

// Blender-style colour management for the whole picture: a view transform, a look (contrast), exposure and gamma.
QStringList colorViews();   // Standard, Filmic, Raw
QStringList colorLooks();   // None, Low Contrast, Medium Contrast, High Contrast
bool colorManagementActive(const QString &view, const QString &look, double exposure, double gamma);
void applyColorManagement(QImage &img, const QString &view, const QString &look, double exposure, double gamma);
