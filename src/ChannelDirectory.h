#pragma once
#include "Data.h"
#include <QMap>
#include <QString>

class QDialog;
class QWidget;
class ChannelDirectory {
public:
    explicit ChannelDirectory(QString directory,QString active="main");
    QString current()const{return activeId;}
    QMap<QString,QString> list()const;
    QString add(const QString &name,QString *error=nullptr);
    bool rename(const QString &id,const QString &name,QString *error=nullptr);
    static QString profileName(const QString &id);
    QString windowEndpoint(const QString &id) const;
    static QByteArray outputFormat(const DisplaySettings &display);
    bool sharesOutput()const;
    bool shareOutput(bool enabled,const DisplaySettings &display,QString *error=nullptr);
    bool publishOutput(const DisplaySettings &display,QString *error=nullptr);
    bool readOutput(DisplaySettings &display)const;
    QDialog *manager(QWidget *parent,Library *library);
private:
    QString ini,activeId;
};
