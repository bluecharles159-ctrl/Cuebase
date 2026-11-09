#include "ChannelDirectory.h"
#include "UiControls.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QLocalSocket>
#include <QProgressBar>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace {
bool finish(QSettings &settings,QString *error){
    settings.sync();
    if(settings.status()==QSettings::NoError)return true;
    if(error)*error="Could not save the local channel directory.";
    return false;
}
QJsonObject format(const DisplaySettings &d){
    return {{"size",d.resolution},{"fps",d.fps},{"base",d.fpsBase},{"scale",d.resScale},
            {"aspectX",d.pixAspX},{"aspectY",d.pixAspY},{"bitrate",d.bitrate}};
}
}

ChannelDirectory::ChannelDirectory(QString dir,QString active):ini(QDir(dir).filePath("channels.ini")),activeId(std::move(active)){}
QString ChannelDirectory::windowEndpoint(const QString &id)const{
    const auto profile=profileName(id);
    if(profile.isEmpty())return {};
    QDir directory=QFileInfo(ini).absoluteDir();
    if(id!="main"){
        directory.cdUp();
        directory=QDir(directory.filePath(profile));
    }
    const auto digest=QCryptographicHash::hash(directory.absolutePath().toUtf8(),QCryptographicHash::Sha256);
    return "cuebase-"+QString::fromLatin1(digest.toHex().left(32));
}
QMap<QString,QString> ChannelDirectory::list()const{
    QSettings config(ini,QSettings::IniFormat);QMap<QString,QString> result;
    result["main"]=config.value("names/main","Main channel").toString();
    config.beginGroup("names");
    for(const auto &key:config.childKeys())if(!profileName(key).isEmpty())result[key]=config.value(key).toString();
    return result;
}
QString ChannelDirectory::profileName(const QString &id){
    if(id=="main")return "Cuebase";
    const QUuid uuid(id);
    if(uuid.isNull() || uuid.toString(QUuid::WithoutBraces)!=id)return {};
    return "Cuebase-"+id;
}
bool ChannelDirectory::rename(const QString &id,const QString &name,QString *error){
    const QString trimmed=name.trimmed();
    if(!list().contains(id) || trimmed.isEmpty() || trimmed.size()>160){if(error)*error="Choose an existing channel and a name of 1–160 characters.";return false;}
    QSettings config(ini,QSettings::IniFormat);config.setValue("names/"+id,trimmed);return finish(config,error);
}
QString ChannelDirectory::add(const QString &name,QString *error){
    const auto trimmed=name.trimmed();
    if(trimmed.isEmpty() || trimmed.size()>160){if(error)*error="Enter a channel name (1–160 characters).";return {};}
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSettings config(ini,QSettings::IniFormat);config.setValue("names/"+id,trimmed);
    return finish(config,error)?id:QString();
}
bool ChannelDirectory::sharesOutput()const{return QSettings(ini,QSettings::IniFormat).value("sharing/"+activeId,false).toBool();}
QByteArray ChannelDirectory::outputFormat(const DisplaySettings &display){return QJsonDocument(format(display)).toJson(QJsonDocument::Compact);}
bool ChannelDirectory::shareOutput(bool enabled,const DisplaySettings &display,QString *error){
    QSettings config(ini,QSettings::IniFormat);config.setValue("sharing/"+activeId,enabled);
    if(enabled && !config.contains("shared/output"))config.setValue("shared/output",QJsonDocument(format(display)).toJson(QJsonDocument::Compact));
    return finish(config,error);
}
bool ChannelDirectory::publishOutput(const DisplaySettings &display,QString *error){
    if(!sharesOutput())return true;
    QSettings config(ini,QSettings::IniFormat);config.setValue("shared/output",outputFormat(display));return finish(config,error);
}
bool ChannelDirectory::readOutput(DisplaySettings &display)const{
    if(!sharesOutput())return false;
    QSettings config(ini,QSettings::IniFormat);
    const auto saved=QJsonDocument::fromJson(config.value("shared/output").toByteArray()).object();
    if(saved.isEmpty() || saved==format(display))return false;
    display.resolution=saved["size"].toString(display.resolution);display.fps=saved["fps"].toInt(display.fps);
    display.fpsBase=saved["base"].toDouble(display.fpsBase);display.resScale=saved["scale"].toInt(display.resScale);
    display.pixAspX=saved["aspectX"].toDouble(display.pixAspX);display.pixAspY=saved["aspectY"].toDouble(display.pixAspY);
    display.bitrate=saved["bitrate"].toDouble(display.bitrate);return true;
}
QDialog *ChannelDirectory::manager(QWidget *parent,Library *library){
    auto *dialog=roundedModal(parent,"Channels");dialog->setObjectName("channelManager");dialog->setMinimumWidth(440);
    auto *root=new QVBoxLayout(dialog);root->setContentsMargins(20,18,20,18);root->addWidget(new QLabel("Production channels"));
    auto *channels=new QListWidget;channels->setObjectName("channelDirectoryList");root->addWidget(channels);
    auto *form=new QFormLayout;auto *name=new QLineEdit;name->setObjectName("directoryChannelName");name->setMaxLength(160);form->addRow("Channel name",name);root->addLayout(form);
    auto *actions=new QHBoxLayout;root->addLayout(actions);
    auto *addButton=new QPushButton("Add channel"),*renameButton=new QPushButton("Rename"),*open=new QPushButton("Open window");
    addButton->setObjectName("directoryAdd");renameButton->setObjectName("directoryRename");open->setObjectName("directoryOpen");
    for(auto *b:{addButton,renameButton,open})actions->addWidget(b);
    auto *share=new QCheckBox("Share this channel’s output format with other opted-in channels");
    share->setChecked(sharesOutput());root->addWidget(share);
    auto *note=new QLabel("Shared settings include frame size, rate, pixel aspect and bitrate. Programs, graphics and channel identities are stored separately on this computer.");
    note->setWordWrap(true);root->addWidget(note);
    auto *status=new QLabel;status->setTextFormat(Qt::PlainText);status->setWordWrap(true);root->addWidget(status);
    auto *progress=new QProgressBar;progress->setRange(0,0);progress->setFixedHeight(4);
    progress->setTextVisible(false);progress->hide();root->addWidget(progress);
    auto reload=[=,this](const QString &id){channels->clear();const auto directory=list();
        for(auto it=directory.cbegin();it!=directory.cend();++it){auto *item=new QListWidgetItem(it.value()+(it.key()==activeId?"  ·  Current":""),channels);item->setData(Qt::UserRole,it.key());if(it.key()==id)channels->setCurrentItem(item);}};
    QObject::connect(channels,&QListWidget::currentItemChanged,dialog,[=,this](QListWidgetItem *item){
        const auto id=item?item->data(Qt::UserRole).toString():QString();name->setText(list().value(id));
        renameButton->setEnabled(item);open->setEnabled(item && id!=activeId);
    });
    QObject::connect(addButton,&QPushButton::clicked,dialog,[=,this]{QString error;auto id=add(name->text(),&error);if(!id.isEmpty())reload(id);status->setText(error);});
    QObject::connect(renameButton,&QPushButton::clicked,dialog,[=,this]{if(!channels->currentItem())return;QString error;
        auto id=channels->currentItem()->data(Qt::UserRole).toString();if(rename(id,name->text(),&error))reload(id);status->setText(error);});
    QObject::connect(open,&QPushButton::clicked,dialog,[=,this]{
        if(!channels->currentItem() || !open->isEnabled())return;
        const auto id=channels->currentItem()->data(Qt::UserRole).toString();
        const auto endpoint=windowEndpoint(id);
        const auto channelName=list().value(id);
        if(endpoint.isEmpty() || channelName.isEmpty())return;
        channels->setEnabled(false);open->setEnabled(false);addButton->setEnabled(false);renameButton->setEnabled(false);
        status->setText(QObject::tr("Opening %1…").arg(channelName));progress->show();
        auto *request=new QObject(dialog);
        auto *socket=new QLocalSocket(request);
        auto *retry=new QTimer(request);retry->setSingleShot(true);retry->setInterval(250);
        auto fail=[=](const QString &reason){
            retry->stop();socket->disconnect();socket->abort();
            progress->hide();status->setText(reason);
            channels->setEnabled(true);open->setEnabled(true);addButton->setEnabled(true);renameButton->setEnabled(true);
            request->deleteLater();
        };
        QObject::connect(socket,&QLocalSocket::connected,request,[socket]{socket->write("show");socket->flush();});
        QObject::connect(socket,&QLocalSocket::readyRead,request,[=]{
            if(socket->bytesAvailable()>=5 && socket->read(5)=="ready"){
                socket->disconnect();request->deleteLater();dialog->accept();
            }
        });
        QObject::connect(retry,&QTimer::timeout,request,[=]{socket->connectToServer(endpoint);});
        QObject::connect(socket,&QLocalSocket::errorOccurred,request,[=,started=false](QLocalSocket::LocalSocketError)mutable{
            if(!started){
                started=true;
                if(!QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--channel",id})){
                    fail(QObject::tr("Could not start the channel window."));return;
                }
            }
            retry->start();
        });
        QTimer::singleShot(20000,request,[=]{fail(QObject::tr("The channel window did not respond. Try Open window again."));});
        socket->connectToServer(endpoint);
    });
    QObject::connect(share,&QCheckBox::toggled,dialog,[=,this](bool checked){
        QString error;if(!shareOutput(checked,library->display,&error)){QSignalBlocker guard(share);share->setChecked(sharesOutput());}status->setText(error);
    });
    QObject::connect(channels,&QListWidget::itemDoubleClicked,open,[open]{if(open->isEnabled())open->click();});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);root->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);reload(activeId);return dialog;
}
