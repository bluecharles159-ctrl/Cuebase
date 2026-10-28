#include <QListWidget>
#include "UiTheme.h"
#include <QtTest>
#include "ColorManagement.h"
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
#include "TimelineMedia.h"
#include <QCheckBox>
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
#include "ProductionBoard.h"
#include "PlanHandoff.h"
#include "PlanningCanvas.h"
#include <QTextEdit>
#include <QMenu>
#include <QDialogButtonBox>
#include <QTimer>
#include <QPointer>
#include <QMediaPlayer>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include "UiControls.h"
#include <QTcpSocket>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>

class StreamTests:public QObject{
    Q_OBJECT
private slots:
    void hoverCutUsesProgramHistory(){
        Library library;ProgramItem program;program.id="hover-cut";program.title="Cut me";program.seconds=8;library.programs={program};Playout play(&library);TimelineWidget timeline(&play,&library);timeline.resize(800,240);timeline.show();QTest::qWait(20);
        QSignalSpy cuts(&timeline,&TimelineWidget::cutRequested);connect(&timeline,&TimelineWidget::cutRequested,&library,[&](const QString &id,int seconds){library.cutProgram(id,seconds);});
        QTest::mouseMove(&timeline,QPoint(timeline.blockX(0)+timeline.blockW(0)/2,50));QTest::keyClick(&timeline,Qt::Key_C);QCOMPARE(cuts.size(),1);QCOMPARE(library.programs.size(),2);QVERIFY(library.undoProgramCut());QCOMPARE(library.programs.size(),1);QVERIFY(library.redoProgramCut());QCOMPARE(library.programs.size(),2);
    }
    void timelineSourcePreviews(){
        applyUiTheme("Studio Blue",false);
        QTemporaryDir temp;QVERIFY(temp.isValid());const auto file=temp.filePath("aligned.mkv");QProcess generate;
        generate.start(NetworkOutput::findEncoder(),{"-hide_banner","-loglevel","error","-f","lavfi","-i","color=c=red:s=160x90:r=10:d=1","-f","lavfi","-i","color=c=blue:s=160x90:r=10:d=1","-f","lavfi","-i",R"(aevalsrc=if(lt(t\,1)\,0\,0.7*sin(2*PI*440*t)):s=8000:d=2)","-filter_complex","[0:v][1:v]concat=n=2:v=1:a=0[v]","-map","[v]","-map","2:a","-threads","1","-c:v","ffv1","-c:a","pcm_s16le",file});QVERIFY(generate.waitForFinished(30000));QVERIFY2(generate.exitCode()==0,generate.readAllStandardError().constData());
        TimelineMedia media;QTRY_VERIFY2_WITH_TIMEOUT(!media.thumbnail(file,0).isNull(),qPrintable(media.property("lastError").toString()),90000);QVERIFY(media.thumbnail(file,0).pixelColor(48,27).red()>200);
        QTRY_VERIFY_WITH_TIMEOUT(!media.thumbnail(file,1500).isNull(),90000);QVERIFY(media.thumbnail(file,1500).pixelColor(48,27).blue()>200);
        QTRY_VERIFY_WITH_TIMEOUT(!media.waveform(file,0,2000).peaks.isEmpty(),90000);const auto wave=media.waveform(file,0,2000);float quiet=0,loud=0;for(int i=10;i<220;++i)quiet=qMax(quiet,wave.peaks[i]);for(int i=300;i<500;++i)loud=qMax(loud,wave.peaks[i]);QVERIFY(quiet<.05f);QVERIFY(loud>.3f);
        QTRY_VERIFY_WITH_TIMEOUT(!media.waveform(file,1000,1000).peaks.isEmpty(),90000);QVERIFY(media.waveform(file,1000,1000).peaks[100]>.3f);
        // A fresh preview provider must reuse decoded media from disk.
        TimelineMedia reopened;QVERIFY(!reopened.thumbnail(file,0).isNull());QVERIFY(reopened.thumbnail(file,1500).pixelColor(48,27).blue()>200);QCOMPARE(reopened.waveform(file,0,2000).peaks,wave.peaks);QCOMPARE(reopened.property("decodeCount").toInt(),0);
        QSettings settings;settings.setValue("studio/timelineThumbnails",true);settings.setValue("studio/timelineWaveforms",true);settings.setValue("studio/fixedPlayhead",false);settings.setValue("studio/timelineScale",260.);
        Library library;ProgramItem red;red.id="red";red.title="First second";red.file=file;red.seconds=1;ProgramItem blue=red;blue.id="blue";blue.title="Trimmed second";blue.offsetMs=1000;library.programs={red,blue};Playout play(&library);TimelineWidget timeline(&play,&library);timeline.resize(800,240);timeline.show();QTest::qWait(30);
        auto firstColour=[&]{return timeline.grab().toImage().pixelColor(timeline.blockX(0)+35,80);};auto secondColour=[&]{return timeline.grab().toImage().pixelColor(timeline.blockX(1)+35,80);};
        QTRY_VERIFY_WITH_TIMEOUT(firstColour().red()>180&&firstColour().blue()<80,90000);QTRY_VERIFY_WITH_TIMEOUT(secondColour().blue()>180&&secondColour().red()<80,90000);
        auto lastColour=[&]{return timeline.grab().toImage().pixelColor(timeline.blockX(1)+240,80);};QTRY_VERIFY_WITH_TIMEOUT(lastColour().blue()>180&&lastColour().red()<80,90000);
        // Detect visible amplitude outside the center baseline, allowing antialiased edges.
        auto waveInk=[&](int index){const auto picture=timeline.grab().toImage();int ink=0;for(int y=124;y<141;++y)if(qAbs(y-132)>2){const auto color=picture.pixelColor(timeline.blockX(index)+50,y);ink=qMax(ink,color.green()-color.red());}return ink;};
        QTRY_VERIFY_WITH_TIMEOUT(waveInk(1)>35,90000);QCOMPARE(waveInk(0),0);QVERIFY(timeline.grab().save("artifacts/timeline-source-previews.png"));
        const auto expanded=timeline.wantedHeight();showUiSettings(&timeline);auto *sections=timeline.findChild<QListWidget*>("settingsSections");QVERIFY(sections);sections->setCurrentRow(2);auto *thumbs=timeline.findChild<QCheckBox*>("timelineThumbnails");auto *waves=timeline.findChild<QCheckBox*>("timelineWaveforms");QVERIFY(thumbs);QVERIFY(waves);thumbs->setChecked(false);waves->setChecked(false);QCOMPARE(timeline.wantedHeight(),expanded-68);QVERIFY(!settings.value("studio/timelineThumbnails").toBool());
        thumbs->setChecked(true);waves->setChecked(true);QCOMPARE(timeline.wantedHeight(),expanded);auto *dialog=qobject_cast<QDialog*>(sections->window());QVERIFY(dialog);QVERIFY(dialog->grab().save("artifacts/timeline-settings.png"));dialog->close();
        settings.setValue("studio/timelineScale",14.);settings.setValue("studio/fixedPlayhead",true);
    }
    void planningHandoff(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-handoff");}~Profile(){qApp->setApplicationName(previous);}} profile;
        Library library;library.display.watermarkOn=false;ProgramItem original;original.id="original";original.title="Original";original.seconds=10;library.programs={original};
        Ad ad;ad.id="sponsor";ad.name="Sponsor";library.ads={ad};
        auto design=ElementDesign::defaultScene();DObject shape;shape.id="plate";shape.type="Shape";shape.w=8;shape.h=5;shape.base.rot={90,0,0};shape.mat.color=Qt::red;shape.mat.shadeless=true;design.objects.append(shape);
        Element graphic;graphic.id="graphic";graphic.name="Graphic";graphic.type="Custom";graphic.design=design.toJson();Element bumper;bumper.id="bumper";bumper.name="Break";bumper.type="Break";bumper.text="Next: {next}";library.elements={graphic,bumper};
        Playout live(&library);PlanHandoff handoff(&library,&live);ProductionPlan plan;plan.entries={{"one","Program",original.id,1250},{"two","Ad",ad.id,750},{"three","Element",graphic.id,1337},{"four","Element",bumper.id,500}};
        QString error;library.programs[0].file="missing-plan-source-does-not-exist.mp4";QVERIFY(!handoff.append(plan,&error));QCOMPARE(library.programs.size(),1);library.programs[0].file.clear();live.start();QVERIFY(!handoff.append(plan,&error));live.togglePause();QVERIFY(!handoff.append(plan,&error));live.stop();
        QVERIFY2(handoff.append(plan,&error),qPrintable(error));QCOMPARE(library.programs.size(),5);QCOMPARE(library.programs.first().id,original.id);QCOMPARE(live.totalMs(),qint64(13837));QCOMPARE(live.state(),Playout::Stopped);QVERIFY(handoff.canUndo());QCOMPARE(library.placements.size(),0);
        const auto graphicId=library.programs[3].id;QVERIFY(graphicId!=graphic.id);QCOMPARE(live.segments()[3].ms,1337);QCOMPARE(live.segments()[4].type,Segment::Break);QVERIFY(live.segments()[4].spot);QCOMPARE(live.segments()[4].text,"Next: "+library.display.channel);
        ViewportWidget viewport(&library,&live);live.start(3);live.togglePause();QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::red));live.stop();
        Library loaded;loaded.load();QCOMPARE(loaded.programs[3].elementId,graphic.id);QCOMPARE(loaded.programs[3].lengthMs(),qint64(1337));
        QVERIFY(handoff.undo(&error));QCOMPARE(library.programs.size(),1);QCOMPARE(live.totalMs(),qint64(10000));QVERIFY(!handoff.canUndo());
        QVERIFY(handoff.append(plan,&error));library.programs.last().title="A later user edit";emit library.programsChanged();QVERIFY(!handoff.undo(&error));QCOMPARE(library.programs.last().title,QString("A later user edit"));
        library.programs.move(3,0);emit library.programsChanged();live.start(0);live.togglePause();QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::red));live.stop();
        auto missing=plan;missing.entries[0].sourceId="missing";const auto count=library.programs.size();QVERIFY(!handoff.append(missing,&error));QCOMPARE(library.programs.size(),count);
    }
    void planningHandoffDesignMedia(){
        Library library;auto design=ElementDesign::defaultScene();
        DObject image;image.id="image";image.name="Sponsor photo";image.type="Image";
        QTemporaryDir directory;QVERIFY(directory.isValid());image.image=directory.filePath("missing.png");design.objects.append(image);
        Element element;element.id="graphic-media";element.name="Promo";element.type="Custom";element.design=design.toJson();library.elements={element};
        Playout live(&library);PlanHandoff handoff(&library,&live);ProductionPlan plan;plan.entries={{"graphic-entry","Element",element.id,1000}};QString error;
        const auto before=library.programs.size();QVERIFY(!handoff.append(plan,&error));QVERIFY(error.contains(image.name));QVERIFY(error.contains(image.image));QCOMPARE(library.programs.size(),before);QVERIFY(!handoff.canUndo());
        QImage asset(4,4,QImage::Format_ARGB32);asset.fill(Qt::red);QVERIFY(asset.save(image.image));QVERIFY2(handoff.validate(plan,&error),qPrintable(error));
        design.objects.last().style["audio.file"]=directory.filePath("missing.wav");library.elements[0].design=design.toJson();QVERIFY(!handoff.validate(plan,&error));QVERIFY(error.contains("missing.wav"));
        auto nested=design;design.objects.last().style.remove("audio.file");
        QJsonObject cue{{"id","intro"},{"start",0},{"end",1000}};design.animationCues={cue};
        auto entry=newCueBoardEntry(cue);entry["sections"]=QJsonArray{QJsonObject{{"design",nested.toJson()}}};
        library.elements[0].design=design.toJson();library.elements[0].cueBoard={entry};
        QVERIFY(!handoff.append(plan,&error));QVERIFY(error.contains("missing.wav"));QCOMPARE(library.programs.size(),before);
        entry["disabled"]=true;library.elements[0].cueBoard={entry};QVERIFY2(handoff.validate(plan,&error),qPrintable(error));
        entry["disabled"]=false;entry["cue"]="deleted-cue";library.elements[0].cueBoard={entry};QVERIFY(!handoff.validate(plan,&error));QVERIFY(error.contains("no longer exists"));
    }
    void planningHandoffSaveFailure(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-handoff-failure");}~Profile(){qApp->setApplicationName(previous);}} profile;
        const auto destination=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/cuebase.json";QVERIFY(QDir().mkpath(destination));
        Library library;ProgramItem p;p.id="original";library.programs={p};Playout live(&library);PlanHandoff handoff(&library,&live);ProductionPlan plan;plan.entries={{"item","Program",p.id,1337}};QString error;
        QVERIFY(!handoff.append(plan,&error));QVERIFY(!error.isEmpty());QCOMPARE(library.programs.size(),1);QCOMPARE(live.totalMs(),qint64(60000));QVERIFY(!handoff.canUndo());QVERIFY(QDir().rmdir(destination));
    }
    void planningReviewDialog(){
        applyUiTheme("Studio Blue",false);
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-handoff-dialog");}~Profile(){qApp->setApplicationName(previous);}} profile;
        Library library;ProgramItem p;p.id="show";p.title="Reviewed programme";library.programs={p};Playout live(&library);ProductionBoard board(&library);board.setBroadcastPlayout(&live);QVERIFY(board.appendSource("Program",p.id));
        auto *send=board.findChild<QPushButton*>("planSend");auto *undo=board.findChild<QPushButton*>("planUndoSend");QVERIFY(send);QVERIFY(undo);bool reviewed=false;
        QTimer::singleShot(0,&board,[&]{auto *dialog=qobject_cast<QDialog*>(qApp->activeModalWidget());if(!dialog)return;reviewed=dialog->grab().save("artifacts/plan-handoff-review.png");auto *buttons=dialog->findChild<QDialogButtonBox*>();if(buttons)buttons->button(QDialogButtonBox::Save)->click();else dialog->reject();});
        send->click();QVERIFY(reviewed);QCOMPARE(library.programs.size(),2);QVERIFY(undo->isEnabled());undo->click();QCOMPARE(library.programs.size(),1);QCOMPARE(live.state(),Playout::Stopped);
    }
    void exactProgramDuration(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-exact-duration");}~Profile(){qApp->setApplicationName(previous);}} profile;
        Library library;ProgramItem p;p.id="exact";p.seconds=3;p.durationMs=2501;library.programs={p};Playout play(&library);QCOMPARE(play.totalMs(),qint64(2501));
        const auto second=library.cutProgram(p.id,1,false);QVERIFY(!second.isEmpty());QCOMPARE(library.programs[0].lengthMs(),qint64(1000));QCOMPARE(library.programs[1].lengthMs(),qint64(1501));QCOMPARE(play.totalMs(),qint64(2501));
        Library loaded;loaded.load();QCOMPARE(loaded.programs[1].lengthMs(),qint64(1501));QCOMPARE(loaded.programs[1].offsetMs,1000);
    }
    void planningPreviewIsolation(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-planning");}~Profile(){qApp->setApplicationName(previous);}} profile;
        applyUiTheme("Studio Blue",false);
        Library library;ProgramItem program;program.id="plan-program";program.title="Morning programme";program.kind="Lifestyle";program.seconds=60;library.programs={program};Ad ad;ad.id="plan-ad";ad.name="Sponsor spot";ad.seconds=15;library.ads={ad};Element element;element.id="plan-element";element.name="Coming next";element.type="Custom";element.design=elementPreset("News lower third").toJson();library.elements={element};library.production["sentinel"]="keep";
        Playout live(&library);live.start();live.togglePause();const auto livePosition=live.timelinePosMs();
        ProductionBoard board(&library);QVERIFY(board.previewLibrary()!=&library);QVERIFY(board.previewPlayout()!=&live);QVERIFY(board.appendSource("Program",program.id));QVERIFY(board.appendSource("Ad",ad.id));QVERIFY(board.appendSource("Element",element.id));
        auto plan=board.document();plan.entries.last().durationMs=1337;QVERIFY(board.applyDocument(plan));QCOMPARE(board.previewPlayout()->totalMs(),qint64(76337));QCOMPARE(library.programs.size(),1);QCOMPARE(library.placements.size(),0);QCOMPARE(library.programs.first().seconds,60);
        board.previewAt(60005,true);QCOMPARE(board.previewPlayout()->current()->type,Segment::Ad);QCOMPARE(board.previewPlayout()->segmentElapsed(),qint64(5));QCOMPARE(live.state(),Playout::Paused);QCOMPARE(live.timelinePosMs(),livePosition);
        PlanCard card;card.id="plan-note";card.html="<b>Check sponsor approval</b>";card.rect={70,190,260,150};plan.cards.append(card);QVERIFY(plan.addLink({"plan-link",{card.id,1},{plan.entries[1].id,3},"Dashed"}));QVERIFY(board.applyDocument(plan));QCOMPARE(board.previewPlayout()->state(),Playout::Playing);board.undoPlan();QCOMPARE(board.previewPlayout()->state(),Playout::Playing);QVERIFY(board.document().cards.isEmpty());board.redoPlan();QCOMPARE(board.document().cards.size(),1);
        QString error;QVERIFY2(board.savePlan(&error),qPrintable(error));QCOMPARE(library.production["sentinel"].toString(),QString("keep"));QCOMPARE(library.production["planningBoard"].toObject(),board.document().toJson());
        Library reloaded;reloaded.load();ProductionBoard restored(&reloaded);QCOMPARE(restored.document().toJson(),board.document().toJson());
        board.resize(1100,760);board.show();QTest::qWait(230);QVERIFY(board.grab().save("artifacts/production-board.png"));board.done(QDialog::Accepted);QCOMPARE(board.previewPlayout()->state(),Playout::Stopped);QCOMPARE(live.state(),Playout::Paused);
    }
    void planningCanvasGestures(){
        PlanningCanvas canvas;canvas.resize(1000,620);ProductionPlan latest;connect(&canvas,&PlanningCanvas::documentEdited,&canvas,[&](const ProductionPlan &plan){latest=plan;});canvas.show();QTest::qWait(20);
        auto drag=[&](QPointF from,QPointF to){QTest::mousePress(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,canvas.mapFromScene(from));QTest::mouseMove(canvas.viewport(),canvas.mapFromScene(to),30);QTest::mouseRelease(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,canvas.mapFromScene(to));};
        drag({80,190},{330,350});QCOMPARE(latest.cards.size(),1);QTextEdit *editor=nullptr;for(auto *item:canvas.scene()->items())if(auto *proxy=qgraphicsitem_cast<QGraphicsProxyWidget*>(item))editor=proxy->widget()->findChild<QTextEdit*>("planNoteText");QVERIFY(editor);QTest::keyClicks(editor,"Production note");canvas.finishEditing();QVERIFY(latest.cards.first().html.contains("Production note"));
        drag({100,200},{160,260});QCOMPARE(latest.cards.first().rect.topLeft(),QPointF(140,250));
        PlanCard next;next.id="second-note";next.rect={500,190,250,160};latest.cards.append(next);canvas.setDocument(latest);const auto first=latest.cards.first();drag({first.rect.right(),first.rect.center().y()},{500,270});QCOMPARE(latest.links.size(),1);QCOMPARE(latest.links.first().from.id,first.id);QCOMPARE(latest.links.first().to.id,next.id);
        drag({first.rect.left()+20,first.rect.top()+10},{first.rect.left()+20,40});QVERIFY(latest.cards.first().rect.top()>=ProductionPlan::cardTop);QVERIFY(latest.validate());QVERIFY(canvas.grab().save("artifacts/planning-canvas.png"));
        bool opened=false;QTimer::singleShot(0,&canvas,[&]{auto *menu=qobject_cast<QMenu*>(qApp->activePopupWidget());if(!menu)return;opened=true;QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().first()).center());});
        QTest::mouseClick(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,canvas.mapFromScene(next.rect.center()-QPointF(0,8)));QVERIFY(opened);
        editor=nullptr;for(auto *item:canvas.scene()->items())if(auto *proxy=qgraphicsitem_cast<QGraphicsProxyWidget*>(item))editor=proxy->widget()->findChild<QTextEdit*>("planNoteText");QVERIFY(editor);QTest::keyClicks(editor,"Second note");canvas.finishEditing();QVERIFY(latest.cards.last().html.contains("Second note"));
    }
    void planningSurvivesCrewEdit(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-planning-crew");}~Profile(){qApp->setApplicationName(previous);}} profile;
        Library library;ProductionPlan plan;PlanCard card;card.id="protected-note";plan.cards={card};library.production["planningBoard"]=plan.toJson();CrewSession session(&library);const auto snapshot=session.snapshot();QString error;QVERIFY2(session.apply(snapshot,"Editor","Test editor",&error),qPrintable(error));QCOMPARE(library.production["planningBoard"].toObject(),plan.toJson());
    }
    void planningRecoveryPreservesOriginal(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-planning-recovery");}~Profile(){qApp->setApplicationName(previous);}} profile;
        Library library;ProgramItem program;program.id="recover-program";program.title="Programme";library.programs={program};const QJsonObject damaged{{"version",999},{"unrecognized","Keep this data"}};library.production["planningBoard"]=damaged;ProductionBoard board(&library);QString error;QVERIFY(!board.savePlan(&error));QVERIFY(!error.isEmpty());QVERIFY(!board.appendSource("Program",program.id));QCOMPARE(library.production["planningBoard"].toObject(),damaged);auto *recover=board.findChild<QPushButton*>("planRecover");QVERIFY(recover);recover->click();QCOMPARE(library.production["planningBoardRecovery"].toObject(),damaged);QVERIFY(board.appendSource("Program",program.id));QVERIFY(board.savePlan(&error));QVERIFY(board.document().validate());
    }
    void fixedCenterTimeline(){
        QSettings().setValue("studio/timelineScale",14.);QSettings().setValue("studio/fixedPlayhead",true);
        Library lib;ProgramItem program;program.id="center-show";program.title="Centered playhead";program.seconds=120;lib.programs={program};Playout play(&lib);
        QScrollArea scroll;scroll.setWidgetResizable(true);scroll.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll.resize(640,160);
        auto *timeline=new TimelineWidget(&play,&lib);scroll.setWidget(timeline);scroll.show();QTest::qWait(40);auto *bar=scroll.horizontalScrollBar();
        auto centered=[&]{return qAbs(timeline->blockX(0)+qRound(play.playheadMs()*14./1000)-bar->value()-scroll.viewport()->width()/2)<=1;};
        QVERIFY(centered());play.start(0,60000);play.togglePause();QTest::qWait(20);QVERIFY(centered());
        const auto broadcastTime=play.playheadMs();const int before=bar->value();const QPoint point(before+300,10);
        QTest::mousePress(timeline,Qt::MiddleButton,Qt::NoModifier,point);QTest::mouseMove(timeline,point+QPoint(50,0));QTest::mouseRelease(timeline,Qt::MiddleButton,Qt::NoModifier,point+QPoint(50,0));
        QVERIFY(!timeline->followingPlayhead());QCOMPARE(play.playheadMs(),broadcastTime);QCOMPARE(bar->value(),before-50);
        const int inspected=bar->value();play.seekTo(65000);QCOMPARE(bar->value(),inspected);timeline->revealPlayhead();QVERIFY(centered());
        scroll.resize(820,160);QTest::qWait(30);QVERIFY(centered());QVERIFY(scroll.grab().save("artifacts/fixed-center-studio-timeline.png"));
        lib.display.fps=30;lib.display.fpsBase=1.001;play.seekTo(0);
        QWheelEvent step(QPointF(100,10),timeline->mapToGlobal(QPoint(100,10)),{},QPoint(0,120),Qt::NoButton,Qt::AltModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(timeline,&step);QCOMPARE(play.playheadMs(),qint64(33));
        const auto unchanged=play.playheadMs();timeline->fitView();QVERIFY(!timeline->followingPlayhead());QCOMPARE(play.playheadMs(),unchanged);
        timeline->setFixedPlayhead(false);QCOMPARE(timeline->blockX(0),10);TimelineWidget restored(&play,&lib);QVERIFY(!restored.fixedPlayhead());
        timeline->setFixedPlayhead(true);QSettings().setValue("studio/timelineScale",14.);play.stop();
    }
    void overlayZIndexOrder(){Library lib;lib.display.watermarkOn=false;ProgramItem p;p.id="z-show";lib.programs={p};auto design=ElementDesign::defaultScene();DObject shape;shape.id="plate";shape.type="Shape";shape.w=8;shape.h=5;shape.base.rot={90,0,0};shape.mat.color=Qt::red;shape.mat.shadeless=true;design.objects.append(shape);Element red;red.id="red";red.type="Custom";red.live=true;red.design=design.toJson();red.cueBoardSettings["zIndex"]=10;Element blue=red;blue.id="blue";blue.cueBoardSettings["zIndex"]=0;design.find(shape.id)->mat.color=Qt::blue;blue.design=design.toJson();lib.elements={red,blue};Playout play(&lib);ViewportWidget viewport(&lib,&play);play.start();play.togglePause();QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::red));lib.elements[1].cueBoardSettings["zIndex"]=20;QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::blue));play.stop();}

    void overlayOrderEditorPersistence(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-overlay-order");}~Profile(){qApp->setApplicationName(previous);}} profile;
        Library lib;Element first;first.id="front";first.name="Front";first.cueBoardSettings["zIndex"]=5;Element second=first;second.id="back";second.name="Back";second.cueBoardSettings["zIndex"]=-2;lib.elements={first,second};Playout play(&lib);
        std::unique_ptr<QWidget> page(studioElementsPage(&lib,&play));page->resize(640,1000);page->show();auto *cards=page->findChild<QListWidget*>("screenElementCards");auto *order=page->findChild<QSpinBox*>("elementZIndex");QVERIFY(cards&&order);cards->setCurrentRow(0);QCOMPARE(order->value(),5);order->setValue(15);QMetaObject::invokeMethod(order,"editingFinished",Qt::DirectConnection);QCOMPARE(lib.elements[0].cueBoardSettings["zIndex"].toInt(),15);QCOMPARE(lib.elements[1].cueBoardSettings["zIndex"].toInt(),-2);
        Library reopened;reopened.load();QVERIFY(reopened.element(first.id));QCOMPARE(reopened.element(first.id)->cueBoardSettings["zIndex"].toInt(),15);cards->setCurrentRow(1);QCOMPARE(order->value(),-2);
        const auto destination=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/cuebase.json";QVERIFY(QFile::remove(destination));QVERIFY(QDir().mkpath(destination));order->setValue(33);QMetaObject::invokeMethod(order,"editingFinished",Qt::DirectConnection);QCOMPARE(lib.elements[1].cueBoardSettings["zIndex"].toInt(),-2);QCOMPARE(order->value(),-2);QVERIFY(QDir().rmdir(destination));
    }
    void placementOrderDraftAndRender(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-placement-order");}~Profile(){qApp->setApplicationName(previous);}} profile;
        Library lib;lib.display.watermarkOn=false;ProgramItem show;show.id="order-show";lib.programs={show};auto design=ElementDesign::defaultScene();DObject shape;shape.id="plate";shape.type="Shape";shape.w=8;shape.h=5;shape.base.rot={90,0,0};shape.mat.color=Qt::red;shape.mat.shadeless=true;design.objects.append(shape);Element red;red.id="red";red.type="Custom";red.design=design.toJson();red.cueBoardSettings["zIndex"]=10;Element blue=red;blue.id="blue";blue.live=true;blue.cueBoardSettings["zIndex"]=0;design.find(shape.id)->mat.color=Qt::blue;blue.design=design.toJson();lib.elements={red,blue};Placement copy;copy.id="copy";copy.refId=red.id;copy.name="Independent red";copy.durationMs=10000;lib.placements={copy};Playout play(&lib);ViewportWidget viewport(&lib,&play);play.start();play.togglePause();auto color=[&]{return viewport.programFrame({320,180}).pixelColor(160,90);};QCOMPARE(color(),QColor(Qt::red));
        QPointer<QDialog> dialog=placementOrderDialog(&lib,copy.id);QVERIFY(dialog);dialog->show();auto *inherit=dialog->findChild<QCheckBox*>("inheritPlacementOrder");auto *order=dialog->findChild<QSpinBox*>("placementZIndex");QVERIFY(inherit->isChecked());QVERIFY(!order->isEnabled());inherit->setChecked(false);order->setValue(-5);QCOMPARE(color(),QColor(Qt::red));dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QVERIFY2(lib.placement(copy.id)->zIndex.has_value(),qPrintable(dialog?dialog->findChild<QLabel*>("placementOrderError")->text():QString("No saved order")));QCOMPARE(lib.placement(copy.id)->zIndex.value(),-5);QCOMPARE(color(),QColor(Qt::blue));QCOMPARE(lib.element(red.id)->cueBoardSettings["zIndex"].toInt(),10);QCOMPARE(lib.placement(copy.id)->zIndex.value(),-5);
        Library restored;restored.load();QVERIFY(restored.placement(copy.id));QCOMPARE(restored.placement(copy.id)->zIndex.value(),-5);
        auto duplicate=*lib.placement(copy.id);duplicate.id="duplicate";duplicate.layer=2;duplicate.zIndex=20;lib.placements.append(duplicate);QCOMPARE(color(),QColor(Qt::red));lib.placements.removeLast();QCOMPARE(color(),QColor(Qt::blue));
        auto *reset=placementOrderDialog(&lib,copy.id);reset->show();reset->findChild<QCheckBox*>("inheritPlacementOrder")->setChecked(true);reset->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QVERIFY(!lib.placement(copy.id)->zIndex);QCOMPARE(color(),QColor(Qt::red));
        auto *cancelled=placementOrderDialog(&lib,copy.id);cancelled->findChild<QCheckBox*>("inheritPlacementOrder")->setChecked(false);cancelled->findChild<QSpinBox*>("placementZIndex")->setValue(-50);cancelled->reject();QVERIFY(!lib.placement(copy.id)->zIndex);
        auto *stale=placementOrderDialog(&lib,copy.id);stale->show();lib.placement(copy.id)->zIndex=3;stale->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QVERIFY(stale->isVisible());QCOMPARE(lib.placement(copy.id)->zIndex.value(),3);QVERIFY(stale->findChild<QLabel*>("placementOrderError")->text().contains("changed elsewhere"));stale->reject();
        lib.placement(copy.id)->boardSnapshot=bakeCueBoard(red);lib.elements[0].cueBoardSettings["zIndex"]=90;lib.placement(copy.id)->zIndex.reset();QCOMPARE(lib.placementElement(*lib.placement(copy.id)).cueBoardSettings["zIndex"].toInt(),10);lib.placement(copy.id)->zIndex=-7;QCOMPARE(lib.placementElement(*lib.placement(copy.id)).cueBoardSettings["zIndex"].toInt(),-7);
        const auto destination=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/cuebase.json";QVERIFY(QFile::remove(destination));QVERIFY(QDir().mkpath(destination));auto *failed=placementOrderDialog(&lib,copy.id);failed->show();failed->findChild<QSpinBox*>("placementZIndex")->setValue(100);failed->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QVERIFY(failed->isVisible());QCOMPARE(lib.placement(copy.id)->zIndex.value(),-7);QVERIFY(!failed->findChild<QLabel*>("placementOrderError")->text().isEmpty());failed->reject();QVERIFY(QDir().rmdir(destination));play.stop();
    }
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
    void rundownCueContinueIsIndependent(){
        Library lib;lib.display.watermarkOn=false;auto design=ElementDesign::defaultScene();
        QJsonObject intro{{"id","intro"},{"start",0},{"end",1000},{"hold",true},{"holdManual",true}},exit{{"id","exit"},{"start",1000},{"end",2000},{"hold",false}};
        design.animationCues={intro,exit};Element source;source.id="rundown-source";source.type="Custom";source.design=design.toJson();source.cueBoard={newCueBoardEntry(intro),newCueBoardEntry(exit)};lib.elements={source};
        ProgramItem first;first.id="first-copy";first.elementId=source.id;first.seconds=10;auto second=first;second.id="second-copy";second.offsetMs=500;lib.programs={first,second};
        Playout play(&lib);play.start(1,1500);play.togglePause();auto a=source,b=source;a.id="program:"+first.id;b.id="program:"+second.id;
        auto sample=[&](const Element &e,qint64 start,qint64 offset=0){return play.graphicPlayback(e,play.playheadMs(),start,offset);};
        QCOMPARE(sample(b,10000,500).boardStep,0);QSignalSpy ticks(&play,&Playout::ticked);
        play.triggerElementAnimation(a.id,{});QCOMPARE(ticks.count(),0); // An off-air copy cannot receive a rundown command.
        std::unique_ptr<QWidget> board(studioCueBoard(&lib,&play,source.id));
        auto *next=board->findChild<QPushButton*>("continueRundownCue");auto *hide=board->findChild<QPushButton*>("hideRundownCue");QVERIFY(next);QVERIFY(hide);QVERIFY(next->isEnabled());
        next->click();QCOMPARE(sample(b,10000,500).boardStep,1);QCOMPARE(sample(a,0).boardStep,0);QVERIFY(!play.graphicPlayback(source,play.playheadMs()).visible);
        hide->click();QVERIFY(!sample(b,10000,500).visible);QVERIFY(sample(a,0).visible);QCOMPARE(lib.elements[0].design,source.design);QCOMPARE(lib.elements[0].cueBoard,source.cueBoard);
        play.seekTo(10000);play.togglePause();QVERIFY(sample(b,10000,500).visible);QCOMPARE(sample(b,10000,500).boardStep,0);
        play.seekTo(12000);play.togglePause();QVERIFY(sample(b,10000,500).visible);QCOMPARE(sample(b,10000,500).boardStep,0);
        play.stop();QVERIFY(!next->isEnabled());play.start(1,1500);play.togglePause();QCOMPARE(sample(b,10000,500).boardStep,0);
        play.stop();lib.programs.move(1,0);emit lib.programsChanged();play.start(0,1500);play.togglePause();next->click();QCOMPARE(sample(b,0,500).boardStep,1);
        lib.elements.clear();ticks.clear();play.triggerElementAnimation(b.id,{});QCOMPARE(ticks.count(),0);play.stop();
    }
    void rundownCuePictureAndAudio(){
        QTemporaryDir directory;QVERIFY(directory.isValid());QFile file(directory.filePath("cue.wav"));QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream wav(&file);wav.setByteOrder(QDataStream::LittleEndian);const quint32 bytes=8000*4*2;
        wav.writeRawData("RIFF",4);wav<<quint32(36+bytes);wav.writeRawData("WAVEfmt ",8);wav<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);wav.writeRawData("data",4);wav<<bytes;for(int i=0;i<32000;++i)wav<<qint16(1000*std::sin(i*.3));file.close();
        Library lib;lib.display.watermarkOn=false;auto design=ElementDesign::defaultScene();DObject plate;plate.id="plate";plate.type="Shape";plate.w=8;plate.h=5;plate.base.rot={90,0,0};plate.mat.color=Qt::red;plate.mat.shadeless=true;design.objects.append(plate);
        DObject audio;audio.id="sound";audio.type="Audio";audio.style["audio.file"]=file.fileName();audio.style["audio.duration"]=4000.;design.objects.append(audio);
        QJsonObject cue{{"id","held"},{"start",0},{"end",1000},{"hold",true},{"holdManual",true}};design.animationCues={cue};Element e;e.id="audio-graphic";e.type="Custom";e.design=design.toJson();e.cueBoard={newCueBoardEntry(cue)};lib.elements={e};
        ProgramItem p;p.id="audio-copy";p.elementId=e.id;p.seconds=10;lib.programs={p};Playout play(&lib);ViewportWidget viewport(&lib,&play);
        QList<QPointer<QMediaPlayer>> players;connect(&play,&Playout::audioSourceCreated,&play,[&](QMediaPlayer *player,QAudioOutput*){players.append(player);});
        play.start(0,1500);play.togglePause();QCOMPARE(players.size(),1);QVERIFY(players[0]);QCOMPARE(players[0]->source(),QUrl::fromLocalFile(file.fileName()));QVERIFY(players[0]->playbackState()!=QMediaPlayer::PlayingState);
        QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::red));
        play.triggerElementAnimation("program:"+p.id,{});QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::black));QTRY_VERIFY(players[0].isNull());
        play.seekTo(0);play.togglePause();QList<QPointer<QMediaPlayer>> cuePlayers;for(const auto &player:players)if(player&&player->source()==QUrl::fromLocalFile(file.fileName()))cuePlayers.append(player);QCOMPARE(cuePlayers.size(),1);QCOMPARE(viewport.programFrame({320,180}).pixelColor(160,90),QColor(Qt::red));play.stop();QTRY_VERIFY(cuePlayers[0].isNull());
    }
    void studioBackgroundRenderParity(){
        QTemporaryDir temporary;QVERIFY(temporary.isValid());QImage media(64,32,QImage::Format_ARGB32);media.fill(Qt::blue);{QPainter painter(&media);painter.fillRect(0,0,32,32,Qt::red);}const auto path=temporary.filePath("background.png");QVERIFY(media.save(path));
        Library library;library.display.watermarkOn=false;ProgramItem program;program.id="background-show";library.programs={program};
        auto scene=ElementDesign::defaultScene();scene.world.transparent=false;scene.world.color=QColor("#393939");DObject plate;plate.id="background";plate.type="Shape";plate.w=10;plate.h=6;plate.base.rot={90,0,0};plate.mat.color=Qt::red;plate.mat.shadeless=true;plate.image=path;plate.style["fill.end"]="#245ad5";scene.objects.append(plate);
        auto glass=plate;glass.id="glass";glass.image.clear();glass.w=5;glass.h=3;glass.base.loc.setY(-.1);glass.mat.color=Qt::white;glass.mat.alpha=.2;glass.style={};glass.style["backdrop.blur"]=20.;glass.style["background.type"]="Solid";scene.objects.append(glass);
        Element element;element.id="background-design";element.type="Custom";element.live=true;library.elements={element};Playout play(&library);ViewportWidget monitor(&library,&play);play.start();play.togglePause();
        for(const auto &kind:QStringList{"Solid","Gradient","Media","Texture"}){
            auto *surface=scene.find(plate.id);surface->style["background.type"]=kind=="Texture"?"Solid":kind;if(kind=="Texture"){surface->style["texture.file"]=path;surface->style["texture.rotation"]=17;surface->style["texture.mapping"]="Tile";surface->style["texture.opacity"]=.7;surface->style["texture.blend"]="Screen";}else surface->style.remove("texture.file");library.elements[0].design=scene.toJson();
            QImage expected(640,360,QImage::Format_ARGB32);expected.fill(Qt::transparent);{QPainter painter(&expected);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);scene.render(painter,expected.rect(),0);}
            const auto actual=monitor.programFrame(expected.size());QVERIFY(actual.save("artifacts/studio-background-"+kind.toLower()+".png"));QVERIFY(expected.save("artifacts/studio-background-reference-"+kind.toLower()+".png"));QCOMPARE(actual,expected);
            for(const auto &view:colorViews()){
                library.display.colorView=view;library.display.colorLook="Medium Contrast";library.display.exposure=.7;library.display.gamma=1.2;auto graded=expected;applyColorManagement(graded,view,library.display.colorLook,library.display.exposure,library.display.gamma);QCOMPARE(monitor.programFrame(expected.size()),graded);
            }
            library.display.colorView="Standard";library.display.colorLook="None";library.display.exposure=0;library.display.gamma=1;
        }
        play.stop();
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
    void timelineTrim_data(){QTest::addColumn<bool>("previews");QTest::newRow("with previews")<<true;QTest::newRow("without previews")<<false;}
    void timelineTrim(){
        QFETCH(bool,previews);QSettings().setValue("studio/timelineThumbnails",previews);QSettings().setValue("studio/timelineWaveforms",previews);
        QSettings().setValue("studio/timelineScale",14.);
        Library library;ProgramItem program;program.id="trim";program.seconds=30;library.programs={program};
        Placement clip;clip.id="overlay";clip.type="Element";clip.layer=1;clip.startMs=1000;clip.durationMs=5000;clip.seconds=5;library.placements={clip};
        Playout play(&library);TimelineWidget timeline(&play,&library);timeline.resize(600,240);timeline.show();QTest::qWait(30);
        const auto bounds=timeline.placementRect("overlay");QVERIFY(!bounds.isEmpty());const auto right=QPoint(bounds.right(),bounds.center().y());
        QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,right);QTest::mouseMove(&timeline,right+QPoint(29,0));QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,right+QPoint(29,0));
        QVERIFY(qAbs(library.placement("overlay")->lengthMs()-7000)<45);
        const auto left=QPoint(bounds.left(),bounds.center().y());QTest::mousePress(&timeline,Qt::LeftButton,Qt::NoModifier,left);QTest::mouseMove(&timeline,left+QPoint(14,0));QTest::mouseRelease(&timeline,Qt::LeftButton,Qt::NoModifier,left+QPoint(14,0));
        QVERIFY(qAbs(library.placement("overlay")->startMs-2000)<45);QVERIFY(qAbs(library.placement("overlay")->offsetMs-1000)<45);QVERIFY(qAbs(library.placement("overlay")->lengthMs()-6000)<45);
        QSettings().setValue("studio/timelineThumbnails",true);QSettings().setValue("studio/timelineWaveforms",true);
    }
    void cueBoardLiveAndDrafts(){
        applyUiTheme("Studio Blue",false);
        Library lib;Element e;e.id="board-element";e.name="Evening headlines";e.type="Custom";auto design=elementPreset("News lower third");QString fieldId;for(const auto &o:design.objects)if(o.type=="Text"&&o.style["studio.editable"].toBool()){fieldId=o.id;break;}QVERIFY(!fieldId.isEmpty());QJsonObject cue{{"id","headline"},{"name","Headline"},{"start",0},{"end",1000},{"repeat",2},{"hold",true},{"holdManual",false},{"holdMs",500}};design.animationCues={cue};e.design=design.toJson();auto entry=newCueBoardEntry(cue);entry["sections"]=QJsonArray{QJsonObject{{"fields",QJsonObject{{fieldId,"Hello"}}}},QJsonObject{{"fields",QJsonObject{{fieldId,"Welcome"}}}}};e.cueBoard={entry};lib.elements={e};ProgramItem p;p.id="show";p.kind="News";p.seconds=30;lib.programs={p};Playout play(&lib);play.start(0,5000);play.togglePause();
        QVERIFY(!play.graphicPlayback(e,5000).visible);play.triggerElementAnimation(e.id,"@board:0");auto first=play.graphicPlayback(e,5000);QCOMPARE(ElementDesign::fromJson(first.design).find(fieldId)->text,QString("Hello"));QVERIFY(!play.graphicPlayback(e,6200).advancing);auto second=play.graphicPlayback(e,6500);QCOMPARE(ElementDesign::fromJson(second.design).find(fieldId)->text,QString("Welcome"));QVERIFY(!play.graphicPlayback(e,8000).visible);play.advanceElementCue(e.id);QCOMPARE(play.graphicPlayback(e,5000).boardStep,1);play.triggerElementAnimation(e.id,{});QVERIFY(!play.graphicPlayback(e,5000).visible);
        auto *page=studioCueBoard(&lib,&play,e.id);page->resize(560,1050);page->show();auto *tree=page->findChild<QListWidget*>("cueBoardList");QVERIFY(tree);QCOMPARE(tree->count(),1);tree->setCurrentItem(tree->item(0));auto *text=page->findChild<QPlainTextEdit*>("cueFieldValue");QVERIFY(text);QCOMPARE(text->toPlainText(),QString("Hello"));text->setPlainText("First repetition edited privately");QCOMPARE(lib.element(e.id)->cueBoard,e.cueBoard);QVERIFY(!play.graphicPlayback(*lib.element(e.id),5000).visible);page->findChild<QPushButton*>("saveCueBoard")->click();QVERIFY(!page->property("dirty").toBool());QCOMPARE(lib.element(e.id)->design,e.design);auto saved=compileCueBoard(design,lib.element(e.id)->cueBoard);QCOMPARE(ElementDesign::fromJson(saved.steps[0].design).find(fieldId)->text,QString("First repetition edited privately"));QCOMPARE(ElementDesign::fromJson(saved.steps[1].design).find(fieldId)->text,QString("Welcome"));
        auto *plays=page->findChild<QComboBox*>("boardPlaySelector");auto *holdSeconds=page->findChild<QDoubleSpinBox*>("boardPlayHoldSeconds");QVERIFY(plays&&holdSeconds);QCOMPARE(plays->count(),2);holdSeconds->setValue(1.25);plays->setCurrentIndex(1);QCOMPARE(holdSeconds->value(),.5);holdSeconds->setValue(.75);plays->setCurrentIndex(0);QCOMPARE(holdSeconds->value(),1.25);page->findChild<QPushButton*>("saveCueBoard")->click();saved=compileCueBoard(design,lib.element(e.id)->cueBoard);QCOMPARE(saved.steps[0].holdMs,qint64(1250));QCOMPARE(saved.steps[1].holdMs,qint64(750));QCOMPARE(saved.duration(),qint64(4000));QTest::qWait(600);QVERIFY(page->grab().save("artifacts/studio-cue-board.png"));
        auto *loopCount=page->findChild<QSpinBox*>("boardRepeat");auto *loopEnabled=page->findChild<QCheckBox*>("boardCueLoop");QVERIFY(loopCount&&loopEnabled);QVERIFY(loopEnabled->isChecked());loopCount->setValue(0);QCOMPARE(plays->count(),2);page->findChild<QPushButton*>("saveCueBoard")->click();saved=compileCueBoard(design,lib.element(e.id)->cueBoard);QCOMPARE(saved.duration(),qint64(-1));QCOMPARE(saved.steps[0].holdMs,qint64(1250));QCOMPARE(saved.steps[1].holdMs,qint64(750));
        loopCount->setValue(2);loopEnabled->setChecked(false);QVERIFY(!loopCount->isEnabled());QCOMPARE(plays->count(),1);loopEnabled->setChecked(true);QVERIFY(loopCount->isEnabled());QVERIFY(loopEnabled->isChecked());loopCount->setValue(2);page->findChild<QPushButton*>("saveCueBoard")->click();saved=compileCueBoard(design,lib.element(e.id)->cueBoard);QCOMPARE(saved.duration(),qint64(4000));for(const auto &theme:QStringList{"Light","Darker","Studio Blue"}){applyUiTheme(theme,false);QTest::qWait(50);QVERIFY(page->grab().save("artifacts/studio-cue-board-"+theme.toLower().replace(" ","-")+".png"));}delete page;play.stop();
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
        applyUiTheme("Studio Blue",false);
        MainWindow window;window.resize(1600,1000);window.show();
        QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
        auto *settings=window.findChild<QToolButton*>("windowSettings"),*minimize=window.findChild<QToolButton*>("windowMinimize"),*maximize=window.findChild<QToolButton*>("windowMaximize"),*close=window.findChild<QToolButton*>("windowClose");
        QVERIFY(settings&&minimize&&maximize&&close);QTest::qWait(30);QVERIFY(settings->x()<minimize->x());QVERIFY(minimize->x()<maximize->x());QVERIFY(maximize->x()<close->x());
        settings->click();QTest::qWait(20);auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);QCOMPARE(dialog->windowTitle(),QString("Settings"));dialog->reject();QTest::qWait(20);
        maximize->click();QVERIFY(window.isMaximized());maximize->click();QVERIFY(!window.isMaximized());
        auto *editor=window.findChild<EditorPanel*>();QVERIFY(editor);editor->showTool(3);QTest::qWait(60);auto *cards=editor->findChild<QListWidget*>("screenElementCards");QVERIFY(cards);cards->setCurrentRow(0);QTest::qWait(60);QVERIFY(window.grab().save("artifacts/studio-workspace.png"));editor->showTool(4);QTest::qWait(60);QVERIFY(window.grab().save("artifacts/studio-monitor-workspace.png"));window.hide();
    }
    void channelProfileLogoWorkflow(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-channel-profile");}~Profile(){qApp->setApplicationName(previous);}} profile;
        applyUiTheme("Studio Blue",false);QTemporaryDir media;QImage image(80,40,QImage::Format_ARGB32);image.fill(Qt::red);const auto redPath=media.filePath("red.png"),bluePath=media.filePath("blue.png");QVERIFY(image.save(redPath));image.fill(Qt::blue);QVERIFY(image.save(bluePath));Library seed;seed.display.channel="Original channel";seed.display.channelLogo=redPath;Element built;built.id="built-logo";built.name="Built identity";built.type="Custom";built.design=elementPreset("Sponsor spotlight").toJson();seed.elements={built};QString error;QVERIFY2(seed.saveChecked(&error),qPrintable(error));
        MainWindow window;window.resize(1200,850);window.show();auto *profileButton=window.findChild<QToolButton*>("channelProfile");QVERIFY(profileButton);auto icon=[&]{return profileButton->icon().pixmap(26,26).toImage();};QCOMPARE(icon().pixelColor(0,0).alpha(),0);QCOMPARE(icon().pixelColor(13,13),QColor(Qt::red));
        bool visited=false;QTimer::singleShot(0,&window,[&]{auto *dialog=qobject_cast<QDialog*>(qApp->activeModalWidget());if(!dialog)return;QTimer::singleShot(3000,dialog,&QDialog::reject);visited=true;QVERIFY(dialog->windowFlags().testFlag(Qt::FramelessWindowHint));dialog->findChild<QLineEdit*>("channelName")->setText("Cancelled name");dialog->findChild<QLineEdit*>("channelLogoPath")->setText(bluePath);auto preview=dialog->findChild<QLabel*>("channelLogoPreview")->pixmap().toImage();QCOMPARE(preview.pixelColor(0,0).alpha(),0);QCOMPARE(preview.pixelColor(32,32),QColor(Qt::blue));QCOMPARE(icon().pixelColor(13,13),QColor(Qt::red));dialog->reject();});profileButton->click();QVERIFY(visited);Library cancelled;cancelled.load();QCOMPARE(cancelled.display.channel,QString("Original channel"));
        QTimer::singleShot(0,&window,[&]{auto *dialog=qobject_cast<QDialog*>(qApp->activeModalWidget());if(!dialog)return;QTimer::singleShot(3000,dialog,&QDialog::reject);dialog->findChild<QLineEdit*>("channelName")->setText("Renovation Studio");dialog->findChild<QPlainTextEdit*>("channelDescription")->setPlainText("Homes, design and renovation");dialog->findChild<QLineEdit*>("channelLogoPath")->setText(bluePath);QVERIFY(dialog->grab().save("artifacts/channel-profile.png"));dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});profileButton->click();Library saved;saved.load();QCOMPARE(saved.display.channel,QString("Renovation Studio"));QCOMPARE(saved.display.channelDescription,QString("Homes, design and renovation"));QCOMPARE(saved.display.channelLogo,bluePath);QCOMPARE(icon().pixelColor(13,13),QColor(Qt::blue));
        QTimer::singleShot(0,&window,[&]{auto *dialog=qobject_cast<QDialog*>(qApp->activeModalWidget());if(!dialog)return;QTimer::singleShot(3000,dialog,&QDialog::reject);auto *picker=dialog->findChild<QComboBox*>("channelLogoElement");const int index=picker->findData(built.id);QVERIFY(index>=0);picker->setCurrentIndex(index);QVERIFY(!dialog->findChild<QLabel*>("channelLogoPreview")->pixmap().isNull());dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});profileButton->click();saved.load();QCOMPARE(saved.display.channelLogoElement,built.id);QCOMPARE(icon().pixelColor(0,0).alpha(),0);window.hide();
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
        if(protocol=="rtmp"){QTRY_VERIFY2_WITH_TIMEOUT((serverLog+=server.readAll()).contains("is publishing"),qPrintable(QString::fromUtf8(serverLog)+"\nOutput: "+output.status()+"\nError: "+output.property("lastError").toString()),15000);receiver.start(ffmpeg,args);QVERIFY(receiver.waitForStarted(10000));}
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
