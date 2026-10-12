#include "UiControls.h"
#include "PropertyInspector.h"
#include "ElementLibrary.h"
#include "BuildModel.h"
#include "Data.h"
#include "MediaFrames.h"
#include <QCheckBox>
#include "ColorPicker.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QSettings>

PropertyInspector::PropertyInspector(BuildModel *model,Library *library,QWidget *parent):QWidget(parent),m(model),lib(library){
    layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->setSpacing(3);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    setStyleSheet("QDoubleSpinBox,QComboBox,QLineEdit{min-height:18px;padding:1px 3px;} QToolButton{border:0;text-align:left;padding:3px;} QToolButton:checked{background:#383838;} QPushButton{padding:2px;border-radius:3px;} QLabel{font-size:10px;}");
    connect(m,&BuildModel::selectionChanged,this,[this]{rebuild();});
    connect(m,&BuildModel::changed,this,[this]{rebuild();refresh();});
    connect(m,&BuildModel::timeChanged,this,[this]{refresh();});
    rebuild();
}
void PropertyInspector::apply(const std::function<void()> &operation){if(syncing)return;m->pushUndo();operation();m->notify();}
QFormLayout *PropertyInspector::section(const QString &title,bool open){
    const auto *object=m->active();const QString scope=(object?object->id:QString("scene"))+"/"+title;
    const bool defaultOpen=title=="TRANSFORM"||(title=="TYPOGRAPHY"&&object&&object->type=="Text");
    open=expanded.value(scope,QSettings().value("inspector/objects/"+scope,defaultOpen).toBool());
    auto *button=new QToolButton;button->setText(title);button->setCheckable(true);button->setChecked(open);button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);button->setIcon(uiIcon(open?"down":"right"));button->setIconSize(QSize(12,12));button->setToolTip("Expand / collapse "+title);
    button->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);layout->addWidget(button);
    auto *body=new QWidget;auto *form=new QFormLayout(body);form->setContentsMargins(18,2,5,8);form->setSpacing(4);form->setSizeConstraint(QLayout::SetMinimumSize);body->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);body->setVisible(open);layout->addWidget(body);
    connect(button,&QToolButton::toggled,body,[=, this](bool visible){expanded[scope]=visible;QSettings().setValue("inspector/objects/"+scope,visible);body->setVisible(visible);button->setIcon(uiIcon(visible?"down":"right"));});return form;
}
void PropertyInspector::number(QFormLayout *form,const QString &label,const QString &path,double low,double high,double step,const QString &suffix){
    auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);h->setSpacing(2);
    auto *spin=new ValueInput;spin->setRange(low,high);spin->setSingleStep(step);spin->setDecimals(step<1?3:1);spin->setSuffix(suffix);spin->setKeyboardTracking(false);spin->setObjectName("property_"+path);
    auto *key=new QPushButton("◇");key->setFixedWidth(23);key->setToolTip("Toggle this property's key at the playhead");key->setObjectName("key_"+path);
    auto *reset=new QPushButton;reset->setIcon(uiIcon("reset"));reset->setObjectName("reset_"+path);reset->setFixedWidth(20);reset->setToolTip("Reset to the original default value");
    h->addWidget(spin,1);h->addWidget(key);h->addWidget(reset);form->addRow(label,row);
    connect(spin,&QDoubleSpinBox::valueChanged,this,[=, this](double v){
        if(syncing)return;m->pushUndo();
        for(auto *o:m->inspected()) {
            if (path == "position.x" || path == "position.z") {
                auto p=o->at(m->t);const auto basis=m->design.parentMatrix(*o,m->t);const auto world=basis.map(p.loc); const auto camera=m->design.sceneCamera(m->t,lib->display.aspect());
                const QRectF frame(0,0,lib->display.resX(),lib->display.resY());QPointF pixel;
                if (!camera.toScreen(world,frame,&pixel)) continue;
                if (path=="position.x")pixel.setX(v);else pixel.setY(v);
                QVector3D origin,direction;camera.ray(pixel,frame,&origin,&direction);
                const auto normal=camera.forward();double divisor=QVector3D::dotProduct(normal,direction);
                if(qAbs(divisor)>.000001)p.loc=basis.inverted().map(origin+direction*float(QVector3D::dotProduct(normal,world-origin)/divisor));
                m->setProps(o->id,p);
            } else m->editProperty(o->id,path,v);
        }
        m->notify();
    });
    connect(key,&QPushButton::clicked,this,[=, this]{for(auto *o:m->inspected())m->togglePropertyKey(o->id,path);});
    connect(reset,&QPushButton::clicked,this,[=, this]{for(auto *o:m->inspected())m->resetProperty(o->id,path);});
    sync.append([=, this]{
        auto *o=m->active();if(!o)return;
        auto displayed=[=, this](DObject *object){
            if(path=="position.x"||path=="position.z"){
                QPointF pixel;const auto camera=m->design.sceneCamera(m->t,lib->display.aspect());
                camera.toScreen(m->design.worldMatrix(*object,m->t).map(QVector3D()),QRectF(0,0,lib->display.resX(),lib->display.resY()),&pixel);
                return path=="position.x"?pixel.x():pixel.y();
            }
            return object->propertyValue(path,m->t);
        };
        double value=displayed(o);bool mixed=false;
        for(auto *other:m->inspected())mixed=mixed||qAbs(displayed(other)-value)>.00001;
        spin->setValue(value);
        if(mixed){auto *line=spin->findChild<QLineEdit*>();line->clear();line->setPlaceholderText("Mixed");}
        const bool keyed=m->propertyIsKeyed(*o,path,m->t),animated=o->track(path)!=nullptr;
        const bool modified=keyed&&o->overrides.contains(path)&&qAbs(o->propertyValue(path,m->t)-o->track(path)->evaluate(m->t,o->propertyBase(path)))>.000001;
        spin->setStyleSheet(QString("QDoubleSpinBox{background:%1;color:#eeeeee;border-radius:3px;}").arg(keyed?"#79722c":animated?"#426b29":"#454545"));
        key->setText(keyed?"◆":"◇");key->setStyleSheet(keyed?"color:#fbbf24;":animated?"color:#60a5fa;":"color:#64748b;");
        if(modified){key->setText("◈");key->setStyleSheet("color:#ff9f43;");spin->setStyleSheet("QDoubleSpinBox{background:#87552c;color:white;border-radius:3px;}");}
        key->setToolTip(modified?"Edited value differs from the key at this frame. Click to remove the key, then click again to record the edited value.":"Toggle this property's key at the playhead");
        spin->setEnabled(!o->locked);key->setEnabled(!o->locked);reset->setEnabled(!o->locked);
    });
}
void PropertyInspector::choice(QFormLayout *form,const QString &label,const QString &path,const QStringList &options,const QString &fallback){
    auto *combo=new QComboBox;combo->addItems(options);form->addRow(label,combo);
    connect(combo,&QComboBox::activated,this,[=, this]{apply([&]{for(auto *o:m->inspected())if(!o->locked)o->style[path]=combo->currentText();});});
    sync.append([=, this]{if(auto *o=m->active()){
        combo->setCurrentText(o->style[path].toString(fallback));combo->setEnabled(!o->locked);
        if(path=="fill.kind")for(int i=2;i<form->rowCount();++i)form->setRowVisible(i,combo->currentText()!="Solid");
    }});
}
void PropertyInspector::color(QFormLayout *form,const QString &label,const QString &path,const QString &fallback){
    auto *button=new QPushButton;form->addRow(label,button);
    auto get=[=, this]{auto *o=m->active();return o?(path=="fill.color"?o->mat.color:path=="light.color"?o->light.color:QColor(o->style[path].toString(fallback))):QColor(fallback);};
    connect(button,&QPushButton::clicked,this,[=, this]{QColor c=ColorPicker::getColor(get(),this,label,QColorDialog::ShowAlphaChannel);if(c.isValid())apply([&]{for(auto *o:m->inspected())if(!o->locked){if(path=="fill.color")o->mat.color=c;else if(path=="light.color")o->light.color=c;else o->style[path]=c.name(QColor::HexArgb);}});});
    sync.append([=, this]{QColor c=get();button->setText(c.name(QColor::HexArgb));button->setStyleSheet("background:"+c.name()+";color:"+(c.lightness()>140?"#111":"#fff")+";");});
}
void PropertyInspector::refresh(){syncing=true;for(const auto &fn:sync)fn();syncing=false;}
void PropertyInspector::rebuild(){
    auto *o=m->active();QString sig=o?o->id+o->type+QString::number(m->sel.size()):QString("none");
    if(sig==signature)return;signature=sig;sync.clear();
    while(auto *item=layout->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    if(!o){layout->addWidget(new QLabel("Select a layer to inspect its properties."));layout->addStretch();return;}
    auto *general=section(m->sel.size()>1?QString("%1 LAYERS").arg(m->sel.size()):o->type.toUpper());
    auto *name=new QLineEdit(o->name);general->addRow("Name",name);
    sync.append([=,this]{if(auto *object=m->active();object&&!name->hasFocus()){QSignalBlocker blocker(name);name->setText(object->name);}});
    connect(name,&QLineEdit::editingFinished,this,[=, this]{m->rename(m->activeId,name->text());});
    auto *visible=new QToolButton,*locked=new QToolButton;visible->setCheckable(true);locked->setCheckable(true);visible->setToolTip("Show / hide layer");locked->setToolTip("Lock / unlock layer");
    general->addRow(visible);general->addRow(locked);
    connect(visible,&QToolButton::toggled,this,[=, this](bool v){apply([&]{for(auto *x:m->inspected())x->visible=v;});});
    connect(locked,&QToolButton::toggled,this,[=, this](bool v){apply([&]{for(auto *x:m->inspected())x->locked=v;});});
    sync.append([=, this]{if(auto *x=m->active()){visible->setChecked(x->visible);locked->setChecked(x->locked);visible->setIcon(uiIcon(x->visible?"eye":"hidden",x->visible));locked->setIcon(uiIcon(x->locked?"lock":"unlock",x->locked));}});
    if(!o->parentId.isEmpty()){auto *parent=m->byId(o->parentId);general->addRow("Parent",new QLabel(parent?parent->name:"Missing parent"));}
    const bool multi=m->sel.size()>1;
    auto *isolate=new QToolButton;isolate->setText("Edit parent only");isolate->setCheckable(true);isolate->setToolTip("Keep children's world transforms while adjusting this parent");general->addRow(isolate);connect(isolate,&QToolButton::toggled,this,[this](bool on){m->isolateParents=on;});sync.append([=,this]{bool parent=false;for(const auto &child:m->design.objects)parent|=child.parentId==m->activeId;isolate->setVisible(parent);isolate->setChecked(m->isolateParents);});
    if(o->type=="Audio"){
        auto *audio=section("AUDIO");number(audio,"Start","audio.start",0,86400000,40," ms");number(audio,"Volume","audio.volume",0,1,.05);
        number(audio,"Source offset","audio.offset",0,86400000,40," ms");number(audio,"Playback speed","audio.speed",.1,4,.05);
        number(audio,"Play length","audio.length",0,86400000,40," ms");number(audio,"Fade in","audio.fadeIn",0,60000,40," ms");number(audio,"Fade out","audio.fadeOut",0,60000,40," ms");
        choice(audio,"Repeat","audio.repeat",{"Once","Loop"},"Once");choice(audio,"Mute","audio.mute",{"Off","On"},"Off");
        audio->addRow("Source",new QLabel(QFileInfo(o->style["audio.file"].toString()).fileName()));
        layout->addStretch();refresh();return;
    }
    auto *transform=section("TRANSFORM");
    number(transform,"Canvas X","position.x",-10000,10000,1," px");
    number(transform,"Canvas Y","position.z",-10000,10000,1," px");
    number(transform,"Depth Z","position.y",-10000,10000,.1," units");
    number(transform,"Rotation X","rotation.x",-36000,36000,1,"°");
    number(transform,"Rotation Y","rotation.y",-36000,36000,1,"°");
    number(transform,"Rotation Z","rotation.z",-36000,36000,1,"°");
    auto *scale=section("SCALE",false);
    for(const auto &axis:QStringList{"x","y","z"})number(scale,axis.toUpper(),"scale."+axis,-1000,1000,.1);
    if(o->isRenderable() || multi){auto *appearance=section("APPEARANCE");number(appearance,"Opacity","opacity",0,1,.05);choice(appearance,"Blend mode","blend",{"Normal","Multiply","Screen","Overlay","Darken","Lighten","Color Dodge","Color Burn","Hard Light","Soft Light","Difference","Exclusion"},"Normal");}
    if(!multi && (o->type=="Text"||o->type=="Image"||o->type=="Shape")){
        auto *size=section("LAYOUT");number(size,"Width","layout.width",1,8192,1," px");number(size,"Height","layout.height",1,8192,1," px");
        if(o->type=="Shape")choice(size,"Shape","shape",{"Rectangle","Ellipse","Triangle"},"Rectangle");
        auto *aspectLock=new QCheckBox("Maintain aspect ratio");size->addRow(aspectLock);
        auto *unlit=new QCheckBox("Ignore lighting");size->addRow(unlit);connect(unlit,&QCheckBox::toggled,this,[=,this](bool value){apply([&]{if(auto *object=m->active())object->mat.shadeless=value;});});sync.append([=,this]{if(auto *object=m->active())unlit->setChecked(object->mat.shadeless);});
        connect(aspectLock,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *x=m->active())x->style["layout.aspectLock"]=on;});});
        sync.append([=,this]{if(auto *x=m->active())aspectLock->setChecked(x->style["layout.aspectLock"].toBool());});
        {
        auto *corners=section("CORNERS",false);number(corners,"All corners","corner.radius",0,2000,1," px");
        choice(corners,"Units","corner.unit",{"px","%"},"px");
        auto *linked=new QCheckBox("Link corners");linked->setToolTip("Use one radius for all corners");corners->addRow(linked);
        connect(linked,&QCheckBox::toggled,this,[=,this](bool value){apply([&]{for(auto *object:m->inspected())if(!object->locked)object->style["corner.linked"]=value;});});
        sync.append([=,this]{if(auto *object=m->active())linked->setChecked(object->style["corner.linked"].toBool(true));});
        for(auto key:QStringList{"tl","tr","br","bl"})number(corners,key.toUpper(),"corner."+key,0,2000,1," px");
        sync.append([=,this]{bool show=!linked->isChecked();for(int row=3;row<corners->rowCount();++row)corners->setRowVisible(row,show);});
        auto *stroke=section("STROKE",false);number(stroke,"Width","stroke.width",0,100,1," px");color(stroke,"Color","stroke.color","#ffffff");choice(stroke,"Style","stroke.style",{"Solid","Dashed","Dotted"},"Solid");
        choice(stroke,"Join","stroke.join",{"Miter","Round","Bevel"},"Round");
        }
        QFormLayout *backgroundForm=nullptr;
        if(o->type=="Text"){
            auto *background=section("TEXT BACKGROUND");auto *enabled=new QCheckBox("Background");background->addRow(enabled);
            connect(enabled,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{m->active()->style["text.background"]=on;});});sync.append([=,this]{enabled->setChecked(m->active()->style["text.background"].toBool());});color(background,"Color","text.backgroundColor","#cc202020");
        }
        {
            auto *fill=section(o->type=="Text"?"FILL":"BACKGROUND");
            backgroundForm=fill;
            if(o->type!="Text")choice(fill,"Type","background.type",{"Solid","Gradient","Media"},o->type=="Image"?"Media":o->style["fill.kind"].toString("Solid")=="Solid"?"Solid":"Gradient");
            color(fill,"Color / start","fill.color","#ffffff");choice(fill,o->type=="Text"?"Type":"Gradient","fill.kind",o->type=="Text"?QStringList{"Solid","Linear","Radial","Conic"}:QStringList{"Linear","Radial","Conic"},o->type=="Text"?"Solid":"Linear");
            color(fill,"Middle","fill.middle","#6366f1");number(fill,"Middle stop","fill.stop",.001,.999,.05);color(fill,"End","fill.end","#a855f7");number(fill,"Angle","fill.angle",0,360,1,"°");
            if(o->type!="Text")sync.append([=,this]{if(auto *object=m->active()){auto type=object->style["background.type"].toString(object->type=="Image"?"Media":object->style["fill.kind"].toString("Solid")=="Solid"?"Solid":"Gradient");fill->setRowVisible(1,type!="Media");for(int row=2;row<7;++row)fill->setRowVisible(row,type=="Gradient");}});
        }
        if(o->type!="Text"){
            auto *mediaBody=new QWidget;auto *media=new QFormLayout(mediaBody);media->setContentsMargins(0,0,0,0);backgroundForm->addRow(mediaBody);choice(media,"Source type","media.kind",{"Image","Video"},"Image");
            sync.append([=,this]{if(auto *object=m->active())mediaBody->setVisible(object->style["background.type"].toString(object->type=="Image"?"Media":"Solid")=="Media");});
            auto *file=new QPushButton("Choose file…");media->addRow(file);file->setToolTip("Use an image or video as this object's background");
            connect(file,&QPushButton::clicked,this,[=,this]{auto *object=m->active();if(!object||object->locked)return;bool video=object->style["media.kind"].toString()=="Video";
                const QString path=QFileDialog::getOpenFileName(this,video?"Video background":"Image background",{},video?"Video (*.mp4 *.mov *.mkv *.webm *.avi *.mxf)":"Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.svg *.ico)");if(path.isEmpty())return;
                QString error;const auto duration=video?MediaFrames::duration(path,&error):0;if(video&&!error.isEmpty()){QMessageBox::warning(this,"Video background",error);return;}
                if(!video&&QImage(path).isNull()){QMessageBox::warning(this,"Image background","This image could not be opened.");return;}
                apply([&]{object->image=path;object->style["background.type"]="Media";object->style["media.duration"]=double(duration);});
            });
            choice(media,"Fit","image.fit",{"Contain","Cover","Fill","None","Scale Down"},"Contain");number(media,"Position X","image.x",0,1,.05);number(media,"Position Y","image.y",0,1,.05);
            number(media,"Trim start","media.offset",0,86400000,40," ms");number(media,"Playback speed","media.speed",.1,4,.1);number(media,"Audio volume","media.volume",0,1,.05);
            auto *loop=new QCheckBox("Loop video");media->addRow(loop);connect(loop,&QCheckBox::toggled,this,[=,this](bool value){apply([&]{if(auto *object=m->active())object->style["media.loop"]=value;});});sync.append([=,this]{if(auto *object=m->active())loop->setChecked(object->style["media.loop"].toBool(true));});
            sync.append([=,this]{if(auto *object=m->active())for(int row=5;row<media->rowCount();++row)media->setRowVisible(row,object->style["media.kind"].toString()=="Video");});
        }
        auto *shadow=section("SHADOW",false);auto *on=new QCheckBox("Drop shadow");shadow->addRow(on);
        connect(on,&QCheckBox::toggled,this,[=, this](bool v){apply([&]{if(auto *x=m->active())x->style["shadow.enabled"]=v;});});
        sync.append([=, this]{if(auto *x=m->active())on->setChecked(x->style["shadow.enabled"].toBool());});
        number(shadow,"X","shadow.x",-1000,1000,1," px");number(shadow,"Y","shadow.y",-1000,1000,1," px");number(shadow,"Blur","shadow.blur",0,80,1," px");color(shadow,"Color","shadow.color","#99000000");
        auto *effects=section("EFFECTS",false);
        number(effects,"Backdrop blur","backdrop.blur",0,80,1," px");number(effects,"Backdrop brightness","backdrop.brightness",0,3,.05);
        number(effects,"Blur","effects.blur",0,80,1," px");
        number(effects,"Reveal","effects.reveal",0,1,.01);
        choice(effects,"Reveal mode","effects.revealMode",{"Linear","Radial"},"Linear");
        number(effects,"Reveal roughness","effects.revealRoughness",0,1,.01);
        number(effects,"Reveal softness","effects.revealSoftness",0,.2,.005);
        number(effects,"Reveal direction","effects.revealAngle",-180,180,1);
        number(effects,"Reveal center X","effects.revealX",0,1,.01);
        number(effects,"Reveal center Y","effects.revealY",0,1,.01);
        for(const auto &path:QStringList{"brightness","contrast","saturation"})number(effects,path,"effects."+path,0,3,.05);
        for(const auto &path:QStringList{"grayscale","invert","sepia"})number(effects,path,"effects."+path,0,1,.05);
        number(effects,"Hue","effects.hue",-360,360,1,"°");
    }
    if(!multi&&(o->type=="Text"||o->type=="Image")){
        auto *studio=section("STUDIO FIELD",false);auto *enabled=new QCheckBox("Editable in Studio");enabled->setChecked(o->style["studio.editable"].toBool());studio->addRow(enabled);
        enabled->setToolTip("Expose this object's content in Screen Elements without changing its animation. Text shorthand: {{studio:Sponsor}}Your text");
        auto *label=new QLineEdit(o->style["studio.label"].toString(o->name));studio->addRow("Field name",label);
        sync.append([=,this]{if(auto *x=m->active()){QSignalBlocker block(enabled);enabled->setChecked(x->style["studio.editable"].toBool());if(!label->hasFocus()){QSignalBlocker labelBlock(label);label->setText(x->style["studio.label"].toString(x->name));}}});
        auto *fit=new QCheckBox("Fit long text inside its box");fit->setChecked(o->style["studio.fit"].toBool(true));if(o->type=="Text")studio->addRow(fit);else fit->deleteLater();
        connect(enabled,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *x=m->active()){x->style["studio.editable"]=on;x->style["studio.label"]=label->text();}});});
        connect(label,&QLineEdit::editingFinished,this,[=,this]{apply([&]{if(auto *x=m->active())x->style["studio.label"]=label->text().trimmed();});});
        if(o->type=="Text")connect(fit,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *x=m->active())x->style["studio.fit"]=on;});});
    }
    if(!multi && o->type=="Text"){
        auto *text=section("TYPOGRAPHY");auto *content=new QPlainTextEdit(o->text);content->setMaximumHeight(70);text->addRow(content);
        sync.append([=,this]{if(auto *object=m->active();object&&!content->hasFocus()&&content->toPlainText()!=object->text){QSignalBlocker blocker(content);content->setPlainText(object->text);}});
        connect(content,&QPlainTextEdit::textChanged,this,[=, this]{apply([&]{if(auto *x=m->active()){if(parseStudioText(*x,content->toPlainText())){QSignalBlocker block(content);content->setPlainText(x->text);}}});});
        choice(text,"Formatting","text.format",{"Plain text","Markdown"},"Plain text");
        content->setToolTip("Markdown: **bold**, *italic*, ~~strike~~, # heading, - list, > quote, `code`. Choose Markdown below. Studio field prefixes still work.");
        auto *family=new QFontComboBox;family->setCurrentFont(QFont(o->style["text.family"].toString("Segoe UI")));text->addRow("Font",family);
        connect(family,&QFontComboBox::currentFontChanged,this,[=, this](const QFont &font){apply([&]{if(auto *x=m->active())x->style["text.family"]=font.family();});});
        number(text,"Size","text.size",1,1000,1," px");
        auto *advanced=section("ADVANCED TYPOGRAPHY",false);number(advanced,"Weight","text.weight",100,900,100);
        auto *italic=new QCheckBox("Italic");advanced->addRow(italic);
        connect(italic,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *x=m->active();x&&!x->locked)x->style["text.italic"]=on;});});
        sync.append([=,this]{if(auto *x=m->active()){italic->setChecked(x->style["text.italic"].toBool());italic->setEnabled(!x->locked);}});
        number(advanced,"Line height","text.lineHeight",.2,5,.1);number(advanced,"Tracking","text.tracking",-30,200,.5," px");number(advanced,"Word spacing","text.wordSpacing",-30,200,.5," px");
        choice(advanced,"Align","text.align",{"Left","Center","Right","Justify"},"Center");choice(advanced,"Vertical","text.vertical",{"Top","Center","Bottom"},"Center");
        choice(advanced,"Transform","text.transform",{"None","Uppercase","Lowercase","Capitalize"},"None");choice(advanced,"Decoration","text.decoration",{"None","Underline","Line Through"},"None");
    }
    if(!multi && o->type=="Image"){
        auto *image=section("IMAGE");auto *source=new QPushButton("Choose image…");image->addRow(source);
        connect(source,&QPushButton::clicked,this,[this]{QString path=QFileDialog::getOpenFileName(this,"Image",{},"Images (*.png *.jpg *.jpeg *.webp *.bmp)");if(!QImage(path).isNull())apply([&]{m->active()->image=path;});});
        choice(image,"Fit","image.fit",{"Fill","Contain","Cover","None","Scale Down"},"Contain");number(image,"Horizontal","image.x",0,1,.05);number(image,"Vertical","image.y",0,1,.05);
    }
    if(!multi&&(o->type=="Shape"||o->type=="Text"||o->type=="Image")){
        auto *overflow=section("OVERFLOW",false);choice(overflow,"X","overflow.x",{"Visible","Hidden"},"Visible");choice(overflow,"Y","overflow.y",{"Visible","Hidden"},"Visible");
    }
    if(!multi && o->type=="Mesh"){
        auto *material=section("MATERIAL");color(material,"Base color","fill.color","#c8ccd4");number(material,"Roughness","material.roughness",0,1,.05);number(material,"Emission","material.emission",0,5,.1);number(material,"Alpha","material.alpha",0,1,.05);
        auto *shadeless=new QCheckBox("Ignore lighting");material->addRow(shadeless);connect(shadeless,&QCheckBox::toggled,this,[=, this](bool v){apply([&]{m->active()->mat.shadeless=v;});});sync.append([=, this]{shadeless->setChecked(m->active()->mat.shadeless);});
    }
    if(!multi && o->type=="Light"){
        auto *light=section("LIGHTING");auto *kind=new QComboBox;kind->addItems({"Point","Sun","Spot","Area"});kind->setCurrentText(o->light.kind);light->addRow("Type",kind);connect(kind,&QComboBox::activated,this,[=, this]{m->changeLightKind(kind->currentText());});
        color(light,"Color","light.color","#ffffff");number(light,"Power","light.power",0,100000,10);number(light,"Spot angle","light.angle",1,179,1,"°");number(light,"Spot blend","light.blend",0,1,.05);
    }
    if(!multi && o->type=="Camera"){
        auto *camera=section("FIXED RENDER CAMERA");auto *ortho=new QCheckBox("Orthographic");camera->addRow(ortho);
        connect(ortho,&QCheckBox::toggled,this,[=, this](bool v){apply([&]{if(!m->active()->locked)m->active()->cam.ortho=v;});});sync.append([=, this]{ortho->setChecked(m->active()->cam.ortho);ortho->setEnabled(!m->active()->locked);});
        number(camera,"Field of view","camera.fov",5,170,1,"°");number(camera,"Ortho width","camera.scale",.1,1000,.1);
    }
    if (o->type=="Text" && !multi) {
        for(int i=0;i+1<layout->count();++i) {
            auto *button=qobject_cast<QToolButton*>(layout->itemAt(i)->widget());
            if(!button || button->text()!="TYPOGRAPHY")continue;
            auto *titleItem=layout->takeAt(i);auto *bodyItem=layout->takeAt(i);
            layout->insertWidget(2,titleItem->widget());layout->insertWidget(3,bodyItem->widget());delete titleItem;delete bodyItem;break;
        }
    }
    // Keep visibility and locking available; all property sections respect the lock.
    for(int i=3;i<layout->count();i+=2){
        if(auto *body=layout->itemAt(i)->widget())sync.append([this,body]{if(auto *x=m->active())body->setEnabled(!x->locked);});
    }
    layout->addStretch();refresh();
}
