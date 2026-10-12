#include <QDialogButtonBox>
#include <QDialog>
#include <QtTest>
#include <QToolButton>
#include <QMenu>
#include <QSettings>
#include "ElementLibrary.h"
#include "SceneEffects.h"
#include "CueSequence.h"
#include "AssetImport.h"
#include "DesignAudio.h"
#include "MediaFrames.h"
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPushButton>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QUuid>
#include <QTreeWidget>
#include "BuildModel.h"
#include "BuildCanvas.h"
#include "BuildWorkspace.h"
#include "AnimationEditor.h"
#include "Data.h"
#include "UiControls.h"
#include "ColorPicker.h"
#include "BuildContextAction.h"
#include "SurfaceEffects.h"
#include "FrameExport.h"
#include <QDir>
#include "AnimationCuePanel.h"
#include "PropertyInspector.h"
#include <QCheckBox>

class BuildTests:public QObject {
    Q_OBJECT
private slots:
    void inspectorExpansionPerObject(){BuildModel m;Library lib;const auto first=m.addText();PropertyInspector inspector(&m,&lib);auto button=[&](QString title)->QToolButton*{QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);for(auto *b:inspector.findChildren<QToolButton*>())if(b->text()==title)return b;return nullptr;};QVERIFY(button("CORNERS"));QVERIFY(!button("CORNERS")->isChecked());button("CORNERS")->setChecked(true);const auto second=m.addText();QVERIFY(button("CORNERS"));QVERIFY(!button("CORNERS")->isChecked());m.select(first);QVERIFY(button("CORNERS")->isChecked());m.select(second);QVERIFY(!button("CORNERS")->isChecked());m.addText();QVERIFY(button("TYPOGRAPHY"));QVERIFY(button("TYPOGRAPHY")->isChecked());}

    void revealAndFrameExport(){
        ElementDesign scene;scene.durationMs=120;scene.ensureCamera();scene.camera()->cam.ortho=true;
        DObject shape;shape.id="reveal-shape";shape.type="Shape";shape.w=4;shape.h=2;shape.base.rot={90,0,0};shape.mat.shadeless=true;
        const auto original=scene.textureFor(shape);shape.style["effects.reveal"]=1.;QCOMPARE(scene.textureFor(shape),original);
        shape.style["effects.reveal"]=0.;QCOMPARE(qAlpha(scene.textureFor(shape).pixel(100,100)),0);
        shape.style["effects.reveal"]=.5;const auto middle=scene.textureFor(shape);QVERIFY(qAlpha(middle.pixel(10,100))>240);QCOMPARE(qAlpha(middle.pixel(middle.width()-10,100)),0);
          shape.style["effects.revealMode"]="Radial";shape.style["effects.revealRoughness"]=0.;shape.style["effects.reveal"]=.15;
          const auto radial=scene.textureFor(shape);QVERIFY(qAlpha(radial.pixel(radial.width()/2,radial.height()/2))>240);QCOMPARE(qAlpha(radial.pixel(10,10)),0);
          shape.style["effects.revealX"]=.2;const auto shifted=scene.textureFor(shape);QVERIFY(qAlpha(shifted.pixel(shifted.width()/5,shifted.height()/2))>240);QCOMPARE(qAlpha(shifted.pixel(shifted.width()/2,shifted.height()/2)),0);
          shape.style["effects.revealMode"]="Linear";
        shape.keyChannel("effects.reveal",0,0);shape.keyChannel("effects.reveal",80,1);scene.objects.append(shape);
        const auto restored=ElementDesign::fromJson(scene.toJson());QCOMPARE(restored.find(shape.id)->propertyValue("effects.reveal",80),1.);
        QTemporaryDir directory;QVERIFY(directory.isValid());FrameExportOptions options;options.size={320,180};options.endFrame=3;
        const auto result=exportSceneFrames(scene,directory.path(),options);QVERIFY2(result.success,qPrintable(result.error));QCOMPARE(result.written,3);
        QImage first(directory.filePath("frame_000000.png")),last(directory.filePath("frame_000002.png"));QVERIFY(!first.isNull());QCOMPARE(qAlpha(first.pixel(160,90)),0);QVERIFY(qAlpha(last.pixel(160,90))>240);
        QFile manifest(directory.filePath("sequence.json"));QVERIFY(manifest.open(QIODevice::ReadOnly));const auto metadata=QJsonDocument::fromJson(manifest.readAll()).object();QCOMPARE(metadata["endFrameExclusive"].toInt(),3);QCOMPARE(metadata["fps"].toInt(),25);
        QVERIFY(!exportSceneFrames(scene,directory.path(),options).success);QCOMPARE(QImage(directory.filePath("frame_000002.png")),last);
        QTemporaryDir cancelDirectory;const auto cancelled=exportSceneFrames(scene,cancelDirectory.path(),options,[](int done,int){return done<1;});QVERIFY(cancelled.cancelled);QCOMPARE(cancelled.written,1);QVERIFY(!QFileInfo::exists(cancelDirectory.filePath("sequence.json")));
        shape.tracks.clear();shape.style["effects.reveal"]=1.;shape.style["shape"]="Triangle";const auto triangle=scene.textureFor(shape);QCOMPARE(qAlpha(triangle.pixel(5,5)),0);QVERIFY(qAlpha(triangle.pixel(triangle.width()/2,triangle.height()/2))>240);
          QTemporaryDir portable;QVERIFY(portable.isValid());QVERIFY(last.save(portable.filePath("texture.png")));scene.objects.last().image="texture.png";
          QFile project(portable.filePath("scene.cuebuild"));QVERIFY(project.open(QIODevice::WriteOnly));project.write(scene.toJson().toUtf8());project.close();
          Library lib;BuildWorkspace workspace(&lib);QVERIFY(workspace.importFile(project.fileName()));auto *model=workspace.findChild<BuildModel*>();QVERIFY(model);
          QCOMPARE(model->design.find(shape.id)->image,portable.filePath("texture.png"));
    }

    void markdownTextRendering(){
        ElementDesign scene;DObject text;text.id="markdown-text";text.type="Text";text.w=5;text.h=3;text.fontPx=32;text.text="# Headline\n**Bold** and *italic*\n- First\n- Second";text.style["text.align"]="Left";text.style["text.vertical"]="Top";
        const auto plain=scene.textureFor(text);text.style["text.format"]="Markdown";const auto formatted=scene.textureFor(text);QVERIFY(!formatted.isNull());QVERIFY(formatted!=plain);scene.objects.append(text);const auto reopened=ElementDesign::fromJson(scene.toJson());const auto *saved=reopened.find(text.id);QVERIFY(saved);QCOMPARE(saved->style["text.format"].toString(),QString("Markdown"));QCOMPARE(reopened.textureFor(*saved),formatted);
    }

    void initTestCase(){
        installUiBehavior(qApp);
        QStandardPaths::setTestModeEnabled(true);
        qApp->setOrganizationName("CuebaseBuildTests");
        qApp->setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));
        qApp->setStyleSheet("QWidget{background:#0b1220;color:#e2e8f0;font-size:12px;} QLineEdit,QDoubleSpinBox,QSpinBox,QComboBox,QPlainTextEdit{background:#182539;border:1px solid #334155;padding:3px;} QPushButton{background:#1e293b;border:1px solid #334155;padding:4px;} QTreeWidget::item:selected{background:#1d4ed8;} QHeaderView::section{background:#172438;}");
    }
    void baseAndAutoKey(){
        BuildModel m;m.reset(ElementDesign::defaultScene());QString id=m.addMesh("Cube");
        QVERIFY(!m.autoKey);QVERIFY(m.active()->tracks.isEmpty());
        m.editProps(id,[](DProps &p){p.loc.setX(2);});QVERIFY(m.active()->tracks.isEmpty());QCOMPARE(m.active()->at(0).loc.x(),2.f);
        m.keyProperty(id,"position.x");QCOMPARE(m.active()->tracks.size(),1);QCOMPARE(m.active()->track("position.x")->keys.size(),1);
        m.setTime(1000);m.autoKey=true;m.editProps(id,[](DProps &p){p.loc.setX(6);});
        QCOMPARE(m.active()->tracks.size(),1);QCOMPARE(m.active()->track("position.x")->keys.size(),2);
        m.setTime(500);QCOMPARE(m.active()->at(500).loc.x(),4.f);
        m.autoKey=false;m.editProps(id,[](DProps &p){p.loc.setX(8);});QCOMPARE(m.active()->at(500).loc.x(),8.f);
        QCOMPARE(m.active()->track("position.x")->keys.size(),2);
        m.keyProperty(id,"position.x");QCOMPARE(m.active()->track("position.x")->keys.size(),3);
        m.keyProperty(id,"position.x");QCOMPARE(m.active()->track("position.x")->keys.size(),3);
        m.setTime(0);m.selChannel="position.x";m.deleteKey();QCOMPARE(m.active()->track("position.x")->keys.size(),2);
        QVERIFY(m.undo());QCOMPARE(m.active()->track("position.x")->keys.size(),3);QVERIFY(m.redo());QCOMPARE(m.active()->track("position.x")->keys.size(),2);
    }
    void screenEffects(){
        auto design=sceneEffectPreset("Fade");QVERIFY(!design.isEmpty());QVERIFY(sceneEffect(design));
        for(const auto &o:design.objects)QVERIFY(!o.isRenderable());
        QImage a(96,54,QImage::Format_ARGB32);a.fill(Qt::red);QImage b(a.size(),a.format());b.fill(Qt::blue);
        QCOMPARE(blendSceneEffect(a,b,design,0),a);QCOMPARE(blendSceneEffect(a,b,design,design.durationMs),b);
        design.loop=true;QCOMPARE(blendSceneEffect(a,b,design,design.durationMs),b);
        auto gray=sceneEffectPreset("Desaturate and slow");auto image=applySceneEffect(a,gray,1200);auto pixel=image.pixel(48,27);QVERIFY(qAbs(qRed(pixel)-qGreen(pixel))<=1);QVERIFY(qAbs(qBlue(pixel)-qGreen(pixel))<=1);
        QCOMPARE(sceneEffectValue(ElementDesign::fromJson(gray.toJson()),"scene.speed",1200,1),.35);
        for(const auto &name:sceneEffectPresets()){auto effect=sceneEffectPreset(name);QVERIFY(!sceneEffectPreview(effect,{160,90},600).isNull());}
        BuildModel model;model.reset(design);model.select(sceneEffect(model.design)->id);
        auto *editor=sceneEffectEditor(&model);editor->resize(420,760);editor->show();QTest::qWait(30);
        QDir().mkpath("artifacts");QVERIFY(editor->grab().save("artifacts/scene-effects.png"));delete editor;
    }
    void independentChannels(){
        BuildModel m;auto id=m.addMesh("Cube");m.keyProperty(id,"position.x");
        m.setTime(1000);m.editProperty(id,"position.x",10);m.keyProperty(id,"position.x");
        m.setTime(500);m.editProperty(id,"opacity",.25);m.keyProperty(id,"opacity");
        QCOMPARE(m.active()->at(500).loc.x(),5.f);QCOMPARE(m.active()->at(500).opacity,.25);
        QCOMPARE(m.active()->track("position.x")->keys.size(),2);
        m.selChannel="opacity";m.deleteKey();QVERIFY(!m.active()->track("opacity"));QCOMPARE(m.active()->tracks.size(),1);
        PropertyTrack track{"x",{{0,0,"Constant"},{1000,10,"Linear"}}};
        QCOMPARE(track.evaluate(999,0),0.0);QCOMPARE(track.evaluate(1000,0),10.0);
        for(const auto &kind:interpolationKinds()){track.keys[0].interpolation=kind;QVERIFY(qIsFinite(track.evaluate(400,0)));}
    }
    void cueBoardTimingAndIsolation(){
        auto source=elementPreset("News lower third");QString textId;for(const auto &o:source.objects)if(o.type=="Text"&&o.style["studio.editable"].toBool()){textId=o.id;break;}QVERIFY(!textId.isEmpty());
        QJsonObject cue{{"id","bounce"},{"name","Bounce"},{"start",0},{"end",1000},{"repeat",4},{"hold",true},{"holdManual",false},{"holdMs",3000}};source.animationCues={cue};
        auto entry=newCueBoardEntry(cue);entry["sections"]=QJsonArray{QJsonObject{{"fields",QJsonObject{{textId,"Hello"}}}},QJsonObject{{"fields",QJsonObject{{textId,"Welcome"}}}}};
        auto plan=compileCueBoard(source,{entry});QVERIFY2(plan.error.isEmpty(),qPrintable(plan.error));QCOMPARE(plan.steps.size(),4);QCOMPARE(plan.duration(),qint64(16000));QCOMPARE(plan.sample(999).frame,qint64(999));QVERIFY(plan.sample(999).advancing);QCOMPARE(plan.sample(1000).frame,qint64(1000));QVERIFY(!plan.sample(1000).advancing);QCOMPARE(plan.sample(3999).step,0);QCOMPARE(plan.sample(4000).step,1);QVERIFY(!plan.sample(16000).visible);
        QCOMPARE(ElementDesign::fromJson(plan.steps[0].design).find(textId)->text,QString("Hello"));QCOMPARE(ElementDesign::fromJson(plan.steps[1].design).find(textId)->text,QString("Welcome"));QVERIFY(source.find(textId)->text!="Hello");
        auto variant=source;variant.find(textId)->base.loc.setX(25);variant.animationCues={QJsonObject{{"id","rotate"},{"name","Rotate"},{"start",0},{"end",400},{"hold",false}},QJsonObject{{"id","reverse"},{"name","Reverse"},{"start",400},{"end",1000},{"hold",false}}};auto sections=entry["sections"].toArray();auto first=sections[0].toObject();first["design"]=variant.toJson();sections[0]=first;entry["sections"]=sections;plan=compileCueBoard(source,{entry});QCOMPARE(plan.steps.size(),5);QCOMPARE(plan.duration(),qint64(16000));QCOMPARE(plan.sample(400).step,1);QCOMPARE(plan.steps[1].cueId,QString("reverse"));QCOMPARE(ElementDesign::fromJson(plan.steps[0].design).find(textId)->base.loc.x(),25.f);QVERIFY(ElementDesign::fromJson(plan.steps[2].design).find(textId)->base.loc.x()!=25.f);
        entry["loop"]=true;plan=compileCueBoard(source,{entry});QCOMPARE(plan.duration(),qint64(-1));QCOMPARE(plan.sample(16000).step,0);QCOMPARE(plan.sample(160000000).step,0);
        entry["holdManual"]=true;plan=compileCueBoard(source,{entry});QCOMPARE(plan.sample(500000).step,1);QVERIFY(!plan.sample(500000).advancing);QCOMPARE(plan.next(1),2);
        auto child=variant.animationCues[0].toObject();child["disabled"]=true;variant.animationCues[0]=child;first["design"]=variant.toJson();sections[0]=first;entry["sections"]=sections;plan=compileCueBoard(source,{entry});QCOMPARE(plan.steps.first().cueId,QString("reverse"));
    }
    void bakedBoardsAndLiveProtection(){
        auto design=ElementDesign::defaultScene();QJsonObject cue{{"id","intro"},{"name","Intro"},{"start",0},{"end",3000},{"hold",true},{"holdMs",2000},{"holdManual",false},{"repeat",2}};design.animationCues={cue};
        Element source;source.id="original";source.type="Custom";source.design=design.toJson();source.cueBoard={newCueBoardEntry(cue)};source.cueBoardSettings={{"repeat",2},{"hold",true},{"holdMs",1000},{"holdManual",false}};
        auto plan=compileCueBoard(design,source.cueBoard,source.cueBoardSettings);QCOMPARE(plan.duration(),qint64(22000));
        Library lib;lib.elements={source};Placement first;first.id="first";first.refId=source.id;first.boardSnapshot=bakeCueBoard(source);first.boardCycleMs=plan.duration();first.durationMs=plan.duration();auto second=first;second.id="second";lib.placements={first,second};
        auto instance=lib.placementElement(first);auto other=lib.placementElement(second);QVERIFY(instance.id!=other.id);QCOMPARE(instance.design,first.boardSnapshot["design"].toString());lib.elements[0].design=ElementDesign::defaultScene().toJson();QCOMPARE(lib.placementElement(first).design,first.boardSnapshot["design"].toString());
        auto changed=instance;auto entry=changed.cueBoard[0].toObject();entry["repeat"]=3;changed.cueBoard[0]=entry;QString error;QVERIFY(canEditCueBoard(instance,changed,400000,0,true,&error));QVERIFY(!canEditCueBoard(instance,changed,299999,0,true,&error));QVERIFY(!error.isEmpty());QVERIFY(canEditCueBoard(instance,changed,0,10000,false,&error));
        auto snapshot=second.boardSnapshot;snapshot["name"]="Independent";second.boardSnapshot=snapshot;QCOMPARE(first.boardSnapshot["name"].toString(),source.name);
    }
    void cueVariantBuildSave(){
        Library lib;Element e;e.id="cue-source";e.name="Cue source";e.type="Custom";auto design=elementPreset("News lower third");QJsonObject cue{{"id","bounce"},{"name","Bounce"},{"start",0},{"end",1000},{"hold",false},{"repeat",2}};design.animationCues={cue};e.design=design.toJson();auto entry=newCueBoardEntry(cue);e.cueBoard={entry};lib.elements={e};BuildWorkspace workspace(&lib);workspace.editCueSection(e.id,entry["id"].toString(),1);auto *model=workspace.findChild<BuildModel*>();QVERIFY(model);QVERIFY(model->design.animationCues.isEmpty());const auto text=model->addText();model->active()->text="Only this repetition";model->notify();emit model->cueFramesRequested(0,400);auto *range=workspace.findChild<QDialog*>("cueRangeDialog");QVERIFY(range);range->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QCOMPARE(model->design.animationCues.size(),1);QVERIFY(workspace.findChild<QPushButton*>("buildTab_Animation Cues")->isChecked());workspace.findChild<QPushButton*>("buildSave")->click();
        QCOMPARE(lib.element(e.id)->design,e.design);const auto sections=lib.element(e.id)->cueBoard[0].toObject()["sections"].toArray();QCOMPARE(sections.size(),2);QVERIFY(sections[0].toObject()["design"].toString().isEmpty());const auto variant=ElementDesign::fromJson(sections[1].toObject()["design"].toString());QVERIFY(variant.find(text));QCOMPARE(variant.animationCues.size(),1);QVERIFY(!workspace.hasUnsavedChanges());Library reopened;reopened.load();QCOMPARE(reopened.element(e.id)->cueBoard,lib.element(e.id)->cueBoard);
    }
    void defaultsToggleAndDuplicates(){
        BuildModel m;m.reset(ElementDesign::defaultScene());auto id=m.addText();const auto original=m.active()->fontPx;
        m.editProperty(id,"text.size",250);m.resetProperty(id,"text.size");QCOMPARE(m.active()->fontPx,original);
        m.togglePropertyKey(id,"text.size");QVERIFY(m.active()->track("text.size"));m.togglePropertyKey(id,"text.size");QVERIFY(!m.active()->track("text.size"));
        m.keyProperty(id,"position.x");m.setTime(1000);m.editProperty(id,"position.x",5);m.keyProperty(id,"position.x");
        auto copies=m.duplicateSelected();QCOMPARE(copies.size(),1);m.editProperty(copies.first(),"position.x",15);m.keyProperty(copies.first(),"position.x");
        QCOMPARE(m.byId(id)->track("position.x")->keys.last().value,5.0);QCOMPARE(m.byId(copies.first())->track("position.x")->keys.last().value,15.0);
        auto last=m.activeId;m.deselectAll();QCOMPARE(m.activeId,last);QVERIFY(m.active());QVERIFY(m.sel.isEmpty());QCOMPARE(m.inspected().size(),1);
    }
    void bezierPersistence(){
        PropertyTrack track{"position.x",{{0,0,"Bezier"},{1000,10,"Linear"}}};
        track.keys[0].customHandles=true;track.keys[0].rightHandle={200,12};track.keys[1].customHandles=true;track.keys[1].leftHandle={-200,0};
        QVERIFY(track.evaluate(500,0)>7);QCOMPARE(track.evaluate(0,0),0.0);QCOMPARE(track.evaluate(1000,0),10.0);
        auto d=ElementDesign::defaultScene();DObject o;o.id="curve";o.tracks={track};d.objects.append(o);auto read=ElementDesign::fromJson(d.toJson());
        auto *restored=read.find("curve")->track("position.x");QVERIFY(restored);QCOMPARE(restored->keys[0].rightHandle,QPointF(200,12));QCOMPARE(restored->evaluate(500,0),track.evaluate(500,0));
    }
    void parentingAndOverflow(){
        BuildModel m;m.reset(ElementDesign::defaultScene());DObject parent;parent.id="parent";parent.type="Shape";parent.w=2;parent.h=2;parent.base.rot={90,0,0};parent.mat.color=Qt::red;parent.mat.shadeless=true;parent.style["overflow.x"]="Hidden";parent.style["overflow.y"]="Hidden";
        DObject child=parent;child.id="child";child.mat.color=Qt::green;child.style=QJsonObject{};child.base.loc={1.5f,0,0};m.design.objects<<parent<<child;m.setSelection({"child","parent"},"parent");
        const auto before=m.design.worldMatrix(*m.byId("child"),0);QVERIFY(m.parentSelected());QCOMPARE(m.byId("child")->parentId,QString("parent"));QCOMPARE(m.design.worldMatrix(*m.byId("child"),0),before);
        QImage image(640,360,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter p(&image);m.design.render(p,image.rect(),0);p.end();
        QVERIFY(qGreen(image.pixel(350,180))>200);QCOMPARE(qAlpha(image.pixel(390,180)),0);
        m.editProperty("parent","position.x",2);QCOMPARE(m.design.worldMatrix(*m.byId("child"),0).map(QVector3D()).x(),3.5f);
        m.isolateParents=true;const auto childBefore=m.design.worldMatrix(*m.byId("child"),0);m.editProperty("parent","position.x",4);QCOMPARE(m.design.worldMatrix(*m.byId("child"),0),childBefore);m.isolateParents=false;
        m.setSelection({"parent","child"},"child");QVERIFY(!m.parentSelected());
        m.select("child");const auto world=m.design.worldMatrix(*m.active(),0);QVERIFY(m.parentSelected(true));QVERIFY(m.active()->parentId.isEmpty());QCOMPARE(m.design.worldMatrix(*m.active(),0),world);
        auto roundtrip=ElementDesign::fromJson(m.design.toJson());QCOMPARE(roundtrip.worldMatrix(*roundtrip.find("child"),0),world);
    }
    void horizontalInputs(){
        ValueInput input;input.setRange(0,100);input.setValue(25);input.show();
        QWheelEvent vertical(QPointF(10,10),QPointF(10,10),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&input,&vertical);QCOMPARE(input.value(),25.0);
        QWheelEvent horizontal(QPointF(10,10),QPointF(10,10),QPoint(),QPoint(120,0),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&input,&horizontal);QCOMPARE(input.value(),26.0);
        auto *line=input.findChild<QLineEdit*>();QVERIFY(line);const QPoint center=line->rect().center();QTest::mousePress(line,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(line,center+QPoint(20,0));QVERIFY(input.value()>26);
        QTest::mousePress(line,Qt::RightButton,Qt::NoModifier,center+QPoint(20,0));QTest::mouseRelease(line,Qt::RightButton,Qt::NoModifier,center+QPoint(20,0));QTest::mouseRelease(line,Qt::LeftButton,Qt::NoModifier,center+QPoint(20,0));QCOMPARE(input.value(),26.);
        QTest::mouseClick(line,Qt::LeftButton,Qt::NoModifier,center);QVERIFY(line->hasSelectedText());QTest::mouseClick(line,Qt::LeftButton,Qt::NoModifier,QPoint(5,center.y()));QVERIFY(!line->hasSelectedText());line->selectAll();QTest::keyClicks(line,"42");QTest::keyClick(line,Qt::Key_Return);QCOMPARE(input.value(),42.);QVERIFY(!line->hasSelectedText());QVERIFY(!line->hasFocus());
    }
    void colorPickerAndContext(){
        bool inspected=false;QTimer::singleShot(50,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;auto *hex=dialog->findChild<QLineEdit*>();if(!hex)return;hex->setText("#804488cc");QMetaObject::invokeMethod(hex,"editingFinished");QDir().mkpath("artifacts");inspected=dialog->grab().save("artifacts/color-picker.png");dialog->accept();});
        QCOMPARE(ColorPicker::getColor(Qt::white),QColor("#804488cc"));QVERIFY(inspected);
        BuildContextButton button;button.show();const auto initial=button.grab().toImage();QTest::qWait(370);QVERIFY(initial!=button.grab().toImage());QVERIFY(button.grab().save("artifacts/build-context.png"));
    }
    void migrationAndRoundTrip(){
        QJsonObject key0{{"ms",0},{"loc",QJsonArray{1,2,3}},{"rot",QJsonArray{0,0,0}},{"scl",QJsonArray{1,1,1}},{"opacity",1},{"interp","Linear"}};
        auto key1=key0;key1["ms"]=1000;key1["loc"]=QJsonArray{5,2,3};
        QJsonObject object{{"id","old"},{"type","Text"},{"text","Broadcast"},{"keys",QJsonArray{key0,key1}}};
        QJsonObject root{{"version",2},{"objects",QJsonArray{object}}};
        auto d=ElementDesign::fromJson(QString::fromUtf8(QJsonDocument(root).toJson()));
        auto *o=d.find("old");QVERIFY(o);QCOMPARE(o->tracks.size(),10);QCOMPARE(o->at(500).loc.x(),3.f);
        auto loaded=ElementDesign::fromJson(d.toJson());QCOMPARE(loaded.find("old")->at(500).loc.x(),3.f);QCOMPARE(loaded.toJson(),d.toJson());
        root.remove("version");object["type"]="Rect";object["keys"]=QJsonArray{QJsonObject{{"ms",0},{"x",100},{"y",200},{"w",300},{"h",100}}};root["objects"]=QJsonArray{object};
        auto flat=ElementDesign::fromJson(QString::fromUtf8(QJsonDocument(root).toJson()));QVERIFY(flat.find("old"));QCOMPARE(flat.find("old")->tracks.size(),10);
    }
    void cameraAndCanvas(){
        BuildModel m;Library lib;m.reset(ElementDesign::defaultScene());QString id=m.addMesh("Cube");
        BuildCanvas canvas(&m,&lib);canvas.resize(900,600);canvas.show();QTest::qWait(30);
        const auto before=m.design.toJson();
        QTest::mousePress(&canvas,Qt::MiddleButton,Qt::NoModifier,QPoint(450,300));
        QTest::mouseMove(&canvas,QPoint(520,330));QTest::mouseRelease(&canvas,Qt::MiddleButton,Qt::NoModifier,QPoint(520,330));
        canvas.frameAll();canvas.actualSize();canvas.frameSelected();QCOMPARE(m.design.toJson(),before);
        canvas.actualSize();const auto fullSize=canvas.grab().toImage();canvas.actualSize();QCOMPARE(canvas.grab().toImage(),fullSize);
        auto camera=m.design.camera()->base;
        m.editProps(id,[](DProps &p){p.rot={25,45,35};p.loc.setY(3);});
        QCOMPARE(m.design.camera()->base.loc,camera.loc);QCOMPARE(m.design.camera()->base.rot,camera.rot);
        m.select(m.design.camera()->id);m.editProps(m.activeId,[](DProps&p){p.loc.setX(500);});QCOMPARE(m.design.camera()->base.loc,camera.loc);
        canvas.placeCursor=true;QTest::mouseMove(&canvas,QPoint(600,350));QCOMPARE(canvas.cursor().shape(),Qt::CrossCursor);
        const auto oldCursor=m.design.cursor;QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(600,350));QVERIFY(m.design.cursor!=oldCursor);
        const auto placed=m.design.cursor;QVERIFY(m.undo());QCOMPARE(m.design.cursor,oldCursor);QVERIFY(m.redo());QCOMPARE(m.design.cursor,placed);
        auto text=m.addText();QCOMPARE(m.byId(text)->base.loc,placed);
        canvas.placeCursor=false;QTest::mouseClick(&canvas,Qt::RightButton,Qt::ShiftModifier,QPoint(400,250));QVERIFY(m.design.cursor!=placed);
    }
    void meshOperations(){
        BuildModel m;m.addMesh("Cube");QVERIFY(m.enterEdit());m.editSelectAll(true);
        QVERIFY(m.opSubdivide());QVERIFY(m.undo());QVERIFY(m.redo());
        Mesh mesh=Mesh::primitive("Plane");EditSel sel;sel.f={0};QVector3D normal;
        QVERIFY(mesh.extrude(sel,SelMode::Face,&normal));QVERIFY(mesh.v.size()>4);
        mesh=Mesh::primitive("Plane");sel.clear();sel.f={0};QVector<std::pair<int,QVector3D>> inset;
        QVERIFY(mesh.inset(sel,SelMode::Face,&inset));QVERIFY(!inset.isEmpty());
        mesh=Mesh::primitive("Cube");sel.clear();QVERIFY(mesh.loopCut(mesh.edges().first(),1,sel));
        for(const auto &kind:Mesh::primitiveKinds()){auto primitive=Mesh::primitive(kind);QVERIFY(!primitive.v.isEmpty());QVERIFY(!primitive.f.isEmpty());}
    }
    void renderedStyles(){
        auto d=ElementDesign::defaultScene();DObject o;o.id="shape";o.type="Shape";o.w=3;o.h=2;o.base.rot={90,0,0};o.style["fill.kind"]="Linear";o.style["corner.radius"]=20;
        auto texture=d.textureFor(o);QVERIFY(!texture.isNull());QVERIFY(qAlpha(texture.pixel(0,0))<255);
        QVERIFY(texture.pixel(texture.width()/4,texture.height()/2)!=texture.pixel(texture.width()*3/4,texture.height()/2));
        o.keyChannel("layout.width",0,360);o.keyChannel("layout.width",1000,720);QCOMPARE(o.evaluated(500).w,4.5);
        d.objects.append(o);QImage frame(640,360,QImage::Format_ARGB32_Premultiplied);frame.fill(Qt::transparent);QPainter painter(&frame);d.render(painter,frame.rect(),500);painter.end();QVERIFY(qAlpha(frame.pixel(320,180))>0);
        QTemporaryDir temp;QVERIFY(frame.save(temp.filePath("image.png")));BuildModel m;QVERIFY(!m.addImage(temp.filePath("image.png")).isEmpty());QVERIFY(m.active()->tracks.isEmpty());
        QVERIFY(!m.addText().isEmpty());QVERIFY(!d.textureFor(*m.active()).isNull());
    }
    void modifierControls(){
        BuildModel model;const auto id=model.addMesh("Cube");auto *object=model.byId(id);
        Modifier mirror;mirror.type="Mirror";mirror.options["offset.x"]=2.;object->mods={mirror};
        QVector3D low,high;QVERIFY(model.design.evalMesh(*object,0).bounds(low,high));QVERIFY(high.x()>4);
        mirror.options["offset.x"]=0.;mirror.options["bisect.x"]=true;object->mods={mirror};QVERIFY(!model.design.evalMesh(*object,0).isEmpty());
        Modifier bevel;bevel.type="Bevel";bevel.width=.15;bevel.options["segments"]=3;object->mods={bevel};
        const Mesh rounded=model.design.evalMesh(*object,0);QVERIFY(rounded.v.size()>object->mesh.v.size());QVERIFY(rounded.isClosed());
        bevel.options["affect"]="Vertices";object->mods={bevel};QVERIFY(model.design.evalMesh(*object,0).isClosed());
        auto restored=ElementDesign::fromJson(model.design.toJson());QCOMPARE(restored.find(id)->mods.first().options,bevel.options);
    }
    void externalAssets(){
        for(const auto &format:QStringList{"fbx","glb"}){QString error;auto objects=AssetImport::meshes(QCoreApplication::applicationDirPath()+"/../tests/fixtures/cube."+format,&error);QVERIFY2(!objects.isEmpty(),qPrintable(error));QVERIFY(!objects.first().mesh.isEmpty());}
        ElementDesign scene=ElementDesign::defaultScene();DObject audio;audio.id="audio";audio.type="Audio";audio.style["audio.file"]="tone.wav";audio.style["audio.duration"]=2000.;audio.style["audio.start"]=1000.;audio.style["audio.volume"]=.5;scene.objects.append(audio);
        QVERIFY(designAudioCues(scene,500,true).isEmpty());const auto cues=designAudioCues(scene,1500,true);QCOMPARE(cues.size(),1);QCOMPARE(cues.first().position,500);QCOMPARE(cues.first().volume,.5);
        const QString video=QCoreApplication::applicationDirPath()+"/../tests/fixtures/material.mp4";QString error;QVERIFY2(MediaFrames::duration(video,&error)>1500,qPrintable(error));
        const auto first=MediaFrames::frame(video,0,"test"),later=MediaFrames::frame(video,1000,"test");QVERIFY(!first.isNull());QVERIFY(first!=later);QCOMPARE(MediaFrames::frame(video,0,"test"),first);
        BuildModel model;model.design.cursor={2,0,1};const auto id=model.addMesh("Cube");QCOMPARE(model.byId(id)->base.loc,model.design.cursor);QCOMPARE(model.byId(id)->mat.color,QColor(Qt::white));auto restored=ElementDesign::fromJson(model.design.toJson());QCOMPARE(restored.cursor,model.design.cursor);
    }
    void workspaceAndGraph(){
        Library lib;BuildWorkspace workspace(&lib);workspace.resize(1600,1000);workspace.show();QTest::qWait(100);
        auto *m=workspace.findChild<BuildModel*>();QVERIFY(m);auto id=m->addText();m->active()->text="LIVE";m->notify();QTest::qWait(50);
        QVERIFY(m->design.camera()->cam.ortho);
        for(auto *button:workspace.findChildren<QToolButton*>())if(button->text()=="TYPOGRAPHY")button->setChecked(true);
        auto *size=workspace.findChild<QDoubleSpinBox*>("property_text.size");QVERIFY(size);size->setValue(110);QCOMPARE(m->active()->fontPx,110);
        auto *key=workspace.findChild<QPushButton*>("key_text.size");QVERIFY(key);QTest::mouseClick(key,Qt::LeftButton);QVERIFY(m->active()->track("text.size"));
        m->setTime(1000);m->editProperty(id,"text.size",180);m->keyProperty(id,"text.size");
        QDir().mkpath("artifacts");QVERIFY(workspace.grab().save("artifacts/build-workspace.png"));
        AnimationEditor graph(m,true);graph.resize(1000,350);graph.show();QTest::qWait(50);
        auto *tree=graph.findChild<QTreeWidget*>();QVERIFY(tree);QVERIFY(tree->topLevelItemCount()>0);
        auto *object=tree->topLevelItem(0);QVERIFY(object);auto *group=object->child(0);QVERIFY(group);QVERIFY(!group->isExpanded());group->setExpanded(true);
        auto *channel=group->child(0);QVERIFY(channel);
        QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(channel).center());
        auto *curve=graph.findChild<QWidget*>("animationCurve");QVERIFY(curve);
        for(auto *button:graph.findChildren<QToolButton*>())if(button->toolTip()=="View All")button->click();
        const QRectF area=QRectF(curve->rect()).adjusted(62,28,-14,-26);
        const auto range=curve->property("viewRange").toList();
        auto graphPoint=[&](double time,double value){return QPoint(qRound(area.left()+(time-range[0].toDouble())/(range[1].toDouble()-range[0].toDouble())*area.width()),qRound(area.bottom()-(value-range[2].toDouble())/(range[3].toDouble()-range[2].toDouble())*area.height()));};
        QPoint first=graphPoint(0,110);
        QTest::mousePress(curve,Qt::LeftButton,Qt::NoModifier,first);
        QCOMPARE(m->t,qint64(1000));
        QTest::mouseMove(curve,first+QPoint(35,-25));QTest::mouseRelease(curve,Qt::LeftButton,Qt::NoModifier,first+QPoint(35,-25));
        auto *track=m->byId(id)->track("text.size");QVERIFY(track);QVERIFY(track->keys.first().ms>0);QVERIFY(track->keys.first().value>110);
        QCOMPARE(m->t,qint64(1000));
        QVERIFY(m->undo());QCOMPARE(m->byId(id)->track("text.size")->keys.first().ms,0);
        track=m->byId(id)->track("text.size");track->keys.first().interpolation="Bezier";m->notify();
        QTest::mouseClick(curve,Qt::LeftButton,Qt::NoModifier,first);
        const QPointF handle=track->handle(0,true);
        const QPoint handlePoint=graphPoint(handle.x(),handle.y());
        const double beforeCurve=track->evaluate(500,0);
        QTest::mousePress(curve,Qt::LeftButton,Qt::NoModifier,handlePoint);QTest::mouseMove(curve,handlePoint+QPoint(5,-25));QTest::mouseRelease(curve,Qt::LeftButton,Qt::NoModifier,handlePoint+QPoint(5,-25));
        track=m->byId(id)->track("text.size");QVERIFY(track->keys.first().customHandles);QVERIFY(track->evaluate(500,0)!=beforeCurve);QCOMPARE(m->t,qint64(1000));
        QVERIFY(graph.grab().save("artifacts/build-graph.png"));
        Element element;element.id="saved";element.type="Custom";element.name="Round trip";element.design=m->design.toJson();lib.elements.append(element);
        workspace.editElement("saved");QCOMPARE(m->design.toJson(),element.design);
        QPushButton *save=nullptr;for(auto *button:workspace.findChildren<QPushButton*>())if(button->toolTip()=="Save to Screen Elements")save=button;
        QVERIFY(save);QTest::mouseClick(save,Qt::LeftButton);
        Library reloaded;reloaded.load();QVERIFY(reloaded.element("saved"));QCOMPARE(reloaded.element("saved")->design,m->design.toJson());
    }
    void editingTransactionsAndSurfaces(){
        BuildModel model;auto id=model.addMesh("Cube");model.editProperty(id,"corner.radius",20);
        uiGesture()->begin();model.pushUndo();model.editProperty(id,"corner.radius",35);model.pushUndo();model.editProperty(id,"corner.radius",50);uiGesture()->finish(false);
        QVERIFY(model.undo());QCOMPARE(model.byId(id)->propertyBase("corner.radius"),20.);
        uiGesture()->begin();model.pushUndo();model.editProperty(id,"corner.radius",45);uiGesture()->finish(true);QCOMPARE(model.byId(id)->propertyBase("corner.radius"),20.);
        DObject shape;shape.style["corner.unit"]="%";shape.style["corner.radius"]=50.;auto outline=surfaceOutline(QRectF(0,0,200,100),shape);QVERIFY(!outline.contains(QPointF(1,1)));QVERIFY(outline.contains(QPointF(100,1)));
        QImage image(21,21,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::black);image.setPixelColor(10,10,Qt::white);auto blurred=backdropImage(image,2,1.);QVERIFY(blurred.pixelColor(10,10).red()<255);QVERIFY(blurred.pixelColor(11,10).red()>0);QCOMPARE(blurred.pixelColor(0,0),QColor(Qt::black));
        Element element;element.type="Custom";auto design=ElementDesign::defaultScene();design.durationMs=1540;element.design=design.toJson();QCOMPARE(element.lengthMs(),qint64(1540));
        Placement placement;placement.seconds=2;placement.durationMs=1540;QCOMPARE(placement.lengthMs(),qint64(1540));
        DObject audio;audio.id="track";audio.type="Audio";audio.style["audio.file"]="music.wav";audio.style["audio.duration"]=10000.;audio.style["audio.offset"]=1000.;audio.style["audio.speed"]=2.;audio.style["audio.length"]=6000.;audio.style["audio.fadeIn"]=1000.;audio.style["audio.fadeOut"]=1000.;audio.style["audio.repeat"]="Loop";design.objects.append(audio);
        auto cues=designAudioCues(design,500,true,{},1,false);QCOMPARE(cues.size(),1);QCOMPARE(cues[0].position,qint64(2000));QCOMPARE(cues[0].volume,.5);QCOMPARE(cues[0].rate,2.);
        cues=designAudioCues(design,5500,true,{},1,false);QCOMPARE(cues[0].position,qint64(3000));QCOMPARE(cues[0].volume,.5);
    }
    void cuePlaybackIgnoresSceneRange(){
        Library lib;BuildWorkspace workspace(&lib);auto *model=workspace.findChild<BuildModel*>();QVERIFY(model);auto design=ElementDesign::defaultScene();design.startMs=1000;design.durationMs=2000;design.animationCues={QJsonObject{{"id","independent"},{"start",0},{"end",400},{"hold",false}}};model->reset(design);model->previewCueId="independent";model->t=1000;emit model->cuePlaybackRequested();QVERIFY(model->playing);QCOMPARE(model->t,qint64(0));model->playing=false;model->notify();
    }
    void cueEditorPersistence(){
        BuildModel model;auto *editor=animationCueEditor(&model);editor->resize(340,390);editor->show();
        auto *add=editor->findChild<QPushButton*>("createAnimationCue");QVERIFY(add);QTest::mouseClick(add,Qt::LeftButton);QVERIFY(model.design.animationCues.isEmpty());
        auto *dialog=editor->findChild<QDialog*>("cueRangeDialog");QVERIFY(dialog);QVERIFY(model.cueRangeStart>=0);
        dialog->findChild<QLineEdit*>("cueName")->setText("Exit");dialog->findChild<QSpinBox*>("cueEnd")->setValue(50);dialog->findChild<QCheckBox*>("cueHold")->setChecked(false);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QCOMPARE(model.cueRangeStart,qint64(-1));
        auto cue=model.design.animationCues.first().toObject();QCOMPARE(cue["name"].toString(),QString("Exit"));QVERIFY(!cue["hold"].toBool());QCOMPARE(cue["end"].toInt(),50*model.frameMs());QCOMPARE(ElementDesign::fromJson(model.design.toJson()).animationCues,model.design.animationCues);
        QVERIFY(editor->grab().save("artifacts/animation-cues.png"));QVERIFY(model.undo());QVERIFY(model.design.animationCues.isEmpty());delete editor;
    }
    void graphCoordinatesAndInterpolation(){
        QSettings().remove("animation/graphView");
        BuildModel model;model.reset(ElementDesign::defaultScene());auto id=model.addMesh("Cube");auto *object=model.byId(id);
        object->keyChannel("position.x",1000,2);object->keyChannel("position.x",2000,6);
        object->keyChannel("position.y",1000,1);object->keyChannel("position.y",2000,3);
        object->track("position.x")->keys[0].interpolation="Linear";
        object->track("position.y")->keys[0].interpolation="Constant";
        AnimationEditor editor(&model,true);editor.resize(1000,400);editor.show();QTest::qWait(40);
        auto *tree=editor.findChild<QTreeWidget*>();auto *curve=editor.findChild<QWidget*>("animationCurve");QVERIFY(tree&&curve);
        tree->setCurrentItem(tree->topLevelItem(0));
        QToolButton *interpolation=nullptr;for(auto *b:editor.findChildren<QToolButton*>())if(b->toolTip().startsWith("Interpolation:"))interpolation=b;
        QVERIFY(interpolation);QCOMPARE(interpolation->text(),QString("Mixed"));
        for(auto *action:interpolation->menu()->actions())if(action->text()=="Bezier")action->trigger();
        for(auto &track:model.byId(id)->tracks)for(auto &key:track.keys)QCOMPARE(key.interpolation,QString("Bezier"));
        QCOMPARE(interpolation->text(),QString("Bezier"));
        auto initial=curve->property("viewRange").toList();auto saved=model.design.toJson();
        QPoint center=curve->rect().center();QTest::mousePress(curve,Qt::MiddleButton,Qt::NoModifier,center);
        QTest::mouseMove(curve,center+QPoint(50,30));QTest::mouseRelease(curve,Qt::MiddleButton,Qt::NoModifier,center+QPoint(50,30));
        auto panned=curve->property("viewRange").toList();QVERIFY(initial!=panned);QCOMPARE(model.design.toJson(),saved);
        const QRectF area=QRectF(curve->rect()).adjusted(62,28,-14,-26);
        QVERIFY(qAbs(panned[0].toDouble()-(initial[0].toDouble()-50/area.width()*(initial[1].toDouble()-initial[0].toDouble())))<.001);
        model.setTime(500);model.notify();QCOMPARE(curve->property("viewRange").toList(),panned);
        QTest::mousePress(curve,Qt::LeftButton,Qt::ControlModifier,center);QTest::mouseMove(curve,center+QPoint(60,2));QTest::mouseRelease(curve,Qt::LeftButton,Qt::ControlModifier,center+QPoint(60,2));
        auto zoomed=curve->property("viewRange").toList();QVERIFY(zoomed[1].toDouble()-zoomed[0].toDouble()<panned[1].toDouble()-panned[0].toDouble());QCOMPARE(zoomed[2],panned[2]);QCOMPARE(model.design.toJson(),saved);
        auto *track=model.byId(id)->track("position.x");auto key=track->keys[0];
        QPoint point(qRound(area.left()+(key.ms-zoomed[0].toDouble())/(zoomed[1].toDouble()-zoomed[0].toDouble())*area.width()),qRound(area.bottom()-(key.value-zoomed[2].toDouble())/(zoomed[3].toDouble()-zoomed[2].toDouble())*area.height()));
        QTest::mousePress(curve,Qt::LeftButton,Qt::NoModifier,point);QTest::mouseMove(curve,point+QPoint(0,-20));QTest::mouseRelease(curve,Qt::LeftButton,Qt::NoModifier,point+QPoint(0,-20));
        double expected=key.value+20/area.height()*(zoomed[3].toDouble()-zoomed[2].toDouble());
        QVERIFY(qAbs(model.byId(id)->track("position.x")->keys[0].value-expected)<.001);QCOMPARE(model.t,qint64(500));QCOMPARE(curve->property("viewRange").toList(),zoomed);
        auto moved=model.design.toJson();QTest::keyClick(curve,Qt::Key_Escape);QCOMPARE(model.design.toJson(),moved);
        QVERIFY(model.undo());QCOMPARE(model.design.toJson(),saved);
        AnimationEditor reopened(&model,true);auto *restored=reopened.findChild<QWidget*>("animationCurve");QCOMPARE(restored->property("viewRange").toList(),zoomed);
    }
    void buildLibrarySwitchAndNew(){
        Library library;Element a;a.id="scene-a";a.type="Custom";a.name="First";a.design=elementPreset("Sponsor spotlight").toJson();Element b=a;b.id="scene-b";b.name="Second";library.elements={a,b};BuildWorkspace workspace(&library);workspace.editElement(a.id);auto *model=workspace.findChild<BuildModel*>();QVERIFY(model);DObject *text=nullptr;for(auto &o:model->design.objects)if(o.type=="Text")text=&o;QVERIFY(text);const auto textId=text->id;text->text="Changed in Build";model->notify();auto *picker=workspace.findChild<QComboBox*>("buildElementPicker");QVERIFY(picker);int index=picker->findData(b.id);QVERIFY(index>=0);picker->setCurrentIndex(index);QMetaObject::invokeMethod(picker,"activated",Q_ARG(int,index));QCOMPARE(ElementDesign::fromJson(library.element(a.id)->design).find(textId)->text,QString("Changed in Build"));QCOMPARE(picker->currentText(),QString("Second"));
        auto *fresh=workspace.findChild<QPushButton*>("newBuildScene");QVERIFY(fresh);fresh->click();QCOMPARE(picker->currentText(),QString("New element"));QVERIFY(model->design.find(textId)==nullptr);QCOMPARE(library.elements.size(),2);
    }
    void reusableStudioFields(){
        auto design=elementPreset("Sponsor spotlight");DObject *text=nullptr;for(auto &o:design.objects)if(o.type=="Text")text=&o;QVERIFY(text);
        const auto id=text->id;const auto tracks=text->tracks;const auto base=text->base.loc;
        QVERIFY(parseStudioText(*text,"{{studio:Sponsor}}Sponsored by NVIDIA"));QCOMPARE(text->text,QString("Sponsored by NVIDIA"));QCOMPARE(text->style["studio.label"].toString(),QString("Sponsor"));
        QString error;QVERIFY(setStudioField(design,id,"Sponsored by Microsoft and our community partners",&error));QCOMPARE(design.find(id)->base.loc,base);QCOMPARE(design.find(id)->tracks.size(),tracks.size());for(int i=0;i<tracks.size();++i){QCOMPARE(design.find(id)->tracks[i].keys.size(),tracks[i].keys.size());for(int j=0;j<tracks[i].keys.size();++j){QCOMPARE(design.find(id)->tracks[i].keys[j].ms,tracks[i].keys[j].ms);QCOMPARE(design.find(id)->tracks[i].keys[j].value,tracks[i].keys[j].value);}}
        QVERIFY(!setStudioField(design,design.objects.first().id,"Forbidden",&error));QVERIFY(!setStudioField(design,id,QString(10001,'x'),&error));
        Element e;e.id="template";e.name="Sponsor";e.type="Custom";e.design=design.toJson();e.previewMs=1000;e.previewX=.1;e.previewY=.2;e.previewW=.7;e.previewH=.5;e.description="Sponsor template";e.playlist="Sponsors";refreshElementThumbnail(e);QVERIFY(!elementThumbnail(e).isNull());
        Library lib;lib.elements={e};lib.elementPlaylists.append("Sponsors");lib.save();Library restored;restored.load();auto *saved=restored.element(e.id);QVERIFY(saved);QCOMPARE(saved->description,e.description);QCOMPARE(saved->thumbnail,e.thumbnail);QCOMPARE(saved->previewW,e.previewW);QVERIFY(restored.elementPlaylists.contains("Sponsors"));
        QTemporaryDir mediaDirectory;QVERIFY(mediaDirectory.isValid());QImage replacement(16,16,QImage::Format_ARGB32);replacement.fill(Qt::cyan);const auto imagePath=mediaDirectory.filePath("replacement.png");QVERIFY(replacement.save(imagePath));
        DObject media;media.id="exposed-media";media.type="Image";media.style["studio.editable"]=true;media.keyChannel("opacity",0,0);media.keyChannel("opacity",1000,1);design.objects.append(media);QVERIFY(setStudioField(design,media.id,imagePath,&error));QCOMPARE(design.find(media.id)->image,imagePath);QCOMPARE(design.find(media.id)->tracks.first().keys.size(),2);QVERIFY(!setStudioField(design,media.id,mediaDirectory.filePath("missing.png"),&error));QCOMPARE(design.find(media.id)->image,imagePath);
        QTimer::singleShot(80,[]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(dialog){dialog->grab().save("artifacts/element-preview-dialog.png");dialog->accept();}});QVERIFY(editElementPresentation(e));
    }
    void cleanupTestCase(){QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/cuebase.json");}
};
QTEST_MAIN(BuildTests)
#include "BuildTests.moc"
