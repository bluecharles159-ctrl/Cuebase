#include <QListWidget>
#include <QtTest>
#include <QDataStream>
#include <cmath>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QUdpSocket>
#include "NetworkOutput.h"
#include "Playout.h"
#include "Replay.h"
#include "Continuity.h"
#include "SceneEffects.h"
#include "CrewWorkspace.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslError>
#include <QCryptographicHash>
#include "ViewportWidget.h"
#include "TimelineWidget.h"
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QListWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QToolButton>
#include "EditorPanel.h"
#include "ElementLibrary.h"
#include "StudioElements.h"
#include "StudioCueBoard.h"
#include "StudioDashboard.h"
#include "MainWindow.h"
#include "UiControls.h"
#include <QTcpSocket>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDoubleSpinBox>

class StreamTests:public QObject{
    Q_OBJECT
private slots:
    void overlayZIndexOrder(){Library lib;lib.display.watermarkOn=false;ProgramItem p;p.id="z-show";lib.programs={p};auto design=ElementDesign::defaultScene();DObject shape;shape.id="plate";shape.type="Shape";shape.w=8;shape.h=5;shape.base.rot={90,0,0};shape.mat.color=Qt::red;shape.mat.shadeless=true;design.objects.append(shape);Element red;red.id="red";red.type="Custom";red.live=true;red.design=design.toJson();red.cueBoardSettings["zIndex"]=10;Element blue=red;blue.id="blue";blue.cueBoardSettings["zIndex"]=0;design.find(shape.id)->mat.color=Qt::blue;blue.design=design.toJson();lib.elements={red,blue};Playout play(&lib);ViewportWidget viewport(&lib,&play);play.start();play.togglePause();QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::red));lib.elements[1].cueBoardSettings["zIndex"]=20;QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::blue));play.stop();}

    void initTestCase(){installUiBehavior(qApp);QStandardPaths::setTestModeEnabled(true);qApp->setOrganizationName("CuebaseStreamTests");qApp->setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));QDir().mkpath("artifacts");}
    void cleanProgram(){
        Library library;library.display.watermarkOn=false;library.display.safeArea=false;
        ProgramItem program;program.id="test";program.title="Test program";library.programs={program};
        ElementDesign design=ElementDesign::defaultScene();DObject rectangle;rectangle.id="graphic";rectangle.type="Shape";rectangle.w=4;rectangle.h=2;rectangle.base.rot={90,0,0};rectangle.mat.color=Qt::red;rectangle.mat.shadeless=true;design.objects.append(rectangle);
        Element element;element.id="graphic";element.type="Custom";element.live=true;element.design=design.toJson();library.elements.append(element);
        Playout play(&library);ViewportWidget monitor(&library,&play);const QSize size(640,360);
        QImage black(size,QImage::Format_ARGB32);black.fill(Qt::black);QCOMPARE(monitor.programFrame(size),black);
        play.start();play.togglePause();const auto clean=monitor.programFrame(size);QVERIFY(clean!=black);QVERIFY(qRed(clean.pixel(320,180))>200);QVERIFY(qGreen(clean.pixel(320,180))<30);
        library.display.safeArea=true;QCOMPARE(monitor.programFrame(size),clean);
        library.elements.first().live=false;QVERIFY(monitor.programFrame(size)!=clean);
        play.stop();QCOMPARE(monitor.programFrame(size),black);
    }
    void placedCueContinueIsIndependent(){
        Library lib;ProgramItem program;program.id="cue-show";program.seconds=60;lib.programs={program};auto design=ElementDesign::defaultScene();QJsonObject intro{{"id","intro"},{"name","Intro"},{"start",0},{"end",1000},{"hold",true},{"holdManual",true}},exit{{"id","exit"},{"name","Exit"},{"start",1000},{"end",2000},{"hold",false}};design.animationCues={intro,exit};Element e;e.id="cue-source";e.type="Custom";e.design=design.toJson();e.cueBoard={newCueBoardEntry(intro),newCueBoardEntry(exit)};lib.elements={e};Placement first;first.id="first";first.type="Element";first.refId=e.id;first.durationMs=10000;first.boardSnapshot=bakeCueBoard(e);auto second=first;second.id="second";second.layer=2;lib.placements={first,second};Playout play(&lib);play.start(0,1500);play.togglePause();auto a=lib.placementElement(first),b=lib.placementElement(second);QCOMPARE(play.graphicPlayback(a,play.playheadMs(),0).boardStep,0);play.advanceElementCue(a.id);QCOMPARE(play.graphicPlayback(a,play.playheadMs(),0).boardStep,1);QCOMPARE(play.graphicPlayback(b,play.playheadMs(),0).boardStep,0);play.triggerElementAnimation(a.id,QString());QVERIFY(!play.graphicPlayback(a,play.playheadMs(),0).visible);QVERIFY(play.graphicPlayback(b,play.playheadMs(),0).visible);play.stop();
    }
    void studioBackdropRetainsTransparency(){
        Library lib;lib.display.watermarkOn=false;ProgramItem program;program.id="frosted-show";lib.programs={program};auto design=ElementDesign::defaultScene();DObject red;red.id="underneath";red.type="Shape";red.w=8;red.h=5;red.base.rot={90,0,0};red.mat.color=Qt::red;red.mat.shadeless=true;DObject glass=red;glass.id="glass";glass.w=4;glass.h=2;glass.base.loc.setY(-.1);glass.mat.color=Qt::white;glass.mat.alpha=.2;glass.style["backdrop.blur"]=12.;glass.style["backdrop.brightness"]=1.;design.objects.append(red);design.objects.append(glass);Element element;element.id="glass-design";element.type="Custom";element.live=true;element.design=design.toJson();lib.elements={element};Playout play(&lib);ViewportWidget monitor(&lib,&play);monitor.resize(640,394);monitor.show();play.start();play.togglePause();auto frame=monitor.programFrame({640,360});auto pixel=frame.pixelColor(320,180);QVERIFY2(pixel.red()>200&&pixel.green()>20&&pixel.green()<150,qPrintable(pixel.name()));auto widget=monitor.grab().toImage();auto shown=widget.pixelColor(320,180);QVERIFY2(shown.green()>5&&shown.red()>shown.green()*1.5,qPrintable(shown.name()));QVERIFY(widget.save("artifacts/studio-backdrop-transparency.png"));play.stop();
    }
    void privatePreviewPreservesLivePicture(){
        Library lib;lib.display.watermarkOn=false;ProgramItem program;program.id="preview-show";program.title="Preview program";program.seconds=60;lib.programs={program};Playout play(&lib);ViewportWidget monitor(&lib,&play);monitor.resize(640,420);monitor.show();play.start(0,10000);play.togglePause();const auto onAir=monitor.programFrame({320,180});const auto position=play.playheadMs();
        QTest::mouseMove(&monitor,{200,354});QTest::mousePress(&monitor,Qt::LeftButton,Qt::NoModifier,{200,354});QTest::mouseMove(&monitor,{350,354});QTest::mouseRelease(&monitor,Qt::LeftButton,Qt::NoModifier,{350,354});QVERIFY(monitor.detached());QVERIFY(monitor.viewMs()!=position);QTest::qWait(350);QCOMPARE(play.playheadMs(),position);QCOMPARE(monitor.programFrame({320,180}),onAir);QVERIFY(monitor.grab().save("artifacts/live-preview-picture-in-picture.png"));
        QTest::mouseClick(&monitor,Qt::LeftButton,Qt::NoModifier,{580,354});QVERIFY(!monitor.detached());QTest::qWait(450);QCOMPARE(play.playheadMs(),position);QCOMPARE(monitor.programFrame({320,180}),onAir);play.stop();
    }
    void continuityOverrideAndReturn(){
        Library lib;lib.display.watermarkOn=false;ProgramItem program;program.id="show";lib.programs={program};auto design=ElementDesign::defaultScene();DObject shape;shape.id="red";shape.type="Shape";shape.w=8;shape.h=5;shape.base.rot={90,0,0};shape.mat.color=Qt::red;shape.mat.shadeless=true;design.objects.append(shape);Element element;element.id="graphic";element.type="Custom";element.live=true;element.design=design.toJson();lib.elements={element};
        Playout play(&lib);ViewportWidget monitor(&lib,&play);play.start();play.togglePause();const auto before=monitor.programFrame({320,180});auto *standby=play.continuity();QString reason;QVERIFY(standby->configure("Black",{},{},{},&reason));standby->setActive(true);const auto blackout=monitor.programFrame({320,180});QCOMPARE(blackout.pixelColor(160,90),QColor(Qt::black));QCOMPARE(play.state(),Playout::Paused);QVERIFY(standby->configure("Standby",{},{},"Editing the program",&reason));QVERIFY(monitor.programFrame({320,180})!=blackout);standby->setActive(false);QCOMPARE(monitor.programFrame({320,180}),before);QVERIFY(!standby->configure("Video",{},"missing-video.mp4",{},&reason));QVERIFY(!reason.isEmpty());lib.elements[0].cueBoard={QJsonObject{{"id","invalid"},{"cue","missing"}}};QVERIFY(!standby->configure("Element",element.id,{},{},&reason));QVERIFY(!reason.isEmpty());play.stop();
    }
    void validation(){StreamSettings s;s.executable=NetworkOutput::findEncoder();QVERIFY(!s.executable.isEmpty());s.endpoint="file:///tmp/output";QVERIFY(!NetworkOutput::validate(s).isEmpty());s.endpoint="rtmp://127.0.0.1/live/test";QVERIFY(NetworkOutput::validate(s).isEmpty());s.size={321,180};QVERIFY(!NetworkOutput::validate(s).isEmpty());NetworkOutput output;QVERIFY(!output.start(s));QCOMPARE(output.property("lastError").toString(),NetworkOutput::validate(s));}
    void replayIndependentClock(){
        Library library;ProgramItem p;p.id="replay-show";p.kind="News";p.seconds=60;library.programs={p};Playout play(&library);auto *replay=play.replay();QString reason;
        QVERIFY(!replay->start(0,1000,"preset:Fade","preset:Fade",false,&reason));play.start(0,10000);
        QVERIFY(replay->start(0,2000,"preset:Fade","preset:Fade",false,&reason));const auto before=play.playheadMs();QTRY_VERIFY_WITH_TIMEOUT(play.playheadMs()>before,2000);QTRY_VERIFY_WITH_TIMEOUT(replay->position>0,2000);replay->cancel();QCOMPARE(play.state(),Playout::Playing);
        QVERIFY(replay->start(0,2000,"preset:Fade","preset:Fade",true,&reason));const auto held=play.playheadMs();QTest::qWait(120);QCOMPARE(play.playheadMs(),held);QCOMPARE(play.state(),Playout::Paused);replay->cancel();QCOMPARE(play.state(),Playout::Playing);
        play.togglePause();QVERIFY(replay->start(0,2000,"preset:Fade","preset:Fade",true,&reason));replay->cancel();QCOMPARE(play.state(),Playout::Paused);
        QVERIFY(!replay->start(0,50000,"preset:Fade","preset:Fade",false,&reason));play.stop();QVERIFY(!replay->active());
        play.start(0,59900);QVERIFY(replay->start(0,2000,"preset:Fade","preset:Fade",false,&reason));
        QTRY_COMPARE_WITH_TIMEOUT(play.state(),Playout::Paused,2000);QVERIFY(replay->active());
        replay->cancel();QCOMPARE(play.state(),Playout::Stopped);
    }
    void crewPermissionsAndConflicts(){
        Library library;auto design=elementPreset("News lower third");Element element;element.id="crew-text";element.design=design.toJson();library.elements={element};CrewSession session(&library);auto first=session.snapshot();auto fields=first["fields"].toArray();QVERIFY(!fields.isEmpty());auto field=fields[0].toObject();field["value"]="Updated by the graphics crew";fields[0]=field;first["fields"]=fields;QString error;
        QVERIFY(!session.apply(first,"Viewer","Guest",&error));QCOMPARE(session.revision(),quint64(1));QVERIFY(session.apply(first,"Editor","Graphics",&error));QCOMPARE(session.revision(),quint64(2));QVERIFY(!session.apply(first,"Editor","Other",&error));
        auto saved=ElementDesign::fromJson(library.elements.first().design);QCOMPARE(saved.find(field["object"].toString())->text,QString("Updated by the graphics crew"));QVERIFY(!saved.find(field["object"].toString())->tracks.isEmpty());
        auto invalid=session.snapshot();auto rows=invalid["fields"].toArray();auto bad=rows[0].toObject();bad["object"]="not-exposed";rows[0]=bad;invalid["fields"]=rows;QVERIFY(!session.apply(invalid,"Editor","Graphics",&error));QCOMPARE(session.revision(),quint64(2));
        Library reopened;reopened.load();QCOMPARE(reopened.production["audit"].toArray().last().toObject()["author"].toString(),QString("Graphics"));
        auto *page=crewWorkspace(&library);page->resize(760,650);page->show();QTest::qWait(20);QVERIFY(page->grab().save("artifacts/crew-workspace.png"));delete page;
    }
    void crewEncryptedSession(){
        Library library;CrewSession session(&library);QString error;QVERIFY2(session.startSession(0,&error,QHostAddress::LocalHost),qPrintable(error));QString member;auto code=session.invite("Viewer","Viewer",&member);QVERIFY(!code.isEmpty());QNetworkAccessManager manager;
        auto request=[&](QByteArray method,QByteArray body={}){QNetworkRequest req(QUrl(QString("https://127.0.0.1:%1/api/session").arg(session.serverPort())));req.setRawHeader("Authorization","Bearer "+code.toLatin1());req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");auto *reply=manager.sendCustomRequest(req,method,body);connect(reply,&QNetworkReply::sslErrors,reply,[&session,reply](const QList<QSslError> &errors){bool expected=true;for(const auto &e:errors)expected&=QString::fromLatin1(e.certificate().digest(QCryptographicHash::Sha256).toHex(':'))==session.fingerprint();if(expected)reply->ignoreSslErrors(errors);});return reply;};
        auto *get=request("GET");QTRY_VERIFY_WITH_TIMEOUT(get->isFinished(),15000);QCOMPARE(get->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),200);auto document=get->readAll();QCOMPARE(QJsonDocument::fromJson(document).object()["role"].toString(),QString("Viewer"));delete get;
        auto *post=request("POST",document);QTRY_VERIFY_WITH_TIMEOUT(post->isFinished(),15000);QCOMPARE(post->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),403);delete post;session.revoke(member);
        get=request("GET");QTRY_VERIFY_WITH_TIMEOUT(get->isFinished(),15000);QCOMPARE(get->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),403);delete get;code=session.invite("Producer","Editor");post=request("POST",document);QTRY_VERIFY_WITH_TIMEOUT(post->isFinished(),15000);QCOMPARE(post->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),200);delete post;post=request("POST",document);QTRY_VERIFY_WITH_TIMEOUT(post->isFinished(),15000);QCOMPARE(post->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),409);delete post;session.stopSession();QVERIFY(!session.isListening());
    }
    void timelineTrim(){
        QSettings().setValue("studio/timelineScale",14.);
        Library library;ProgramItem program;program.id="trim";program.seconds=30;library.programs={program};
        Placement clip;clip.id="overlay";clip.type="Element";clip.layer=1;clip.startMs=1000;clip.durationMs=5000;clip.seconds=5;library.placements={clip};
        Playout play(&library);TimelineWidget timeline(&play,&library);timeline.resize(600,240);timeline.show();QTest::qWait(30);
        // At 14 px/s the clip spans x=24..94, in the first overlay lane.
        QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(93,113));QTest::mouseMove(&timeline,QPoint(122,113));QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(122,113));
        QVERIFY(qAbs(library.placement("overlay")->lengthMs()-7000)<45);
        QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(24,113));QTest::mouseMove(&timeline,QPoint(38,113));QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,QPoint(38,113));
        QVERIFY(qAbs(library.placement("overlay")->startMs-2000)<45);QVERIFY(qAbs(library.placement("overlay")->offsetMs-1000)<45);QVERIFY(qAbs(library.placement("overlay")->lengthMs()-6000)<45);
    }
    void cueBoardLiveAndDrafts(){
        Library lib;Element e;e.id="board-element";e.name="Evening headlines";e.type="Custom";auto design=elementPreset("News lower third");QString fieldId;for(const auto &o:design.objects)if(o.type=="Text"&&o.style["studio.editable"].toBool()){fieldId=o.id;break;}QVERIFY(!fieldId.isEmpty());QJsonObject cue{{"id","headline"},{"name","Headline"},{"start",0},{"end",1000},{"repeat",2},{"hold",true},{"holdManual",false},{"holdMs",500}};design.animationCues={cue};e.design=design.toJson();auto entry=newCueBoardEntry(cue);entry["sections"]=QJsonArray{QJsonObject{{"fields",QJsonObject{{fieldId,"Hello"}}}},QJsonObject{{"fields",QJsonObject{{fieldId,"Welcome"}}}}};e.cueBoard={entry};lib.elements={e};ProgramItem p;p.id="show";p.kind="News";p.seconds=30;lib.programs={p};Playout play(&lib);play.start(0,5000);play.togglePause();
        QVERIFY(!play.graphicPlayback(e,5000).visible);play.triggerElementAnimation(e.id,"@board:0");auto first=play.graphicPlayback(e,5000);QCOMPARE(ElementDesign::fromJson(first.design).find(fieldId)->text,QString("Hello"));QVERIFY(!play.graphicPlayback(e,6200).advancing);auto second=play.graphicPlayback(e,6500);QCOMPARE(ElementDesign::fromJson(second.design).find(fieldId)->text,QString("Welcome"));QVERIFY(!play.graphicPlayback(e,8000).visible);play.advanceElementCue(e.id);QCOMPARE(play.graphicPlayback(e,5000).boardStep,1);play.triggerElementAnimation(e.id,{});QVERIFY(!play.graphicPlayback(e,5000).visible);
        auto *page=studioCueBoard(&lib,&play,e.id);page->resize(560,1050);page->show();auto *tree=page->findChild<QListWidget*>("cueBoardList");QVERIFY(tree);QCOMPARE(tree->count(),1);tree->setCurrentItem(tree->item(0));auto *text=page->findChild<QPlainTextEdit*>("cueFieldValue");QVERIFY(text);QCOMPARE(text->toPlainText(),QString("Hello"));text->setPlainText("First repetition edited privately");QCOMPARE(lib.element(e.id)->cueBoard,e.cueBoard);QVERIFY(!play.graphicPlayback(*lib.element(e.id),5000).visible);page->findChild<QPushButton*>("saveCueBoard")->click();QVERIFY(!page->property("dirty").toBool());QCOMPARE(lib.element(e.id)->design,e.design);auto saved=compileCueBoard(design,lib.element(e.id)->cueBoard);QCOMPARE(ElementDesign::fromJson(saved.steps[0].design).find(fieldId)->text,QString("First repetition edited privately"));QCOMPARE(ElementDesign::fromJson(saved.steps[1].design).find(fieldId)->text,QString("Welcome"));QTest::qWait(600);QVERIFY(page->grab().save("artifacts/studio-cue-board.png"));delete page;play.stop();
    }
    void animationCuesHoldExitAndSchedule(){
        Library library;ProgramItem program;program.id="show";program.seconds=20;ProgramItem spot;spot.id="spot";spot.kind="Ad";spot.adId="advert";spot.seconds=5;library.programs={program,spot};Ad ad;ad.id="advert";ad.seconds=5;library.ads={ad};
        auto design=ElementDesign::defaultScene();design.durationMs=2000;design.animationCues={QJsonObject{{"id","in"},{"name","Entrance"},{"start",0},{"end",1000},{"hold",true},{"trigger","Manual"}},QJsonObject{{"id","out"},{"name","Exit"},{"start",1000},{"end",2000},{"hold",false},{"trigger","Before ad"},{"triggerMs",10000}}};
        Element element;element.id="logo";element.type="Custom";element.design=design.toJson();library.elements={element};Playout play(&library);
        QVERIFY(!play.graphicPlayback(element,0).visible);play.triggerElementAnimation("logo","in");auto entrance=play.graphicPlayback(element,500);QVERIFY(entrance.visible);QCOMPARE(entrance.time,qint64(500));
        auto hold=play.graphicPlayback(element,5000);QVERIFY(hold.visible);QCOMPARE(hold.time,qint64(1000));QVERIFY(!hold.advancing);
        auto exit=play.graphicPlayback(element,10500);QCOMPARE(exit.cue,QString("out"));QVERIFY(exit.visible);QCOMPARE(exit.time,qint64(1500));QVERIFY(!play.graphicPlayback(element,11000).visible);
        play.start(0,12000);play.togglePause();play.triggerElementAnimation("logo","in");QCOMPARE(play.graphicPlayback(element,play.timelinePosMs()).cue,QString("in"));
        play.triggerElementAnimation("logo",{});QVERIFY(!play.graphicPlayback(element,play.timelinePosMs()).visible);play.stop();
        QCOMPARE(ElementDesign::fromJson(element.design).animationCues,design.animationCues);element.cueBoard={newCueBoardEntry(design.animationCues.first().toObject())};library.elements={element};play.triggerElementAnimation(element.id,"@board:0");QCOMPARE(play.graphicPlayback(element,500).boardStep,0);QCOMPARE(play.graphicPlayback(element,10500).cue,QString("out"));play.triggerElementAnimation(element.id,{});QVERIFY(!play.graphicPlayback(element,500).visible);QCOMPARE(play.graphicPlayback(element,10500).cue,QString("out"));
    }
    void screenElementControls(){
        Library library;auto design=ElementDesign::defaultScene();design.animationCues={QJsonObject{{"id","entrance"},{"name","Logo entrance"},{"start",0},{"end",1000},{"hold",true}}};Element element;element.id="screen";element.name="Channel logo";element.type="Custom";element.design=design.toJson();library.elements={element};
        Playout play(&library);EditorPanel editor(&library,&play);editor.setStyleSheet("QWidget{background:#303135;color:#dedee3;font-size:11px;}QPushButton,QComboBox,QLineEdit{background:#45474d;border:0;border-radius:4px;padding:4px;}QScrollBar:vertical{width:6px;}");editor.resize(620,850);editor.show();editor.showTool(3);QTest::qWait(30);
        QListWidgetItem *item=nullptr;QListWidget *list=nullptr;for(auto *candidate:editor.findChildren<QListWidget*>())for(int i=0;i<candidate->count();++i)if(candidate->item(i)->data(Qt::UserRole).toString()=="screen"){list=candidate;item=candidate->item(i);}
        QVERIFY(list&&item);list->setCurrentItem(item);QTest::qWait(30);QPushButton *trigger=nullptr;for(auto *b:editor.findChildren<QPushButton*>())if(b->text()=="Replay cue")trigger=b;QVERIFY(trigger);for(auto *toggle:editor.findChildren<QToolButton*>())if(toggle->text()=="Animation schedules")toggle->setChecked(true);QVERIFY(trigger->isVisible());QTest::mouseClick(trigger,Qt::LeftButton);QCOMPARE(play.graphicPlayback(library.elements.first(),0).cue,QString("entrance"));
        QDir().mkpath("artifacts");QVERIFY(editor.grab().save("artifacts/screen-elements.png"));
    }
    void studioContentPreviewAndPlacement(){
        Library library;ProgramItem program;program.id="sponsor-show";program.seconds=120;library.programs={program};Element element;element.id="sponsor";element.name="Sponsor spotlight";element.type="Custom";auto source=elementPreset(element.name);source.animationCues={};element.design=source.toJson();element.previewMs=1000;refreshElementThumbnail(element);library.elements={element};
        Playout play(&library);auto *page=studioElementsPage(&library,&play);page->resize(640,1100);page->show();auto *list=page->findChild<QListWidget*>("screenElementCards");QVERIFY(list);list->setCurrentRow(0);QTest::qWait(30);auto *input=page->findChild<QPlainTextEdit*>();QVERIFY(input);const auto original=library.elements.first().design;input->setPlainText("Sponsored by Microsoft");QCOMPARE(library.elements.first().design,original);QPushButton *apply=nullptr,*place=nullptr;for(auto *b:page->findChildren<QPushButton*>()){if(b->text()=="Apply content")apply=b;if(b->text()=="Place on timeline")place=b;}QVERIFY(apply&&place);apply->click();QVERIFY(library.elements.first().design!=original);auto design=ElementDesign::fromJson(library.elements.first().design);bool changed=false;for(auto &o:design.objects)if(o.type=="Text"){QCOMPARE(o.text,QString("Sponsored by Microsoft"));QVERIFY(!o.tracks.isEmpty());changed=true;}QVERIFY(changed);
        auto *time=page->findChild<QDoubleSpinBox*>("elementPlacementTime");QVERIFY(time);time->setValue(12.5);place->click();QCOMPARE(library.placements.size(),1);QVERIFY(qAbs(library.placements.first().startMs-12500)<=40);play.start(0,20000);play.togglePause();time->setValue(2);place->click();QCOMPARE(library.placements.size(),1);play.stop();
        QVERIFY(page->grab().save("artifacts/studio-template-fields.png"));delete page;
    }
    void dashboardMetrics(){
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));connect(&server,&QTcpServer::newConnection,&server,[&]{auto *socket=server.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,socket,[socket]{socket->readAll();const QByteArray body="paths_readers{name=\"cuebase\",readerType=\"rtmpConn\"} 2\npaths_readers{name=\"cuebase\",readerType=\"hlsMuxer\"} 1\nhls_sessions{id=\"one\",path=\"cuebase\"} 1\npaths_readers{name=\"other\",readerType=\"rtmpConn\"} 10\n";socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);});
        Library library;Playout play(&library);auto *page=studioDashboard(&play);page->resize(600,800);page->show();auto lines=page->findChildren<QLineEdit*>();QVERIFY(lines.size()>=2);lines[0]->setText(QString("http://127.0.0.1:%1/metrics").arg(server.serverPort()));lines[1]->setText("cuebase");QPushButton *button=nullptr;for(auto *b:page->findChildren<QPushButton*>())if(b->text()=="Connect metrics")button=b;QVERIFY(button);button->click();auto received=[=]{for(auto *label:page->findChildren<QLabel*>())if(label->text()=="3 receiver connections")return true;return false;};QTRY_VERIFY_WITH_TIMEOUT(received(),10000);QVERIFY(page->grab().save("artifacts/studio-monitor.png"));QVERIFY(!lines[0]->isEnabled());button->click();QVERIFY(lines[0]->isEnabled());lines[1]->setText("other");bool isolated=false;for(auto *label:page->findChildren<QLabel*>())if(label->text().startsWith("Peak connections: —"))isolated=true;QVERIFY(isolated);button->click();auto other=[=]{for(auto *label:page->findChildren<QLabel*>())if(label->text()=="10 receiver connections")return true;return false;};QTRY_VERIFY_WITH_TIMEOUT(other(),10000);delete page;
    }
    void studioLayout(){
        Library library;ProgramItem program;program.id="layout-show";program.title="Evening edition";program.seconds=120;library.programs={program};Element element;element.id="layout-sponsor";element.name="Sponsor spotlight";element.type="Custom";element.design=elementPreset(element.name).toJson();element.previewMs=1000;refreshElementThumbnail(element);library.elements={element};library.save();
        MainWindow window;window.resize(1600,1000);window.show();auto *editor=window.findChild<EditorPanel*>();QVERIFY(editor);editor->showTool(3);QTest::qWait(60);auto *cards=editor->findChild<QListWidget*>("screenElementCards");QVERIFY(cards);cards->setCurrentRow(0);QTest::qWait(60);QVERIFY(window.grab().save("artifacts/studio-workspace.png"));editor->showTool(4);QTest::qWait(60);QVERIFY(window.grab().save("artifacts/studio-monitor-workspace.png"));window.hide();
    }
    void cleanupTestCase(){QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/cuebase.json");QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/broadcast-metrics.json");}
    void pausedAnimation(){
        Library library;library.display.watermarkOn=false;library.display.safeArea=false;
        ProgramItem program;program.id="clock";program.seconds=30;library.programs={program};
        auto design=ElementDesign::defaultScene();DObject shape;shape.id="animated";shape.type="Shape";shape.w=2;shape.h=2;shape.base.rot={90,0,0};shape.mat.color=Qt::red;shape.mat.shadeless=true;shape.keyChannel("position.x",0,-3);shape.keyChannel("position.x",2000,3);design.objects.append(shape);
        Element element;element.id="animated";element.type="Custom";element.live=true;element.design=design.toJson();library.elements={element};
        Playout play(&library);ViewportWidget viewport(&library,&play);play.start();play.togglePause();auto frame=viewport.programFrame({320,180});QTest::qWait(180);QCOMPARE(viewport.programFrame({320,180}),frame);QTest::qWait(180);QCOMPARE(viewport.programFrame({320,180}),frame);play.stop();
    }
    void loopback_data(){QTest::addColumn<QString>("protocol");QTest::newRow("RTMP")<<QString("rtmp");QTest::newRow("SRT")<<QString("srt");}
    void loopback(){
        QFETCH(QString,protocol);QTemporaryDir temp;QVERIFY(temp.isValid());const QString ffmpeg=NetworkOutput::findEncoder();QVERIFY(!ffmpeg.isEmpty());
        QFile tone(temp.filePath("tone.wav"));QVERIFY(tone.open(QIODevice::WriteOnly));
        QDataStream wav(&tone);wav.setByteOrder(QDataStream::LittleEndian);
        const quint32 bytes=48000*8*4;wav.writeRawData("RIFF",4);wav<<quint32(36+bytes);wav.writeRawData("WAVEfmt ",8);wav<<quint32(16)<<quint16(1)<<quint16(2)<<quint32(48000)<<quint32(192000)<<quint16(4)<<quint16(16);wav.writeRawData("data",4);wav<<bytes;
        for(int i=0;i<48000*8;++i){qint16 sample=qint16(6000*std::sin(i*2*3.141592653589793*440/48000));wav<<sample<<sample;}tone.close();
        quint16 port=0;{if(protocol=="rtmp"){QTcpServer reserve;QVERIFY(reserve.listen(QHostAddress::LocalHost,0));port=reserve.serverPort();}else{QUdpSocket reserve;QVERIFY(reserve.bind(QHostAddress(QHostAddress::LocalHost),quint16(0)));port=reserve.localPort();}}
        const QString target=protocol=="rtmp"?QString("rtmp://127.0.0.1:%1/live/test").arg(port):QString("srt://127.0.0.1:%1?mode=caller&latency=120000").arg(port);
        const QString received=temp.filePath(protocol=="rtmp"?"received.flv":"received.ts");
        QProcess server;QByteArray serverLog;
        if(protocol=="rtmp"){
            const QString executable=QCoreApplication::applicationDirPath()+"/../tools/mediamtx/mediamtx.exe";QVERIFY2(QFileInfo::exists(executable),"Install the local MediaMTX test receiver in tools/mediamtx.");
            QFile config(temp.filePath("mediamtx.yml"));QVERIFY(config.open(QIODevice::WriteOnly));config.write(QString("logLevel: info\nreadTimeout: 30s\nwriteTimeout: 30s\nrtsp: no\nhls: no\nwebrtc: no\nmoq: no\nsrt: no\nrtmp: yes\nrtmpAddress: 127.0.0.1:%1\npaths:\n  all_others:\n").arg(port).toUtf8());config.close();
            server.setProcessChannelMode(QProcess::MergedChannels);server.start(executable,{config.fileName()});QVERIFY(server.waitForStarted(10000));QByteArray log;
            QTRY_VERIFY_WITH_TIMEOUT((log+=server.readAll()).contains("started with listener"),15000);
        }
        QStringList args{"-hide_banner","-loglevel","error"};if(protocol=="rtmp")args<<"-rtmp_live"<<"live";args<<"-i"<<(protocol=="rtmp"?target:QString("srt://127.0.0.1:%1?mode=listener&latency=120000").arg(port))<<"-t"<<"3"<<"-c"<<"copy"<<"-y"<<received;
        QProcess receiver;if(protocol!="rtmp"){receiver.start(ffmpeg,args);QVERIFY(receiver.waitForStarted(10000));QTest::qWait(1000);}
        NetworkOutput output;int frames=0;output.setFrameProvider([&](QSize size){QImage image(size,QImage::Format_ARGB32);image.fill(QColor(30+(frames++%100),90,190));return image;});
        QMediaPlayer player;player.setLoops(QMediaPlayer::Infinite);output.attachPlayer(&player);player.setSource(QUrl::fromLocalFile(temp.filePath("tone.wav")));player.play();
        QTRY_VERIFY2_WITH_TIMEOUT(player.position()>0,qPrintable(player.errorString()),90000);
        StreamSettings settings;settings.executable=ffmpeg;settings.endpoint=target;settings.size={320,180};settings.fps=25;settings.videoKbps=500;settings.reconnects=0;
        QVERIFY(output.start(settings));QTRY_VERIFY_WITH_TIMEOUT(output.streaming()||!output.active(),45000);QVERIFY2(output.streaming(),qPrintable(output.status()+"\nReceiver: "+QString::fromUtf8(server.readAll())));
        if(protocol=="rtmp"){QTRY_VERIFY2_WITH_TIMEOUT((serverLog+=server.readAll()).contains("is publishing"),serverLog.constData(),15000);receiver.start(ffmpeg,args);QVERIFY(receiver.waitForStarted(10000));}
        QTRY_VERIFY_WITH_TIMEOUT(receiver.state()==QProcess::NotRunning,20000);const auto outputStatus=output.status();output.stop();player.stop();if(server.state()!=QProcess::NotRunning){server.kill();server.waitForFinished(5000);}
        const auto errors=receiver.readAllStandardError()+outputStatus.toUtf8()+serverLog+server.readAll();QVERIFY2(QFileInfo(received).size()>1000,errors.constData());
        const QString ffprobe=QFileInfo(ffmpeg).dir().filePath("ffprobe.exe");QProcess probe;probe.start(ffprobe,{"-v","error","-show_streams","-of","json",received});QVERIFY(probe.waitForFinished(10000));auto streams=QJsonDocument::fromJson(probe.readAllStandardOutput()).object()["streams"].toArray();bool video=false,audio=false;
        for(const auto &value:streams){auto stream=value.toObject();if(stream["codec_type"]=="video"){video=true;QCOMPARE(stream["codec_name"].toString(),QString("h264"));QCOMPARE(stream["width"].toInt(),320);QCOMPARE(stream["height"].toInt(),180);}if(stream["codec_type"]=="audio"){audio=true;QCOMPARE(stream["codec_name"].toString(),QString("aac"));}}
        QVERIFY(video);QVERIFY(audio);QProcess decode;decode.start(ffmpeg,{"-hide_banner","-loglevel","error","-i",received,"-map","0:a:0","-f","s16le","-ac","1","-ar","48000","pipe:1"});QVERIFY(decode.waitForFinished(10000));auto samples=decode.readAllStandardOutput();QVERIFY(samples.size()>48000);qint64 energy=0;for(int i=0;i+1<samples.size();i+=2){qint16 sample=qint16(quint8(samples[i])|(quint16(quint8(samples[i+1]))<<8));energy+=qAbs(int(sample));}QVERIFY2(energy/(samples.size()/2)>100,"Stream audio was silent");
        QDir().mkpath("artifacts");QFile::remove(QString("artifacts/loopback-%1.%2").arg(protocol,protocol=="rtmp"?"flv":"ts"));QVERIFY(QFile::copy(received,QString("artifacts/loopback-%1.%2").arg(protocol,protocol=="rtmp"?"flv":"ts"))||QFileInfo::exists(QString("artifacts/loopback-%1.%2").arg(protocol,protocol=="rtmp"?"flv":"ts")));
        Library replayLibrary;ProgramItem recorded;recorded.id="recorded";recorded.kind="Video";recorded.file=received;recorded.seconds=3;replayLibrary.programs={recorded};Playout replayPlay(&replayLibrary);replayPlay.start(0,2000);replayPlay.togglePause();QString reason;
        auto *replay=replayPlay.replay();QVERIFY2(replay->start(0,1800,"preset:Fade","preset:Fade",false,&reason),qPrintable(reason));
        QTRY_VERIFY_WITH_TIMEOUT(!replay->frame.isNull(),15000);QVERIFY(replay->position<1800);QCOMPARE(replayPlay.state(),Playout::Paused);replay->cancel();replayPlay.stop();
    }
};
QTEST_MAIN(StreamTests)
#include "StreamTests.moc"
