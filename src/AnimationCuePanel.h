#pragma once
#include <QWidget>
#include <QString>
#include <functional>
class BuildModel;
class Library;
QWidget *animationCueEditor(BuildModel *model);
QWidget *animationCueControls(Library *library,const QString &elementId,std::function<void(QString)> trigger);
