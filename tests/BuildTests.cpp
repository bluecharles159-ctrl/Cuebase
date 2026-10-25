#include <QDialogButtonBox>
#include <QDialog>
#include <QLayout>
#include <QFileDialog>
#include <QTimer>
#include <QPointer>
#include <QDateTime>
#include <QtTest>
#include <algorithm>
#include <QToolButton>
#include <QMenu>
#include <QSettings>
#include "ElementLibrary.h"
#include "SceneEffects.h"
#include "CueSequence.h"
#include "AssetImport.h"
#include "DesignAssets.h"
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
#include <QListWidget>
#include <QPlainTextEdit>
#include "BuildModel.h"
#include "BuildCanvas.h"
#include "BuildWorkspace.h"
#include "AnimationEditor.h"
#include "Data.h"
#include "UiControls.h"
#include "UiTheme.h"
#include "ColorPicker.h"
#include "BuildContextAction.h"
#include "SurfaceEffects.h"
#include "FrameExport.h"
#include "ColorManagement.h"
#include <QDir>
#include "AnimationCuePanel.h"
#include "PropertyInspector.h"
#include <QCheckBox>
#include <QComboBox>

class BuildTests:public QObject {
    Q_OBJECT
private slots:
    void lightingModesHierarchyAndExport(){
        Library library;BuildModel model;auto scene=ElementDesign::defaultScene();scene.objects.erase(std::remove_if(scene.objects.begin(),scene.objects.end(),[](const DObject &o){return o.type=="Light";}),scene.objects.end());scene.world.transparent=false;scene.world.color=QColor("#393939");scene.world.ambient=.02;
        DObject cube;cube.id="lit-cube";cube.type="Mesh";cube.mesh=Mesh::primitive("Cube");cube.base.scl={2,2,2};cube.base.rot={20,0,25};cube.mat.color=QColor("#74a7db");scene.objects.append(cube);DObject group;group.id="light-rig";group.type="Group";scene.objects.append(group);DObject light;light.id="key-light";light.type="Light";light.parentId=group.id;light.base.loc={0,-6,0};light.base.rot={90,0,0};light.light.color=QColor("#ffe0b0");scene.objects.append(light);model.reset(scene);
        BuildCanvas canvas(&model,&library);canvas.resize(708,428);canvas.showOverlays=false;canvas.setProperty("previewQuality","Full");canvas.show();QTest::qWait(20);auto picture=[&](int mode){canvas.setShading(mode);return canvas.grab().toImage().copy(34,34,640,360).convertToFormat(QImage::Format_ARGB32_Premultiplied);};const auto work=picture(BuildCanvas::Solid),material=picture(BuildCanvas::MaterialPreview);QVERIFY(work!=material);QTemporaryDir temporary;QVERIFY(temporary.isValid());
        QImage proof(960,4*204,QImage::Format_ARGB32_Premultiplied);proof.fill(QColor("#202327"));QPainter proofPainter(&proof);proofPainter.setPen(Qt::white);int row=0;
        for(const auto &kind:QStringList{"Point","Sun","Spot","Area"}){
            auto *lamp=model.design.find(light.id);lamp->light.kind=kind;lamp->light.energy=kind=="Sun"?3:600;const auto lit=picture(BuildCanvas::Rendered);model.design.find(group.id)->visible=false;const auto hidden=picture(BuildCanvas::Rendered);QVERIFY2(lit!=hidden,qPrintable(kind));model.design.find(group.id)->visible=true;lamp->visible=false;QCOMPARE(picture(BuildCanvas::Rendered),hidden);lamp->visible=true;QCOMPARE(picture(BuildCanvas::Rendered),lit);
            lamp->light.color=Qt::blue;QCOMPARE(picture(BuildCanvas::Solid),work);QCOMPARE(picture(BuildCanvas::MaterialPreview),material);QVERIFY(picture(BuildCanvas::Rendered)!=lit);lamp->light.color=QColor("#ffe0b0");
            const auto restored=ElementDesign::fromJson(model.design.toJson());FrameExportOptions options;options.size=lit.size();options.endFrame=1;const auto destination=temporary.filePath(kind);const auto result=exportSceneFrames(restored,destination,options);QVERIFY2(result.success,qPrintable(result.error));QImage saved(destination+"/frame_000000.png");QCOMPARE(saved.convertToFormat(lit.format()),lit);
            proofPainter.drawText(10,row*204+18,kind+" — Work / Material / Final");proofPainter.drawImage(QRect(0,row*204+24,320,180),work);proofPainter.drawImage(QRect(320,row*204+24,320,180),material);proofPainter.drawImage(QRect(640,row*204+24,320,180),lit);++row;
        }
        proofPainter.end();QVERIFY(proof.save("artifacts/build-lighting-modes.png"));auto *lamp=model.design.find(light.id);lamp->light.kind="Point";lamp->keyChannel("light.power",0,0);lamp->keyChannel("light.power",1000,600);model.t=0;const auto dark=picture(BuildCanvas::Rendered);model.t=1000;const auto bright=picture(BuildCanvas::Rendered);QVERIFY(bright!=dark);FrameExportOptions options;options.size=bright.size();options.firstFrame=25;options.endFrame=26;const auto path=temporary.filePath("animated");const auto result=exportSceneFrames(model.design,path,options);QVERIFY2(result.success,qPrintable(result.error));QImage saved(path+"/frame_000025.png");QCOMPARE(saved.convertToFormat(bright.format()),bright);
    }
    void displayColorMatchesFinalAndExport(){
        Library library;BuildModel model;auto scene=ElementDesign::defaultScene();scene.world.transparent=false;scene.world.color=QColor("#393939");DObject plate;plate.id="graded";plate.type="Shape";plate.w=10;plate.h=6;plate.base.rot={90,0,0};plate.mat.shadeless=true;plate.mat.color=QColor("#557fa8");plate.mat.alpha=.65;scene.objects.append(plate);model.reset(scene);
        BuildCanvas canvas(&model,&library);canvas.resize(708,428);canvas.showOverlays=false;canvas.setProperty("previewQuality","Full");canvas.setShading(BuildCanvas::Solid);canvas.show();QTest::qWait(20);const auto work=canvas.grab().toImage();const auto unchanged=model.design.toJson();QTemporaryDir temporary;QVERIFY(temporary.isValid());
        QImage base(640,360,QImage::Format_ARGB32_Premultiplied);base.fill(Qt::transparent);{QPainter painter(&base);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);scene.render(painter,base.rect(),0);}
        int index=0;for(const auto &view:colorViews())for(const auto &look:colorLooks()){
            auto &display=library.display;display.colorView=view;display.colorLook=look;display.exposure=.7;display.gamma=1.2;canvas.setShading(BuildCanvas::Solid);QCOMPARE(canvas.grab().toImage(),work);canvas.setShading(BuildCanvas::Rendered);
            auto expected=base;applyColorManagement(expected,view,look,display.exposure,display.gamma);QVERIFY(expected!=base);QCOMPARE(canvas.grab().toImage().copy(34,34,640,360).convertToFormat(expected.format()),expected);
            FrameExportOptions options;options.size=base.size();options.endFrame=1;options.colorView=view;options.colorLook=look;options.exposure=display.exposure;options.gamma=display.gamma;const auto path=temporary.filePath(QString::number(index++));const auto result=exportSceneFrames(scene,path,options);QVERIFY2(result.success,qPrintable(result.error));QImage saved(path+"/frame_000000.png");QCOMPARE(saved.convertToFormat(expected.format()),expected);
        }
        QCOMPARE(model.design.toJson(),unchanged);
        BuildWorkspace workspace(&library);workspace.resize(1200,760);workspace.findChild<BuildModel*>()->reset(scene);workspace.show();QTest::qWait(20);auto *preview=workspace.findChild<QWidget*>("compositionCleanPreview");QVERIFY(preview);auto clean=preview->grab().toImage();auto expectedClean=base;applyColorManagement(expectedClean,library.display.colorView,library.display.colorLook,library.display.exposure,library.display.gamma);QCOMPARE(clean.pixelColor(clean.width()/2,clean.height()/2),expectedClean.pixelColor(320,180));QVERIFY(canvas.grab().save("artifacts/build-color-managed-final.png"));
        QImage alpha(2,1,QImage::Format_ARGB32_Premultiplied);alpha.fill(Qt::transparent);alpha.setPixelColor(1,0,QColor(80,100,120,96));applyColorManagement(alpha,"Filmic","High Contrast",1.,1.2);QCOMPARE(alpha.pixelColor(0,0).alpha(),0);QCOMPARE(alpha.pixelColor(1,0).alpha(),96);
    }
    void frameExportRetainsMotionBlur(){
        auto scene=ElementDesign::defaultScene();scene.durationMs=1000;scene.loop=false;
        DObject object;object.id="moving";object.type="Shape";object.w=1;object.h=3;object.base.rot={90,0,0};object.mat.color=Qt::white;object.mat.shadeless=true;object.keyChannel("position.x",0,-6);object.keyChannel("position.x",1000,6);scene.objects.append(object);
        FrameExportOptions options;options.size={320,180};options.setProjectFrameRate(25,1);options.firstFrame=12;options.endFrame=13;options.renderSettings.motionBlur=true;options.renderSettings.shutter=2;options.renderSettings.samples=7;
        auto render=[&](bool blur){QImage image(options.size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);auto settings=options.renderSettings;settings.motionBlur=blur;settings.frameMs=40;scene.render(painter,image.rect(),480,true,&settings,false);painter.end();return image;};
        const auto expected=render(true);QVERIFY(expected!=render(false));QTemporaryDir directory;QVERIFY(directory.isValid());const auto result=exportSceneFrames(scene,directory.path(),options);QVERIFY2(result.success,qPrintable(result.error));QImage saved(directory.filePath("frame_000012.png"));QCOMPARE(saved.convertToFormat(expected.format()),expected);
    }
    void backgroundInspectorPreviewExport(){
        Library library;BuildModel model;QTemporaryDir temporary;QVERIFY(temporary.isValid());QImage media(64,32,QImage::Format_ARGB32);media.fill(Qt::blue);{QPainter p(&media);p.fillRect(0,0,32,32,Qt::red);}const auto path=temporary.filePath("background.png");QVERIFY(media.save(path));
        auto scene=ElementDesign::defaultScene();scene.world.transparent=false;scene.world.color=QColor("#393939");DObject plate;plate.id="background";plate.type="Shape";plate.w=10;plate.h=6;plate.base.rot={90,0,0};plate.mat.color=Qt::red;plate.mat.shadeless=true;plate.image=path;plate.style["fill.end"]="#245ad5";scene.objects.append(plate);
        auto glass=plate;glass.id="glass";glass.image.clear();glass.w=5;glass.h=3;glass.base.loc.setY(-.1);glass.mat.color=Qt::white;glass.mat.alpha=.2;glass.style={};glass.style["backdrop.blur"]=20.;glass.style["background.type"]="Solid";scene.objects.append(glass);model.reset(scene);model.select(plate.id);
        PropertyInspector inspector(&model,&library);auto *background=inspector.findChild<QComboBox*>("property_background.type");QVERIFY(background);BuildCanvas canvas(&model,&library);canvas.resize(708,428);canvas.showOverlays=false;canvas.setShading(BuildCanvas::Rendered);canvas.setProperty("previewQuality","Full");canvas.show();QTest::qWait(20);
        for(const auto &kind:QStringList{"Solid","Gradient","Media"}){
            background->setCurrentText(kind);QMetaObject::invokeMethod(background,"activated",Qt::DirectConnection,Q_ARG(int,background->currentIndex()));QCOMPARE(model.byId(plate.id)->style["background.type"].toString(),kind);
            QImage expected(640,360,QImage::Format_ARGB32_Premultiplied);expected.fill(Qt::transparent);{QPainter p(&expected);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);model.design.render(p,expected.rect(),0);}
            const auto preview=canvas.grab().toImage().copy(34,34,640,360).convertToFormat(expected.format());QCOMPARE(preview,expected);
            const auto restored=ElementDesign::fromJson(model.design.toJson());QCOMPARE(restored.find(plate.id)->style["background.type"].toString(),kind);FrameExportOptions options;options.size=expected.size();options.endFrame=1;const auto destination=temporary.filePath(kind);const auto result=exportSceneFrames(restored,destination,options);QVERIFY2(result.success,qPrintable(result.error));QImage output(destination+"/frame_000000.png");QCOMPARE(output.convertToFormat(expected.format()),expected);QVERIFY(preview.save("artifacts/background-"+kind.toLower()+".png"));
        }
    }
    void revealInspectorAndExport(){
        Library library;BuildModel model;auto design=ElementDesign::defaultScene();design.durationMs=1000;design.loop=false;
        DObject shape;shape.id="reveal-demo";shape.type="Shape";shape.name="Reveal demo";shape.w=9;shape.h=5;shape.base.rot={90,0,0};shape.mat.shadeless=true;shape.mat.color=QColor("#208fdf");shape.style["corner.radius"]=28;shape.style["background.type"]="Gradient";shape.style["fill.end"]="#7043c7";shape.keyChannel("effects.reveal",0,0);shape.keyChannel("effects.reveal",1000,1);design.objects.append(shape);model.reset(design);model.select(shape.id);
        PropertyInspector inspector(&model,&library);inspector.resize(320,800);inspector.show();for(auto *section:inspector.findChildren<QToolButton*>())if(section->text()=="EFFECTS")section->setChecked(true);
        auto *style=inspector.findChild<QComboBox*>("property_effects.revealStyle");auto *timing=inspector.findChild<QComboBox*>("property_effects.revealTiming");QVERIFY(style&&timing);
        auto choose=[](QComboBox *combo,const QString &value){combo->setCurrentText(value);QMetaObject::invokeMethod(combo,"activated",Qt::DirectConnection,Q_ARG(int,combo->currentIndex()));};
        auto render=[&](qint64 time){QImage image(320,180,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);model.design.render(p,image.rect(),time,true,nullptr,false);p.end();return image;};
        const QStringList styles{"Wipe","Pixels","Blocks","Liquid","Assemble"},timings{"Linear","Smooth","Constant","Steps","Flicker"};QImage sheet(960,styles.size()*210,QImage::Format_ARGB32_Premultiplied);sheet.fill(QColor("#171c23"));QPainter sheetPainter(&sheet);sheetPainter.setPen(Qt::white);QTemporaryDir exports;QVERIFY(exports.isValid());
        for(int row=0;row<styles.size();++row){const auto previous=model.design.toJson();choose(style,styles[row]);QCOMPARE(model.byId(shape.id)->style["effects.revealStyle"].toString(),styles[row]);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),previous);QVERIFY(model.redo());
            for(const auto &name:timings){choose(timing,name);const auto expected=render(400);render(800);QCOMPARE(render(400),expected);const auto restored=ElementDesign::fromJson(model.design.toJson());QCOMPARE(restored.find(shape.id)->style["effects.revealStyle"].toString(),styles[row]);QCOMPARE(restored.find(shape.id)->style["effects.revealTiming"].toString(),name);
                FrameExportOptions options;options.size={320,180};options.firstFrame=10;options.endFrame=11;const auto path=exports.filePath(styles[row]+"-"+name);const auto result=exportSceneFrames(restored,path,options);QVERIFY2(result.success,qPrintable(result.error));QImage saved(path+"/frame_000010.png");QCOMPARE(saved.convertToFormat(expected.format()),expected);
            }
            choose(timing,"Smooth");sheetPainter.drawText(12,row*210+22,styles[row]+" — 20% / 55% / complete");sheetPainter.drawImage(0,row*210+30,render(200));sheetPainter.drawImage(320,row*210+30,render(550));sheetPainter.drawImage(640,row*210+30,render(1000));
        }
        sheetPainter.end();QVERIFY(sheet.save("artifacts/reveal-styles-proof.png"));
    }
    void compositionGuidesPersistAndUndo(){
        Library library;BuildWorkspace workspace(&library);workspace.resize(1200,760);workspace.show();auto *model=workspace.findChild<BuildModel*>();auto *canvas=workspace.findChild<BuildCanvas*>();QVERIFY(model&&canvas);model->reset(ElementDesign::defaultScene());canvas->frameAll();QTest::qWait(20);canvas->grab();
        auto frame=canvas->property("compositionFrame").toRectF();QVERIFY(frame.width()>100);auto position=[&](double fraction){return QPoint(qRound(frame.left()+fraction*frame.width()),qRound(frame.center().y()));};
        QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(10,position(.25).y()));QTest::mouseMove(canvas,position(.25));QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,position(.25));
        auto guides=[&]{return model->design.camera()->style["editor.guides.vertical"].toArray();};QCOMPARE(guides().size(),1);QVERIFY(qAbs(guides().first().toDouble()-.25)<.01);QVERIFY(model->undo());QVERIFY(guides().isEmpty());QVERIFY(model->redo());QCOMPARE(guides().size(),1);
        const auto saved=model->design.toJson();model->reset(ElementDesign::fromJson(saved));QCOMPARE(guides().size(),1);library.display.resolution="3840x2160";canvas->grab();frame=canvas->property("compositionFrame").toRectF();
        QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,position(.25));QTest::mouseMove(canvas,position(.6));QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,position(.6));QCOMPARE(guides().size(),1);QVERIFY(qAbs(guides().first().toDouble()-.6)<.01);QVERIFY(model->undo());QVERIFY(qAbs(guides().first().toDouble()-.25)<.01);
        auto *clear=workspace.findChild<QAction*>("clearCompositionGuides");QVERIFY(clear);clear->trigger();QVERIFY(guides().isEmpty());QVERIFY(model->undo());QCOMPARE(guides().size(),1);
        auto *settings=workspace.findChild<QAction*>("compositionGuideSettings");QVERIFY(settings);bool edited=false;
        QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;QTimer::singleShot(3000,dialog,&QDialog::reject);auto *title=dialog->findChild<QDoubleSpinBox*>("guideTitleMargin");auto *grid=dialog->findChild<QDoubleSpinBox*>("guideGridSpacing");auto *color=dialog->findChild<QLineEdit*>("compositionGuideColor");QVERIFY(title&&grid&&color);title->setValue(12);grid->setValue(80);color->setText("#edb950");QVERIFY(dialog->grab().save("artifacts/composition-guides-dialog.png"));edited=true;dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});settings->trigger();QVERIFY(edited);QCOMPARE(model->design.camera()->style["editor.titleSafe"].toDouble(),.12);QCOMPARE(model->design.camera()->style["editor.gridSpacing"].toDouble(),80.);QCOMPARE(QColor(model->design.camera()->style["editor.guideColor"].toString()),QColor("#edb950"));QVERIFY(model->undo());QVERIFY(!model->design.camera()->style.contains("editor.titleSafe"));QVERIFY(model->redo());
        const auto beforeCancel=model->design.toJson();QTimer::singleShot(0,[&]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){dialog->findChild<QDoubleSpinBox*>("guideTitleMargin")->setValue(20);dialog->reject();}});settings->trigger();QCOMPARE(model->design.toJson(),beforeCancel);
        canvas->grab();QVERIFY(canvas->grab().save("artifacts/composition-guides.png"));model->reset(ElementDesign::defaultScene());QVERIFY(guides().isEmpty());
    }
    void buildPreviewQualityAndBackdrop(){
        Library library;BuildModel model;auto scene=ElementDesign::defaultScene();scene.durationMs=1000;
        DObject left;left.id="left";left.type="Shape";left.w=4;left.h=5;left.base.rot={90,0,0};left.base.loc.setX(-2);left.mat.color=Qt::red;left.mat.shadeless=true;
        auto right=left;right.id="right";right.base.loc.setX(2);right.mat.color=Qt::blue;
        auto glass=left;glass.id="glass";glass.w=5;glass.h=3;glass.base.loc={0,-.1f,0};glass.mat.color=Qt::white;glass.mat.alpha=.15;glass.style["backdrop.blur"]=24.;glass.style["backdrop.brightness"]=1.;scene.objects.append(left);scene.objects.append(right);scene.objects.append(glass);model.reset(scene);
        BuildCanvas canvas(&model,&library);canvas.resize(640,428);canvas.showOverlays=false;canvas.setShading(BuildCanvas::Rendered);canvas.setProperty("previewQuality","Full");canvas.show();QTest::qWait(20);
        const auto before=model.design.toJson();const auto full=canvas.grab().toImage();const QSize fullSize=canvas.property("previewPixelSize").toSize();QVERIFY(fullSize.width()>0);QVERIFY(full.save("artifacts/build-backdrop-preview.png"));
        QImage expected(fullSize,QImage::Format_ARGB32_Premultiplied);expected.fill(Qt::transparent);{QPainter p(&expected);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);scene.render(p,expected.rect(),0);}
        const double ratio=double(fullSize.width())/(canvas.width()-68);const auto actual=full.pixelColor(full.width()/2+1,full.height()/2);const auto reference=expected.pixelColor(expected.width()/2+qRound(ratio),expected.height()/2);
        QVERIFY2(qAbs(actual.red()-reference.red())<12&&qAbs(actual.blue()-reference.blue())<12,qPrintable(actual.name()+" / "+reference.name()));QVERIFY(actual.red()>45&&actual.blue()>45);
        canvas.setProperty("previewQuality","Auto");model.playing=true;canvas.grab();const QSize playing=canvas.property("previewPixelSize").toSize();QVERIFY(qAbs(playing.width()*2-fullSize.width())<=1);model.playing=false;canvas.grab();QCOMPARE(canvas.property("previewPixelSize").toSize(),fullSize);
        canvas.setProperty("previewQuality","Quarter");canvas.grab();QVERIFY(qAbs(canvas.property("previewPixelSize").toSize().width()*4-fullSize.width())<=3);QCOMPARE(model.design.toJson(),before);
        canvas.setProperty("previewQuality","Full");const QPointF anchor(canvas.rect().center());for(int i=0;i<24;++i){QWheelEvent wheel(anchor,canvas.mapToGlobal(anchor.toPoint()),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&canvas,&wheel);}canvas.grab();const QSize zoomed=canvas.property("previewPixelSize").toSize();QVERIFY(qint64(zoomed.width())*zoomed.height()<=2073600);QVERIFY(canvas.property("previewRenderMs").toLongLong()>=0);QCOMPARE(model.design.toJson(),before);
    }
    void buildAppearanceModes(){
        QSettings settings;const auto previous=settings.value("build/viewMode");
        struct Restore {QVariant value;~Restore(){QSettings s;if(value.isValid())s.setValue("build/viewMode",value);else s.remove("build/viewMode");}} restore{previous};
        settings.setValue("build/viewMode",int(BuildCanvas::Solid));
        Library library;BuildWorkspace workspace(&library);workspace.resize(1200,760);workspace.show();
        auto *model=workspace.findChild<BuildModel*>();auto *canvas=workspace.findChild<BuildCanvas*>();auto *control=workspace.findChild<QToolButton*>("buildViewMode");QVERIFY(model&&canvas&&control);
        model->reset(elementPreset("Sponsor spotlight"));model->setTime(1500);canvas->frameAll();QCOMPARE(canvas->shading(),int(BuildCanvas::Solid));QCOMPARE(control->text(),QString("Work"));
        const auto original=model->design.toJson();
        auto render=[&]{QImage frame(640,360,QImage::Format_ARGB32_Premultiplied);frame.fill(Qt::transparent);QPainter p(&frame);model->design.render(p,frame.rect(),model->t);p.end();return frame;};
        const auto output=render();
        auto workAction=control->menu()->findChild<QAction*>("buildViewMode_Work");auto finalAction=control->menu()->findChild<QAction*>("buildViewMode_Final");auto materialAction=control->menu()->findChild<QAction*>("buildViewMode_Material");QVERIFY(workAction&&finalAction&&materialAction);
        QTest::qWait(30);const auto work=canvas->grab().toImage();QVERIFY(work.save("artifacts/build-work-mode.png"));
        materialAction->trigger();QCOMPARE(canvas->shading(),int(BuildCanvas::MaterialPreview));QCOMPARE(control->text(),QString("Material"));QVERIFY(materialAction->isChecked());const auto material=canvas->grab().toImage();QVERIFY(material.save("artifacts/build-material-mode.png"));QVERIFY(work!=material);
        finalAction->trigger();QCOMPARE(canvas->shading(),int(BuildCanvas::Rendered));QCOMPARE(control->text(),QString("Final"));QVERIFY(finalAction->isChecked());QVERIFY(!materialAction->isChecked());QVERIFY(canvas->grab().save("artifacts/build-final-mode.png"));QCOMPARE(render(),output);QCOMPARE(model->design.toJson(),original);
        workAction->trigger();QCOMPARE(canvas->grab().toImage(),work);QTest::keyClick(canvas,Qt::Key_Z);QCOMPARE(control->text(),QString("Material"));QVERIFY(materialAction->isChecked());QCOMPARE(model->design.toJson(),original);
        BuildWorkspace reopened(&library);QCOMPARE(reopened.findChild<BuildCanvas*>()->shading(),int(BuildCanvas::MaterialPreview));
        // Work surfaces remain neutral even with a saturated material and effects.
        auto scene=ElementDesign::defaultScene();DObject plate;plate.id="plain";plate.type="Shape";plate.w=8;plate.h=4;plate.base.rot={90,0,0};plate.mat.color=Qt::red;plate.style["effects.invert"]=1.;scene.objects.append(plate);
        QImage frame(320,180,QImage::Format_ARGB32_Premultiplied);frame.fill(Qt::transparent);QPainter painter(&frame);PaintOpts options;options.shade=PaintOpts::Solid;scene.paint(painter,frame.rect(),scene.sceneCamera(0,320./180),0,options);painter.end();const auto center=frame.pixelColor(160,90);QCOMPARE(center.red(),center.green());QCOMPARE(center.green(),center.blue());QVERIFY(center.alpha()>0);
    }
    void elementNamingSaveFailure(){
        struct Profile {QString previous=qApp->applicationName();Profile(){qApp->setApplicationName(previous+"-element-save");}~Profile(){qApp->setApplicationName(previous);}} profile;
        Library library;Element element;element.id="identity";element.name="Original";element.type="Custom";element.playlist="Overlays";element.design=elementPreset("Sponsor spotlight").toJson();library.elements={element};library.display.channelLogoElement="previous-logo";QString error;QVERIFY2(library.saveChecked(&error),qPrintable(error));
        BuildWorkspace workspace(&library);workspace.editElement(element.id);auto *picker=workspace.findChild<QComboBox*>("buildElementPicker");QVERIFY(picker);QVERIFY(picker->lineEdit());QSignalSpy changed(&library,&Library::elementsChanged);
        const QString tag="{{element:main-logo|Primary identity}}";picker->lineEdit()->setText(tag);
        const auto destination=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/cuebase.json";QVERIFY(QFile::remove(destination));QVERIFY(QDir().mkpath(destination));
        QVERIFY(!workspace.saveForClose());QCOMPARE(changed.count(),0);QCOMPARE(library.elements.size(),1);QCOMPARE(library.elements.first().name,QString("Original"));QCOMPARE(library.elements.first().playlist,QString("Overlays"));QCOMPARE(library.display.channelLogoElement,QString("previous-logo"));QCOMPARE(picker->currentText(),tag);QVERIFY(workspace.hasUnsavedChanges());
        QVERIFY(QDir().rmdir(destination));QVERIFY(workspace.saveForClose());QCOMPARE(changed.count(),1);QCOMPARE(library.elements.first().name,QString("Primary identity"));QCOMPARE(library.elements.first().playlist,QString("Logos"));QCOMPARE(library.display.channelLogoElement,element.id);QVERIFY(!workspace.hasUnsavedChanges());
        Library reopened;reopened.load();QVERIFY(reopened.element(element.id));QCOMPARE(reopened.element(element.id)->name,QString("Primary identity"));QCOMPARE(reopened.display.channelLogoElement,element.id);QCOMPARE(ElementDesign::fromJson(reopened.element(element.id)->design).camera()->style["studio.sceneRole"].toString(),QString("main-logo"));
        picker->lineEdit()->setText("{{element:break|Next program}}");QVERIFY(workspace.saveForClose());QCOMPARE(library.elements.first().playlist,QString("Break screens"));QCOMPARE(library.elements.first().name,QString("Next program"));QVERIFY(QFile::remove(destination));
    }
    void portableCompositionMedia(){
        QTemporaryDir temporary;QVERIFY(temporary.isValid());QDir root(temporary.path());QVERIFY(root.mkdir("original"));QVERIFY(root.mkdir("export"));
        QImage image(16,16,QImage::Format_ARGB32);image.fill(Qt::cyan);const auto imagePath=root.filePath("original/plate.png");QVERIFY(image.save(imagePath));
        const auto audioPath=root.filePath("original/sound.wav");QFile audioFile(audioPath);QVERIFY(audioFile.open(QIODevice::WriteOnly));const QByteArray audioBytes("portable audio fixture");QCOMPARE(audioFile.write(audioBytes),qint64(audioBytes.size()));audioFile.close();
        auto design=ElementDesign::defaultScene();DObject picture;picture.id="picture";picture.name="Picture";picture.type="Image";picture.image=imagePath;design.objects.append(picture);auto duplicate=picture;duplicate.id="duplicate";design.objects.append(duplicate);DObject audio;audio.id="audio";audio.type="Audio";audio.style["audio.file"]=audioPath;design.objects.append(audio);
        const auto original=design.toJson();const auto destination=root.filePath("export/portable.cuebuild");QString error;QVERIFY2(exportPortableDesign(design,destination,&error),qPrintable(error));QCOMPARE(design.toJson(),original);
        QFile manifest(destination);QVERIFY(manifest.open(QIODevice::ReadOnly));const auto bytes=manifest.readAll();manifest.close();auto packed=ElementDesign::fromJson(QString::fromUtf8(bytes));QCOMPARE(packed.objects[packed.objects.size()-3].image,packed.objects[packed.objects.size()-2].image);for(const auto &asset:designAssets(packed))QVERIFY(QFileInfo(asset.path).isRelative());
        const auto assetFolders=QDir(root.filePath("export")).entryList(QDir::Dirs|QDir::NoDotAndDotDot);QCOMPARE(assetFolders.size(),1);QCOMPARE(QDir(root.filePath("export/"+assetFolders.first())).entryList(QDir::Files).size(),2);
        QVERIFY(root.rename("export","moved"));resolveDesignAssets(packed,root.filePath("moved"));QVERIFY(missingDesignAsset(packed).isEmpty());QCOMPARE(QImage(packed.find("picture")->image),image);QFile copiedAudio(packed.find("audio")->style["audio.file"].toString());QVERIFY(copiedAudio.open(QIODevice::ReadOnly));QCOMPARE(copiedAudio.readAll(),audioBytes);
        const auto moved=root.filePath("moved/portable.cuebuild");int progressCalls=0;QVERIFY(!exportPortableDesign(design,moved,&error,[&](int,int){return ++progressCalls<3;}));QCOMPARE(progressCalls,3);QVERIFY(error.contains("cancelled"));QFile unchanged(moved);QVERIFY(unchanged.open(QIODevice::ReadOnly));QCOMPARE(unchanged.readAll(),bytes);unchanged.close();QCOMPARE(QDir(root.filePath("moved")).entryList(QDir::Dirs|QDir::NoDotAndDotDot),assetFolders);
        design.find("picture")->image=root.filePath("missing.png");QVERIFY(!exportPortableDesign(design,moved,&error));QVERIFY(error.contains("Picture"));QVERIFY(unchanged.open(QIODevice::ReadOnly));QCOMPARE(unchanged.readAll(),bytes);
    }
    void relativeCompositionAudioImport(){
        QTemporaryDir directory;QVERIFY(directory.isValid());QFile audio(directory.filePath("audio.wav"));QVERIFY(audio.open(QIODevice::WriteOnly));audio.write("fixture");audio.close();
        auto design=ElementDesign::defaultScene();DObject sound;sound.id="relative-audio";sound.type="Audio";sound.style["audio.file"]="audio.wav";design.objects.append(sound);
        QFile document(directory.filePath("relative.cuebuild"));QVERIFY(document.open(QIODevice::WriteOnly));document.write(design.toJson().toUtf8());document.close();
        Library lib;BuildWorkspace workspace(&lib);QVERIFY(workspace.importFile(document.fileName()));auto *model=workspace.findChild<BuildModel*>();QVERIFY(model);QCOMPARE(model->design.find(sound.id)->style["audio.file"].toString(),directory.filePath("audio.wav"));
        const auto before=model->design.toJson();sound.style["audio.file"]="missing.wav";design.objects.last()=sound;QVERIFY(document.open(QIODevice::WriteOnly|QIODevice::Truncate));document.write(design.toJson().toUtf8());document.close();QVERIFY(!workspace.importFile(document.fileName()));QCOMPARE(model->design.toJson(),before);
    }
    void compositionMediaRelink(){
        QTemporaryDir directory;QVERIFY(directory.isValid());QImage image(8,8,QImage::Format_ARGB32);image.fill(Qt::yellow);const auto replacement=directory.filePath("replacement.png");QVERIFY(image.save(replacement));
        auto design=ElementDesign::defaultScene();DObject a;a.id="media-a";a.name="First image";a.type="Image";a.image=directory.filePath("missing.png");a.keyChannel("opacity",0,.5);a.keyChannel("opacity",1000,1.);auto b=a;b.id="media-b";design.objects.append(a);design.objects.append(b);const auto original=design.toJson();QString error;
        QVERIFY(!relinkDesignAsset(design,a.image,directory.filePath("absent.png"),&error));QCOMPARE(design.toJson(),original);
        BuildModel model;model.reset(design);QPointer<QDialog> dialog=createDesignMediaDialog(&model);dialog->show();auto *tree=dialog->findChild<QTreeWidget*>("compositionMediaList");QVERIFY(tree);QCOMPARE(tree->topLevelItemCount(),1);QCOMPARE(tree->topLevelItem(0)->text(2),QString("Missing"));tree->setCurrentItem(tree->topLevelItem(0));
        bool selected=false;QTimer::singleShot(0,dialog,[&]{if(auto *picker=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){QTimer::singleShot(3000,picker,&QDialog::reject);picker->selectFile(replacement);selected=QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);}});
        dialog->findChild<QPushButton*>("relinkCompositionMedia")->click();QVERIFY(selected);QCOMPARE(model.design.toJson(),original);QCOMPARE(tree->topLevelItem(0)->text(2),QString("Available"));
        auto *buttons=dialog->findChild<QDialogButtonBox*>();QVERIFY(buttons->button(QDialogButtonBox::Apply)->isEnabled());buttons->button(QDialogButtonBox::Apply)->click();QCOMPARE(model.design.find(a.id)->image,replacement);QCOMPARE(model.design.find(b.id)->image,replacement);QCOMPARE(model.design.find(a.id)->track("opacity")->keys.size(),2);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),original);
        auto *cancelled=createDesignMediaDialog(&model);cancelled->reject();QCOMPARE(model.design.toJson(),original);
        QPointer<QDialog> stale=createDesignMediaDialog(&model);stale->show();auto *staleTree=stale->findChild<QTreeWidget*>("compositionMediaList");staleTree->setCurrentItem(staleTree->topLevelItem(0));auto *relink=stale->findChild<QPushButton*>("relinkCompositionMedia");QVERIFY(relink->isEnabled());
        auto *search=stale->findChild<QLineEdit*>("mediaSearch");search->setText("no matching source");QVERIFY(staleTree->topLevelItem(0)->isHidden());QVERIFY(!relink->isEnabled());search->clear();staleTree->setCurrentItem(staleTree->topLevelItem(0));
        QTimer::singleShot(0,stale,[&]{if(auto *picker=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){QTimer::singleShot(3000,picker,&QDialog::reject);picker->selectFile(replacement);QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);}});relink->click();
        model.design.find(a.id)->name="Changed while relinking";model.notify();const auto concurrent=model.design.toJson();stale->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();QCOMPARE(model.design.toJson(),concurrent);QVERIFY(stale->isVisible());QVERIFY(stale->findChild<QLabel*>("mediaStatus")->text().contains("composition changed"));stale->reject();
    }
    void perObjectInspectorExpansion(){
        Library lib;BuildModel model;auto design=ElementDesign::defaultScene();
        DObject first;first.id=Library::newId();first.type="Shape";first.name="Circle";first.style["shape"]="Ellipse";auto second=first;second.id=Library::newId();second.name="Square";second.style["shape"]="Rectangle";auto third=second;third.id=Library::newId();auto text=first;text.id=Library::newId();text.type="Text";
        design.objects.append(first);design.objects.append(second);design.objects.append(third);design.objects.append(text);model.reset(design);model.select(first.id);
        auto inspector=std::make_unique<PropertyInspector>(&model,&lib);
        auto header=[&](const QString &name)->QToolButton*{for(int i=0;i<inspector->QWidget::layout()->count();++i)if(auto *button=qobject_cast<QToolButton*>(inspector->QWidget::layout()->itemAt(i)->widget());button&&button->text()==name)return button;return nullptr;};
        QVERIFY(header("TRANSFORM"));QVERIFY(header("TRANSFORM")->isChecked());QVERIFY(header("CORNERS"));QVERIFY(!header("CORNERS")->isChecked());QVERIFY(!header("APPEARANCE")->isChecked());header("CORNERS")->click();
        model.select(second.id);QVERIFY(!header("CORNERS")->isChecked());QVERIFY(header("TRANSFORM")->isChecked());header("APPEARANCE")->click();
        model.select(first.id);QVERIFY(header("CORNERS")->isChecked());QVERIFY(!header("APPEARANCE")->isChecked());
        model.select(third.id);QVERIFY(!header("CORNERS")->isChecked());QVERIFY(!header("APPEARANCE")->isChecked());
        model.select(text.id);QVERIFY(header("TYPOGRAPHY"));QVERIFY(header("TYPOGRAPHY")->isChecked());QVERIFY(header("TRANSFORM")->isChecked());header("TYPOGRAPHY")->click();
        model.select(second.id);QVERIFY(header("APPEARANCE")->isChecked());model.select(text.id);QVERIFY(!header("TYPOGRAPHY")->isChecked());
        inspector.reset();QSettings().sync();inspector=std::make_unique<PropertyInspector>(&model,&lib);QVERIFY(!header("TYPOGRAPHY")->isChecked());model.select(first.id);QVERIFY(header("CORNERS")->isChecked());QVERIFY(!header("APPEARANCE")->isChecked());
    }
    void rigidBodySupportAndReplay(){
        auto cube=[](QString id,QVector3D position,QVector3D scale,bool active){DObject o;o.id=id;o.type="Mesh";o.mesh=Mesh::primitive("Cube");o.base.loc=position;o.base.scl=scale;o.phys.enabled=true;o.phys.active=active;o.phys.bounce=0;o.phys.friction=.6;return o;};
        ElementDesign stable;stable.objects={cube("support",{0,0,2},{1,1,.25f},false),cube("body",{0,0,4.5f},{.5f,.5f,.5f},true)};
        auto supported=stable.worldMatrix(*stable.find("body"),4000);QVERIFY(qAbs(supported.column(3).z()-2.75)<.06);QVERIFY(qAbs(supported.column(3).x())<.08);
        ElementDesign falling=stable;falling.find("body")->base.loc.setX(1.2f);const auto initial=falling.worldMatrix(*falling.find("body"),0);const auto tipping=falling.worldMatrix(*falling.find("body"),1600);
        QVERIFY((tipping.column(0)-initial.column(0)).length()>.08);const auto landed=falling.worldMatrix(*falling.find("body"),4000);QVERIFY(landed.column(3).z()<1.1);QVERIFY(landed.column(3).x()>1.3);
        const auto replay=falling.worldMatrix(*falling.find("body"),1600);for(int i=0;i<16;++i)QVERIFY(qAbs(replay.constData()[i]-tipping.constData()[i])<.00001);
        ElementDesign independent=falling;independent.world.gravity=0;QVERIFY(qAbs(independent.worldMatrix(*independent.find("body"),4000).column(3).z()-4.5)<.001);QCOMPARE(falling.worldMatrix(*falling.find("body"),1600),replay);
        ElementDesign parented;DObject parent;parent.id="parent";parent.type="Group";parent.base.loc={3,0,0};auto child=cube("child",{0,0,4},{.5f,.5f,.5f},true);child.parentId=parent.id;parented.objects={parent,child};const auto world=parented.worldMatrix(*parented.find("child"),4000);QVERIFY(qAbs(world.column(3).z()-.5)<.06);ElementDesign unparented;child.parentId.clear();child.base.loc.setX(3);unparented.objects={child};const auto expected=unparented.worldMatrix(*unparented.find("child"),4000);for(int i=0;i<16;++i)QVERIFY(qAbs(world.constData()[i]-expected.constData()[i])<.00001);QVERIFY(qAbs(parented.propsAt(*parented.find("child"),4000).loc.x()-(world.column(3).x()-3))<.00001);
        ElementDesign flat;auto platform=cube("flat-support",{0,0,2},{1,1,1},false);platform.type="Shape";platform.w=2;platform.h=.5;platform.base.rot={90,0,0};auto tile=platform;tile.id="flat-body";tile.w=1;tile.h=1;tile.phys.active=true;tile.base.loc={1.2f,0,4.5f};flat.objects={platform,tile};const auto flatEnd=flat.worldMatrix(*flat.find(tile.id),4000);QVERIFY(qAbs(flatEnd.column(3).y())<.001);QVERIFY(flatEnd.column(3).z()<1.1);QVERIFY(flatEnd.column(3).x()>1.3);
    }
    void rigidBodyAnimatedSupport(){
        ElementDesign scene;
        DObject support;support.id="support";support.type="Mesh";support.mesh=Mesh::primitive("Cube");support.base.loc={0,0,2};support.base.scl={1,1,.25f};support.phys.enabled=true;support.phys.active=false;
        support.tracks.append(PropertyTrack{"position.x",{{0,0,"Linear"},{1000,5,"Linear"}}});
        DObject body=support;body.id="body";body.tracks.clear();body.base.loc={0,0,3};body.base.scl={.5f,.5f,.5f};body.phys.active=true;body.phys.bounce=0;
        scene.objects={support,body};
        const auto moved=scene.worldMatrix(*scene.find("body"),4000);
        QVERIFY(moved.column(3).z()<1.1);
        QCOMPARE(scene.worldMatrix(*scene.find("support"),4000).column(3).x(),5.f);
        scene.world.floor=false;
        QVERIFY(scene.worldMatrix(*scene.find("body"),4000).column(3).z()<-10);
        scene.world.gravity=0;
        QVERIFY(qAbs(scene.worldMatrix(*scene.find("body"),4000).column(3).z()-3)<.001);
    }
    void sharedThemesAndIcons(){
        for(const auto &theme:QStringList{"Studio Blue","Current","Darker","Light"}){
            applyUiTheme(theme,false);QCOMPARE(currentUiTheme(),theme);QCOMPARE(qApp->palette().color(QPalette::Window).lightness()>128,theme=="Light");
            QSet<QByteArray> symbols;for(const auto &name:QStringList{"inspector","align","graph","cue","modifier"}){const auto icon=uiIcon(name).pixmap(24,24).toImage();QVERIFY(!icon.isNull());symbols.insert(QByteArray(reinterpret_cast<const char*>(icon.constBits()),icon.sizeInBytes()));}QCOMPARE(symbols.size(),5);
            BuildModel model;Library library;model.addText();PropertyInspector panel(&model,&library);panel.resize(310,720);panel.show();QTest::qWait(20);QDir().mkpath("artifacts");QVERIFY(panel.grab().save("artifacts/inspector-"+theme.toLower()+".png"));
            QComboBox options;options.addItems({"Channel logo","Sample element","Next program","New card"});options.resize(240,25);options.show();options.showPopup();QTest::qWait(20);QCOMPARE(options.view()->objectName(),QString("optionPopup"));QVERIFY(options.view()->window()->grab().save("artifacts/options-"+theme.toLower()+".png"));options.hidePopup();
        }
        applyUiTheme("Darker",false);
    }
    void proceduralRevealStyles(){
        ElementDesign scene;DObject shape;shape.id="procedural";shape.type="Shape";shape.w=2;shape.h=1;const auto complete=scene.textureFor(shape);
        for(const auto &style:QStringList{"Pixels","Blocks","Liquid","Assemble"}){
            shape.style["effects.revealStyle"]=style;shape.style["effects.reveal"]=0.;auto hidden=scene.textureFor(shape);for(int y=0;y<hidden.height();++y)for(int x=0;x<hidden.width();++x)QCOMPARE(qAlpha(hidden.pixel(x,y)),0);
            shape.style["effects.reveal"]=.5;const auto middle=scene.textureFor(shape);QVERIFY(middle!=complete);QVERIFY(middle!=hidden);QCOMPARE(scene.textureFor(shape),middle);
            shape.style["effects.reveal"]=1.;QCOMPARE(scene.textureFor(shape),complete);
            shape.style["effects.reveal"]=.5;QCOMPARE(scene.textureFor(shape),middle);
        }
        shape.style["effects.revealStyle"]="Blocks";shape.style["effects.revealTiming"]="Constant";shape.style["effects.reveal"]=.9;const auto constant=scene.textureFor(shape);QCOMPARE(qAlpha(constant.pixel(10,10)),0);
        shape.style["effects.revealTiming"]="Steps";shape.style["effects.revealSteps"]=4;shape.style["effects.reveal"]=.51;const auto step=scene.textureFor(shape);shape.style["effects.reveal"]=.74;QCOMPARE(scene.textureFor(shape),step);
    }
    void graphSelectionAndCanvasZoom(){
        BuildModel model;Library library;const auto id=model.addMesh("Cube");
        for(const auto &path:QStringList{"position.x","position.y","position.z"}){model.active()->keyChannel(path,0,0);model.active()->keyChannel(path,1000,1);}
        AnimationEditor editor(&model,true);editor.resize(900,400);editor.show();QTest::qWait(30);
        auto *tree=editor.findChild<QTreeWidget*>();auto *curve=editor.findChild<QWidget*>("animationCurve");QVERIFY(tree);QVERIFY(curve);
        QTreeWidgetItem *x=nullptr,*y=nullptr;
        for(QTreeWidgetItemIterator it(tree);*it;++it){const auto path=(*it)->data(0,Qt::UserRole+1).toString();if(path=="position.x")x=*it;if(path=="position.y")y=*it;}
        QVERIFY(x);QVERIFY(y);tree->clearSelection();tree->setCurrentItem(x);x->setSelected(true);QCOMPARE(curve->property("visibleChannels").toStringList(),QStringList{id+"|position.x"});
        y->setSelected(true);tree->setCurrentItem(y,0,QItemSelectionModel::NoUpdate);QCOMPARE(curve->property("visibleChannels").toStringList().size(),2);
        tree->clearSelection();QVERIFY(curve->property("visibleChannels").toStringList().isEmpty());
        BuildCanvas canvas(&model,&library);canvas.resize(800,500);canvas.show();canvas.frameAll();QTest::qWait(30);canvas.grab();
        const auto saved=model.design.toJson();const auto before=canvas.property("compositionFrame").toRectF();QVERIFY(before.width()>0);QPointF anchor(450,250);
        const QPointF normalized((anchor.x()-before.x())/before.width(),(anchor.y()-before.y())/before.height());
        QWheelEvent wheel(anchor,canvas.mapToGlobal(anchor.toPoint()),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&canvas,&wheel);canvas.grab();
        const auto after=canvas.property("compositionFrame").toRectF();QVERIFY(after.width()<before.width());QVERIFY(qAbs((anchor.x()-after.x())/after.width()-normalized.x())<.001);QVERIFY(qAbs((anchor.y()-after.y())/after.height()-normalized.y())<.001);QCOMPARE(model.design.toJson(),saved);
        const auto spacing=canvas.property("rulerSpacing").toPointF();QVERIFY(spacing.x()>=60);QVERIFY(spacing.y()>=60);
    }
    void textAutoFitAndShadow(){
        DObject text;text.id="fit-text";text.type="Text";text.text="Cuebase";text.fontPx=32;text.w=4;text.h=2;
        text.style["text.autoFit"]=true;text.style["text.padding"]=12.;text.style["text.background"]=true;
        const auto shortText=text.evaluated(0);text.text="Cuebase production graphics";const auto longText=text.evaluated(0);
        QVERIFY(longText.w>shortText.w);QCOMPARE(longText.h,shortText.h);QCOMPARE(text.w,4.);
        text.style["text.autoFitX"]=false;const auto wrapped=text.evaluated(0);QCOMPARE(wrapped.w,4.);QVERIFY(wrapped.h>=shortText.h);
        text.style["text.autoFitY"]=false;QCOMPARE(text.evaluated(0).h,2.);
        text.style["text.autoFitX"]=true;text.style["text.autoFitY"]=true;
        ElementDesign scene;const auto original=scene.textureFor(text);text.style["textShadow.enabled"]=true;text.style["textShadow.x"]=5.;text.style["textShadow.y"]=4.;text.style["textShadow.blur"]=3.;
        const auto shadowed=scene.textureFor(text);QVERIFY(shadowed!=original);QVERIFY(shadowed.width()>original.width());
        text.style["text.background"]=false;const auto noBackground=scene.textureFor(text);text.style["textShadow.enabled"]=false;QCOMPARE(scene.textureFor(text),noBackground);
        scene.objects.append(text);const auto restored=ElementDesign::fromJson(scene.toJson());QCOMPARE(restored.find(text.id)->evaluated(0).w,text.evaluated(0).w);
    }
    void modifiedKeyReplacesBeforeRemoval(){
        BuildModel model;const auto id=model.addText();model.keyProperty(id,"text.size");const auto initial=model.active()->propertyValue("text.size",0);
        model.editProperty(id,"text.size",initial+25);QVERIFY(model.active()->overrides.contains("text.size"));
        model.togglePropertyKey(id,"text.size");QVERIFY(model.active()->track("text.size"));QCOMPARE(model.active()->track("text.size")->keys.size(),1);QCOMPARE(model.active()->propertyValue("text.size",0),initial+25);QVERIFY(!model.active()->overrides.contains("text.size"));
        model.togglePropertyKey(id,"text.size");QVERIFY(!model.active()->track("text.size"));
    }
    void modifierApplyAndPicking(){
        BuildModel m;Library lib;m.reset(ElementDesign::defaultScene());const auto id=m.addMesh("Cube");
        Modifier mirror;mirror.type="Mirror";mirror.ax=true;mirror.options["offset.x"]=2.;m.active()->mods.append(mirror);
        Modifier array;array.type="Array";array.count=2;array.rel={2,0,0};m.active()->mods.append(array);
        const Mesh before=m.design.evalMesh(*m.active(),0);QVERIFY(m.applyModifier(id,1));QVERIFY(m.active()->mods.isEmpty());QCOMPARE(m.active()->mesh.v,before.v);QCOMPARE(m.active()->mesh.f,before.f);
        QVERIFY(m.undo());QCOMPARE(m.active()->mods.size(),2);QVERIFY(m.applyModifier(id,0));QCOMPARE(m.active()->mods.size(),1);QCOMPARE(m.design.evalMesh(*m.active(),0).v,before.v);
        QVERIFY(m.undo());QVERIFY(m.enterEdit());QVERIFY(!m.applyModifier(id,0));m.exitEdit();m.active()->mods.clear();
        BuildCanvas canvas(&m,&lib);canvas.resize(800,500);canvas.show();canvas.frameAll();QTest::qWait(30);
        const auto withOrigins=canvas.grab().toImage();canvas.showOrigins=false;QVERIFY(canvas.grab().toImage()!=withOrigins);canvas.showOrigins=true;
        QString picked;canvas.pickSceneObject([&](QString object){picked=object;});QTest::keyClick(&canvas,Qt::Key_Escape);QVERIFY(picked.isEmpty());
        canvas.pickSceneObject([&](QString object){picked=object;});QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,canvas.rect().center());QCOMPARE(picked,id);QCOMPARE(m.activeId,id);
        canvas.showCamera=false;canvas.showLights=false;
        QSignalSpy feedback(&canvas,&BuildCanvas::message);const auto scene=m.design.toJson();QTest::keyClick(&canvas,Qt::Key_A,Qt::ControlModifier);QVERIFY(!feedback.isEmpty());QCOMPARE(m.design.toJson(),scene);QCOMPARE(m.sel,QSet<QString>{id});
        DObject parent;parent.id="group";parent.type="Group";parent.base.loc={3.7f,0,1.3f};m.design.objects.append(parent);m.byId(id)->parentId=parent.id;QVERIFY(m.enterEdit());
        const QRectF frame(34,44.125,732,411.75);const auto cam=m.design.sceneCamera(0,lib.display.aspect());QPointF vertex;QVERIFY(cam.toScreen(m.design.worldMatrix(*m.byId(id),0).map(m.byId(id)->mesh.v[0]),frame,&vertex));QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,vertex.toPoint());QVERIFY(!m.esel.v.isEmpty());m.exitEdit();
        auto d=ElementDesign::defaultScene();DObject shape;shape.id="triangle";shape.type="Shape";shape.style["shape"]="Triangle";shape.mat.shadeless=true;d.objects.append(shape);m.reset(d);m.select(shape.id);QVERIFY(m.enterEdit());QCOMPARE(m.active()->mesh.v.size(),3);QVERIFY(m.active()->mat.shadeless);
    }
    void independentCuePlays(){
        auto design=ElementDesign::defaultScene();design.durationMs=2000;
        QJsonObject cue{{"id","varied"},{"start",0},{"end",1000},{"repeat",3},{"hold",false}};
        cue["plays"]=QJsonArray{QJsonObject{{"hold",true},{"holdManual",false},{"holdMs",500}},QJsonObject{{"hold",false}},QJsonObject{{"hold",true},{"holdManual",false},{"holdMs",1500}}};
        design.animationCues={cue};const auto plan=compileAnimationCue(design,cue);QCOMPARE(plan.steps.size(),3);QCOMPARE(plan.duration(),qint64(5000));QCOMPARE(plan.sample(1499).step,0);QCOMPARE(plan.sample(1500).step,1);QCOMPARE(plan.sample(2500).step,2);QVERIFY(!plan.sample(5000).visible);
        auto entry=newCueBoardEntry(cue);const auto board=compileCueBoard(design,QJsonArray{entry});QCOMPARE(board.duration(),plan.duration());QCOMPARE(board.steps[0].holdMs,qint64(500));QCOMPARE(board.steps[1].holdMs,qint64(0));QCOMPARE(board.steps[2].holdMs,qint64(1500));
        auto second=entry;auto plays=second["plays"].toArray();auto timing=plays[0].toObject();timing["holdMs"]=2500;plays[0]=timing;second["plays"]=plays;QCOMPARE(compileCueBoard(design,QJsonArray{second}).duration(),qint64(7000));QCOMPARE(compileCueBoard(design,QJsonArray{entry}).duration(),qint64(5000));
        cue["repeat"]=2;cue["plays"]=QJsonArray{QJsonObject{{"hold",true},{"holdManual",true}},QJsonObject{{"hold",true},{"holdManual",true}}};design.animationCues={cue};
        BuildModel model;model.reset(design);model.previewCueId="varied";model.playing=true;model.previewCueElapsed=999;QVERIFY(!model.cuePreviewWaiting());QVERIFY(!model.continueCuePreview());
        model.previewCueElapsed=1000;QVERIFY(model.cuePreviewWaiting());QVERIFY(model.continueCuePreview());QCOMPARE(model.previewCueStep,1);QCOMPARE(model.previewCueElapsed,qint64(0));QVERIFY(model.playing);
        model.previewCueElapsed=1000;QVERIFY(model.continueCuePreview());QVERIFY(!model.playing);QVERIFY(!model.continueCuePreview());
    }
    void mirrorModifierCardPicker(){
        Library library;BuildWorkspace workspace(&library);workspace.resize(1280,900);workspace.show();auto *model=workspace.findChild<BuildModel*>();auto *canvas=workspace.findChild<BuildCanvas*>();QVERIFY(model&&canvas);model->reset(ElementDesign::defaultScene());const auto reference=model->addMesh("Cube");const auto owner=model->addMesh("Cube");model->byId(owner)->base.loc={3,0,0};Modifier mirror;mirror.type="Mirror";model->byId(owner)->mods.append(mirror);model->notify();
        auto *modifierTab=workspace.findChild<QPushButton*>("buildTab_Modifiers");QVERIFY(modifierTab);modifierTab->click();for(auto *section:workspace.findChildren<QToolButton*>())if(section->text()=="MIRROR"&&!section->isChecked())section->click();canvas->showCamera=false;canvas->showLights=false;canvas->frameAll();QTest::qWait(60);auto *pick=workspace.findChild<QPushButton*>("pickMirrorObject");QVERIFY(pick);QVERIFY(pick->isVisible());const auto original=model->design.toJson();pick->click();QTest::keyClick(canvas,Qt::Key_Escape);QCOMPARE(model->design.toJson(),original);QCOMPARE(model->activeId,owner);
        pick->click();QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,canvas->rect().center());QCOMPARE(model->byId(owner)->mods.first().target,reference);QCOMPARE(model->activeId,owner);QVERIFY(model->undo());QVERIFY(model->byId(owner)->mods.first().target.isEmpty());QVERIFY(model->redo());QCOMPARE(model->byId(owner)->mods.first().target,reference);
        const auto saved=ElementDesign::fromJson(model->design.toJson());QCOMPARE(saved.find(owner)->mods.first().target,reference);
    }
    void inspectorExpansionPerObject(){BuildModel m;Library lib;const auto first=m.addText();PropertyInspector inspector(&m,&lib);auto button=[&](QString title)->QToolButton*{QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);for(auto *b:inspector.findChildren<QToolButton*>())if(b->text()==title)return b;return nullptr;};QVERIFY(button("CORNERS"));QVERIFY(!button("CORNERS")->isChecked());button("CORNERS")->setChecked(true);const auto second=m.addText();QVERIFY(button("CORNERS"));QVERIFY(!button("CORNERS")->isChecked());m.select(first);QVERIFY(button("CORNERS")->isChecked());m.select(second);QVERIFY(!button("CORNERS")->isChecked());m.addText();QVERIFY(button("TYPOGRAPHY"));QVERIFY(button("TYPOGRAPHY")->isChecked());}

    void revealAndFrameExport(){
          FrameExportOptions fractional;QVERIFY(fractional.setProjectFrameRate(30,1.001));QCOMPARE(fractional.fps,30000);QCOMPARE(fractional.fpsBase,1001);
          QVERIFY(fractional.setProjectFrameRate(25,1.));QCOMPARE(fractional.fps,25);QCOMPARE(fractional.fpsBase,1);QVERIFY(!fractional.setProjectFrameRate(30,0.));QCOMPARE(fractional.fps,0);
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
    void markdownInspectorWorkflow(){
        Library library;BuildModel model;auto scene=ElementDesign::defaultScene();DObject text;text.id="editable-markdown";text.type="Text";text.text="Original";text.w=6;text.h=3;text.base.rot={90,0,0};text.mat.shadeless=true;text.keyChannel("opacity",0,.5);text.keyChannel("opacity",1000,1);scene.objects.append(text);model.reset(scene);model.select(text.id);
        PropertyInspector inspector(&model,&library);inspector.resize(350,900);inspector.show();auto *content=inspector.findChild<QPlainTextEdit*>("textContent");auto *format=inspector.findChild<QComboBox*>("property_text.format");QVERIFY(content&&format);
        const QString markdown="# Tonight\n**Renovation** and *design*\n- New rooms\n- New ideas";content->setPlainText("{{studio:Headline}}"+markdown);QCOMPARE(model.active()->text,markdown);QVERIFY(model.active()->style["studio.editable"].toBool());QCOMPARE(content->toPlainText(),markdown);
        format->setCurrentText("Markdown");QVERIFY(QMetaObject::invokeMethod(format,"activated",Q_ARG(int,format->currentIndex())));QCOMPARE(model.active()->style["text.format"].toString(),QString("Markdown"));QCOMPARE(model.active()->track("opacity")->keys.size(),2);
        const auto formatted=model.design.toJson();QVERIFY(model.undo());QCOMPARE(model.active()->style["text.format"].toString("Plain text"),QString("Plain text"));QVERIFY(model.redo());QCOMPARE(model.design.toJson(),formatted);
        auto restored=ElementDesign::fromJson(formatted);QString error;QVERIFY(setStudioField(restored,text.id,"# Tomorrow\n**The next chapter**",&error));QCOMPARE(restored.find(text.id)->style["text.format"].toString(),QString("Markdown"));QCOMPARE(restored.find(text.id)->track("opacity")->keys.size(),2);QCOMPARE(restored.find(text.id)->track("opacity")->keys[1].value,1.);
        QTemporaryDir output;FrameExportOptions options;options.size={320,180};options.firstFrame=12;options.endFrame=13;const auto exported=exportSceneFrames(restored,output.path(),options);QVERIFY2(exported.success,qPrintable(exported.error));QImage expected(options.size,QImage::Format_ARGB32_Premultiplied);expected.fill(Qt::transparent);{QPainter painter(&expected);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);restored.render(painter,expected.rect(),480,true,nullptr,false);}const QImage actual(output.filePath("frame_000012.png"));QVERIFY(!actual.isNull());QCOMPARE(actual.convertToFormat(QImage::Format_ARGB32_Premultiplied),expected);
    }

    void initTestCase(){
        installUiBehavior(qApp);
        QDir().mkpath("artifacts");
        QStandardPaths::setTestModeEnabled(true);
        qApp->setOrganizationName("CuebaseBuildTests");
        qApp->setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));
        applyUiTheme("Darker",false);
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
        QWidget host;host.setFocusPolicy(Qt::StrongFocus);ValueInput input(&host);input.setRange(0,100);input.setValue(25);host.show();QTest::qWait(30);
        QWheelEvent vertical(QPointF(10,10),QPointF(10,10),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&input,&vertical);QCOMPARE(input.value(),25.0);
        QWheelEvent horizontal(QPointF(10,10),QPointF(10,10),QPoint(),QPoint(120,0),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&input,&horizontal);QCOMPARE(input.value(),26.0);
        auto *line=input.findChild<QLineEdit*>();QVERIFY(line);const QPoint center=line->rect().center();QTest::mousePress(line,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(line,center+QPoint(20,0));QVERIFY(input.value()>26);
        QTest::mousePress(line,Qt::RightButton,Qt::NoModifier,center+QPoint(20,0));QTest::mouseRelease(line,Qt::RightButton,Qt::NoModifier,center+QPoint(20,0));QTest::mouseRelease(line,Qt::LeftButton,Qt::NoModifier,center+QPoint(20,0));QCOMPARE(input.value(),26.);
        QTest::mouseClick(line,Qt::LeftButton,Qt::NoModifier,center);QVERIFY(line->hasSelectedText());QTest::mouseClick(line,Qt::LeftButton,Qt::NoModifier,QPoint(5,center.y()));QVERIFY(!line->hasSelectedText());line->selectAll();QTest::keyClicks(line,"42");QTest::keyClick(line,Qt::Key_Return);QCOMPARE(input.value(),42.);QVERIFY(!line->hasSelectedText());QVERIFY(!line->hasFocus());
        input.setFocus();line->setText("54");QTest::keyClick(&input,Qt::Key_Enter);QCoreApplication::processEvents();QCOMPARE(input.value(),54.);QVERIFY(!input.hasFocus());QVERIFY(!line->hasFocus());
        QSpinBox native(&host);native.setKeyboardTracking(false);native.show();native.setFocus();auto *nativeLine=native.findChild<QLineEdit*>();nativeLine->setText("73");QTest::keyClick(&native,Qt::Key_Return);QCoreApplication::processEvents();QCOMPARE(native.value(),73);QVERIFY(!native.hasFocus());QVERIFY(!nativeLine->hasFocus());
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
    void restoredImageRefreshesTexture(){
        QTemporaryDir directory;QVERIFY(directory.isValid());DObject object;object.id="restored-image";object.type="Image";object.w=1;object.h=1;object.image=directory.filePath("image.png");ElementDesign design;
        auto texture=design.textureFor(object);QCOMPARE(texture.pixelColor(texture.width()/2,texture.height()/2).alpha(),0);
        QImage source(16,16,QImage::Format_ARGB32);source.fill(Qt::red);QVERIFY(source.save(object.image));texture=design.textureFor(object);QCOMPARE(texture.pixelColor(texture.width()/2,texture.height()/2),QColor(Qt::red));
        source.fill(Qt::blue);QVERIFY(source.save(object.image));QFile file(object.image);QVERIFY(file.open(QIODevice::ReadWrite));QVERIFY(file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(2),QFileDevice::FileModificationTime));file.close();
        texture=design.textureFor(object);QCOMPARE(texture.pixelColor(texture.width()/2,texture.height()/2),QColor(Qt::blue));QVERIFY(QFile::remove(object.image));texture=design.textureFor(object);QCOMPARE(texture.pixelColor(texture.width()/2,texture.height()/2).alpha(),0);
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
        QVERIFY(dialog->windowFlags().testFlag(Qt::FramelessWindowHint));QVERIFY(dialog->testAttribute(Qt::WA_TranslucentBackground));
        auto *repeat=dialog->findChild<QSpinBox*>("cueRepeat");QVERIFY(!repeat->isEnabled());dialog->findChild<QCheckBox*>("cueLoop")->setChecked(true);QVERIFY(repeat->isEnabled());repeat->setValue(2);
        dialog->findChild<QCheckBox*>("cueHold")->setChecked(true);dialog->findChild<QDoubleSpinBox*>("cueHoldSeconds")->setValue(2.5);dialog->findChild<QComboBox*>("cuePlaySelector")->setCurrentIndex(1);QVERIFY(!dialog->findChild<QCheckBox*>("cueHold")->isChecked());
        QVERIFY(dialog->grab().save("artifacts/animation-cue-modal.png"));
        dialog->findChild<QLineEdit*>("cueName")->setText("Exit");dialog->findChild<QSpinBox*>("cueEnd")->setValue(50);dialog->findChild<QCheckBox*>("cueHold")->setChecked(false);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QCOMPARE(model.cueRangeStart,qint64(-1));
        auto cue=model.design.animationCues.first().toObject();QCOMPARE(cue["name"].toString(),QString("Exit"));QVERIFY(!cue["hold"].toBool());QCOMPARE(cue["end"].toInt(),50*model.frameMs());QCOMPARE(ElementDesign::fromJson(model.design.toJson()).animationCues,model.design.animationCues);
        QCOMPARE(cue["repeat"].toInt(),2);QCOMPARE(compileAnimationCue(model.design,cue).duration(),qint64(6500));QCOMPARE(cue["plays"].toArray()[0].toObject()["holdMs"].toInt(),2500);
        QVERIFY(editor->grab().save("artifacts/animation-cues.png"));QVERIFY(model.undo());QVERIFY(model.design.animationCues.isEmpty());delete editor;
    }
    void infiniteCueEditingPreservesPlays(){
        BuildModel model;auto design=ElementDesign::defaultScene();QJsonObject cue{{"id","looped"},{"name","Looped cue"},{"start",0},{"end",1000},{"repeat",2},{"loop",true},{"plays",QJsonArray{QJsonObject{{"hold",true},{"holdManual",false},{"holdMs",200}},QJsonObject{{"hold",true},{"holdManual",false},{"holdMs",700}}}}};design.animationCues={cue};model.reset(design);
        std::unique_ptr<QWidget> editor(animationCueEditor(&model));editor->resize(520,700);editor->show();QTest::qWait(30);auto *list=editor->findChild<QListWidget*>("buildCueList");QVERIFY(list);QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(0)).center());
        auto *edit=editor->findChild<QPushButton*>("editSelectedCue");QVERIFY(edit);edit->click();auto *dialog=editor->findChild<QDialog*>("cueRangeDialog");QVERIFY(dialog);auto *count=dialog->findChild<QSpinBox*>("cueRepeat");auto *plays=dialog->findChild<QComboBox*>("cuePlaySelector");QCOMPARE(count->value(),0);QCOMPARE(plays->count(),2);QVERIFY(dialog->findChild<QCheckBox*>("cueLoop")->isChecked());
        plays->setCurrentIndex(1);QCOMPARE(dialog->findChild<QDoubleSpinBox*>("cueHoldSeconds")->value(),.7);dialog->findChild<QDoubleSpinBox*>("cueHoldSeconds")->setValue(.9);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        auto saved=model.design.animationCues.first().toObject();QVERIFY(saved["loop"].toBool());QCOMPARE(saved["repeat"].toInt(),2);QCOMPARE(compileAnimationCue(model.design,saved).duration(),qint64(-1));QCOMPARE(saved["plays"].toArray()[0].toObject()["holdMs"].toInt(),200);QCOMPARE(saved["plays"].toArray()[1].toObject()["holdMs"].toInt(),900);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);edit->click();dialog=editor->findChild<QDialog*>("cueRangeDialog");QVERIFY(dialog);count=dialog->findChild<QSpinBox*>("cueRepeat");count->setValue(3);QCOMPARE(dialog->findChild<QComboBox*>("cuePlaySelector")->count(),3);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();saved=model.design.animationCues.first().toObject();QVERIFY(!saved["loop"].toBool());QCOMPARE(saved["repeat"].toInt(),3);QCOMPARE(compileAnimationCue(model.design,saved).duration(),qint64(4100));
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
