#include <QtTest>
#include <QToolButton>
#include <QMenu>
#include <QSettings>
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
#include "AnimationCuePanel.h"
#include <QCheckBox>

class BuildTests:public QObject {
    Q_OBJECT
private slots:
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
    void cueEditorPersistence(){
        BuildModel model;auto *editor=animationCueEditor(&model);editor->resize(340,390);editor->show();
        QPushButton *add=nullptr,*apply=nullptr;for(auto *b:editor->findChildren<QPushButton*>()){if(b->text()=="Add")add=b;if(b->text()=="Apply")apply=b;}QVERIFY(add&&apply);QTest::mouseClick(add,Qt::LeftButton);QCOMPARE(model.design.animationCues.size(),1);
        editor->findChild<QLineEdit*>("cueName")->setText("Exit");editor->findChild<QSpinBox*>("cueEnd")->setValue(50);editor->findChild<QCheckBox*>("cueHold")->setChecked(false);QTest::mouseClick(apply,Qt::LeftButton);
        auto cue=model.design.animationCues.first().toObject();QCOMPARE(cue["name"].toString(),QString("Exit"));QVERIFY(!cue["hold"].toBool());QCOMPARE(cue["end"].toInt(),50*model.frameMs());QCOMPARE(ElementDesign::fromJson(model.design.toJson()).animationCues,model.design.animationCues);
        QVERIFY(editor->grab().save("artifacts/animation-cues.png"));QVERIFY(model.undo());QVERIFY(model.design.animationCues.first().toObject()["hold"].toBool());delete editor;
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
    void cleanupTestCase(){QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/cuebase.json");}
};
QTEST_MAIN(BuildTests)
#include "BuildTests.moc"
