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
#include <QTreeWidgetItemIterator>
#include <QPainter>
#include <QWheelEvent>
#include "BuildCanvas.h"
#include "AnimationEditor.h"
#include "CurveNoise.h"
#include "Data.h"
#include "FrameExport.h"
#include "UiControls.h"
#include "UiTheme.h"

class EditorInteractionTests:public QObject{
    Q_OBJECT
    ElementDesign scene(){auto design=ElementDesign::defaultScene();DObject object;object.id="plate";object.name="Plate";object.type="Shape";object.w=4;object.h=2;object.base.rot={90,0,0};object.mat.shadeless=true;object.keyChannel("position.x",0,0);object.keyChannel("position.x",1000,2);object.keyChannel("rotation.y",0,0);object.keyChannel("rotation.y",1000,30);design.objects.append(object);return design;}
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);qApp->setApplicationName("CuebaseEditorInteractionTests");installUiBehavior(qApp);applyUiTheme("Studio Blue",false);QDir().mkpath("artifacts");}
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
