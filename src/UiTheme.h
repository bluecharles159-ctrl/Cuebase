#pragma once
#include <QString>
class QWidget;
QString currentUiTheme();
void applyUiTheme(const QString &name, bool persist=true);
void showUiSettings(QWidget *parent);
