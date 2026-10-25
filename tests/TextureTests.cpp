#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <memory>
#include "TextureLibrary.h"
#include "BuildCanvas.h"
#include "BuildModel.h"
#include "Data.h"
#include "DesignAssets.h"
#include "FrameExport.h"
#include "UiTheme.h"

class TextureTests:public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QString imagePath,profileName;
    DObject object() const {DObject o;o.id="textured";o.type="Shape";o.name="Brand surface";o.w=6;o.h=4;o.base.rot={90,0,0};o.mat.shadeless=true;o.mat.color=Qt::white;return o;}
    ElementDesign scene() const {auto d=ElementDesign::defaultScene();d.world.transparent=false;d.world.color=QColor("#393939");d.objects.append(object());return d;}
private slots:
    void initTestCase(){
        QVERIFY(temporary.isValid());QStandardPaths::setTestModeEnabled(true);profileName="CuebaseTextureTests-"+QUuid::createUuid().toString(QUuid::WithoutBraces);QCoreApplication::setApplicationName(profileName);applyUiTheme("Studio Blue",false);
        QImage image(80,60,QImage::Format_ARGB32);image.fill(QColor("#d8ae59"));QPainter painter(&image);painter.fillRect(0,0,40,30,QColor("#114466"));painter.fillRect(40,30,40,30,QColor("#bc6342"));painter.end();imagePath=temporary.filePath("woven.png");QVERIFY(image.save(imagePath));
    }
    void importMetadataAndReopen(){
        Library library;library.production={};QString error;const auto id=TextureLibrary::importFile(library,imagePath,&error);QVERIFY2(!id.isEmpty(),qPrintable(error));
        const auto path=TextureLibrary::file(TextureLibrary::entry(library,id));QVERIFY(path!=imagePath);QVERIFY(QFileInfo::exists(path));QCOMPARE(QImage(path),QImage(imagePath));
        QVERIFY(TextureLibrary::update(library,id,"Warm woven","Brand / Gold",true,&error));Library reopened;reopened.load();const auto entry=TextureLibrary::entry(reopened,id);QCOMPARE(entry["name"].toString(),QString("Warm woven"));QCOMPARE(entry["collection"].toString(),QString("Brand / Gold"));QVERIFY(entry["favorite"].toBool());QVERIFY(!TextureLibrary::thumbnail(entry).isNull());
        const auto before=TextureLibrary::entries(library);QVERIFY(TextureLibrary::importFile(library,temporary.filePath("absent.png"),&error).isEmpty());QCOMPARE(TextureLibrary::entries(library),before);
        QVERIFY(TextureLibrary::remove(library,id,&error));QVERIFY(TextureLibrary::entry(library,id).isEmpty());QVERIFY(QFileInfo::exists(path));QVERIFY(QFileInfo::exists(imagePath));
    }
    void panelApplySelectionUndoAndFilters(){
        Library library;library.production={};QString error;const auto id=TextureLibrary::importFile(library,imagePath,&error);QVERIFY(!id.isEmpty());const auto other=TextureLibrary::importFile(library,imagePath,&error);QVERIFY(!other.isEmpty());
        BuildModel model;model.reset(scene());auto locked=object();locked.id="locked";locked.locked=true;model.design.objects.append(locked);model.setSelection({"textured","locked"},"textured");
        std::unique_ptr<QWidget> panel(TextureLibrary::createPanel(&model,&library));panel->resize(350,660);panel->show();QTest::qWait(20);
        auto *list=panel->findChild<QListWidget*>("textureItems");QVERIFY(list);QCOMPARE(list->count(),2);list->setCurrentRow(0);
        panel->findChild<QLineEdit*>("textureName")->setText("Ocean fabric");panel->findChild<QLineEdit*>("textureCollection")->setText("Studio");panel->findChild<QCheckBox*>("textureFavorite")->setChecked(true);panel->findChild<QPushButton*>("textureSaveDetails")->click();
        panel->findChild<QCheckBox*>("textureFavoritesOnly")->setChecked(true);QCOMPARE(list->count(),1);panel->findChild<QLineEdit*>("textureSearch")->setText("ocean");QCOMPARE(list->count(),1);panel->findChild<QLineEdit*>("textureSearch")->setText("missing query");QCOMPARE(list->count(),0);panel->findChild<QLineEdit*>("textureSearch")->clear();list->setCurrentRow(0);
        auto *filter=panel->findChild<QComboBox*>("textureCollectionFilter");filter->setCurrentText("Studio");QCOMPARE(list->count(),1);list->setCurrentRow(0);
        const auto before=model.design.toJson();panel->findChild<QPushButton*>("textureApply")->click();QVERIFY(!model.byId("textured")->style["texture.file"].toString().isEmpty());QVERIFY(model.byId("locked")->style["texture.file"].toString().isEmpty());const auto after=model.design.toJson();QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);QVERIFY(model.redo());QCOMPARE(model.design.toJson(),after);
        QDir().mkpath("artifacts");QVERIFY(panel->grab().save("artifacts/texture-library-panel.png"));
    }
    void presetsPreserveContentAnimationAndPortability(){
        Library library;library.production={};BuildModel model;model.reset(scene());model.select("textured");QString error;const auto texture=TextureLibrary::importFile(library,imagePath,&error);QVERIFY(TextureLibrary::apply(model,library,texture,{},&error));
        auto *source=model.active();source->style["texture.rotation"]=25;source->style["texture.scaleX"]=.4;source->style["texture.mapping"]="Tile";source->style["texture.tint"]="#72aaff";source->mat.alpha=.75;source->text="Not preset content";source->keyChannel("position.x",0,4);
        const auto preset=TextureLibrary::savePreset(library,*source,"Woven blue",&error);QVERIFY2(!preset.isEmpty(),qPrintable(error));const auto exported=temporary.filePath("woven.cuemat");QVERIFY2(TextureLibrary::exportEntry(library,preset,exported,&error),qPrintable(error));
        QFile file(exported);QVERIFY(file.open(QIODevice::ReadOnly));auto portable=ElementDesign::fromJson(QString::fromUtf8(file.readAll()));QCOMPARE(designAssets(portable).size(),1);QVERIFY(QFileInfo(designAssets(portable).first().path).isRelative());resolveDesignAssets(portable,temporary.path());QVERIFY(missingDesignAsset(portable).isEmpty());
        const auto imported=TextureLibrary::importFile(library,exported,&error);QVERIFY2(!imported.isEmpty(),qPrintable(error));
        DObject text=object();text.id="title";text.type="Text";text.text="My own headline";text.fontPx=64;text.base.loc={1,2,3};text.keyChannel("position.x",0,9);model.design.objects.append(text);model.select(text.id);const auto before=model.design.toJson();
        QVERIFY(TextureLibrary::apply(model,library,imported,{},&error));QCOMPARE(model.active()->text,text.text);QCOMPARE(model.active()->base.loc,text.base.loc);QCOMPARE(model.active()->tracks.first().keys.first().value,9.);QCOMPARE(model.active()->style["texture.rotation"].toDouble(),25.);QCOMPARE(model.active()->mat.alpha,.75);QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
        QVERIFY(TextureLibrary::remove(library,preset,&error));QVERIFY(!QImage(designAssets(portable).first().path).isNull());
    }
    void materialBlendPixelsAndSolidPreset(){
        Library library;library.production={};QString error;const auto id=TextureLibrary::importFile(library,imagePath,&error);BuildModel model;model.reset(scene());model.select("textured");QVERIFY(TextureLibrary::apply(model,library,id,{},&error));auto *surface=model.active();surface->mat.color=QColor(128,160,64);
        const QColor source(216,174,89),base=surface->mat.color;auto pixel=[&]{return model.design.textureFor(*surface).pixelColor(450,100);};QCOMPARE(pixel(),source);
        for(const auto &blend:QStringList{"Multiply","Screen","Overlay"}){
            surface->style["texture.blend"]=blend;const auto actual=pixel();for(int channel=0;channel<3;++channel){const int s=channel==0?source.red():channel==1?source.green():source.blue(),b=channel==0?base.red():channel==1?base.green():base.blue();const int expected=blend=="Multiply"?qRound(s*b/255.):blend=="Screen"?255-qRound((255-s)*(255-b)/255.):b<128?qRound(2*s*b/255.):255-qRound(2*(255-s)*(255-b)/255.);const int value=channel==0?actual.red():channel==1?actual.green():actual.blue();QVERIFY2(qAbs(value-expected)<=2,qPrintable(blend));}
        }
        surface->style["texture.blend"]="Normal";surface->style["texture.tint"]="#80ffffff";const auto translucent=pixel();QVERIFY(qAbs(translucent.red()-qRound((source.red()+base.red())/2.))<=2);surface->style.remove("texture.file");surface->style["fill.kind"]="Linear";surface->style["background.type"]="Solid";const auto plain=model.design.textureFor(*surface);
        const auto preset=TextureLibrary::savePreset(library,*surface,"Solid with retained gradient settings",&error);QVERIFY(!preset.isEmpty());surface->style["background.type"]="Gradient";QVERIFY(TextureLibrary::apply(model,library,preset,{},&error));QCOMPARE(model.active()->style["background.type"].toString(),QString("Solid"));QCOMPARE(model.design.textureFor(*model.active()),plain);
    }
    void missingRepairAndSaveRollback(){
        Library library;library.production={};QString error;const auto id=TextureLibrary::importFile(library,imagePath,&error);QVERIFY(!id.isEmpty());const auto original=TextureLibrary::file(TextureLibrary::entry(library,id));BuildModel model;model.reset(scene());model.select("textured");QVERIFY(TextureLibrary::apply(model,library,id,{},&error));QVERIFY(QFile::remove(original));
        QVERIFY(!missingDesignAsset(model.design).isEmpty());const auto before=model.design.toJson();QVERIFY(!TextureLibrary::apply(model,library,id,{},&error));QCOMPARE(model.design.toJson(),before);QVERIFY(!TextureLibrary::exportEntry(library,id,temporary.filePath("missing.cuemat"),&error));
        QVERIFY(TextureLibrary::relink(library,id,imagePath,&error));const auto replacement=TextureLibrary::file(TextureLibrary::entry(library,id));QVERIFY(QFileInfo::exists(replacement));QVERIFY(relinkDesignAsset(model.design,original,replacement,&error));QVERIFY(missingDesignAsset(model.design).isEmpty());
        const auto production=library.production;const auto root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);const auto metadata=root+"/cuebase.json";QVERIFY(QFile::remove(metadata));QVERIFY(QDir().mkdir(metadata));const auto count=QDir(root+"/textures").entryList(QDir::Files).size();
        QVERIFY(TextureLibrary::importFile(library,imagePath,&error).isEmpty());QCOMPARE(library.production,production);QCOMPARE(QDir(root+"/textures").entryList(QDir::Files).size(),count);QVERIFY(!TextureLibrary::update(library,id,"Must rollback","Changed",true,&error));QCOMPARE(library.production,production);QVERIFY(QDir().rmdir(metadata));QVERIFY(library.saveChecked(&error));
    }
    void renderingControlsAnimationAndExport(){
        Library library;library.production={};QString error;const auto id=TextureLibrary::importFile(library,imagePath,&error);BuildModel model;model.reset(scene());model.select("textured");BuildCanvas canvas(&model,&library);canvas.resize(708,428);canvas.showOverlays=false;canvas.setProperty("previewQuality","Full");canvas.setShading(BuildCanvas::Solid);canvas.show();QTest::qWait(20);const auto work=canvas.grab().toImage();QVERIFY(TextureLibrary::apply(model,library,id,{},&error));QCOMPARE(canvas.grab().toImage(),work);
        auto *o=model.active();const auto baseline=model.design.textureFor(*o);o->style["texture.rotation"]=35;QVERIFY(model.design.textureFor(*o)!=baseline);o->style["texture.rotation"]=0;o->style["texture.opacity"]=0;const auto plain=model.design.textureFor(*o);QVERIFY(plain!=baseline);o->style["texture.opacity"]=1;o->style["texture.mapping"]="Tile";QVERIFY(model.design.textureFor(*o)!=baseline);o->style["texture.mapping"]="Fit";o->keyChannel("texture.offsetX",0,0);o->keyChannel("texture.offsetX",1000,80);QVERIFY(model.design.textureFor(o->evaluated(0))!=model.design.textureFor(o->evaluated(1000)));
        canvas.setShading(BuildCanvas::Rendered);model.t=1000;const auto final=canvas.grab().toImage().copy(34,34,640,360).convertToFormat(QImage::Format_ARGB32_Premultiplied);FrameExportOptions options;options.size={640,360};options.firstFrame=25;options.endFrame=26;const auto output=temporary.filePath("frames");const auto result=exportSceneFrames(ElementDesign::fromJson(model.design.toJson()),output,options);QVERIFY2(result.success,qPrintable(result.error));QCOMPARE(QImage(output+"/frame_000025.png").convertToFormat(final.format()),final);QVERIFY(final.save("artifacts/texture-final-frame.png"));
        o=model.active();o->type="Text";o->text="CUEBASE";o->fontPx=72;const auto glyphs=model.design.textureFor(*o);o->style["texture.enabled"]=false;QVERIFY(glyphs!=model.design.textureFor(*o));
    }
    void canvasDropAndLockedTarget(){
        Library library;library.production={};QString error;const auto id=TextureLibrary::importFile(library,imagePath,&error);BuildModel model;model.reset(scene());BuildCanvas canvas(&model,&library);canvas.resize(708,428);canvas.show();QTest::qWait(20);QMimeData mime;mime.setData(TextureLibrary::mimeType,id.toUtf8());
        const auto before=model.design.toJson();QDragEnterEvent enter(QPoint(354,214),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&canvas,&enter);QVERIFY(enter.isAccepted());QDropEvent drop(QPointF(354,214),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&canvas,&drop);QVERIFY(drop.isAccepted());QVERIFY(!model.byId("textured")->style["texture.file"].toString().isEmpty());QVERIFY(model.undo());QCOMPARE(model.design.toJson(),before);
        model.byId("textured")->locked=true;QVERIFY(!TextureLibrary::apply(model,library,id,"textured",&error));QVERIFY(model.byId("textured")->style["texture.file"].toString().isEmpty());
    }
    void cleanupTestCase(){const auto path=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QVERIFY(QFileInfo(path).fileName()==profileName);QVERIFY(QDir(path).removeRecursively());}
};
QTEST_MAIN(TextureTests)
#include "TextureTests.moc"
