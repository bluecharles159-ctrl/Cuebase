#pragma once
#include "Data.h"
#include <QTcpServer>
#include <QSslCertificate>
#include <QSslKey>
#include <QJsonObject>
#include <QHostAddress>
class QWidget;
class CrewSession : public QTcpServer {
    Q_OBJECT
public:
    explicit CrewSession(Library *library,QObject *parent=nullptr);
    QJsonObject snapshot()const;
    bool apply(const QJsonObject &document,const QString &role,const QString &author,QString *error);
    bool startSession(quint16 port,QString *error,const QHostAddress &address=QHostAddress::AnyIPv4);
    void stopSession();
    QString invite(const QString &name,const QString &role,QString *id=nullptr);
    void revoke(const QString &id);
    QString fingerprint()const;
    quint64 revision()const{return version;}
signals:
    void updated();
protected:
    void incomingConnection(qintptr descriptor)override;
private:
    struct Member{QString id,name,role;QByteArray hash;};
    Library *lib;quint64 version=1;bool applying=false;
    QList<Member> members;QSslCertificate certificate;QSslKey privateKey;
};
QWidget *crewWorkspace(Library *library);
