#include <QtTest>
#include <QDialogButtonBox>
#include <QToolButton>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QListWidget>
#include <QTreeWidgetItemIterator>
#include <QPainter>
#include <QWheelEvent>
#include <QVBoxLayout>
#include "BuildCanvas.h"
#include "AnimationEditor.h"
#include "CurveNoise.h"
#include "Data.h"
#include "FrameExport.h"
#include "UiControls.h"
#include "UiTheme.h"
#include "PropertyInspector.h"
#include "Toast.h"
#include "BuildWorkspace.h"
#include "AudioLibrary.h"
#include "DesignAudio.h"
#include "LayerPanel.h"

class EditorInteractionTests:public QObject{
    Q_OBJECT
    ElementDesign scene(){auto design=ElementDesign::defaultScene();DObject object;object.id="plate";object.name="Plate";object.type="Shape";object.w=4;object.h=2;object.base.rot={90,0,0};object.mat.shadeless=true;object.keyChannel("position.x",0,0);object.keyChannel("position.x",1000,2);object.keyChannel("rotation.y",0,0);object.keyChannel("rotation.y",1000,30);design.objects.append(object);return design;}
private slots:
    void audioSaveFailureRestoresDataAndControls(){
        const auto previousName=qApp->applicationName();qApp->setApplicationName(previousName+"-audio-save-failure");
        const auto restoreName=qScopeGuard([&]{qApp->setApplicationName(previousName);});
        const auto destination=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/cuebase.json";
        QVERIFY(QDir().mkpath(destination));const auto cleanup=qScopeGuard([&]{QDir().rmdir(destination);});
        QTemporaryDir temporary;QFile file(temporary.filePath("audio.wav"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");file.close();
        Library library;library.production["audioLibrary"]=QJsonArray{QJsonObject{{"id","audio"},{"name","Sound"},{"file",file.fileName()},{"durationMs",4000},{"speed",1.}}};
        const auto before=library.production;QSignalSpy placements(&library,&Library::placementsChanged),production(&library,&Library::productionChanged);QString error;
        QVERIFY(AudioLibrary::addToTimeline(library,"audio",0,1,&error).isEmpty());QVERIFY(!error.isEmpty());QVERIFY(library.placements.isEmpty());QCOMPARE(library.production,before);QCOMPARE(placements.size(),0);
        AudioLibraryPanel panel(&library);panel.findChild<QListWidget*>("audioStored")->setCurrentRow(0);auto *speed=panel.findChild<QDoubleSpinBox*>("audioSpeed");QVERIFY(speed);speed->setValue(2);
        QCOMPARE(speed->value(),1.);QCOMPARE(library.production,before);QCOMPARE(production.size(),0);
        Placement clip;clip.id="placed";clip.type="Sound";clip.refId="audio";clip.durationMs=4000;library.placements.append(clip);panel.selectPlacement(clip.id);speed->setValue(2);
        QCOMPARE(speed->value(),1.);QCOMPARE(library.placement(clip.id)->lengthMs(),qint64(4000));QCOMPARE(library.production,before);QCOMPARE(placements.size(),0);
    }
    void layerOrderActionsPreserveSelectionAndLocks(){
        BuildModel model;auto design=scene();auto first=*design.find("plate");first.id="second";first.style["zIndex"]=3;design.objects.append(first);auto locked=first;locked.id="locked";locked.locked=true;locked.style["zIndex"]=8;design.objects.append(locked);model.reset(design);
        LayerPanel panel(&model);model.setSelection(QSet<QString>{"plate","second","locked"},"plate");panel.shiftOrder(1,true);
        QCOMPARE(model.byId("plate")->style["zIndex"].toInt(),9);QCOMPARE(model.byId("second")->style["zIndex"].toInt(),12);QCOMPARE(model.byId("locked")->style["zIndex"].toInt(),8);QCOMPARE(model.sel.size(),3);
        QVERIFY(model.undo());QCOMPARE(model.byId("plate")->style["zIndex"].toInt(),0);QCOMPARE(model.byId("second")->style["zIndex"].toInt(),3);
        panel.shiftOrder(-1,true);QCOMPARE(model.byId("second")->style["zIndex"].toInt(),3); // already behind the locked layer
        panel.shiftOrder(-1);QCOMPARE(model.byId("plate")->style["zIndex"].toInt(),-1);QCOMPARE(model.byId("second")->style["zIndex"].toInt(),2);
        for(QTreeWidgetItemIterator it(&panel);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="locked"){(*it)->setData(3,Qt::EditRole,99);QCOMPARE((*it)->data(3,Qt::EditRole).toInt(),8);break;}
        QCOMPARE(model.byId("locked")->style["zIndex"].toInt(),8);
    }
    void explicitLayerOrderPreservesGeometry(){
        BuildModel model;auto design=scene();auto *plate=design.find("plate");plate->tracks.clear();plate->mat.color=Qt::red;
        auto front=*plate;front.id="front";front.name="Front";front.mat.color=Qt::blue;design.objects.append(front);model.reset(design);
        auto pixel=[&]{QImage image(320,180,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);model.design.render(painter,image.rect(),0);painter.end();return image.pixelColor(160,90);};
        QVERIFY(pixel().blue()>200);const auto original=model.byId("plate")->base.loc;LayerPanel layers(&model);
        QTreeWidgetItem *row=nullptr;for(QTreeWidgetItemIterator it(&layers);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="plate")row=*it;QVERIFY(row);
        row->setData(3,Qt::EditRole,2);QCOMPARE(model.byId("plate")->style["zIndex"].toInt(),2);QCOMPARE(model.byId("plate")->base.loc,original);QVERIFY(pixel().red()>200);
        QVERIFY(model.undo());QVERIFY(pixel().blue()>200);QVERIFY(model.redo());QVERIFY(pixel().red()>200);
        const auto restored=ElementDesign::fromJson(model.design.toJson());QCOMPARE(restored.find("plate")->style["zIndex"].toInt(),2);
    }
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);qApp->setApplicationName("CuebaseEditorInteractionTests");installUiBehavior(qApp);applyUiTheme("Studio Blue",false);QDir().mkpath("artifacts");}
    void themeTextContrast(){
        auto luminance=[](const QColor &c){auto linear=[](double value){return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);};return .2126*linear(c.redF())+.7152*linear(c.greenF())+.0722*linear(c.blueF());};
        for(const auto &theme:QStringList{"Studio Blue","Current","Darker","Light"}){
            applyUiTheme(theme,false);const auto palette=qApp->palette();
            for(const auto &roles:QList<QPair<QPalette::ColorRole,QPalette::ColorRole>>{{QPalette::WindowText,QPalette::Window},{QPalette::Text,QPalette::Base},{QPalette::ButtonText,QPalette::Button},{QPalette::PlaceholderText,QPalette::Base},{QPalette::PlaceholderText,QPalette::Window},{QPalette::HighlightedText,QPalette::Highlight}}){
                const double a=luminance(palette.color(roles.first)),b=luminance(palette.color(roles.second));const double ratio=(qMax(a,b)+.05)/(qMin(a,b)+.05);
                QVERIFY2(ratio>=4.5,qPrintable(QString("%1 text role %2 contrast %3").arg(theme).arg(roles.first).arg(ratio)));
            }
        }
        applyUiTheme("Studio Blue",false);
    }
    void drawShapesAndCancel(){
        Library library;BuildModel model;model.reset(scene());BuildCanvas canvas(&model,&library);canvas.resize(800,500);canvas.show();QTest::qWait(20);
        const auto before=model.design.toJson();const auto count=model.design.objects.size();
        canvas.startDrawShape("Rectangle");QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(250,180));QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(450,280));
        QCOMPARE(model.design.objects.size(),count+1);QCOMPARE(model.active()->type,QString("Shape"));QVERIFY(qAbs(model.active()->w/model.active()->h-2)<.01);
        model.undo();QCOMPARE(model.design.toJson(),before);model.redo();QCOMPARE(model.design.objects.size(),count+1);
        canvas.startDrawShape("Ellipse");QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(250,180));QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::ShiftModifier,QPoint(400,230));QVERIFY(qAbs(model.active()->w-model.active()->h)<.01);QCOMPARE(model.active()->style["shape"].toString(),QString("Ellipse"));
        const auto drawn=model.design.toJson();canvas.startDrawShape("Rectangle");QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(250,180));QTest::keyClick(&canvas,Qt::Key_Escape);QCOMPARE(model.design.toJson(),drawn);QVERIFY(QWidget::mouseGrabber()!=&canvas);
    }
    void colorPreviewIsTransactional(){
        Library library;BuildModel model;model.reset(scene());model.select("plate");PropertyInspector inspector(&model,&library);inspector.show();QTest::qWait(20);
        auto *button=inspector.findChild<QPushButton*>("color_fill.color");QVERIFY(button);const QColor original=model.active()->mat.color;
        auto choose=[&](bool accept){QTimer::singleShot(0,[&model,accept]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);auto *hex=dialog->findChild<QLineEdit*>("colorHex");QVERIFY(hex);hex->selectAll();QTest::keyClicks(hex,"#ff336699");QCOMPARE(model.active()->mat.color,QColor("#ff336699"));accept?dialog->accept():dialog->reject();});button->click();};
        choose(false);QCOMPARE(model.active()->mat.color,original);choose(true);QCOMPARE(model.active()->mat.color,QColor("#ff336699"));model.undo();QCOMPARE(model.active()->mat.color,original);model.redo();QCOMPARE(model.active()->mat.color,QColor("#ff336699"));
    }
    void programCutsUndoAndRedo(){
        Library library;ProgramItem program;program.id="cut-test";program.title="Test";program.seconds=12;program.offsetMs=700;library.programs.append(program);
        const auto second=library.cutProgram(program.id,4);QVERIFY(!second.isEmpty());QCOMPARE(library.programs.size(),2);QCOMPARE(library.programs[1].offsetMs,4700);
        QVERIFY(library.undoProgramCut());QCOMPARE(library.programs.size(),1);QCOMPARE(library.programs[0].lengthMs(),12000);QVERIFY(library.redoProgramCut());QCOMPARE(library.programs[1].id,second);
        library.programs[0].title="New edit";library.commitPrograms();QVERIFY(!library.undoProgramCut());QCOMPARE(library.programs[0].title,QString("New edit"));
    }
    void snappingAlignsEdgesAndOffCameraPicking(){
        Library library;BuildModel model;auto design=scene();design.find("plate")->tracks.clear();auto second=*design.find("plate");second.id="target";second.base.loc={5,0,0};design.objects.append(second);model.reset(design);model.select("plate");
        BuildCanvas canvas(&model,&library);canvas.resize(900,650);canvas.setShading(BuildCanvas::Solid);canvas.show();QTest::qWait(30);canvas.grab();
        QWheelEvent zoom(QPointF(450,325),QPointF(canvas.mapToGlobal(QPoint(450,325))),{},QPoint(0,-240),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&canvas,&zoom);canvas.grab();
        const auto frame=canvas.property("compositionFrame").toRectF();const auto camera=model.design.sceneCamera(0,library.display.aspect());QPointF start,end;
        QVERIFY(camera.toScreen(QVector3D(),frame,&start));QVERIFY(camera.toScreen(QVector3D(1,0,0),frame,&end));
        auto move=[&](QPointF point){QMouseEvent event(QEvent::MouseMove,point,QPointF(canvas.mapToGlobal(point.toPoint())),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&canvas,&event);};
        move(start);canvas.startMove();move(end-QPointF(3,0));QTest::keyClick(&canvas,Qt::Key_Return);QVERIFY2(qAbs(model.byId("plate")->at(0).loc.x()-1)<.03,qPrintable(QString::number(model.byId("plate")->at(0).loc.x())));
        model.byId("target")->base.loc={9.2f,0,0};model.byId("target")->w=2;model.notify();QPointF outside;QVERIFY(camera.toScreen(QVector3D(9.2f,0,0),frame,&outside));QVERIFY(!frame.contains(outside));QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,outside.toPoint());QCOMPARE(model.activeId,QString("target"));
        canvas.grab().save("artifacts/build-draw-and-align.png");
    }
    void modifierCategoriesAndNotices(){
        Library library;BuildWorkspace workspace(&library);workspace.resize(1200,800);workspace.show();QTest::qWait(30);
        auto *overlayButton=workspace.findChild<QToolButton*>("viewportOverlays");QVERIFY(overlayButton);QVERIFY(overlayButton->menu());
        auto *master=workspace.findChild<QCheckBox*>("overlayMaster");QVERIFY(master);auto *canvas=workspace.findChild<BuildCanvas*>();QVERIFY(canvas);
        const bool original=master->isChecked();master->setChecked(false);QVERIFY(!canvas->showOverlays);master->setChecked(true);QVERIFY(canvas->showOverlays);master->setChecked(original);
        auto *filter=workspace.findChild<QComboBox*>("modifierCategory");QVERIFY(filter);QCOMPARE(filter->currentText(),QString("Modifiers"));filter->setCurrentIndex(1);QCOMPARE(filter->currentText(),QString("Physics"));
        ToastHost::notify(&workspace,"audio","Audio added successfully",QString(200,'x'));auto *host=workspace.findChild<ToastHost*>();QVERIFY(host);QVERIFY(host->isVisible());auto *card=host->findChild<QWidget*>("noticeCard");QVERIFY(card);QVERIFY(card->width()<=460);workspace.resize(500,600);QTest::qWait(20);QVERIFY(host->geometry()==workspace.rect());QVERIFY(card->geometry().right()<=host->width());
        workspace.grab().save("artifacts/build-modifier-categories.png");
    }
    void sharedAudioRemainsSeparateFromVisualLayers(){
        QTemporaryDir temporary;QFile file(temporary.filePath("sound.wav"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");file.close();
        Library library;library.production["audioLibrary"]=QJsonArray{QJsonObject{{"id","asset"},{"name","Thunder"},{"file",file.fileName()},{"durationMs",4000},{"speed",2.},{"offset",1.},{"volume",.8},{"fadeIn",1.},{"fadeOut",.2}}};
        BuildModel model;model.reset(scene());const auto id=AudioLibrary::addToScene(model,library,"asset");QVERIFY(!id.isEmpty());LayerPanel layers(&model);for(QTreeWidgetItemIterator it(&layers);*it;++it)QVERIFY((*it)->data(0,Qt::UserRole).toString()!=id);
        const auto cues=designAudioCues(model.design,500,true,{},1,false);QCOMPARE(cues.size(),1);QCOMPARE(cues[0].position,qint64(2000));QCOMPARE(cues[0].rate,2.);QVERIFY(qAbs(cues[0].volume-.4)<.0001);QVERIFY(model.undo());QVERIFY(!model.byId(id));QVERIFY(model.redo());QVERIFY(model.byId(id));
        model.selectAll();QVERIFY(!model.sel.contains(id));model.select(id);QCOMPARE(model.activeId,id);
        const auto placement=AudioLibrary::addToTimeline(library,"asset",2000,2);QVERIFY(!placement.isEmpty());QCOMPARE(library.placement(placement)->lengthMs(),qint64(1500));AudioLibraryPanel studio(&library),build(&library,&model);QCOMPARE(studio.findChild<QListWidget*>("audioStored")->count(),1);QCOMPARE(build.findChild<QListWidget*>("audioStored")->count(),1);
        studio.selectPlacement(placement);QCOMPARE(studio.findChild<QDoubleSpinBox*>("audioOffset")->maximum(),3.99);studio.findChild<QDoubleSpinBox*>("audioSpeed")->setValue(1);QCOMPARE(library.placement(placement)->lengthMs(),qint64(3000));QCOMPARE(AudioLibrary::placementSettings(library,placement)["speed"].toDouble(),1.);QCOMPARE(AudioLibrary::asset(library,"asset")["speed"].toDouble(),2.);
        model.select(id);auto *remove=build.findChild<QPushButton*>("removeSceneAudio");QVERIFY(remove);QVERIFY(remove->isEnabled());remove->click();QVERIFY(!model.byId(id));QVERIFY(model.byId("plate"));QVERIFY(QFileInfo::exists(file.fileName()));QVERIFY(!AudioLibrary::asset(library,"asset").isEmpty());QVERIFY(model.undo());QVERIFY(model.byId(id));
    }
    void numericValuesRemainVisible(){
        for(const auto &theme:QStringList{"Studio Blue","Current","Darker","Light"}){
            applyUiTheme(theme,false);
            QWidget parent;QVBoxLayout layout(&parent);ValueInput value;FrameInput frame;
            layout.addWidget(&value);layout.addWidget(&frame);
            value.setRange(-1000,1000);value.setDecimals(3);value.setValue(12.375);frame.setValue(42);
            parent.show();QTest::qWait(30);
            for(auto *input:QList<QAbstractSpinBox*>{&value,&frame})for(bool enabled:{true,false}){
                input->setEnabled(enabled);input->clearFocus();auto *line=input->findChild<QLineEdit*>();QVERIFY(line);
                line->deselect();QTest::qWait(10);QVERIFY(line->width()>=input->width()-12);const auto displayed=line->text();QVERIFY(!displayed.isEmpty());
                const auto withText=input->grab().toImage();line->setText("");const auto withoutText=input->grab().toImage();line->setText(displayed);
                int changed=0;for(int y=0;y<withText.height();++y)for(int x=0;x<withText.width();++x)if(withText.pixel(x,y)!=withoutText.pixel(x,y))++changed;
                if(changed<=12){withText.save("artifacts/numeric-with-text.png");withoutText.save("artifacts/numeric-empty.png");}
                QVERIFY2(changed>12,qPrintable(QString("Numeric glyphs clipped in %1, enabled=%2, editor=%3x%4, changed=%5").arg(theme).arg(enabled).arg(line->width()).arg(line->height()).arg(changed)));
            }
        }
        applyUiTheme("Studio Blue",false);
    }
    void noiseEvaluationAndPersistence(){
        auto design=scene();auto *object=design.find("plate");auto *track=object->track("position.x");const auto original=track->keys;track->noise={{"scale",.23},{"strength",3},{"seed",7},{"depth",3},{"roughness",.6}};
        const auto restored=ElementDesign::fromJson(design.toJson());bool changed=false;
        for(int time=-500;time<=2500;time+=37){double value=track->evaluate(time,0);QVERIFY(std::isfinite(value));QCOMPARE(value,restored.find("plate")->propertyValue("position.x",time));QCOMPARE(value,track->evaluate(time,0));changed|=qAbs(value-track->evaluateBase(time,0))>1e-5;}
        QVERIFY(changed);QCOMPARE(track->keys.size(),original.size());QCOMPARE(track->keys.first().value,original.first().value);
        auto without=track->noise;track->noise["enabled"]=false;QCOMPARE(track->evaluate(355,0),track->evaluateBase(355,0));track->noise=without;track->noise["influence"]=0;QCOMPARE(track->evaluate(355,0),track->evaluateBase(355,0));
        track->noise=without;track->noise["restricted"]=true;track->noise["start"]=.2;track->noise["end"]=.8;track->noise["fadeIn"]=.1;track->noise["fadeOut"]=.1;
        for(int time:{0,200,800,1000})QCOMPARE(track->evaluate(time,0),track->evaluateBase(time,0));QVERIFY(track->evaluate(400,0)!=track->evaluateBase(400,0));
        track->noise={{"blend","Add"},{"strength",2},{"scale",.1}};for(int time=0;time<1000;time+=19)QVERIFY(track->evaluate(time,0)>=track->evaluateBase(time,0));track->noise["blend"]="Subtract";for(int time=0;time<1000;time+=19)QVERIFY(track->evaluate(time,0)<=track->evaluateBase(time,0));
        track->noise={{"strength",0},{"scale",0},{"depth",1000000}};QCOMPARE(track->evaluate(300,0),track->evaluateBase(300,0));
    }
    void noiseDialogSaveCancelUndoAndRemove(){
        BuildModel model;model.reset(scene());model.select("plate");const auto before=model.design.toJson();QWidget parent;
        QTimer::singleShot(20,[&]{auto *dialog=parent.findChild<QDialog*>("curveNoiseDialog");QVERIFY(dialog);auto guard=qScopeGuard([&]{dialog->reject();});auto *strength=dialog->findChild<QDoubleSpinBox*>("noise_strength");QVERIFY(strength);strength->setValue(4);dialog->findChild<QComboBox*>("noiseChannel")->setCurrentIndex(1);strength->setValue(8);QVERIFY(dialog->grab().save("artifacts/animation-noise-controls.png"));dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
        editCurveNoise(&model,{{"plate","position.x"},{"plate","rotation.y"}},&parent);
        QCOMPARE(model.active()->track("position.x")->noise["strength"].toDouble(),4.);QCOMPARE(model.active()->track("rotation.y")->noise["strength"].toDouble(),8.);const auto modified=model.design.toJson();QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);QVERIFY(model.redo());QCOMPARE(model.design.toJson(),modified);
        QTimer::singleShot(20,[&]{auto *dialog=parent.findChild<QDialog*>("curveNoiseDialog");QVERIFY(dialog);dialog->findChild<QDoubleSpinBox*>("noise_strength")->setValue(99);dialog->reject();});editCurveNoise(&model,{{"plate","position.x"}},&parent);QCOMPARE(model.design.toJson(),modified);
        QTimer::singleShot(20,[&]{auto *dialog=parent.findChild<QDialog*>("curveNoiseDialog");QVERIFY(dialog);auto guard=qScopeGuard([&]{dialog->reject();});dialog->findChild<QPushButton*>("noiseRemove")->click();dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});editCurveNoise(&model,{{"plate","position.x"}},&parent);QVERIFY(model.active()->track("position.x")->noise.isEmpty());QVERIFY(!model.active()->track("rotation.y")->noise.isEmpty());
    }
    void noiseToolbarAndExport(){
        BuildModel model;model.reset(scene());model.select("plate");AnimationEditor editor(&model,true);editor.resize(900,400);editor.show();QTest::qWait(20);
        auto *tree=editor.findChild<QTreeWidget*>();QVERIFY(tree);bool selected=false;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole+1).toString()=="position.x"){tree->clearSelection();(*it)->setSelected(true);tree->setCurrentItem(*it);selected=true;break;}QVERIFY(selected);
        QToolButton *button=nullptr;for(auto *candidate:editor.findChildren<QToolButton*>())if(candidate->accessibleName().startsWith("Noise modifier"))button=candidate;QVERIFY(button);
        QTimer::singleShot(20,[&]{auto *dialog=editor.findChild<QDialog*>("curveNoiseDialog");QVERIFY(dialog);auto guard=qScopeGuard([&]{dialog->reject();});dialog->findChild<QDoubleSpinBox*>("noise_strength")->setValue(4);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});button->click();QVERIFY(!model.active()->track("position.x")->noise.isEmpty());QVERIFY(model.active()->track("rotation.y")->noise.isEmpty());
        QTemporaryDir directory;FrameExportOptions options;options.size={320,180};options.firstFrame=8;options.endFrame=9;const auto result=exportSceneFrames(model.design,directory.path(),options);QVERIFY2(result.success,qPrintable(result.error));QImage expected(options.size,QImage::Format_ARGB32_Premultiplied);expected.fill(Qt::transparent);{QPainter painter(&expected);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);model.design.render(painter,expected.rect(),320);}QImage actual(directory.filePath("frame_000008.png"));QCOMPARE(actual.convertToFormat(expected.format()),expected);
    }
    void duplicateCancelAndMouseOwnership(){
        Library library;BuildModel model;model.reset(scene());model.select("plate");BuildCanvas canvas(&model,&library);canvas.resize(708,428);canvas.show();QTest::qWait(20);
        uiGesture()->begin();model.pushUndo();model.active()->name="Edited earlier";const auto before=model.design.toJson();const auto originalCount=model.design.objects.size();QTest::keyClick(&canvas,Qt::Key_D,Qt::ShiftModifier);QVERIFY(canvas.modalActive());QCOMPARE(model.design.objects.size(),originalCount+1);QCOMPARE(QWidget::mouseGrabber(),&canvas);
        QTest::mouseClick(&canvas,Qt::RightButton,Qt::ShiftModifier,QPoint(350,210));QVERIFY(!canvas.modalActive());QCOMPARE(model.design.toJson(),before);QVERIFY(QWidget::mouseGrabber()!=&canvas);uiGesture()->finish(false);
        model.select("plate");QTest::keyClick(&canvas,Qt::Key_G);QVERIFY(canvas.modalActive());QCOMPARE(QWidget::mouseGrabber(),&canvas);QTest::keyClick(&canvas,Qt::Key_Escape);QVERIFY(!canvas.modalActive());QVERIFY(QWidget::mouseGrabber()!=&canvas);
    }
    void singleClickReplacesMultipleSelection(){
        Library library;BuildModel model;auto design=scene();auto second=*design.find("plate");second.id="second";second.base.loc={5,0,0};second.tracks.clear();design.objects.append(second);model.reset(design);model.setSelection({"plate","second"},"second");BuildCanvas canvas(&model,&library);canvas.resize(708,428);canvas.showOverlays=false;canvas.show();QTest::qWait(20);QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(354,214));QCOMPARE(model.sel,QSet<QString>{"plate"});
    }
    void timelineHistoryWheel(){
        for(bool graph:{false,true}){BuildModel model;model.reset(scene());model.select("plate");AnimationEditor editor(&model,graph);editor.resize(800,360);editor.show();QTest::qWait(10);QWidget *curve=nullptr;for(auto *child:editor.findChildren<QWidget*>())if(child->property("viewRange").isValid()){curve=child;break;}QVERIFY(curve);model.pushUndo(true);model.active()->name="History edit";model.notify();
            auto wheel=[&](int delta){QWheelEvent event(QPointF(100,100),QPointF(curve->mapToGlobal(QPoint(100,100))),{},QPoint(0,delta),Qt::NoButton,Qt::AltModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(curve,&event);};wheel(120);QCOMPARE(model.active()->name,QString("Plate"));wheel(-120);QCOMPARE(model.active()->name,QString("History edit"));}
    }
    void offCameraObjectsRemainVisible(){
        Library library;BuildModel model;auto design=scene();auto *object=design.find("plate");object->tracks.clear();object->base.loc={9.2f,0,0};object->w=2;model.reset(design);
        BuildCanvas canvas(&model,&library);canvas.resize(900,650);canvas.showOverlays=false;canvas.showGizmos=false;canvas.show();QTest::qWait(20);
        canvas.setShading(BuildCanvas::Solid);const auto withObject=canvas.grab().toImage();model.design.find("plate")->visible=false;const auto withoutObject=canvas.grab().toImage();QVERIFY(withObject!=withoutObject);
        model.design.find("plate")->visible=true;canvas.setShading(BuildCanvas::Rendered);const auto finalWith=canvas.grab().toImage();model.design.find("plate")->visible=false;QCOMPARE(canvas.grab().toImage(),finalWith);
    }
};
QTEST_MAIN(EditorInteractionTests)
#include "EditorInteractionTests.moc"
