#include "CrewWorkspace.h"
#include "CrewPortal.h"
#include "ElementLibrary.h"
#include "UiControls.h"
#include <QSslSocket>
#include <QSslConfiguration>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QProcess>
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QDateTime>
#include <QTimer>
#include <QNetworkInterface>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <memory>
#include <algorithm>

CrewSession::CrewSession(Library *library,QObject *parent):QTcpServer(parent),lib(library){
    auto changed=[this]{if(!applying){++version;emit updated();}};
    connect(lib,&Library::productionChanged,this,changed);connect(lib,&Library::elementsChanged,this,changed);connect(lib,&Library::programsChanged,this,changed);
}
QJsonObject CrewSession::snapshot()const{
    QJsonArray fields,programs;for(const auto &program:lib->programs)programs.append(QJsonObject{{"id",program.id},{"title",program.title},{"notes",program.description}});
    for(const auto &element:lib->elements){const auto design=ElementDesign::fromJson(element.design);for(const auto &object:design.objects)if(object.type=="Text"&&object.style["studio.editable"].toBool())fields.append(QJsonObject{{"element",element.id},{"elementName",element.name},{"object",object.id},{"label",object.style["studio.label"].toString(object.name)},{"value",object.text}});}
    return {{"revision",double(version)},{"production",lib->production},{"fields",fields},{"programs",programs}};
}
bool CrewSession::apply(const QJsonObject &document,const QString &role,const QString &author,QString *error){
    auto fail=[&](QString reason){if(error)*error=reason;return false;};
    if(role!="Editor")return fail("This session has view-only permission.");
    if(document["revision"].toDouble()!=double(version))return fail("Someone changed this production. Refresh before saving; your changes have not overwritten theirs.");
    if(!document["production"].isObject()||!document["fields"].isArray())return fail("Invalid production document.");
    auto production=document["production"].toObject();QJsonObject clean;
    const QMap<QString,int> columns{{"crew",5},{"tasks",6},{"schedule",6},{"script",4}};
    for(auto it=columns.cbegin();it!=columns.cend();++it){const auto rows=production[it.key()].toArray();if(rows.size()>500)return fail("Each table supports up to 500 rows.");QJsonArray sanitized;QSet<QString> ids;
        for(const auto &value:rows){if(!value.isObject())return fail("Invalid table row.");const auto row=value.toObject();const auto id=row["id"].toString();if(id.isEmpty()||id.size()>80||ids.contains(id))return fail("Table row identifiers must be unique.");ids.insert(id);QJsonObject result{{"id",id}};for(int c=0;c<it.value();++c){const auto text=row["c"+QString::number(c)].toString();if(text.size()>2000)return fail("A table cell exceeds 2,000 characters.");result["c"+QString::number(c)]=text;}sanitized.append(result);}clean[it.key()]=sanitized;
    }
    QJsonObject sheet;for(const auto &name:QStringList{"Production","Date","Call time","Location","Address","Parking","Emergency contact","Notes"}){auto text=production["callSheet"].toObject()[name].toString();if(text.size()>4000)return fail("A call-sheet field exceeds 4,000 characters.");sheet[name]=text;}clean["callSheet"]=sheet;
    auto elements=lib->elements;QSet<QString> seen;
    if(document["fields"].toArray().size()>2000)return fail("Too many text fields.");
    for(const auto &value:document["fields"].toArray()){const auto field=value.toObject();const auto elementId=field["element"].toString(),objectId=field["object"].toString();const auto key=elementId+"/"+objectId;if(seen.contains(key))return fail("Duplicate text field.");seen.insert(key);bool found=false;
        for(auto &element:elements)if(element.id==elementId){auto design=ElementDesign::fromJson(element.design);const auto *object=design.find(objectId);if(!object||object->type!="Text")return fail("Only exposed text fields may be edited by crew.");QString reason;if(!setStudioField(design,objectId,field["value"].toString(),&reason))return fail(reason);element.design=design.toJson();found=true;break;}if(!found)return fail("A screen element was removed. Refresh and try again.");
    }
    auto audit=lib->production["audit"].toArray();audit.append(QJsonObject{{"author",author.left(80)},{"time",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});while(audit.size()>100)audit.removeFirst();clean["audit"]=audit;
    for(auto &element:elements)if(const auto *old=lib->element(element.id);old&&old->design!=element.design)refreshElementThumbnail(element);
    const auto priorProduction=lib->production;const auto priorElements=lib->elements;
    lib->production=clean;lib->elements=elements;QString reason;if(!lib->saveChecked(&reason)){lib->production=priorProduction;lib->elements=priorElements;return fail("Could not save the production: "+reason);}
    applying=true;emit lib->productionChanged();emit lib->elementsChanged();applying=false;++version;emit updated();return true;
}
QString CrewSession::invite(const QString &name,const QString &role,QString *id){
    if(!isListening()||(role!="Editor"&&role!="Viewer")||name.trimmed().isEmpty()||members.size()>=50)return {};
    QByteArray bytes;for(int i=0;i<8;++i){quint32 value=QRandomGenerator::system()->generate();bytes.append(reinterpret_cast<const char*>(&value),sizeof(value));}
    const auto code=bytes.toHex();Member member{Library::newId(),name.trimmed().left(80),role,QCryptographicHash::hash(code,QCryptographicHash::Sha256)};members.append(member);if(id)*id=member.id;return QString::fromLatin1(code);
}
void CrewSession::revoke(const QString &id){members.removeIf([&](const Member &m){return m.id==id;});}
QString CrewSession::fingerprint()const{return QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex(':'));}
bool CrewSession::startSession(quint16 port,QString *error,const QHostAddress &address){
    auto fail=[&](QString reason){if(error)*error=reason;return false;};if(isListening())return true;
    if(!QSslSocket::supportsSsl())return fail("The Qt TLS backend is unavailable. Install the UCRT64 OpenSSL runtime.");
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/crew";if(!QDir().mkpath(directory))return fail("Could not create the crew certificate directory.");
    const auto certFile=directory+"/host.crt",keyFile=directory+"/host.key";
    if(!QFile::exists(certFile)||!QFile::exists(keyFile)){
        const auto executable=QStandardPaths::findExecutable("openssl");if(executable.isEmpty())return fail("OpenSSL is required to create the host certificate.");
        QProcess process;process.start(executable,{"req","-x509","-newkey","rsa:2048","-sha256","-nodes","-keyout",keyFile,"-out",certFile,"-days","365","-subj","/CN=Cuebase Crew","-addext","subjectAltName=DNS:localhost,IP:127.0.0.1"});
        if(!process.waitForFinished(30000)){process.kill();process.waitForFinished();return fail("Certificate creation timed out.");}if(process.exitCode()!=0)return fail("Certificate creation failed: "+QString::fromUtf8(process.readAllStandardError()).left(500));QFile::setPermissions(keyFile,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    }
    QFile cert(certFile),key(keyFile);if(!cert.open(QIODevice::ReadOnly)||!key.open(QIODevice::ReadOnly))return fail("Could not read the host certificate.");certificate=QSslCertificate(cert.readAll());privateKey=QSslKey(key.readAll(),QSsl::Rsa);
    if(certificate.isNull()||privateKey.isNull()||certificate.expiryDate()<QDateTime::currentDateTimeUtc())return fail("The host TLS certificate is invalid or expired. Replace the certificate and key in "+directory);
    if(!listen(address,port))return fail(errorString());return true;
}
void CrewSession::stopSession(){close();members.clear();for(auto *socket:findChildren<QSslSocket*>())socket->abort();}
void CrewSession::incomingConnection(qintptr descriptor){
    auto *socket=new QSslSocket(this);if(!socket->setSocketDescriptor(descriptor)){socket->deleteLater();return;}
    if(findChildren<QSslSocket*>().size()>24){socket->abort();socket->deleteLater();return;}
    socket->setReadBufferSize(1048576);socket->setLocalCertificate(certificate);socket->setPrivateKey(privateKey);socket->setProtocol(QSsl::TlsV1_2OrLater);socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    auto buffer=std::make_shared<QByteArray>();auto done=std::make_shared<bool>(false);QTimer::singleShot(15000,socket,[socket]{socket->abort();});connect(socket,&QSslSocket::disconnected,socket,&QObject::deleteLater);
    connect(socket,&QSslSocket::readyRead,socket,[this,socket,buffer,done]{
        if(*done)return;buffer->append(socket->readAll());auto reply=[&](int status,QByteArray body,QByteArray type="application/json"){*done=true;socket->write("HTTP/1.1 "+QByteArray::number(status)+" Response\r\nContent-Type: "+type+"\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; frame-ancestors 'none'; base-uri 'none'\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();};
        auto error=[&](int status,QString reason){reply(status,QJsonDocument(QJsonObject{{"error",reason}}).toJson(QJsonDocument::Compact));};
        if(buffer->size()>1048576){error(413,"Request is too large.");return;}const int end=buffer->indexOf("\r\n\r\n");if(end<0){if(buffer->size()>16384)error(431,"Headers are too large.");return;}
        const auto lines=buffer->left(end).split('\n');const auto request=lines.first().trimmed().split(' ');if(request.size()!=3){error(400,"Invalid request.");return;}QMap<QByteArray,QByteArray> headers;for(int i=1;i<lines.size();++i){int colon=lines[i].indexOf(':');if(colon>0){auto name=lines[i].left(colon).trimmed().toLower();if(headers.contains(name)){error(400,"Duplicate header.");return;}headers[name]=lines[i].mid(colon+1).trimmed();}}
        if(headers.contains("transfer-encoding")){error(400,"Chunked requests are not supported.");return;}bool ok=true;const auto length=headers.contains("content-length")?headers["content-length"].toLongLong(&ok):0;if(!ok||length<0||length>1000000){error(413,"Invalid request length.");return;}if(buffer->size()<end+4+length)return;
        if(request[0]=="GET"&&request[1]=="/"){reply(200,crewPortal(),"text/html; charset=utf-8");return;}
        if(request[1]!="/api/session"){error(404,"Not found.");return;}const auto authorization=headers.value("authorization");if(!authorization.startsWith("Bearer ")){error(401,"Enter a session code from the host.");return;}
        const auto hash=QCryptographicHash::hash(authorization.mid(7),QCryptographicHash::Sha256);const Member *member=nullptr;for(const auto &candidate:members)if(candidate.hash==hash){member=&candidate;break;}if(!member){error(403,"The session code is invalid or has been revoked.");return;}
        if(request[0]=="POST"){QJsonParseError parse;auto document=QJsonDocument::fromJson(buffer->mid(end+4,length),&parse);if(parse.error!=QJsonParseError::NoError||!document.isObject()){error(400,"Invalid JSON.");return;}QString reason;if(!apply(document.object(),member->role,member->name,&reason)){error(member->role=="Editor"?409:403,reason);return;}}
        else if(request[0]!="GET"){error(405,"Method not allowed.");return;}
        auto data=snapshot();data["role"]=member->role;reply(200,QJsonDocument(data).toJson(QJsonDocument::Compact));
    });socket->startServerEncryption();
}

QWidget *crewWorkspace(Library *library){
    // The local editor and encrypted portal submit the same revision-checked document.
    auto *page=new QWidget;page->setObjectName("crewWorkspace");auto *layout=new QVBoxLayout(page);layout->setContentsMargins(12,12,12,12);layout->setSpacing(8);auto *session=new CrewSession(library,page);
    page->setStyleSheet("QWidget{background:#2c2f35;color:#dce1e8;font-size:10px;}QLineEdit,QComboBox,QTableWidget,QListWidget{background:#343941;border:1px solid #4c535e;border-radius:4px;}QPushButton{background:#444b56;border:0;border-radius:4px;padding:5px 8px;}QPushButton:hover{background:#526075;}QHeaderView::section{background:#3b424c;padding:4px;border:0;}QTabBar::tab{padding:6px;background:#323741;}QTabBar::tab:selected{background:#46576d;}QScrollBar:vertical{width:4px;}QScrollBar:horizontal{height:4px;}");
    auto *title=new QLabel("PRODUCTION / CREW");title->setStyleSheet("font-weight:600;font-size:12px;");layout->addWidget(title);auto *hint=new QLabel("Plan the show together. The Cuebase host keeps control of broadcast, replay and media files.");hint->setWordWrap(true);layout->addWidget(hint);
    auto *tabs=new QTabWidget;layout->addWidget(tabs);auto draft=std::make_shared<QJsonObject>(session->snapshot());auto dirty=std::make_shared<bool>(false),loading=std::make_shared<bool>(false);
    struct Table{QString key;QStringList columns;QTableWidget *widget;};auto tables=std::make_shared<QList<Table>>();
    const QList<QPair<QString,QStringList>> schemas{{"crew",{"Name","Department","Role","Contact","Call time"}},{"tasks",{"Task","Assignee","Status","Due","Program","Notes"}},{"schedule",{"Time","Program","Item","Owner","Duration","Location"}},{"script",{"Time","Program","Video / action","Audio / words"}}};
    for(const auto &schema:schemas){auto *tab=new QWidget;auto *box=new QVBoxLayout(tab);box->setContentsMargins(4,4,4,4);auto *table=new QTableWidget(0,schema.second.size());table->setHorizontalHeaderLabels(schema.second);table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);table->horizontalHeader()->setStretchLastSection(true);table->verticalHeader()->setDefaultSectionSize(25);table->verticalHeader()->hide();table->setMinimumHeight(220);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setObjectName("crewTable_"+schema.first);box->addWidget(table);
        auto *row=new QHBoxLayout;auto *add=new QPushButton(uiIcon("add"),"Add"),*remove=new QPushButton(uiIcon("delete"),"Remove selected");row->addWidget(add);row->addWidget(remove);row->addStretch();box->addLayout(row);tables->append({schema.first,schema.second,table});tabs->addTab(tab,schema.first=="script"?"A/V rundown":schema.first.left(1).toUpper()+schema.first.mid(1));
        QObject::connect(table,&QTableWidget::itemChanged,page,[=]{if(!*loading)*dirty=true;});
        QObject::connect(add,&QPushButton::clicked,page,[=]{const int r=table->rowCount();table->insertRow(r);for(int c=0;c<table->columnCount();++c)table->setItem(r,c,new QTableWidgetItem);table->item(r,0)->setData(Qt::UserRole,Library::newId());if(schema.first=="tasks")table->item(r,2)->setText("To do");table->setCurrentCell(r,0);table->editItem(table->item(r,0));*dirty=true;});
        QObject::connect(remove,&QPushButton::clicked,page,[=]{auto selected=table->selectionModel()->selectedRows();std::sort(selected.begin(),selected.end(),[](auto a,auto b){return a.row()>b.row();});for(const auto &index:selected)table->removeRow(index.row());if(!selected.isEmpty())*dirty=true;});
    }
    auto *sheetPage=new QWidget;auto *sheetForm=new QFormLayout(sheetPage);auto sheetFields=std::make_shared<QMap<QString,QLineEdit*>>();for(const auto &name:QStringList{"Production","Date","Call time","Location","Address","Parking","Emergency contact","Notes"}){auto *input=new QLineEdit;input->setMaxLength(4000);sheetFields->insert(name,input);sheetForm->addRow(name,input);QObject::connect(input,&QLineEdit::textEdited,page,[=]{*dirty=true;});}tabs->addTab(sheetPage,"Call sheet");
    auto *status=new QLabel;status->setWordWrap(true);layout->addWidget(status);auto *actions=new QHBoxLayout;auto *save=new QPushButton(uiIcon("save"),"Save production"),*refresh=new QPushButton(uiIcon("reset"),"Reload"),*exportSheet=new QPushButton(uiIcon("save"),"Export call sheet");actions->addWidget(save);actions->addWidget(refresh);actions->addWidget(exportSheet);layout->addLayout(actions);
    auto collect=[=]{auto document=*draft;auto production=document["production"].toObject();for(const auto &entry:*tables){QJsonArray rows;for(int r=0;r<entry.widget->rowCount();++r){QJsonObject row;for(int c=0;c<entry.columns.size();++c){auto *item=entry.widget->item(r,c);row["c"+QString::number(c)]=item?item->text():QString();if(c==0)row["id"]=item?item->data(Qt::UserRole).toString():Library::newId();}rows.append(row);}production[entry.key]=rows;}QJsonObject sheet;for(auto it=sheetFields->cbegin();it!=sheetFields->cend();++it)sheet[it.key()]=it.value()->text();production["callSheet"]=sheet;document["production"]=production;return document;};
    auto reload=[=]{*loading=true;*draft=session->snapshot();const auto production=(*draft)["production"].toObject();for(const auto &entry:*tables){const auto rows=production[entry.key].toArray();entry.widget->setRowCount(rows.size());for(int r=0;r<rows.size();++r){const auto row=rows[r].toObject();for(int c=0;c<entry.columns.size();++c){auto *item=new QTableWidgetItem(row["c"+QString::number(c)].toString());if(c==0)item->setData(Qt::UserRole,row["id"].toString());entry.widget->setItem(r,c,item);}}}const auto sheet=production["callSheet"].toObject();for(auto it=sheetFields->cbegin();it!=sheetFields->cend();++it)it.value()->setText(sheet[it.key()].toString());*loading=false;*dirty=false;status->setText("Saved revision "+QString::number(session->revision()));};reload();
    QObject::connect(save,&QPushButton::clicked,page,[=]{page->setFocus();QString error;if(session->apply(collect(),"Editor","Host",&error))reload();else status->setText(error);});
    QObject::connect(refresh,&QPushButton::clicked,page,[=]{if(*dirty&&QMessageBox::question(page,"Reload production","Discard your unsaved changes and load the latest production?")!=QMessageBox::Yes)return;reload();});
    QObject::connect(session,&CrewSession::updated,page,[=]{if(*dirty)status->setText("Production changed elsewhere. Reload before saving your draft.");else reload();});
    QObject::connect(exportSheet,&QPushButton::clicked,page,[=]{const auto file=QFileDialog::getSaveFileName(page,"Export call sheet",{},"HTML call sheet (*.html)");if(file.isEmpty())return;const auto production=collect()["production"].toObject();QString html="<!doctype html><meta charset='utf-8'><title>Cuebase call sheet</title><style>body{font:14px system-ui;margin:40px;color:#20242a}table{border-collapse:collapse;width:100%;margin-bottom:24px}td,th{border:1px solid #bbb;padding:8px;text-align:left}h1{font-size:24px}@media print{button{display:none}}</style><h1>"+production["callSheet"].toObject()["Production"].toString("Call sheet").toHtmlEscaped()+"</h1>";const auto sheet=production["callSheet"].toObject();for(auto it=sheet.begin();it!=sheet.end();++it)html+="<p><b>"+it.key().toHtmlEscaped()+":</b> "+it.value().toString().toHtmlEscaped()+"</p>";for(const auto &entry:*tables){html+="<h2>"+entry.key.toHtmlEscaped()+"</h2><table><tr>";for(const auto &label:entry.columns)html+="<th>"+label.toHtmlEscaped()+"</th>";html+="</tr>";for(const auto &value:production[entry.key].toArray()){html+="<tr>";for(int c=0;c<entry.columns.size();++c)html+="<td>"+value.toObject()["c"+QString::number(c)].toString().toHtmlEscaped()+"</td>";html+="</tr>";}html+="</table>";}QSaveFile output(file);const auto bytes=html.toUtf8();if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size()||!output.commit())status->setText(output.errorString());else status->setText("Call sheet exported. Open it in a browser to print or save as PDF.");});
    auto *team=new QWidget;auto *teamLayout=new QVBoxLayout(team);teamLayout->setContentsMargins(4,4,4,4);auto *start=new QPushButton(uiIcon("crew"),"Start crew session");teamLayout->addWidget(start);auto *address=new QLabel("Session stopped");address->setWordWrap(true);address->setTextInteractionFlags(Qt::TextSelectableByMouse);teamLayout->addWidget(address);auto *security=new QLabel("Crew connect using a browser on your studio network or VPN. Verify the certificate fingerprint shown here before entering an invitation code. No router or firewall settings are changed automatically.");security->setWordWrap(true);teamLayout->addWidget(security);
    auto *name=new QLineEdit;name->setPlaceholderText("Crew member name");name->setMaxLength(80);auto *role=new QComboBox;role->addItems({"Viewer","Editor"});auto *invite=new QPushButton("Create invitation");auto *form=new QFormLayout;form->addRow("Name",name);form->addRow("Permission",role);teamLayout->addLayout(form);teamLayout->addWidget(invite);auto *code=new QLineEdit;code->setReadOnly(true);code->setPlaceholderText("New invitation code appears here");teamLayout->addWidget(code);auto *copy=new QPushButton("Copy invitation code");teamLayout->addWidget(copy);auto *members=new QListWidget;teamLayout->addWidget(members);auto *revoke=new QPushButton(uiIcon("lock"),"Revoke selected invitation");teamLayout->addWidget(revoke);teamLayout->addStretch();tabs->addTab(team,"Team session");
    QObject::connect(start,&QPushButton::clicked,page,[=]{if(session->isListening()){session->stopSession();members->clear();code->clear();address->setText("Session stopped · invitations revoked");start->setText("Start crew session");return;}QString error;if(!session->startSession(8943,&error)){address->setText(error);return;}QStringList urls;for(const auto &ip:QNetworkInterface::allAddresses())if(ip.protocol()==QAbstractSocket::IPv4Protocol&&!ip.isLoopback())urls.append("https://"+ip.toString()+":8943/");if(urls.isEmpty())urls.append("https://127.0.0.1:8943/");address->setText(urls.join("\n")+"\nSHA-256 certificate:\n"+session->fingerprint());start->setText("Stop crew session");});
    QObject::connect(invite,&QPushButton::clicked,page,[=]{QString id;const auto token=session->invite(name->text(),role->currentText(),&id);if(token.isEmpty()){status->setText("Start the session and enter a crew member name first (maximum 50 invitations).");return;}code->setText(token);auto *item=new QListWidgetItem(name->text()+" · "+role->currentText(),members);item->setData(Qt::UserRole,id);status->setText("Share this code directly with that crew member. It is valid until revoked or the session stops.");});
    QObject::connect(copy,&QPushButton::clicked,page,[=]{if(!code->text().isEmpty())QApplication::clipboard()->setText(code->text());});QObject::connect(revoke,&QPushButton::clicked,page,[=]{if(auto *item=members->currentItem()){session->revoke(item->data(Qt::UserRole).toString());delete members->takeItem(members->row(item));code->clear();}});return page;
}
