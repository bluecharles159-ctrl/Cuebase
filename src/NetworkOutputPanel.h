#pragma once
#include <QWidget>
class NetworkOutput;class Library;class Playout;
class NetworkOutputPanel:public QWidget{
public:NetworkOutputPanel(NetworkOutput *output,Library *library,Playout *playout,QWidget *parent=nullptr);
};
