#include "SceneEffects.h"
#include "BuildModel.h"
#include "UiControls.h"
#include "SurfaceEffects.h"
#include "MediaFrames.h"
#include "Data.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QSignalBlocker>
#include <cmath>
#include <memory>

const QVector<SceneEffectControl> &sceneEffectControls(){
    static const QVector<SceneEffectControl> controls{
        {"Transition","Dissolve","scene.mix",0,1,0,.01},
        {"Transform","Zoom","scene.zoom",.1,5,1,.01},
        {"Transform","Horizontal offset","scene.x",-1,1,0,.01},
        {"Transform","Vertical offset","scene.y",-1,1,0,.01},
        {"Transform","Rotation","scene.rotation",-180,180,0,1},
        {"Transform","Pivot X","scene.pivotX",0,1,.5,.01},
        {"Transform","Pivot Y","scene.pivotY",0,1,.5,.01},
        {"Color","Exposure (stops)","scene.exposure",-5,5,0,.05},
        {"Color","Contrast","scene.contrast",0,3,1,.01},
        {"Color","Saturation","scene.saturation",0,2,1,.01},
        {"Color","Temperature","scene.temperature",-1,1,0,.01},
        {"Color","Tint","scene.tint",-1,1,0,.01},
        {"Optics","Blur (px)","scene.blur",0,40,0,.5},
        {"Optics","Vignette","scene.vignette",0,1,0,.01},
        {"Optics","Chromatic offset (px)","scene.chromatic",0,16,0,.25},
        {"Texture","Film grain","scene.grain",0,.3,0,.005},
        {"Texture","Scanlines","scene.scanlines",0,.5,0,.01},
        {"Motion","Pulse amplitude","scene.pulse",0,.3,0,.005},
        {"Motion","Pulse frequency (Hz)","scene.frequency",.1,10,1,.1},
        {"Motion","Shake (screen fraction)","scene.shake",0,.08,0,.001},
        {"Timing","Playback speed","scene.speed",.1,2,1,.05}
    };return controls;
}
DObject *sceneEffect(ElementDesign &design){for(auto &o:design.objects)if(o.type=="Effect")return &o;return nullptr;}
const DObject *sceneEffect(const ElementDesign &design){for(const auto &o:design.objects)if(o.type=="Effect")return &o;return nullptr;}
double sceneEffectValue(const ElementDesign &design,const QString &key,qint64 time,double fallback){
    const auto *o=sceneEffect(design);if(!o||(!o->style.contains(key)&&!o->track(key)))return fallback;
    const qint64 at=key!="scene.mix"&&design.loop&&design.durationMs>0?qMax<qint64>(0,time)%design.durationMs:qBound(qint64(0),time,qint64(design.durationMs));
    double value=o->propertyValue(key,at);return std::isfinite(value)?value:fallback;
}
QStringList sceneEffectPresets(){return {"Fade","Zoom reveal","Screen breathing","Pulse","Desaturate and slow","Flash","Soft focus","Chromatic impact"};}
ElementDesign sceneEffectPreset(const QString &name){
    auto d=ElementDesign::defaultScene();d.loop=false;d.durationMs=1200;d.world.transparent=true;
    d.objects.removeIf([](const DObject &o){return o.type!="Camera";});
    DObject o;o.id=Library::newId();o.type="Effect";o.name="Screen effects";
    for(const auto &c:sceneEffectControls())o.style[c.key]=c.initial;
    o.keyChannel("scene.mix",0,0);o.keyChannel("scene.mix",d.durationMs,1);
    auto key=[&](QString path,double a,double b,double c){o.keyChannel(path,0,a);o.keyChannel(path,d.durationMs/2,b);o.keyChannel(path,d.durationMs,c);};
    if(name=="Zoom reveal"){key("scene.zoom",1,1.45,1);key("scene.blur",0,9,0);}
    else if(name=="Screen breathing"){d.durationMs=3000;d.loop=true;o.style["scene.pulse"]=.045;o.style["scene.frequency"]=.65;}
    else if(name=="Pulse"){o.style["scene.pulse"]=.1;o.style["scene.frequency"]=2.5;}
    else if(name=="Desaturate and slow"){o.keyChannel("scene.saturation",0,1);o.keyChannel("scene.saturation",1200,0);o.keyChannel("scene.speed",0,1);o.keyChannel("scene.speed",1200,.35);key("scene.zoom",1,1.08,1.12);o.style["scene.vignette"]=.55;o.style["scene.grain"]=.025;}
    else if(name=="Flash"){key("scene.exposure",0,3,0);key("scene.blur",0,6,0);}
    else if(name=="Soft focus"){key("scene.blur",0,18,0);key("scene.vignette",0,.45,0);}
    else if(name=="Chromatic impact"){key("scene.chromatic",0,10,0);key("scene.shake",0,.025,0);key("scene.contrast",1,1.5,1);}
    o.captureDefaults();d.objects.append(o);return d;
}
QImage sceneEffectSample(QSize size,bool alternate){
    QImage image(size,QImage::Format_ARGB32);image.fill(QColor("#18232b"));QPainter p(&image);
    QLinearGradient gradient(0,0,size.width(),size.height());gradient.setColorAt(0,alternate?QColor("#634764"):QColor("#1e657b"));gradient.setColorAt(1,QColor("#18232b"));p.fillRect(image.rect(),gradient);
    p.setPen(QPen(QColor(255,255,255,45),1));for(int x=0;x<size.width();x+=qMax(1,size.width()/12))p.drawLine(x,0,x,size.height());for(int y=0;y<size.height();y+=qMax(1,size.height()/7))p.drawLine(0,y,size.width(),y);
    p.setPen(Qt::white);QFont font("Segoe UI");font.setPixelSize(qMax(10,size.height()/12));font.setBold(true);p.setFont(font);p.drawText(image.rect(),Qt::AlignCenter,alternate?"REPLAY":"PROGRAM");return image;
}
QImage applySceneEffect(const QImage &source,const ElementDesign &design,qint64 time,bool layers){
    if(source.isNull())return {};const auto *effect=sceneEffect(design);if(!effect)return source;
    auto v=[&](const char *key,double fallback=0.){return sceneEffectValue(design,key,time,fallback);};
    QImage image=source.convertToFormat(QImage::Format_ARGB32);const int width=image.width(),height=image.height();
    const double exposure=std::pow(2.,v("scene.exposure")),contrast=v("scene.contrast",1),saturation=v("scene.saturation",1),temperature=v("scene.temperature"),tint=v("scene.tint"),vignette=v("scene.vignette"),grain=v("scene.grain"),scan=v("scene.scanlines");
    if(exposure!=1||contrast!=1||saturation!=1||temperature||tint||vignette||grain||scan){
        for(int y=0;y<height;++y){auto *row=reinterpret_cast<QRgb*>(image.scanLine(y));for(int x=0;x<width;++x){auto pixel=row[x];double r=qRed(pixel)/255.,g=qGreen(pixel)/255.,b=qBlue(pixel)/255.;double l=.2126*r+.7152*g+.0722*b;
            const double nx=(x-width*.5)/qMax(1.,width*.5),ny=(y-height*.5)/qMax(1.,height*.5);const double shade=1-vignette*qBound(0.,(nx*nx+ny*ny)*.65,1.);
            const quint32 hash=quint32(x)*374761393u+quint32(y)*668265263u+quint32(time/40)*2246822519u;const double noise=((hash^(hash>>13))%1024/1023.-.5)*grain;
            auto channel=[&](double c,double offset){return qBound(0,qRound(((((l+(c-l)*saturation)*exposure-.5)*contrast+.5+offset)*shade+noise-((y&1)?scan:0))*255),255);};
            row[x]=qRgba(channel(r,temperature*.12),channel(g,tint*.12),channel(b,-temperature*.12),qAlpha(pixel));}}
    }
    const int chromatic=qRound(v("scene.chromatic")*width/1920.);if(chromatic){const QImage copy=image;for(int y=0;y<height;++y){auto *row=reinterpret_cast<QRgb*>(image.scanLine(y));for(int x=0;x<width;++x)row[x]=qRgba(qRed(copy.pixel(qBound(0,x+chromatic,width-1),y)),qGreen(row[x]),qBlue(copy.pixel(qBound(0,x-chromatic,width-1),y)),qAlpha(row[x]));}}
    const double blur=v("scene.blur")*width/1920.;if(blur>.1){auto small=image.scaled(qMax(1,width/2),qMax(1,height/2),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);image=backdropImage(small,qRound(blur/2),1).scaled(image.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);}
    const double phase=time/1000.*6.28318530718*v("scene.frequency",1),zoom=v("scene.zoom",1)*(1+v("scene.pulse")*std::sin(phase)),shake=v("scene.shake");
    if(zoom!=1||shake||v("scene.x")||v("scene.y")||v("scene.rotation")){
        QImage transformed(image.size(),image.format());transformed.fill(Qt::black);QPainter p(&transformed);p.setRenderHint(QPainter::SmoothPixmapTransform);QPointF pivot(width*v("scene.pivotX",.5),height*v("scene.pivotY",.5));p.translate(pivot+QPointF(width*(v("scene.x")+shake*std::sin(phase*7.1)),height*(v("scene.y")+shake*std::sin(phase*9.3))));p.rotate(v("scene.rotation"));p.scale(zoom,zoom);p.translate(-pivot);p.drawImage(0,0,image);p.end();image=transformed;
    }
    if(layers){QPainter p(&image);design.render(p,image.rect(),time,true,nullptr,design.loop);}return image;
}
QImage sceneEffectPreview(const ElementDesign &design,QSize size,qint64 time){
    QImage source;if(const auto *o=sceneEffect(design))source.load(o->style["scene.preview"].toString());
    if(source.isNull())source=sceneEffectSample(size);else source=source.scaled(size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    return applySceneEffect(source,design,time);
}
QImage blendSceneEffect(const QImage &from,const QImage &to,const ElementDesign &design,qint64 time){
    if(from.isNull())return applySceneEffect(to,design,time);QImage result=from.convertToFormat(QImage::Format_ARGB32);QPainter p(&result);p.setOpacity(qBound(0.,sceneEffectValue(design,"scene.mix",qMin<qint64>(time,design.durationMs),design.durationMs?double(time)/design.durationMs:1),1.));p.drawImage(result.rect(),to);p.end();return applySceneEffect(result,design,time);
}
QWidget *sceneEffectEditor(BuildModel *model){
    auto *page=new QWidget;auto *layout=new QVBoxLayout(page);layout->setContentsMargins(8,8,8,8);
    auto *heading=new QLabel("SCREEN EFFECTS");layout->addWidget(heading);auto *hint=new QLabel("Effects transform the program picture. Add optional graphics with Shift+A. Diamonds animate any control in the graph.");hint->setWordWrap(true);layout->addWidget(hint);
    auto *preset=new QComboBox;preset->addItems(sceneEffectPresets());auto *apply=new QPushButton(uiIcon("composition"),"Apply preset");layout->addWidget(preset);layout->addWidget(apply);
    QObject::connect(apply,&QPushButton::clicked,page,[=]{model->pushUndo();auto next=sceneEffectPreset(preset->currentText());const auto controller=*sceneEffect(next);model->design.objects.removeIf([](const DObject &o){return o.type=="Effect";});model->design.objects.append(controller);model->design.durationMs=next.durationMs;model->design.loop=next.loop;model->select(controller.id);model->notify();});
    auto *preview=new QPushButton(uiIcon("image"),"Preview source…");preview->setToolTip("Choose a preview image; it is never included in the effect's broadcast output");layout->addWidget(preview);
    QObject::connect(preview,&QPushButton::clicked,page,[=]{auto file=QFileDialog::getOpenFileName(page,"Effect preview source",{},"Images (*.png *.jpg *.jpeg *.bmp *.webp)");if(file.isEmpty())return;if(auto *o=sceneEffect(model->design)){model->pushUndo();o->style["scene.preview"]=file;model->notify();}});
    auto *loop=new QCheckBox("Repeat motion while replay is on screen");layout->addWidget(loop);QObject::connect(loop,&QCheckBox::toggled,page,[=](bool value){if(model->design.loop==value)return;model->pushUndo();model->design.loop=value;model->notify();});
    struct Row{SceneEffectControl control;ValueInput *input;QToolButton *key;};auto rows=std::make_shared<QVector<Row>>();QString group;QFormLayout *form=nullptr;
    for(const auto &control:sceneEffectControls()){
        if(group!=control.group){group=control.group;auto *label=new QLabel(group);label->setStyleSheet("font-weight:600;margin-top:8px;");layout->addWidget(label);form=new QFormLayout;form->setContentsMargins(10,0,0,0);layout->addLayout(form);}
        auto *row=new QWidget;auto *line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);auto *input=new ValueInput;input->setRange(control.minimum,control.maximum);input->setDecimals(3);input->setSingleStep(control.step);input->setToolTip(control.label);input->setObjectName(control.key);auto *key=new QToolButton;key->setIcon(uiIcon("key"));key->setToolTip("Insert / remove key at current frame");auto *reset=new QToolButton;reset->setIcon(uiIcon("reset"));reset->setToolTip("Reset "+control.label);line->addWidget(input,1);line->addWidget(key);line->addWidget(reset);form->addRow(control.label,row);rows->append({control,input,key});
        QObject::connect(input,qOverload<double>(&QDoubleSpinBox::valueChanged),page,[=](double value){if(auto *o=sceneEffect(model->design))model->editProperty(o->id,control.key,value);});
        QObject::connect(key,&QToolButton::clicked,page,[=]{if(auto *o=sceneEffect(model->design)){model->select(o->id);model->togglePropertyKey(o->id,control.key);}});
        QObject::connect(reset,&QToolButton::clicked,page,[=]{if(auto *o=sceneEffect(model->design))model->resetProperty(o->id,control.key);});
    }
    auto refresh=[=]{const auto *o=sceneEffect(model->design);preview->setEnabled(o);{QSignalBlocker block(loop);loop->setChecked(model->design.loop);}for(auto &row:*rows){QSignalBlocker block(row.input);row.input->setEnabled(o);row.key->setEnabled(o);row.input->setValue(o?o->propertyValue(row.control.key,model->t):row.control.initial);const bool keyed=o&&model->propertyIsKeyed(*o,row.control.key,model->t);row.input->setStyleSheet(keyed?"background:#73662f;":o&&o->track(row.control.key)?"background:#3f6336;":"");row.key->setStyleSheet(keyed?"background:#786935;":"");}};
    QObject::connect(model,&BuildModel::changed,page,refresh);QObject::connect(model,&BuildModel::timeChanged,page,refresh);refresh();layout->addStretch();return page;
}
