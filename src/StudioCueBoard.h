#pragma once
#include <QWidget>
class Library;
class Playout;
QWidget *studioCueBoard(Library *library,Playout *play,const QString &elementId,const QString &placementId={});
