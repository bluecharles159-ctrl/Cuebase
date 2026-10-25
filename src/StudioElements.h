#pragma once
#include <QWidget>
class Library;
class Playout;
class QDialog;
QWidget *studioElementsPage(Library *library,Playout *play);
QDialog *placementOrderDialog(Library *library,const QString &placementId,QWidget *parent=nullptr);
