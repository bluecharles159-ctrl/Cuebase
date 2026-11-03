#include <QtTest>
#include <QStandardPaths>
#include <QSettings>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QToolButton>
#include <QDialogButtonBox>
#include <QPainter>
#include <QPointer>
#include <QPlainTextEdit>
#include "AnimationEditor.h"
#include "BuildCanvas.h"
#include "BuildWorkspace.h"
#include "FormAnimation.h"
#include "FormAnimationPanel.h"
#include "OutputFormatDialog.h"
#include "PropertyInspector.h"
#include "UiTheme.h"

class CreativeWorkflowTests : public QObject {
    Q_OBJECT
    ElementDesign scene(const QString &kind="Shape") {
        auto d=ElementDesign::defaultScene();DObject o;o.id="subject";o.name="Subject";o.type=kind;o.base.rot={90,0,0};o.w=6;o.h=3;o.text="**Cuebase**\nProduction";o.fontPx=60;o.mat.color=QColor("#38bdf8");o.mat.shadeless=true;
        o.style["background.type"]="Gradient";o.style["fill.kind"]="Linear";o.style["fill.end"]="#db2777";o.style["corner.radius"]=12.;o.style["shadow.enabled"]=true;o.style["shadow.blur"]=6.;o.style["shadow.x"]=8.;o.style["effects.saturation"]=.6;o.style["text.format"]="Markdown";
        d.objects.append(o);return d;
    }
    QImage render(const ElementDesign &d,qint64 time=0) {
        QImage frame(640,360,QImage::Format_ARGB32_Premultiplied);frame.fill(Qt::transparent);QPainter p(&frame);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);d.render(p,frame.rect(),time,true,nullptr,false);p.end();return frame;
    }
    QTreeWidgetItem *row(QTreeWidget *tree,const QString &path) {
        for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole+1).toString()==path)return *it;return nullptr;
    }
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);qApp->setApplicationName("CuebaseCreativeWorkflowTests");installUiBehavior(qApp);applyUiTheme("Studio Blue",false);QDir().mkpath("artifacts");}
    void convertedSurfacePreservesAppearance_data(){QTest::addColumn<QString>("kind");for(const auto &kind:QStringList{"Shape","Text","Image"})QTest::newRow(qPrintable(kind))<<kind;}
    void convertedSurfacePreservesAppearance(){
        QFETCH(QString,kind);Library lib;BuildModel model;auto d=scene(kind);
        DObject parent;parent.type="Group";parent.id="parent";parent.base.loc={.5f,0,.25f};d.objects.append(parent);d.find("subject")->parentId="parent";
        model.reset(d);model.select("subject");const auto before=model.design.toJson();const auto expected=render(model.design);
        PropertyInspector inspector(&model,&lib);inspector.resize(330,800);inspector.show();
        QVERIFY(model.enterEdit());QCOMPARE(model.active()->type,QString("Mesh"));QCOMPARE(model.active()->surfaceType(),kind);
        QCOMPARE(render(model.design),expected);const auto saved=model.design.toJson();const auto restored=ElementDesign::fromJson(saved);QCOMPARE(render(restored),expected);
        QVERIFY(inspector.findChild<QDoubleSpinBox*>("property_effects.blur"));QVERIFY(inspector.findChild<QDoubleSpinBox*>("property_shadow.blur"));
        if(kind=="Text")QVERIFY(inspector.findChild<QPlainTextEdit*>("textContent"));
        QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);QVERIFY(model.redo());QCOMPARE(render(model.design),expected);
        QVERIFY(expected.save("artifacts/converted-"+kind+".png"));
    }
    void editedSurfaceRetainsTexture(){
        Library lib;BuildModel model;model.reset(scene());model.select("subject");QVERIFY(model.enterEdit());
        const auto before=render(model.design);model.active()->mesh.v[0].setX(model.active()->mesh.v[0].x()+.5f);model.notify();const auto edited=render(model.design);QVERIFY(edited!=before);
        int colored=0;for(int y=0;y<edited.height();++y)for(int x=0;x<edited.width();++x){const auto c=edited.pixelColor(x,y);if(c.alpha()>100&&c.red()!=c.blue())++colored;}QVERIFY(colored>1000);
        QCOMPARE(render(ElementDesign::fromJson(model.design.toJson())),edited);
    }
    void formEditsUseDisplayedPoseAndCancel(){
        Library lib;BuildModel model;model.reset(ElementDesign::defaultScene());const auto id=model.addMesh("Cube");auto *o=model.byId(id);o->base.rot={0,0,0};const Mesh zero=o->mesh;o->mesh.v[0].setX(o->mesh.v[0].x()+2);const auto path=FormAnimation::capture(*o,zero,"Corner");o->style[path]=.25;
        QVERIFY(model.enterEdit());model.esel.v={0};const auto displayed=FormAnimation::evaluate(*model.active(),0).v[0];
        BuildCanvas canvas(&model,&lib);canvas.resize(800,500);canvas.show();QTest::qWait(20);canvas.startMove();QTest::keyClick(&canvas,Qt::Key_X);QTest::keyClick(&canvas,Qt::Key_1);QTest::keyClick(&canvas,Qt::Key_Return);
        QCOMPARE(model.active()->style["form.poses"].toArray().size(),2);const auto moved=FormAnimation::evaluate(*model.active(),0).v[0];QVERIFY((moved-displayed-QVector3D(1,0,0)).length()<.0001);
        const auto confirmed=model.design.toJson();canvas.startScale();QTest::keyClick(&canvas,Qt::Key_2);QTest::keyClick(&canvas,Qt::Key_Escape);QCOMPARE(model.design.toJson(),confirmed);QVERIFY(QWidget::mouseGrabber()!=&canvas);
        QVERIFY(model.undo());QCOMPARE(model.active()->style["form.poses"].toArray().size(),1);
    }
    void extrusionInfluenceAndTopologyRetainKeys(){
        Library lib;BuildModel model;model.reset(ElementDesign::defaultScene());model.addMesh("Cube");QVERIFY(model.enterEdit());model.setSelMode(SelMode::Face);model.esel.f={0};model.active()->mesh.normalizeSel(model.esel,SelMode::Face);
        BuildCanvas canvas(&model,&lib);canvas.resize(800,500);canvas.show();QTest::qWait(20);canvas.startExtrude();QVERIFY(canvas.modalActive());QTest::keyClick(&canvas,Qt::Key_1);QTest::keyClick(&canvas,Qt::Key_Return);
        auto *o=model.active();const auto poses=o->style["form.poses"].toArray();QCOMPARE(poses.size(),1);const auto path="form."+poses[0].toObject()["id"].toString();
        o->keyChannel(path,0,0);o->keyChannel(path,1000,1);for(auto &key:o->track(path)->keys)key.interpolation="Linear";
        const Mesh zero=FormAnimation::evaluate(*o,0),full=FormAnimation::evaluate(*o,1000);QVERIFY(zero.v!=full.v);
        model.editSelectAll(true);const auto saved=model.design.toJson();QVERIFY(model.opSubdivide());
        o=model.active();QVERIFY(o->mesh.v.size()>full.v.size());QVERIFY(FormAnimation::evaluate(*o,0).v!=FormAnimation::evaluate(*o,1000).v);
        QCOMPARE(o->track(path)->keys.size(),2);auto restored=ElementDesign::fromJson(model.design.toJson());QCOMPARE(FormAnimation::evaluate(*restored.find(o->id),500).toJson(),FormAnimation::evaluate(*o,500).toJson());
        QVERIFY(model.undo());QCOMPARE(model.design.toJson(),saved);
    }
    void formPanelTracksTimeAndRemovesTransactionally(){
        BuildModel model;model.reset(ElementDesign::defaultScene());model.addMesh("Cube");auto *o=model.active();auto zero=o->mesh;o->mesh.v[0].setX(2);const auto path=FormAnimation::capture(*o,zero,"Corner");o->keyChannel(path,0,0);o->keyChannel(path,1000,1);for(auto &key:o->track(path)->keys)key.interpolation="Linear";
        FormAnimationPanel panel(&model);panel.resize(380,350);panel.show();QTest::qWait(20);auto *input=panel.findChild<QDoubleSpinBox*>("formInfluence");QVERIFY(input);model.setTime(500);QCOMPARE(input->value(),.5);
        auto *name=panel.findChild<QLineEdit*>("formName");name->setText("Corner lift");QMetaObject::invokeMethod(name,"editingFinished");QCOMPARE(model.active()->style["form.poses"].toArray()[0].toObject()["name"].toString(),QString("Corner lift"));
        const auto before=model.design.toJson();const auto evaluated=FormAnimation::evaluate(*model.active(),model.t).toJson();panel.findChild<QPushButton*>("formBakeRemove")->click();QVERIFY(model.active()->style["form.poses"].toArray().isEmpty());QVERIFY(!model.active()->track(path));QCOMPARE(model.active()->mesh.toJson(),evaluated);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
        QVERIFY(panel.grab().save("artifacts/form-animation-panel.png"));
    }
    void applyingModifierRetainsFormKeys(){
        BuildModel model;model.reset(ElementDesign::defaultScene());model.addMesh("Cube");auto *o=model.active();const auto zero=o->mesh;o->mesh.v[0].setX(2);const auto path=FormAnimation::capture(*o,zero,"Corner");o->keyChannel(path,0,0);o->keyChannel(path,1000,1);for(auto &key:o->track(path)->keys)key.interpolation="Linear";
        Modifier array;array.type="Array";array.count=2;array.rel={2,0,0};o->mods.append(array);
        const auto before=model.design.toJson();const auto atZero=model.design.evalMesh(*o,0).toJson();const auto atFull=model.design.evalMesh(*o,1000).toJson();
        model.setTime(500);QVERIFY(model.applyModifier(o->id,0));o=model.active();QVERIFY(o->mods.isEmpty());QCOMPARE(o->track(path)->keys.size(),2);QCOMPARE(model.design.evalMesh(*o,0).toJson(),atZero);QCOMPARE(model.design.evalMesh(*o,1000).toJson(),atFull);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
    }
    void graphSelectionActiveAndEmptyRemainStable(){
        BuildModel model;model.reset(scene());model.select("subject");for(const auto &axis:QStringList{"x","y","z"}){model.active()->keyChannel("position."+axis,0,0);model.active()->keyChannel("position."+axis,1000,2);}
        AnimationEditor editor(&model,true);editor.resize(1000,420);editor.show();QTest::qWait(20);auto *tree=editor.findChild<QTreeWidget*>();auto *curve=editor.findChild<QWidget*>("animationCurve");QVERIFY(tree&&curve);
        tree->clearSelection();auto *x=row(tree,"position.x"),*y=row(tree,"position.y"),*z=row(tree,"position.z");QVERIFY(x&&y&&z);x->setSelected(true);QCOMPARE(curve->property("visibleChannels").toStringList(),QStringList{"subject|position.x"});y->setSelected(true);z->setSelected(true);QCOMPARE(curve->property("visibleChannels").toStringList().size(),3);
        tree->setCurrentItem(y,0,QItemSelectionModel::NoUpdate);QCOMPARE(curve->property("activeChannel").toString(),QString("subject|position.y"));QVERIFY(y->font(0).bold());QVERIFY(!x->font(0).bold());QCOMPARE(y->parent()->text(0),QString("Translation"));
        tree->expandAll();QVERIFY(editor.grab().save("artifacts/graph-active-curve.png"));tree->clearSelection();QVERIFY(curve->property("visibleChannels").toStringList().isEmpty());model.active()->name="Renamed";model.notify();QVERIFY(curve->property("visibleChannels").toStringList().isEmpty());
    }
    void graphDuplicateCancellationAndClipboard(){
        BuildModel model;model.reset(scene());model.select("subject");model.active()->keyChannel("position.x",0,0);model.active()->keyChannel("position.x",1000,3);
        AnimationEditor editor(&model,true);editor.resize(1000,420);editor.show();QTest::qWait(20);auto *curve=editor.findChild<QWidget*>("animationCurve");QVERIFY(curve);curve->setFocus();QTest::keyClick(curve,Qt::Key_A,Qt::ControlModifier);
        const auto before=model.design.toJson();QTest::keyClick(curve,Qt::Key_D,Qt::ShiftModifier);QCOMPARE(model.active()->track("position.x")->keys.size(),4);QTest::keyClick(curve,Qt::Key_Escape);QCOMPARE(model.design.toJson(),before);QVERIFY(QWidget::mouseGrabber()!=curve);
        QTest::keyClick(curve,Qt::Key_A,Qt::ControlModifier);QTest::keyClick(curve,Qt::Key_C,Qt::ControlModifier);model.setTime(2000);QTest::keyClick(curve,Qt::Key_V,Qt::ControlModifier);QCOMPARE(model.active()->track("position.x")->keys.size(),4);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
    }
    void workViewsLeaveBroadcastUnchanged(){
        Library lib;BuildWorkspace workspace(&lib);workspace.resize(1300,850);workspace.show();auto *model=workspace.findChild<BuildModel*>();auto d=scene();DObject box;box.id="cube";box.type="Mesh";box.mesh=Mesh::primitive("Cube");box.base.loc={3,2,1};d.objects.append(box);model->reset(d);
        auto *canvas=workspace.findChild<BuildCanvas*>();auto *view=workspace.findChild<QComboBox*>("buildWorkCamera");auto *final=workspace.findChild<QWidget*>("compositionCleanPreview");QVERIFY(canvas&&view&&final);QTest::qWait(30);const auto saved=model->design.toJson();const auto output=final->grab().toImage();
        for(int i=1;i<4;++i){view->setCurrentIndex(i);QMetaObject::invokeMethod(view,"activated",Q_ARG(int,i));QVERIFY(!canvas->cameraView());QCOMPARE(canvas->viewName(),view->currentText());QCOMPARE(final->grab().toImage(),output);QCOMPARE(model->design.toJson(),saved);}
        QVERIFY(workspace.grab().save("artifacts/build-dual-camera.png"));canvas->setCameraView(true);QCOMPARE(view->currentIndex(),0);QCOMPARE(render(model->design),render(d));
    }
    void outputPresetsCancelAndPersist(){
        Library lib;const auto original=lib.display.resolution;QPointer<QDialog> cancel=createOutputFormatDialog(&lib,nullptr);cancel->show();auto *preset=cancel->findChild<QComboBox*>("outputFormatPreset");preset->setCurrentIndex(2);QMetaObject::invokeMethod(preset,"activated",Q_ARG(int,2));QCOMPARE(lib.display.resolution,original);cancel->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QPointer<QDialog> dialog=createOutputFormatDialog(&lib,nullptr);dialog->show();preset=dialog->findChild<QComboBox*>("outputFormatPreset");preset->setCurrentIndex(2);QMetaObject::invokeMethod(preset,"activated",Q_ARG(int,2));QSignalSpy changed(&lib,&Library::displayChanged);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QCOMPARE(changed.count(),1);QCOMPARE(lib.display.resolution,QString("1080x1920"));Library reopened;reopened.load();QCOMPARE(reopened.display.resolution,lib.display.resolution);lib.display.resolution=original;QVERIFY(lib.saveChecked());
    }
};
QTEST_MAIN(CreativeWorkflowTests)
#include "CreativeWorkflowTests.moc"
