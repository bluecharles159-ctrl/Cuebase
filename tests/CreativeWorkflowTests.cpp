#include <QtTest>
#include <QStandardPaths>
#include <QSettings>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QToolButton>
#include <QDialogButtonBox>
#include <QPainter>
#include <QWheelEvent>
#include <QPointer>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QMenu>
#include "AnimationEditor.h"
#include "BuildCanvas.h"
#include "BuildWorkspace.h"
#include "FormAnimation.h"
#include "FormAnimationPanel.h"
#include "FrameExport.h"
#include "CueSequence.h"
#include "Data.h"
#include <QSaveFile>
#include <QTemporaryDir>
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
    void buildExtractsTheSelectedPlaysRange() {
        auto design = scene();
        design.durationMs = 4000;
        const QJsonObject cue{{"id","promo"},{"name","Promo"},{"start",0},{"end",2000},
                              {"repeat",2},{"hold",false}};
        design.animationCues = {cue};
        auto entry = newCueBoardEntry(cue);
        entry["plays"] = QJsonArray{QJsonObject{},QJsonObject{{"start",800},{"end",1600}}};
        Element source;
        source.id = "extract-play";
        source.type = "Custom";
        source.design = design.toJson();
        source.cueBoard = {entry};
        Library library;
        library.elements = {source};
        BuildWorkspace workspace(&library);
        workspace.editCueSection(source.id,entry["id"].toString(),1);
        auto *model = workspace.findChild<BuildModel*>();
        QVERIFY(model);
        QCOMPARE(model->design.startMs,800);
        QCOMPARE(model->design.durationMs,1600);
        QCOMPARE(model->t,qint64(800));
        QVERIFY(model->design.animationCues.isEmpty());
        const auto studio = compileCueBoard(design,source.cueBoard);
        QCOMPARE(studio.steps.size(),2);
        QCOMPARE(studio.steps[1].start,qint64(model->design.startMs));
        QCOMPARE(studio.steps[1].end,qint64(model->design.durationMs));
        model->select("subject");
        model->active()->keyChannel("position.x",920,1.);
        model->active()->keyChannel("position.x",1280,2.);
        AnimationEditor graph(model,true);
        graph.resize(800,320);
        graph.show();
        QTest::qWait(20);
        auto *curve = graph.findChild<QWidget*>("animationCurve");
        QVERIFY(curve);
        QTest::keyClick(curve,Qt::Key_A,Qt::ControlModifier);
        QToolButton *cueButton = nullptr;
        for (auto *button : graph.findChildren<QToolButton*>())
            if (button->accessibleName() == "Cue selected frames") cueButton = button;
        QVERIFY(cueButton);
        cueButton->click();
        auto *dialog = workspace.findChild<QDialog*>("cueRangeDialog");
        QVERIFY(dialog);
        QCOMPARE(model->cueRangeStart,qint64(920));
        QCOMPARE(model->cueRangeEnd,qint64(1280));
        QVERIFY(graph.grab().save("artifacts/selected-cue-range.png"));
        dialog->reject();
        QCOMPARE(model->cueRangeStart,qint64(-1));
        QCOMPARE(model->cueRangeEnd,qint64(-1));
        QVERIFY(model->design.animationCues.isEmpty());
        QCOMPARE(library.element(source.id)->design,source.design);
        QCOMPARE(library.element(source.id)->cueBoard,source.cueBoard);
    }
    void implicitNestedCueEntriesStayEditable(){
        auto design=ElementDesign::defaultScene();design.durationMs=1000;
        const QJsonObject cue{{"id","child"},{"name","Child"},{"start",0},{"end",1000},{"hold",false}};design.animationCues={cue};
        Element root;root.design=design.toJson();auto entry=newCueBoardEntry(cue);entry["sections"]=QJsonArray{QJsonObject{{"design",design.toJson()}}};
        auto sibling=entry;sibling["id"]="sibling";root.cueBoard={entry,sibling};
        const QStringList parentPath{entry["id"].toString(),"0"};const auto first=cueBoardContext(root,parentPath);const auto again=cueBoardContext(root,parentPath);
        QCOMPARE(first.cueBoard,again.cueBoard);QCOMPARE(first.cueBoard.size(),1);const auto childId=first.cueBoard.first().toObject()["id"].toString();QVERIFY(!childId.isEmpty());
        auto path=parentPath;path<<childId<<"0";const QJsonObject changed{{"name","Edited first play"}};
        QVERIFY(setCueBoardSection(root,path,changed));QCOMPARE(cueBoardSection(root,path),changed);QCOMPARE(root.cueBoard[1].toObject(),sibling);QCOMPARE(root.design,design.toJson());
        const auto reopened=cueBoardContext(root,parentPath);QCOMPARE(reopened.cueBoard.first().toObject()["id"].toString(),childId);QVERIFY(compileCueBoard(design,root.cueBoard,{}).error.isEmpty());
    }
    void nestedCueBoardRespectsTotalSectionLimit(){
        auto source=ElementDesign::defaultScene();source.durationMs=1000;
        const QJsonObject cue{{"id","child"},{"name","Child"},{"start",0},{"end",1000},{"repeat",100},{"hold",false}};
        source.animationCues={cue};const auto nestedEntry=newCueBoardEntry(cue);
        const QJsonObject section{{"design",source.toJson()},{"board",QJsonArray{nestedEntry}},{"boardSettings",QJsonObject{{"repeat",100}}}};
        auto outer=newCueBoardEntry(cue);outer["repeat"]=1;outer["sections"]=QJsonArray{section};
        const auto exact=compileCueBoard(source,QJsonArray{outer},{});QVERIFY2(exact.error.isEmpty(),qPrintable(exact.error));QCOMPARE(exact.steps.size(),10000);QCOMPARE(exact.duration(),qint64(10000000));
        const auto last=exact.sample(9999999);QVERIFY(last.visible);QCOMPARE(last.step,9999);QCOMPARE(last.frame,qint64(999));
        outer["repeat"]=2;outer["sections"]=QJsonArray{section,section};
        const auto excessive=compileCueBoard(source,QJsonArray{outer},{});QVERIFY(!excessive.error.isEmpty());QVERIFY(excessive.steps.size()<=10000);QCOMPARE(excessive.duration(),qint64(0));QVERIFY(!excessive.sample(0).visible);
        outer["repeat"]=1;const auto siblings=compileCueBoard(source,QJsonArray{outer,outer},{});QVERIFY(!siblings.error.isEmpty());QVERIFY(siblings.steps.size()<=10000);
    }
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);qApp->setApplicationName("CuebaseCreativeWorkflowTests");installUiBehavior(qApp);applyUiTheme("Studio Blue",false);QDir().mkpath("artifacts");}
    void convertedSurfacePreservesAppearance_data(){QTest::addColumn<QString>("kind");for(const auto &kind:QStringList{"Shape","Text","Image"})QTest::newRow(qPrintable(kind))<<kind;}
    void convertedSurfacePreservesAppearance(){
        QFETCH(QString,kind);Library lib;BuildModel model;auto d=scene(kind);
        DObject parent;parent.type="Group";parent.id="parent";parent.base.loc={.5f,0,.25f};d.objects.append(parent);d.find("subject")->parentId="parent";
        model.reset(d);model.select("subject");const auto before=model.design.toJson();const auto expected=render(model.design);
        PropertyInspector inspector(&model,&lib);inspector.resize(330,800);inspector.show();
        QCOMPARE(model.design.toJson(),before);
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
    void convertedTextStillFitsItsContent(){
        BuildModel model;auto design=scene("Text");design.find("subject")->style["text.autoFit"]=true;design.find("subject")->style["text.background"]=true;model.reset(design);model.select("subject");const auto initial=render(model.design);QVERIFY(model.enterEdit());QCOMPARE(render(model.design),initial);
        const QString longer="A longer production headline\nwith a second line";model.active()->text=longer;design.find("subject")->text=longer;QCOMPARE(render(model.design),render(design));
        const auto displayed=model.active()->evaluated(0);QVector3D low,high;QVERIFY(FormAnimation::evaluate(*model.active(),0).bounds(low,high));QVERIFY(qAbs(high.x()-low.x()-displayed.w)<.0001);QVERIFY(qAbs(high.y()-low.y()-displayed.h)<.0001);
        QCOMPARE(render(ElementDesign::fromJson(model.design.toJson())),render(design));
    }
    void planarFormKeepsOutwardEffects_data(){QTest::addColumn<bool>("shadow");QTest::newRow("shadow")<<true;QTest::newRow("blur")<<false;}
    void planarFormKeepsOutwardEffects(){
        QFETCH(bool,shadow);BuildModel model;auto design=scene("Text");design.world.transparent=true;
        auto *text=design.find("subject");text->text.clear();text->style=QJsonObject{{"text.background",true},{"text.backgroundColor","#ffdd2233"}};
        text->style["shadow.enabled"]=shadow;text->style["shadow.color"]="#ff00ff00";text->style["shadow.x"]=60.;text->style["shadow.y"]=30.;text->style["shadow.blur"]=0.;text->style["effects.blur"]=shadow?0.:12.;
        model.reset(design);model.select("subject");const auto initial=render(model.design);QVERIFY(model.enterEdit());QCOMPARE(model.active()->mesh.v.size(),4);
        model.pushUndo(true);const auto zero=model.active()->mesh;model.active()->mesh.v[0].setX(model.active()->mesh.v[0].x()+.7f);
        const auto path=FormAnimation::capture(*model.active(),zero,"Warp corner");QVERIFY(!path.isEmpty());model.active()->keyChannel(path,0,0);model.active()->keyChannel(path,1000,1);model.notify();
        QCOMPARE(render(model.design,0),initial);const auto frame=render(model.design,1000);QVERIFY(frame!=initial);
        const auto camera=model.design.sceneCamera(1000,640./360.);const auto world=model.design.worldMatrix(*model.active(),1000);QPolygonF face;
        const auto mesh=FormAnimation::evaluate(*model.active(),1000);for(int vertex:mesh.f.first()){QPointF at;QVERIFY(camera.toScreen(world.map(mesh.v[vertex]),QRectF(0,0,640,360),&at));face<<at;}
        auto without=model.design;without.find("subject")->style["shadow.enabled"]=false;without.find("subject")->style["effects.blur"]=0.;const auto plain=render(without,1000);
        auto outside=[&](const QImage &image){int count=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(qAlpha(image.pixel(x,y))>30&&!face.containsPoint(QPointF(x+.5,y+.5),Qt::OddEvenFill))++count;return count;};
        QVERIFY2(outside(frame)>outside(plain)+100,"The effect must remain visible outside the deformed face, not be clipped to its triangles.");
        const auto restored=ElementDesign::fromJson(model.design.toJson());QCOMPARE(render(restored,1000),frame);QVERIFY(frame.save(shadow?"artifacts/form-surface-shadow.png":"artifacts/form-surface-blur.png"));
        QVERIFY(model.undo());QCOMPARE(render(model.design),initial);
    }
    void surfaceOriginsPreservePictureAndChildren_data(){convertedSurfacePreservesAppearance_data();}
    void surfaceOriginsPreservePictureAndChildren(){
        QFETCH(QString,kind);Library lib;BuildModel model;auto design=scene(kind);DObject child;child.id="child";child.type="Shape";child.parentId="subject";child.w=1;child.h=.5;child.base.loc={1,0,1};child.mat.color=Qt::yellow;design.objects.append(child);model.reset(design);model.select("subject");
        const auto before=model.design.toJson();const auto expected=render(model.design);const auto childWorld=model.design.worldMatrix(*model.byId("child"),0);
        QVERIFY(model.setOrigin("Bounds:Top Left"));QCOMPARE(model.active()->surfaceOffset(),QVector3D(3,-1.5,0));QCOMPARE(render(model.design),expected);QCOMPARE(model.design.worldMatrix(*model.byId("child"),0),childWorld);
        model.design.cursor={2,1,3};QVERIFY(model.setOrigin("Cursor"));QVERIFY((model.design.worldMatrix(*model.active(),0).map(QVector3D())-model.design.cursor).length()<.0001);QCOMPARE(render(model.design),expected);
        const auto restored=ElementDesign::fromJson(model.design.toJson());QCOMPARE(render(restored),expected);QVERIFY(model.undo());QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
    }
    void formOriginKeepsAnimatedWorldGeometry(){
        BuildModel model;model.reset(ElementDesign::defaultScene());model.addMesh("Cube");auto *o=model.active();auto zero=o->mesh;o->mesh.v[0].setX(3);const auto path=FormAnimation::capture(*o,zero,"Corner");o->keyChannel(path,0,0);o->keyChannel(path,1000,1);o->keyChannel("rotation.z",0,0);o->keyChannel("rotation.z",1000,70);
        QVector<QVector<QVector3D>> expected;for(int time:{0,200,1000}){auto vertices=FormAnimation::evaluate(*o,time).v;const auto world=model.design.worldMatrix(*o,time);for(auto &v:vertices)v=world.map(v);expected.append(vertices);}
        model.setTime(200);const auto before=model.design.toJson();QVERIFY(model.setOrigin("Bottom"));o=model.active();
        const int times[]={0,200,1000};for(int sample=0;sample<3;++sample){const auto mesh=FormAnimation::evaluate(*o,times[sample]);const auto world=model.design.worldMatrix(*o,times[sample]);for(int i=0;i<mesh.v.size();++i)QVERIFY((world.map(mesh.v[i])-expected[sample][i]).length()<.0001);}
        QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
    }
    void originOverlayControlOnlyChangesTheEditor(){
        Library lib;BuildWorkspace workspace(&lib);workspace.resize(1300,850);workspace.show();auto *model=workspace.findChild<BuildModel*>();model->reset(scene());model->select("subject");QVERIFY(model->setOrigin("Bounds:Top Left"));
        auto *canvas=workspace.findChild<BuildCanvas*>();auto *button=workspace.findChild<QToolButton*>("viewportOverlays");QVERIFY(canvas&&button);QCheckBox *origins=nullptr;for(auto *check:button->menu()->findChildren<QCheckBox*>())if(check->text()=="Object origins")origins=check;QVERIFY(origins);
        button->menu()->findChild<QCheckBox*>("overlayMaster")->setChecked(true);origins->setChecked(true);QTest::qWait(20);const auto sceneData=model->design.toJson();const auto final=render(model->design);const auto shown=canvas->grab().toImage();QVERIFY(shown.save("artifacts/origin-position.png"));
        bool visibleOrigin=false;for(int y=0;y<shown.height();++y)for(int x=0;x<shown.width();++x)if(shown.pixelColor(x,y)==QColor("#ffc46b"))visibleOrigin=true;QVERIFY(visibleOrigin);
        origins->setChecked(false);QVERIFY(!canvas->showOrigins);QVERIFY(canvas->grab().toImage()!=shown);QCOMPARE(model->design.toJson(),sceneData);QCOMPARE(render(model->design),final);origins->setChecked(true);
    }
    void canvasZoomKeepsPointerAndFinalStable(){
        Library lib;BuildModel model;model.reset(scene());model.select("subject");BuildCanvas canvas(&model,&lib);canvas.resize(800,500);canvas.show();QTest::qWait(20);
        const auto saved=model.design.toJson();const auto final=render(model.design);
        for(bool broadcast:{true,false}){
            if(broadcast)canvas.setCameraView(true);else canvas.viewFront();canvas.frameAll();canvas.grab();
            const QPointF anchor(110,330);
            for(int delta:{-120,-1200,1200,120,-12000,12000}){
                const auto before=canvas.property("compositionFrame").toRectF();QVERIFY(before.width()>0&&before.height()>0);
                const QPointF normalized((anchor.x()-before.x())/before.width(),(anchor.y()-before.y())/before.height());
                QWheelEvent wheel(anchor,canvas.mapToGlobal(anchor.toPoint()),QPoint(),QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&canvas,&wheel);const auto screenshot=canvas.grab();
                const auto after=canvas.property("compositionFrame").toRectF();QVERIFY(qIsFinite(after.width())&&after.width()>0);QVERIFY(qIsFinite(after.height())&&after.height()>0);
                QVERIFY(qAbs((anchor.x()-after.x())/after.width()-normalized.x())<.001);QVERIFY(qAbs((anchor.y()-after.y())/after.height()-normalized.y())<.001);
                const auto spacing=canvas.property("rulerSpacing").toPointF();QVERIFY(spacing.x()>=60&&spacing.y()>=60);
                QCOMPARE(model.design.toJson(),saved);QCOMPARE(render(model.design),final);
                if(broadcast&&delta==-1200)QVERIFY(screenshot.save("artifacts/canvas-zoom-out.png"));
            }
        }
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
    void modifierCardAndShortcutPreserveStack(){
        Library lib;BuildWorkspace workspace(&lib);workspace.resize(1300,850);workspace.show();
        auto *model=workspace.findChild<BuildModel*>();auto *canvas=workspace.findChild<BuildCanvas*>();QVERIFY(model&&canvas);
        model->reset(ElementDesign::defaultScene());model->addMesh("Cube");
        Modifier mirror;mirror.type="Mirror";mirror.ax=true;mirror.options["offset.x"]=2.;model->active()->mods.append(mirror);
        Modifier array;array.type="Array";array.count=2;array.rel={2,0,0};model->active()->mods.append(array);model->notify();
        auto *tab=workspace.findChild<QPushButton*>("buildTab_Modifiers");QVERIFY(tab);tab->click();
        auto refresh=[&]{QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QCoreApplication::processEvents();};refresh();
        auto expand=[&]{for(auto *section:workspace.findChildren<QToolButton*>())if((section->text()=="MIRROR"||section->text()=="ARRAY")&&!section->isChecked())section->click();};expand();
        auto buttons=workspace.findChildren<QPushButton*>("applyModifier");QCOMPARE(buttons.size(),2);QVERIFY(buttons[1]->isVisible());
        const auto before=model->design.toJson();const auto expected=model->design.evalMesh(*model->active(),0).toJson();
        QTest::mouseClick(buttons[1],Qt::LeftButton);QVERIFY(model->active()->mods.isEmpty());QCOMPARE(model->active()->mesh.toJson(),expected);
        QVERIFY(model->undo());QCOMPARE(model->design.toJson(),before);QVERIFY(model->redo());QCOMPARE(model->active()->mesh.toJson(),expected);QVERIFY(model->undo());refresh();
        canvas->setFocus();QTest::keyClick(canvas,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QCOMPARE(model->active()->mods.size(),1);QCOMPARE(model->active()->mods.first().type,QString("Array"));QCOMPARE(model->design.evalMesh(*model->active(),0).toJson(),expected);
        QVERIFY(model->undo());QCOMPARE(model->design.toJson(),before);QVERIFY(model->enterEdit());refresh();
        buttons=workspace.findChildren<QPushButton*>("applyModifier");QCOMPARE(buttons.size(),2);for(auto *button:buttons)QVERIFY(!button->isEnabled());
        const auto editing=model->design.toJson();QTest::keyClick(canvas,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QCOMPARE(model->design.toJson(),editing);
        model->exitEdit();model->active()->locked=true;model->notify();refresh();buttons=workspace.findChildren<QPushButton*>("applyModifier");QCOMPARE(buttons.size(),2);for(auto *button:buttons)QVERIFY(!button->isEnabled());
        const auto locked=model->design.toJson();QTest::keyClick(canvas,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QCOMPARE(model->design.toJson(),locked);
    }
    void joinedAndSeparatedFormsRetainAnimation(){
        BuildModel model;model.reset(ElementDesign::defaultScene());const auto first=model.addMesh("Cube");auto *o=model.active();const auto zero=o->mesh;o->mesh.v[0].setX(2);
        const auto path=FormAnimation::capture(*o,zero,"Corner");o->keyChannel(path,0,0);o->keyChannel(path,1000,1);
        DObject copy=*o;copy.id="second";copy.base.loc={5,0,1};copy.base.rot={0,40,0};copy.base.scl={2,1,1};model.design.objects.append(copy);model.sel={first,copy.id};model.activeId=first;
        const auto before=model.design.toJson();const auto inverse=model.design.worldMatrix(*model.active(),0).inverted();QVector<Mesh> expected;
        for(int time:{0,500,1000}){Mesh combined=FormAnimation::evaluate(*model.byId(first),time);auto part=FormAnimation::evaluate(copy,time);part.transform(inverse*model.design.worldMatrix(copy,time));combined.append(part);expected.append(combined);}
        QVERIFY(model.joinSelected());QCOMPARE(model.active()->style.value("form.poses").toArray().size(),2);const auto poses=model.active()->style.value("form.poses").toArray();QVERIFY(poses[0].toObject()["id"]!=poses[1].toObject()["id"]);
        for(int i=0;i<3;++i){const auto actual=FormAnimation::evaluate(*model.active(),i*500);QCOMPARE(actual.f,expected[i].f);for(int v=0;v<actual.v.size();++v)QVERIFY((actual.v[v]-expected[i].v[v]).length()<.0001);}
        const auto joined=model.design.toJson();const auto parts=FormAnimation::evaluate(*model.active(),500).looseParts();QVERIFY(model.separateLooseParts());QCOMPARE(model.sel.size(),2);int part=0;
        for(const auto &object:model.design.objects)if(model.sel.contains(object.id)){const auto actual=FormAnimation::evaluate(object,500);QCOMPARE(actual.toJson(),parts[part++].toJson());for(const auto &pose:object.style["form.poses"].toArray())QCOMPARE(object.track("form."+pose.toObject()["id"].toString())->keys.size(),2);}
        QVERIFY(model.undo());QCOMPARE(model.design.toJson(),joined);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
    }
    void insetUsesTheDisplayedForm(){
        Library lib;BuildModel model;model.reset(ElementDesign::defaultScene());model.addMesh("Cube");auto *o=model.active();auto zero=o->mesh;o->mesh.v[0]+=QVector3D(2,1,0);const auto path=FormAnimation::capture(*o,zero,"Corner");o->style[path]=.25;
        QVERIFY(model.enterEdit());model.setSelMode(SelMode::Face);model.esel.f={0};model.active()->mesh.normalizeSel(model.esel,SelMode::Face);
        auto expected=FormAnimation::evaluate(*model.active(),0);auto selection=model.esel;QVector<std::pair<int,QVector3D>> centers;QVERIFY(expected.inset(selection,model.selMode,&centers));for(const auto &pair:centers)expected.v[pair.first]+=(pair.second-expected.v[pair.first])*.5f;
        const auto before=model.design.toJson();BuildCanvas canvas(&model,&lib);canvas.resize(800,500);canvas.show();canvas.startInset();QTest::keyClicks(&canvas,"0.5");QTest::keyClick(&canvas,Qt::Key_Return);
        const auto actual=FormAnimation::evaluate(*model.active(),0);QCOMPARE(actual.f,expected.f);for(int i=0;i<actual.v.size();++i)QVERIFY((actual.v[i]-expected.v[i]).length()<.0001);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
    }
    void formKeysHoverAndExplicitRemoval(){
        BuildModel model;model.reset(ElementDesign::defaultScene());model.addMesh("Cube");auto *o=model.active();auto zero=o->mesh;o->mesh.v[0].setX(2);const auto path=FormAnimation::capture(*o,zero,"Corner");
        FormAnimationPanel panel(&model);panel.resize(380,350);panel.show();QTest::qWait(20);auto *input=panel.findChild<QDoubleSpinBox*>("formInfluence");input->setValue(.25);QCursor::setPos(input->mapToGlobal(input->rect().center()));QTest::keyClick(input,Qt::Key_I);QVERIFY(model.propertyIsKeyed(*model.active(),path,0));
        model.setTime(1000);input->setValue(.75);QCursor::setPos(input->mapToGlobal(input->rect().center()));QTest::keyClick(input,Qt::Key_K);QCOMPARE(model.active()->track(path)->keys.size(),2);
        input->setValue(.5);const auto before=model.design.toJson();panel.findChild<QPushButton*>("formRemoveKey")->click();QVERIFY(!model.propertyIsKeyed(*model.active(),path,1000));QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
    }
    void formAnimationExportsAfterReopen(){
        auto design=ElementDesign::defaultScene();design.durationMs=2000;design.camera()->cam.orthoScale=12;
        DObject object;object.id="form-demo";object.name="Animated form";object.mesh=Mesh::primitive("Cube");object.base.rot={20,0,25};object.mat.color=QColor("#5da9ff");const auto zero=object.mesh;
        for(auto &vertex:object.mesh.v)if(vertex.z()>0)vertex.setZ(vertex.z()+1.5f);
        const auto path=FormAnimation::capture(object,zero,"Lift top face");object.keyChannel(path,0,0);object.keyChannel(path,1000,1);object.keyChannel(path,2000,0);design.objects.append(object);
        const auto json=design.toJson().toUtf8();QSaveFile example("artifacts/FormAnimation.cuebuild");QVERIFY(example.open(QIODevice::WriteOnly));QCOMPARE(example.write(json),json.size());QVERIFY(example.commit());const auto restored=ElementDesign::fromJson(QString::fromUtf8(json));
        QTemporaryDir temporary;FrameExportOptions options;options.size={640,360};options.endFrame=26;const auto result=exportSceneFrames(restored,temporary.path()+"/frames",options);QVERIFY2(result.success,qPrintable(result.error));QCOMPARE(result.written,26);
        for(int frame:{0,25}){const QImage exported(temporary.path()+QString("/frames/frame_%1.png").arg(frame,6,10,QChar('0')));QVERIFY(!exported.isNull());QCOMPARE(exported.convertToFormat(QImage::Format_ARGB32),render(design,frame*40).convertToFormat(QImage::Format_ARGB32));QVERIFY(exported.save(QString("artifacts/form-%1.png").arg(frame==0?"start":"end")));}
        QVERIFY(render(design,0)!=render(design,1000));
    }
    void graphSelectionActiveAndEmptyRemainStable(){
        BuildModel model;model.reset(scene());model.select("subject");int axisIndex=0;for(const auto &axis:QStringList{"x","y","z"}){model.active()->keyChannel("position."+axis,0,0);model.active()->keyChannel("position."+axis,1000,2.-.6*axisIndex++);}
        AnimationEditor editor(&model,true);editor.resize(1000,420);editor.show();QTest::qWait(20);auto *tree=editor.findChild<QTreeWidget*>();auto *curve=editor.findChild<QWidget*>("animationCurve");QVERIFY(tree&&curve);
        tree->clearSelection();auto *x=row(tree,"position.x"),*y=row(tree,"position.y"),*z=row(tree,"position.z");QVERIFY(x&&y&&z);x->setSelected(true);QCOMPARE(curve->property("visibleChannels").toStringList(),QStringList{"subject|position.x"});y->setSelected(true);z->setSelected(true);QCOMPARE(curve->property("visibleChannels").toStringList().size(),3);
        tree->setCurrentItem(y,0,QItemSelectionModel::NoUpdate);QCOMPARE(curve->property("activeChannel").toString(),QString("subject|position.y"));QVERIFY(y->font(0).bold());QVERIFY(!x->font(0).bold());QCOMPARE(y->parent()->text(0),QString("Translation"));
        const auto range=curve->property("viewRange").toList();const QRectF area=QRectF(curve->rect()).adjusted(62,28,-14,-26);const double value=model.active()->track("position.x")->evaluate(800,0);
        const QPointF at(area.left()+(800-range[0].toDouble())/(range[1].toDouble()-range[0].toDouble())*area.width(),area.bottom()-(value-range[2].toDouble())/(range[3].toDouble()-range[2].toDouble())*area.height());
        QTest::mouseClick(curve,Qt::LeftButton,Qt::NoModifier,at.toPoint());QCOMPARE(curve->property("activeChannel").toString(),QString("subject|position.x"));QCOMPARE(curve->property("visibleChannels").toStringList().size(),3);QVERIFY(x->font(0).bold());QVERIFY(!y->font(0).bold());
        tree->expandAll();QVERIFY(editor.grab().save("artifacts/graph-active-curve.png"));tree->clearSelection();QVERIFY(curve->property("visibleChannels").toStringList().isEmpty());model.active()->name="Renamed";model.notify();QVERIFY(curve->property("visibleChannels").toStringList().isEmpty());
    }
    void graphDuplicateCancellationAndClipboard(){
        BuildModel model;model.reset(scene());model.select("subject");model.active()->keyChannel("position.x",0,0);model.active()->keyChannel("position.x",1000,3);
        AnimationEditor editor(&model,true);editor.resize(1000,420);editor.show();QTest::qWait(20);auto *curve=editor.findChild<QWidget*>("animationCurve");QVERIFY(curve);curve->setFocus();QTest::keyClick(curve,Qt::Key_A,Qt::ControlModifier);
        const auto before=model.design.toJson();QTest::keyClick(curve,Qt::Key_D,Qt::ShiftModifier);QCOMPARE(model.active()->track("position.x")->keys.size(),4);QTest::keyClick(curve,Qt::Key_Escape);QCOMPARE(model.design.toJson(),before);QVERIFY(QWidget::mouseGrabber()!=curve);
        QTest::keyClick(curve,Qt::Key_A,Qt::ControlModifier);QTest::keyClick(curve,Qt::Key_C,Qt::ControlModifier);model.setTime(2000);QTest::keyClick(curve,Qt::Key_V,Qt::ControlModifier);QCOMPARE(model.active()->track("position.x")->keys.size(),4);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
    }
    void graphKeyboardScaleAndCancel(){
        BuildModel model;model.reset(scene());model.select("subject");model.active()->keyChannel("position.x",500,1);model.active()->keyChannel("position.x",1500,3);
        AnimationEditor editor(&model,true);editor.resize(1000,420);editor.show();QTest::qWait(20);auto *curve=editor.findChild<QWidget*>("animationCurve");QVERIFY(curve);curve->setFocus();QTest::keyClick(curve,Qt::Key_A,Qt::ControlModifier);
        // A Windows service runner may not move the system cursor. Anchor at its
        // actual position, then deliver the same native-widget mouse event path.
        const auto before=model.design.toJson();const QPoint start=curve->mapFromGlobal(QCursor::pos());
        auto move=[&](QPoint position){QMouseEvent event(QEvent::MouseMove,QPointF(position),QPointF(curve->mapToGlobal(position)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(curve,&event);};
        QTest::keyClick(curve,Qt::Key_S);QTest::keyClick(curve,Qt::Key_Y);move(start+QPoint(0,-40));QTRY_VERIFY(model.active()->track("position.x")->keys[0].value<1);QTest::keyClick(curve,Qt::Key_Enter);
        auto *track=model.active()->track("position.x");QCOMPARE(track->keys[0].ms,500);QCOMPARE(track->keys[1].ms,1500);QVERIFY(track->keys[0].value<1);QVERIFY(track->keys[1].value>3);QVERIFY(track->keys[0].customHandles);QVERIFY(QWidget::mouseGrabber()!=curve);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
        QTest::keyClick(curve,Qt::Key_A,Qt::ControlModifier);const auto grabStart=curve->mapFromGlobal(QCursor::pos());QTest::keyClick(curve,Qt::Key_G);QTest::keyClick(curve,Qt::Key_X);move(grabStart+QPoint(50,0));QTRY_VERIFY(model.design.toJson()!=before);QTest::mouseClick(curve,Qt::RightButton,Qt::NoModifier,grabStart+QPoint(50,0));QCOMPARE(model.design.toJson(),before);QVERIFY(QWidget::mouseGrabber()!=curve);
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
