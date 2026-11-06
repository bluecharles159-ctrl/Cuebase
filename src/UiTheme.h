#pragma once
#include <QString>
#include <functional>
class QWidget;
QString currentUiTheme();
void applyUiTheme(const QString &name, bool persist=true);
void showUiSettings(QWidget *parent,const std::function<void()> &workspaceSetup={});
