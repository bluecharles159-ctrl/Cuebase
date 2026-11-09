#include "UiControls.h"
#include "PropertyInspector.h"
#include "RevealEffects.h"
#include "MeshEmitter.h"
#include "TextureLayers.h"
#include "ElementLibrary.h"
#include "BuildModel.h"
#include "Data.h"
#include "MediaFrames.h"
#include "SurfacePaint.h"
#include <QJsonDocument>
#include <QUuid>
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
#include <QMenu>
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
    setStyleSheet("QDoubleSpinBox,QComboBox,QLineEdit{min-height:18px;padding:1px 3px;} QToolButton{border:0;text-align:left;padding:3px;} QToolButton:checked{background:palette(button);} QPushButton{padding:2px;border-radius:3px;} QLabel{font-size:10px;}");
    connect(m,&BuildModel::selectionChanged,this,[this]{rebuild();});
    connect(m,&BuildModel::changed,this,[this]{rebuild();refresh();});
    connect(m,&BuildModel::timeChanged,this,[this]{refresh();});
    connect(lib,&Library::displayChanged,this,[this]{refresh();});
    rebuild();
}
void PropertyInspector::apply(const std::function<void()> &operation){if(syncing)return;m->pushUndo();operation();m->notify();}
QFormLayout *PropertyInspector::section(const QString &title,bool open){
    const auto *object=m->active();const QString scope=(object?object->id:QString("scene"))+"/"+title;
    const bool defaultOpen=title=="TRANSFORM"||(title=="TYPOGRAPHY"&&object&&object->surfaceType()=="Text");
    open=expanded.value(scope,QSettings().value("inspector/objects/"+scope,defaultOpen).toBool());
    auto *button=new QToolButton;button->setText(title);button->setCheckable(true);button->setChecked(open);button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);button->setIcon(uiIcon(open?"down":"right"));button->setIconSize(QSize(12,12));button->setToolTip("Expand / collapse "+title);
    button->setProperty("sectionHeader",true);button->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);layout->addWidget(button);
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
                auto p=o->at(m->t);const auto basis=m->design.positionMatrix(*o,m->t);const auto world=basis.map(p.loc); const auto camera=m->design.sceneCamera(m->t,lib->display.aspect());
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
            if(object->surfaceType()=="Text"&&object->style.value("text.autoFit").toBool()&&(path=="layout.width"||path=="layout.height"))return object->evaluated(m->t).propertyBase(path);
            if(path=="position.x"||path=="position.z"){
                QPointF pixel;const auto camera=m->design.sceneCamera(m->t,lib->display.aspect());
                camera.toScreen(m->design.worldMatrix(*object,m->t).map(QVector3D()),QRectF(0,0,lib->display.resX(),lib->display.resY()),&pixel);
                return path=="position.x"?pixel.x():pixel.y();
            }
            return object->propertyValue(path,m->t);
        };
        double value=displayed(o);bool mixed=false;
        if(path.startsWith("corner."))spin->setSuffix(o->style.value("corner.unit").toString()=="%"?" %":" px");
        for(auto *other:m->inspected())mixed=mixed||qAbs(displayed(other)-value)>.00001;
        spin->setValue(value);
        if(mixed){auto *line=spin->findChild<QLineEdit*>();line->clear();line->setPlaceholderText("Mixed");}
        const bool keyed=m->propertyIsKeyed(*o,path,m->t),animated=o->track(path)!=nullptr;
        const bool modified=keyed&&o->overrides.contains(path)&&qAbs(o->propertyValue(path,m->t)-o->track(path)->evaluate(m->t,o->propertyBase(path)))>.000001;
        spin->setStyleSheet(QString("QDoubleSpinBox{background:%1;color:%2;border-radius:3px;}").arg(keyed?"#79722c":animated?"#426b29":"palette(base)",keyed||animated?"white":"palette(text)"));
        key->setText(keyed?"◆":"◇");key->setStyleSheet(keyed?"color:#fbbf24;":animated?"color:#60a5fa;":"color:palette(placeholder-text);");
        if(modified){key->setText("◈");key->setStyleSheet("color:#ff9f43;");spin->setStyleSheet("QDoubleSpinBox{background:#87552c;color:white;border-radius:3px;}");}
        key->setToolTip(modified?"Update this key with the edited value. Click an unchanged key to remove it.":"Toggle this property's key at the playhead");
        const bool fitted=o->surfaceType()=="Text"&&o->style.value("text.autoFit").toBool()&&((path=="layout.width"&&o->style.value("text.autoFitX").toBool(true))||(path=="layout.height"&&o->style.value("text.autoFitY").toBool(true)));
        spin->setEnabled(!o->locked&&!fitted);key->setEnabled(!o->locked&&!fitted);reset->setEnabled(!o->locked&&!fitted);
    });
}
void PropertyInspector::choice(QFormLayout *form,const QString &label,const QString &path,const QStringList &options,const QString &fallback){
    auto *combo=new QComboBox;combo->setObjectName("property_"+path);combo->setAccessibleName(label);combo->addItems(options);form->addRow(label,combo);
    connect(combo,&QComboBox::activated,this,[=, this]{apply([&]{for(auto *o:m->inspected())if(!o->locked)o->style[path]=combo->currentText();});});
    sync.append([=, this]{if(auto *o=m->active()){
        combo->setCurrentText(o->style.value(path).toString(fallback));combo->setEnabled(!o->locked);
        if(path=="fill.kind")for(int i=2;i<form->rowCount();++i)form->setRowVisible(i,combo->currentText()!="Solid");
    }});
}
void PropertyInspector::color(QFormLayout *form,const QString &label,const QString &path,const QString &fallback){
    auto *button=new QPushButton;button->setObjectName("color_"+path);form->addRow(label,button);
    auto get=[=, this]{auto *o=m->active();return o?(path=="fill.color"?o->mat.color:path=="light.color"?o->light.color:QColor(o->style.value(path).toString(fallback))):QColor(fallback);};
    connect(button,&QPushButton::clicked,this,[=, this]{
        QHash<QString,QColor> colors;QHash<QString,QJsonValue> styles;
        for(auto *o:m->inspected())if(!o->locked){colors[o->id]=path=="light.color"?o->light.color:o->mat.color;styles[o->id]=o->style.value(path);}
        auto preview=[&](QColor c){for(auto it=colors.cbegin();it!=colors.cend();++it)if(auto *o=m->byId(it.key())){if(path=="fill.color")o->mat.color=c;else if(path=="light.color")o->light.color=c;else o->style[path]=c.name(QColor::HexArgb);}m->notify();};
        const QColor c=ColorPicker::getColor(get(),this,label,QColorDialog::ShowAlphaChannel,preview);
        bool changed=false;
        for(auto it=colors.cbegin();it!=colors.cend();++it)if(auto *o=m->byId(it.key())){
            const QColor old=path=="fill.color"||path=="light.color"?it.value():QColor(styles[it.key()].toString(fallback));changed|=c.isValid()&&c!=old;
            if(path=="fill.color")o->mat.color=it.value();else if(path=="light.color")o->light.color=it.value();else if(styles[it.key()].isUndefined())o->style.remove(path);else o->style[path]=styles[it.key()];
        }
        if(changed){m->pushUndo(true);preview(c);}else m->notify();
    });
    sync.append([=, this]{QColor c=get();button->setText(c.name(QColor::HexArgb));button->setStyleSheet("background:"+c.name()+";color:"+(c.lightness()>140?"#111":"#fff")+";");});
}
void PropertyInspector::particlePathControls(QFormLayout *form){
    auto *pathRow=new QWidget;auto *pathActions=new QHBoxLayout(pathRow);pathActions->setContentsMargins(0,0,0,0);
    auto *pathEdit=new QPushButton;pathEdit->setObjectName("editParticlePath");
    auto *pathClear=new QPushButton("Clear");pathClear->setObjectName("clearParticlePath");
    pathActions->addWidget(pathEdit,1);pathActions->addWidget(pathClear);form->addRow("Motion path",pathRow);
    const int directionRow=form->rowCount();
    choice(form,"Path direction","effects.pathDirection",{"Forward","Reverse"},"Forward");
    auto *pose=new QComboBox;pose->addItems({"Start path","End path"});pose->setObjectName("particlePathPose");
    form->addRow("Edit pose",pose);
    const int blendRow=form->rowCount();
    number(form,"Path blend","effects.pathMix",0,1,.01);
    connect(pose,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    connect(pathEdit,&QPushButton::clicked,this,[this,pose]{emit m->particlePathRequested(pose->currentIndex()==1);});
    connect(pathClear,&QPushButton::clicked,this,[this,pose]{
        if(auto *target=m->active();target && !target->locked)apply([&]{
            if(pose->currentIndex()==0)target->style.remove("effects.particlePath");
            target->style.remove("effects.particleEndPath");
        });
    });
    sync.append([=,this]{
        if(const auto *active=m->active()){
            const bool enabled=active->style.value("meshEmitter.enabled").toBool() || active->style.value("effects.revealParticles").toString("None")!="None";
            const bool hasPath=!active->style.value("effects.particlePath").toArray().isEmpty();
            const bool hasEnd=!active->style.value("effects.particleEndPath").toArray().isEmpty();
            if(!hasPath){QSignalBlocker block(pose);pose->setCurrentIndex(0);}
            form->setRowVisible(pose,enabled && hasPath);
            form->setRowVisible(blendRow,enabled && hasPath && hasEnd);
            form->setRowVisible(pathRow,enabled);pathRow->setEnabled(!active->locked);
            form->setRowVisible(directionRow,enabled && hasPath);
            pathEdit->setText(hasPath?"Edit in viewport…":"Draw in viewport…");
            pathClear->setVisible(pose->currentIndex()==1?hasEnd:hasPath);
            pathClear->setToolTip(pose->currentIndex()==1?"Remove the end pose.":"Remove both path poses.");
        }
    });
}
void PropertyInspector::refresh(){syncing=true;for(const auto &fn:sync)fn();syncing=false;}
void PropertyInspector::maskControls(){
    const auto *object=m->active();
    const auto stack=object->style.value("mask.stack").toArray();
    auto *form=section("MASKS");
    auto *add=new QPushButton("Add mask");
    add->setObjectName("addSurfaceMask");
    auto *menu=new QMenu(add);
    add->setMenu(menu);add->setEnabled(!object->locked);form->addRow(add);
    for(const QString kind:QStringList{"Ellipse","Rectangle","Gradient","Alpha","Luminance","Text","Brush"}){
        menu->addAction(kind,this,[this,kind]{
            auto *o=m->active();if(!o||o->locked)return;
            QString file;
            if(kind=="Alpha"||kind=="Luminance"){
                file=QFileDialog::getOpenFileName(this,"Mask image",{},"Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)");
                if(file.isEmpty())return;
            }
            const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
            const auto prefix="mask."+id+".";
            apply([&]{auto order=o->style.value("mask.stack").toArray();order.append(id);o->style["mask.stack"]=order;
                o->style[prefix+"type"]=kind;
                if(!file.isEmpty())o->style[prefix+"file"]=file;});
        });
    }
    for(int index=0;index<stack.size();++index){
        const auto id=stack.at(index).toString(),prefix="mask."+id+".";
        const auto kind=object->style.value(prefix+"type").toString("Ellipse");
        auto *fields=section(QString("MASK %1 · %2").arg(index+1).arg(kind.toUpper()));
        auto *actionRow=new QWidget;auto *actions=new QHBoxLayout(actionRow);actions->setContentsMargins(0,0,0,0);
        for(const QString action:QStringList{"Earlier","Later","Remove"}){
            auto *b=new QPushButton(action);actions->addWidget(b);
            b->setEnabled(!object->locked && (action!="Earlier" || index>0) && (action!="Later" || index+1<stack.size()));
            connect(b,&QPushButton::clicked,this,[this,action,id,prefix]{
                auto *o=m->active();if(!o||o->locked)return;
                auto order=o->style.value("mask.stack").toArray();qsizetype pos=0;
                while(pos<order.size() && order.at(pos).toString()!=id)++pos;
                if(pos>=order.size())return;
                apply([&]{
                    order.removeAt(pos);
                    if(action=="Remove"){
                        for(const auto &key:o->style.keys())if(key.startsWith(prefix))o->style.remove(key);
                        o->tracks.removeIf([&](const auto &track){return track.property.startsWith(prefix);});
                        for(const auto &key:o->overrides.keys())if(key.startsWith(prefix))o->overrides.remove(key);
                        for(const auto &key:o->defaults.keys())if(key.startsWith(prefix))o->defaults.remove(key);
                    }else order.insert(qBound<qsizetype>(0,pos+(action=="Earlier"?-1:1),order.size()),id);
                    o->style["mask.stack"]=order;
                });
            });
        }
        fields->addRow(actionRow);
        choice(fields,"Enabled",prefix+"enabled",{"On","Off"},"On");
        choice(fields,"Combine",prefix+"combine",{"Intersect","Add","Subtract","Replace"},"Intersect");
        choice(fields,"Invert",prefix+"invert",{"Off","On"},"Off");
        if(kind=="Text"){
            auto *input=new QLineEdit;input->setAccessibleName("Mask text");fields->addRow("Text",input);
            connect(input,&QLineEdit::editingFinished,this,[this,input,prefix]{
                if(auto *o=m->active();o && !o->locked && input->isModified()){
                    input->setModified(false);apply([&]{o->style[prefix+"text"]=input->text();});}
            });
            sync.append([this,input,prefix]{if(auto *o=m->active()){
                if(!input->hasFocus())input->setText(o->style.value(prefix+"text").toString("TITLE"));
                input->setEnabled(!o->locked);}});
        }else if(kind=="Brush"){
            auto *paint=new QPushButton("Paint mask…");paint->setEnabled(!object->locked);fields->addRow(paint);
            connect(paint,&QPushButton::clicked,this,[this,prefix]{SurfacePaint::edit(this,m,prefix+"brush");});
        }else if(kind=="Alpha"||kind=="Luminance"){
            auto *file=new QPushButton;fields->addRow("Image",file);
            connect(file,&QPushButton::clicked,this,[this,prefix]{
                const auto path=QFileDialog::getOpenFileName(this,"Replace mask image",{},"Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)");
                if(auto *o=m->active();o && !o->locked && !path.isEmpty())apply([&]{o->style[prefix+"file"]=path;});
            });
            sync.append([this,file,prefix]{if(auto *o=m->active()){
                file->setText(QFileInfo(o->style.value(prefix+"file").toString()).fileName());file->setEnabled(!o->locked);}});
        }
        number(fields,"Center X",prefix+"x",-2,3,.01);number(fields,"Center Y",prefix+"y",-2,3,.01);
        number(fields,"Width",prefix+"width",.001,4,.01);number(fields,"Height",prefix+"height",.001,4,.01);
        number(fields,"Rotation",prefix+"angle",-3600,3600,1,"°");
        number(fields,"Feather",prefix+"feather",0,100,1," px");number(fields,"Strength",prefix+"opacity",0,1,.01);
    }
}
void PropertyInspector::textureControls(const DObject &object) {
    auto *texture=section("SURFACE TEXTURE",false);
    const auto order=TextureLayers::order(object);
    QString id=selectedTextureLayers.value(object.id,order.last());
    if(!order.contains(id))id=order.last();
    const auto key=TextureLayers::prefix(id);
    auto *layers=new QComboBox;
    layers->setObjectName("surfaceTextureLayers");
    texture->addRow("Layer",layers);
    for(const auto &layer:order)layers->addItem(QString(),layer);
    layers->setCurrentIndex(order.indexOf(id));
    sync.append([=,this]{if(const auto *current=m->active()){
        for(int index=0;index<layers->count();++index){
            const auto path=current->style.value(TextureLayers::prefix(layers->itemData(index).toString())+"file").toString();
            layers->setItemText(index,QString::number(index+1)+"  "+(path.isEmpty()?QString("Base fill"):QFileInfo(path).fileName()));
        }
    }});
    connect(layers,&QComboBox::activated,this,[=,this](int index){
        if(const auto *current=m->active()){
            selectedTextureLayers[current->id]=layers->itemData(index).toString();
            rebuild();refresh();
        }
    });
    auto *orderRow=new QWidget;
    auto *row=new QHBoxLayout(orderRow);row->setContentsMargins(0,0,0,0);
    for(int delta:{-1,1}){
        auto *move=new QPushButton(delta<0?"Move back":"Move forward");
        move->setObjectName(delta<0?"textureLayerBack":"textureLayerForward");
        const int index=order.indexOf(id);
        move->setEnabled(!object.locked && index+delta>=0 && index+delta<order.size());
        connect(move,&QPushButton::clicked,this,[=,this]{
            apply([&]{auto *current=m->active();if(!current || current->locked)return;
                auto latest=TextureLayers::order(*current);const int from=latest.indexOf(id);
                if(from<0 || from+delta<0 || from+delta>=latest.size())return;
                latest.move(from,from+delta);selectedTextureLayers[current->id]=id;
                current->style["texture.stack"]=QJsonArray::fromStringList(latest);
            });
        });
        row->addWidget(move);
    }
    orderRow->setVisible(order.size()>1);texture->addRow(orderRow);
    auto *enabled=new QCheckBox("Use texture");texture->addRow(enabled);
    connect(enabled,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *object=m->active())object->style[key+"enabled"]=on;});});
    auto *path=new QLabel;path->setWordWrap(true);texture->addRow(path);
    sync.append([=,this]{if(auto *object=m->active()){QSignalBlocker block(enabled);enabled->setChecked(object->style.value(key+"enabled").toBool(true));const auto file=object->style.value(key+"file").toString();path->setText(file.isEmpty()?"Apply a texture from the Material tab.":QFileInfo(file).fileName());path->setToolTip(file);}});
    choice(texture,"Mapping",key+"mapping",{"Fit","Tile"},"Fit");
    choice(texture,"Blend",key+"blend",{"Normal","Multiply","Screen","Overlay"},"Normal");
    number(texture,"Scale X",key+"scaleX",.01,100,.05);number(texture,"Scale Y",key+"scaleY",.01,100,.05);
    number(texture,"Offset X",key+"offsetX",-10000,10000,1," px");number(texture,"Offset Y",key+"offsetY",-10000,10000,1," px");
    number(texture,"Rotation",key+"rotation",-3600,3600,1,"°");number(texture,"Opacity",key+"opacity",0,1,.05);color(texture,"Tint",key+"tint","#ffffff");
    auto *mask=new QPushButton("Choose mask image…");texture->addRow("Layer mask",mask);
    connect(mask,&QPushButton::clicked,this,[=,this]{
        const QString path=QFileDialog::getOpenFileName(this,"Choose a texture layer mask",{},"Images (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff)");
        if(!path.isEmpty())apply([&]{if(auto *current=m->active();current&&!current->locked)current->style[key+"maskFile"]=path;});
    });
    auto *clearMask=new QPushButton("Clear mask");texture->addRow(clearMask);
    choice(texture,"Mask channel",key+"maskMode",{"Alpha","Luminance","Inverse alpha","Inverse luminance"},"Alpha");
    sync.append([=,this]{if(auto *current=m->active()){
        const auto file=current->style.value(key+"maskFile").toString();
        mask->setText(file.isEmpty()?"Choose mask image…":QFileInfo(file).fileName());
        clearMask->setVisible(!file.isEmpty());
    }});
    connect(clearMask,&QPushButton::clicked,this,[=,this]{
        apply([&]{if(auto *current=m->active();current&&!current->locked)current->style.remove(key+"maskFile");});
    });
    auto *clear=new QPushButton("Remove texture");clear->setObjectName("removeSurfaceTexture");texture->addRow(clear);
    connect(clear,&QPushButton::clicked,this,[=,this]{apply([&]{
        auto *current=m->active();if(!current || current->locked)return;
        if(id=="base"){current->style.remove("texture.file");return;}
        auto latest=TextureLayers::order(*current);latest.removeAll(id);
        current->style["texture.stack"]=QJsonArray::fromStringList(latest);
        for(const auto &field:current->style.keys())if(field.startsWith(key))current->style.remove(field);
        for(const auto &field:current->defaults.keys())if(field.startsWith(key))current->defaults.remove(field);
        for(const auto &field:current->overrides.keys())if(field.startsWith(key))current->overrides.remove(field);
        current->tracks.removeIf([&](const PropertyTrack &track){return track.property.startsWith(key);});
        current->refreshLegacyKeys();
        selectedTextureLayers[current->id]=latest.last();
    });});
}

void PropertyInspector::rebuild(){
    auto *o=m->active();QString sig=o?o->id+o->type+QString::number(m->sel.size()):QString("none");
    if(o && o->hasSurface())sig+=QString::fromUtf8(QJsonDocument(o->style.value("mask.stack").toArray()).toJson(QJsonDocument::Compact))+QString::number(o->locked);
    if(o && o->hasSurface())sig+=QString::fromUtf8(QJsonDocument(o->style.value("texture.stack").toArray()).toJson(QJsonDocument::Compact))+selectedTextureLayers.value(o->id);
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
        audio->addRow("Source",new QLabel(QFileInfo(o->style.value("audio.file").toString()).fileName()));
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
    if(!multi && o->style.value("meshEmitter.enabled").toBool()){
        auto *form=section("MESH EMITTER",false);
        auto *pick=new QPushButton;pick->setObjectName("pickParticleMesh");form->addRow("Prototype",pick);
        pick->setToolTip("Pick a mesh in the Work viewport. Its evaluated geometry and material are saved with the emitter.");
        connect(pick,&QPushButton::clicked,this,[this]{if(auto *active=m->active())emit m->emitterMeshPickRequested(active->id);});
        auto *capacity=new QLabel;capacity->setWordWrap(true);form->addRow(capacity);
        sync.append([=,this]{if(auto *active=m->active()){
            pick->setText(active->style.value("meshEmitter.prototype").toString("Pick mesh…"));pick->setEnabled(!active->locked);
            const int limit=MeshEmitter::capacity(active->mesh);
            const bool limited=active->propertyValue("meshEmitter.count",m->t)>limit;
            capacity->setVisible(limited);
            if(limited)capacity->setText(QString("This prototype supports %1 simultaneous instances.").arg(limit));
        }});
        number(form,"Count","meshEmitter.count",1,256,1);
        number(form,"Start time","meshEmitter.start",0,86400,.1," s");
        number(form,"Lifetime","meshEmitter.life",.05,120,.05," s");
        number(form,"Size","meshEmitter.size",.001,100,.01);
        number(form,"Rise speed","meshEmitter.speed",-100,100,.05);
        number(form,"Side spread","meshEmitter.spread",0,100,.05);
        number(form,"Depth spread","meshEmitter.depth",0,100,.05);
        number(form,"Gravity","meshEmitter.gravity",-100,100,.05);
        number(form,"Spin","meshEmitter.spin",-1440,1440,1," °/s");
        number(form,"Seed","meshEmitter.seed",0,10000,1);
        particlePathControls(form);
    }
    if(!multi && (o->surfaceType()=="Text"||o->surfaceType()=="Image"||o->surfaceType()=="Shape")){
        auto *size=section("LAYOUT");number(size,"Width","layout.width",1,8192,1," px");number(size,"Height","layout.height",1,8192,1," px");
        auto *pin = new QComboBox;
        pin->setObjectName("framePin");
        pin->addItem("As authored",QString());
        pin->addItem("Top edge","Top");
        pin->addItem("Bottom edge","Bottom");
        pin->addItem("Relative height","Relative");
        pin->setToolTip("Keep the edge margin or relative height when the output aspect ratio changes.");
        size->addRow("Frame pin",pin);
        connect(pin,&QComboBox::activated,this,[=,this](int index){
            apply([&]{
                auto *object = m->active();
                if (!object || object->locked) return;
                const auto mode = pin->itemData(index).toString();
                if (mode.isEmpty()) {
                    object->style.remove("layout.framePin");
                    object->style.remove("layout.pinAspect");
                } else {
                    if (!object->style.contains("layout.framePin")) object->style["layout.pinAspect"] = lib->display.aspect();
                    object->style["layout.framePin"] = mode;
                }
            });
        });
        sync.append([=,this]{if (auto *object=m->active()) {
            QSignalBlocker blocker(pin);
            pin->setCurrentIndex(qMax(0,pin->findData(object->style.value("layout.framePin").toString())));
            pin->setEnabled(!object->locked);
        }});
        if(o->surfaceType()=="Text"){
            auto *fit=new QCheckBox("Fit background to text");fit->setObjectName("textAutoFit");size->addRow(fit);
            auto *axes=new QWidget;auto *row=new QHBoxLayout(axes);row->setContentsMargins(0,0,0,0);auto *x=new QCheckBox("X"),*y=new QCheckBox("Y");row->addWidget(x);row->addWidget(y);row->addStretch();size->addRow("Adapt axes",axes);
            for(auto pair:{qMakePair(fit,QString("text.autoFit")),qMakePair(x,QString("text.autoFitX")),qMakePair(y,QString("text.autoFitY"))})connect(pair.first,&QCheckBox::toggled,this,[=,this](bool value){apply([&]{if(auto *object=m->active())object->style[pair.second]=value;});});
            sync.append([=,this]{if(auto *object=m->active()){QSignalBlocker a(fit),b(x),c(y);fit->setChecked(object->style.value("text.autoFit").toBool());x->setChecked(object->style.value("text.autoFitX").toBool(true));y->setChecked(object->style.value("text.autoFitY").toBool(true));axes->setVisible(fit->isChecked());}});
            number(size,"Text padding","text.padding",0,512,1," px");
        }
        if(o->surfaceType()=="Shape")choice(size,"Shape","shape",{"Rectangle","Ellipse","Triangle"},"Rectangle");
        auto *aspectLock=new QCheckBox("Maintain aspect ratio");size->addRow(aspectLock);
        auto *unlit=new QCheckBox("Ignore lighting");size->addRow(unlit);connect(unlit,&QCheckBox::toggled,this,[=,this](bool value){apply([&]{if(auto *object=m->active())object->mat.shadeless=value;});});sync.append([=,this]{if(auto *object=m->active())unlit->setChecked(object->mat.shadeless);});
        connect(aspectLock,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *x=m->active())x->style["layout.aspectLock"]=on;});});
        sync.append([=,this]{if(auto *x=m->active())aspectLock->setChecked(x->style.value("layout.aspectLock").toBool());});
        {
        auto *corners=section("CORNERS",false);number(corners,"All corners","corner.radius",0,2000,1," px");
        choice(corners,"Units","corner.unit",{"px","%"},"px");
        auto *linked=new QCheckBox("Link corners");linked->setToolTip("Use one radius for all corners");corners->addRow(linked);
        connect(linked,&QCheckBox::toggled,this,[=,this](bool value){apply([&]{for(auto *object:m->inspected())if(!object->locked)object->style["corner.linked"]=value;});});
        sync.append([=,this]{if(auto *object=m->active())linked->setChecked(object->style.value("corner.linked").toBool(true));});
        for(auto key:QStringList{"tl","tr","br","bl"})number(corners,key.toUpper(),"corner."+key,0,2000,1," px");
        sync.append([=,this]{bool show=!linked->isChecked();for(int row=3;row<corners->rowCount();++row)corners->setRowVisible(row,show);});
        auto *stroke=section("STROKE",false);number(stroke,"Width","stroke.width",0,100,1," px");color(stroke,"Color","stroke.color","#ffffff");choice(stroke,"Style","stroke.style",{"Solid","Dashed","Dotted"},"Solid");
        choice(stroke,"Join","stroke.join",{"Miter","Round","Bevel"},"Round");
        }
        QFormLayout *backgroundForm=nullptr;
        if(o->surfaceType()=="Text"){
            auto *background=section("TEXT BACKGROUND");auto *enabled=new QCheckBox("Background");background->addRow(enabled);
            connect(enabled,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{m->active()->style["text.background"]=on;});});sync.append([=,this]{enabled->setChecked(m->active()->style.value("text.background").toBool());});color(background,"Color","text.backgroundColor","#cc202020");
        }
        {
            auto *fill=section(o->surfaceType()=="Text"?"FILL":"BACKGROUND");
            backgroundForm=fill;
            if(o->surfaceType()!="Text")choice(fill,"Type","background.type",{"Solid","Gradient","Media"},o->surfaceType()=="Image"?"Media":o->style.value("fill.kind").toString("Solid")=="Solid"?"Solid":"Gradient");
            color(fill,"Color / start","fill.color","#ffffff");choice(fill,o->surfaceType()=="Text"?"Type":"Gradient","fill.kind",o->surfaceType()=="Text"?QStringList{"Solid","Linear","Radial","Conic"}:QStringList{"Linear","Radial","Conic"},o->surfaceType()=="Text"?"Solid":"Linear");
            color(fill,"Middle","fill.middle","#6366f1");number(fill,"Middle stop","fill.stop",.001,.999,.05);color(fill,"End","fill.end","#a855f7");number(fill,"Angle","fill.angle",0,360,1,"°");
            if(o->surfaceType()!="Text")sync.append([=,this]{if(auto *object=m->active()){auto type=object->style.value("background.type").toString(object->surfaceType()=="Image"?"Media":object->style.value("fill.kind").toString("Solid")=="Solid"?"Solid":"Gradient");fill->setRowVisible(1,type!="Media");for(int row=2;row<7;++row)fill->setRowVisible(row,type=="Gradient");}});
        }
        if(o->surfaceType()!="Text"){
            auto *mediaBody=new QWidget;auto *media=new QFormLayout(mediaBody);media->setContentsMargins(0,0,0,0);backgroundForm->addRow(mediaBody);choice(media,"Source type","media.kind",{"Image","Video"},"Image");
            sync.append([=,this]{if(auto *object=m->active())mediaBody->setVisible(object->style.value("background.type").toString(object->surfaceType()=="Image"?"Media":"Solid")=="Media");});
            auto *file=new QPushButton("Choose file…");media->addRow(file);file->setToolTip("Use an image or video as this object's background");
            connect(file,&QPushButton::clicked,this,[=,this]{auto *object=m->active();if(!object||object->locked)return;bool video=object->style.value("media.kind").toString()=="Video";
                const QString path=QFileDialog::getOpenFileName(this,video?"Video background":"Image background",{},video?"Video (*.mp4 *.mov *.mkv *.webm *.avi *.mxf)":"Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.svg *.ico)");if(path.isEmpty())return;
                QString error;const auto duration=video?MediaFrames::duration(path,&error):0;if(video&&!error.isEmpty()){QMessageBox::warning(this,"Video background",error);return;}
                if(!video&&QImage(path).isNull()){QMessageBox::warning(this,"Image background","This image could not be opened.");return;}
                apply([&]{object->image=path;object->style["background.type"]="Media";object->style["media.duration"]=double(duration);});
            });
            choice(media,"Fit","image.fit",{"Contain","Cover","Fill","None","Scale Down"},"Contain");number(media,"Position X","image.x",0,1,.05);number(media,"Position Y","image.y",0,1,.05);
            number(media,"Trim start","media.offset",0,86400000,40," ms");number(media,"Playback speed","media.speed",.1,4,.1);number(media,"Audio volume","media.volume",0,1,.05);
            auto *loop=new QCheckBox("Loop video");media->addRow(loop);connect(loop,&QCheckBox::toggled,this,[=,this](bool value){apply([&]{if(auto *object=m->active())object->style["media.loop"]=value;});});sync.append([=,this]{if(auto *object=m->active())loop->setChecked(object->style.value("media.loop").toBool(true));});
            sync.append([=,this]{if(auto *object=m->active())for(int row=5;row<media->rowCount();++row)media->setRowVisible(row,object->style.value("media.kind").toString()=="Video");});
        }
        auto *shadow=section("SHADOW",false);auto *on=new QCheckBox("Drop shadow");shadow->addRow(on);
        connect(on,&QCheckBox::toggled,this,[=, this](bool v){apply([&]{if(auto *x=m->active())x->style["shadow.enabled"]=v;});});
        sync.append([=, this]{if(auto *x=m->active())on->setChecked(x->style.value("shadow.enabled").toBool());});
        number(shadow,"X","shadow.x",-1000,1000,1," px");number(shadow,"Y","shadow.y",-1000,1000,1," px");number(shadow,"Blur","shadow.blur",0,80,1," px");color(shadow,"Color","shadow.color","#99000000");
        if(o->surfaceType()=="Text"){
            auto *body=new QWidget;auto *form=new QFormLayout(body);form->setContentsMargins(8,4,0,0);shadow->addRow(body);
            auto *enabled=new QCheckBox("Text shadow");form->addRow(enabled);
            connect(enabled,&QCheckBox::toggled,this,[=,this](bool value){apply([&]{if(auto *object=m->active())object->style["textShadow.enabled"]=value;});});
            sync.append([=,this]{if(auto *object=m->active()){QSignalBlocker block(enabled);enabled->setChecked(object->style.value("textShadow.enabled").toBool());body->setVisible(object->style.value("text.background").toBool());}});
            number(form,"X","textShadow.x",-1000,1000,1," px");number(form,"Y","textShadow.y",-1000,1000,1," px");number(form,"Blur","textShadow.blur",0,80,1," px");color(form,"Color","textShadow.color","#99000000");
        }
        auto *effects=section("EFFECTS",false);
        maskControls();
        auto *boolean=section("SURFACE BOOLEAN");
        choice(boolean,"Operation","boolean.operation",{"Off","Difference","Intersect","Union"},"Off");
        auto *operand=new QComboBox;operand->setObjectName("surfaceBooleanSource");operand->setAccessibleName("Boolean source");
        operand->setToolTip("Use the text, image or shape of another object. Hide the source to show just the result.");
        boolean->addRow("Source",operand);
        connect(operand,&QComboBox::activated,this,[this,operand](int index){
            if(auto *o=m->active();o && !o->locked)apply([&]{o->style["boolean.source"]=operand->itemData(index).toString();});
        });
        sync.append([this,operand]{
            const auto *object=m->active();if(!object)return;
            QList<QPair<QString,QString>> choices{{"Choose an object",{}}};
            for(const auto &candidate:m->design.objects){
                if(!candidate.hasSurface() || candidate.id==object->id)continue;
                QSet<QString> visited{object->id};const DObject *node=&candidate;
                while(node && !visited.contains(node->id)){
                    visited.insert(node->id);node=m->design.find(node->style.value("boolean.source").toString());
                }
                if(!node)choices.append({candidate.name,candidate.id});
            }
            QSignalBlocker block(operand);
            bool changed=operand->count()!=choices.size();
            for(int i=0;!changed && i<choices.size();++i)changed=operand->itemData(i)!=choices[i].second || operand->itemText(i)!=choices[i].first;
            if(changed){operand->clear();for(const auto &[name,id]:choices)operand->addItem(name,id);}
            operand->setCurrentIndex(qMax(0,operand->findData(object->style.value("boolean.source").toString())));
            operand->setEnabled(!object->locked && object->style.value("boolean.operation").toString("Off")!="Off");
        });
        auto *finish=section("SURFACE FINISH");
        choice(finish,"Light sweep","finish.sweep",{"Off","On"},"Off");
        number(finish,"Sweep position","finish.position",-.5,1.5,.01);
        number(finish,"Band width","finish.band",.005,1,.01);
        number(finish,"Brightness","finish.intensity",0,1,.01);
        number(finish,"Sweep angle","finish.angle",-360,360,1,"°");
        color(finish,"Sweep color","finish.tint","#ffffff");
        choice(finish,"Bevel","finish.bevel",{"Off","On"},"Off");
        number(finish,"Edge width","finish.edge",1,24,1," px");
        choice(finish,"Extrusion","finish.extrude",{"Off","On"},"Off");
        number(finish,"Depth","finish.depth",0,64,1," px");
        color(finish,"Side color","finish.sideColor","#293444");
        choice(finish,"Reflection","finish.reflection",{"Off","On"},"Off");
        number(finish,"Reflection height","finish.height",0,256,1," px");
        number(finish,"Gap","finish.gap",0,100,1," px");
        number(finish,"Reflection opacity","finish.reflectOpacity",0,1,.01);
        sync.append([this,finish]{
            if(auto *object=m->active()){
                const auto show=[&](const char *key,int first,int last){
                    const bool enabled=object->style.value(key).toString()=="On";
                    for(int row=first;row<=last;++row)finish->setRowVisible(row,enabled);
                };
                show("finish.sweep",1,5);show("finish.bevel",7,7);
                show("finish.extrude",9,10);show("finish.reflection",12,14);
            }
        });
        number(effects,"Backdrop blur","backdrop.blur",0,80,1," px");number(effects,"Backdrop brightness","backdrop.brightness",0,3,.05);
        number(effects,"Blur","effects.blur",0,80,1," px");
        const bool emitter=o->style.value("effects.emitter").toBool();
        if(!emitter){
        number(effects,"Reveal","effects.reveal",0,1,.01);
        choice(effects,"Reveal style","effects.revealStyle",{"Wipe","Pixels","Blocks","Liquid","Assemble"},"Wipe");
        choice(effects,"Reveal timing","effects.revealTiming",{"Linear","Smooth","Constant","Steps","Flicker"},"Linear");
        number(effects,"Block size","effects.revealBlock",2,512,1," px");number(effects,"Scatter distance","effects.revealSpread",0,4,.05);number(effects,"Timing steps","effects.revealSteps",2,100,1);number(effects,"Reveal seed","effects.revealSeed",0,10000,1);
        choice(effects,"Reveal mode","effects.revealMode",{"Linear","Radial"},"Linear");
        number(effects,"Reveal roughness","effects.revealRoughness",0,1,.01);
        number(effects,"Reveal softness","effects.revealSoftness",0,.2,.005);
        number(effects,"Reveal direction","effects.revealAngle",-180,180,1);
        number(effects,"Reveal center X","effects.revealX",0,1,.01);
        number(effects,"Reveal center Y","effects.revealY",0,1,.01);
        }else{
            number(effects,"Start time","effects.emitterStart",0,86400,.1," s");
            choice(effects,"Emit from","effects.emitterSource",{"Point","Area"},"Point");
            number(effects,"Seed","effects.revealSeed",0,10000,1);
        }
        choice(effects,emitter?"Particles":"Reveal particles","effects.revealParticles",
               {"None","Dust","Sparks","Embers","Fire","Smoke","Fog","Debris","Water droplets","Energy trails","Mesh stamp"},"None");
        const auto dynamicsKey="inspector/objects/"+o->id+"/particleDynamics";
        auto *dynamics=new QToolButton;dynamics->setText("Particle settings");dynamics->setCheckable(true);
        dynamics->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        dynamics->setChecked(QSettings().value(dynamicsKey,false).toBool());
        effects->addRow(dynamics);
        auto *controls=new QWidget;auto *particleForm=new QFormLayout(controls);
        particleForm->setContentsMargins(12,2,0,4);particleForm->setSpacing(4);effects->addRow(controls);
        auto expandDynamics=[=](bool open){
            dynamics->setIcon(uiIcon(open?"down":"right"));controls->setVisible(open);
        };
        expandDynamics(dynamics->isChecked());
        connect(dynamics,&QToolButton::toggled,this,[=](bool open){expandDynamics(open);QSettings().setValue(dynamicsKey,open);});
        number(particleForm,"Count","effects.particleCount",1,1024,1);
        number(particleForm,"Size","effects.particleSize",.5,128,.5," px");
        number(particleForm,"Travel","effects.particleTravel",0,8,.05);
        number(particleForm,"Movement speed","effects.particleSpeed",.05,8,.05," ×");
        if(emitter)number(particleForm,"Lifetime","effects.emitterLifetime",.05,120,.05," s");
        else number(particleForm,"Lifetime (% of reveal)","effects.particleLifetime",1,100,1," %");
        number(particleForm,"Emission angle","effects.particleAngle",-180,180,1,"°");
        number(particleForm,"Size and drift variation","effects.particleRandomness",0,1,.05);
        number(particleForm,"Turbulence","effects.particleTurbulence",0,8,.05);
        number(particleForm,"Gravity","effects.particleGravity",-8,8,.05);
        auto *stampButton=new QToolButton;
        stampButton->setObjectName("revealMeshStamp");stampButton->setPopupMode(QToolButton::InstantPopup);
        stampButton->setToolTip("Capture a mesh silhouette and its material color for these particles.");
        auto *stamps=new QMenu(stampButton);stampButton->setMenu(stamps);
        effects->addRow("Particle mesh",stampButton);
        particlePathControls(effects);
        connect(stamps,&QMenu::aboutToShow,this,[this,stamps]{
            stamps->clear();
            for(const auto &mesh:m->design.objects){
                if(mesh.type!="Mesh" || mesh.id==m->activeId)continue;
                auto *action=stamps->addAction(mesh.name);
                connect(action,&QAction::triggered,this,[this,id=mesh.id]{
                    const auto *source=m->byId(id);auto *target=m->active();
                    if(!source || !target || target->locked)return;
                    const auto stamp=RevealEffects::meshStamp(m->design.evalMesh(*source,m->t),source->evaluated(m->t).mat.color);
                    if(stamp.isEmpty())return;
                    apply([&]{target->style["effects.revealStamp"]=stamp;target->style["effects.revealStampName"]=source->name;});
                });
            }
            if(stamps->isEmpty())stamps->addAction("No other mesh in this scene")->setEnabled(false);
        });
        sync.append([=,this]{
            if(const auto *active=m->active()){
                const auto kind=active->style.value("effects.revealParticles").toString("None");
                effects->setRowVisible(dynamics,kind!="None");
                effects->setRowVisible(controls,kind!="None" && dynamics->isChecked());
                effects->setRowVisible(stampButton,kind=="Mesh stamp");
                stampButton->setEnabled(!active->locked);
                stampButton->setText(active->style.value("effects.revealStampName").toString("Choose mesh…"));
            }
        });
        for(const auto &path:QStringList{"brightness","contrast","saturation"})number(effects,path,"effects."+path,0,3,.05);
        for(const auto &path:QStringList{"grayscale","invert","sepia"})number(effects,path,"effects."+path,0,1,.05);
        number(effects,"Hue","effects.hue",-360,360,1,"°");
    }
    if(!multi&&(o->surfaceType()=="Text"||o->surfaceType()=="Image")){
        auto *studio=section("STUDIO FIELD",false);auto *enabled=new QCheckBox("Editable in Studio");enabled->setChecked(o->style.value("studio.editable").toBool());studio->addRow(enabled);
        enabled->setToolTip("Expose this object's content in Screen Elements without changing its animation. Text shorthand: {{studio:Sponsor}}Your text");
        auto *label=new QLineEdit(o->style.value("studio.label").toString(o->name));studio->addRow("Field name",label);
        sync.append([=,this]{if(auto *x=m->active()){QSignalBlocker block(enabled);enabled->setChecked(x->style.value("studio.editable").toBool());if(!label->hasFocus()){QSignalBlocker labelBlock(label);label->setText(x->style.value("studio.label").toString(x->name));}}});
        auto *fit=new QCheckBox("Fit long text inside its box");fit->setChecked(o->style.value("studio.fit").toBool(true));if(o->surfaceType()=="Text")studio->addRow(fit);else fit->deleteLater();
        connect(enabled,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *x=m->active()){x->style["studio.editable"]=on;x->style["studio.label"]=label->text();}});});
        connect(label,&QLineEdit::editingFinished,this,[=,this]{apply([&]{if(auto *x=m->active())x->style["studio.label"]=label->text().trimmed();});});
        if(o->surfaceType()=="Text")connect(fit,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *x=m->active())x->style["studio.fit"]=on;});});
    }
    if(!multi && o->surfaceType()=="Text"){
        auto *text=section("TYPOGRAPHY");auto *content=new QPlainTextEdit(o->text);content->setObjectName("textContent");content->setAccessibleName("Text content");content->setMaximumHeight(70);text->addRow(content);
        sync.append([=,this]{if(auto *object=m->active();object&&!content->hasFocus()&&content->toPlainText()!=object->text){QSignalBlocker blocker(content);content->setPlainText(object->text);}});
        connect(content,&QPlainTextEdit::textChanged,this,[=, this]{apply([&]{if(auto *x=m->active()){if(parseStudioText(*x,content->toPlainText())){QSignalBlocker block(content);content->setPlainText(x->text);}}});});
        choice(text,"Formatting","text.format",{"Plain text","Markdown"},"Plain text");
        content->setToolTip("Markdown: **bold**, *italic*, ~~strike~~, # heading, - list, > quote, `code`. Choose Markdown below. Studio field prefixes still work.");
        auto *family=new QFontComboBox;family->setCurrentFont(QFont(o->style.value("text.family").toString("Segoe UI")));text->addRow("Font",family);
        connect(family,&QFontComboBox::currentFontChanged,this,[=, this](const QFont &font){apply([&]{if(auto *x=m->active())x->style["text.family"]=font.family();});});
        number(text,"Size","text.size",1,1000,1," px");
        auto *advanced=section("ADVANCED TYPOGRAPHY",false);number(advanced,"Weight","text.weight",100,900,100);
        auto *italic=new QCheckBox("Italic");advanced->addRow(italic);
        connect(italic,&QCheckBox::toggled,this,[=,this](bool on){apply([&]{if(auto *x=m->active();x&&!x->locked)x->style["text.italic"]=on;});});
        sync.append([=,this]{if(auto *x=m->active()){italic->setChecked(x->style.value("text.italic").toBool());italic->setEnabled(!x->locked);}});
        number(advanced,"Line height","text.lineHeight",.2,5,.1);number(advanced,"Tracking","text.tracking",-30,200,.5," px");number(advanced,"Word spacing","text.wordSpacing",-30,200,.5," px");
        choice(advanced,"Align","text.align",{"Left","Center","Right","Justify"},"Center");choice(advanced,"Vertical","text.vertical",{"Top","Center","Bottom"},"Center");
        choice(advanced,"Transform","text.transform",{"None","Uppercase","Lowercase","Capitalize"},"None");choice(advanced,"Decoration","text.decoration",{"None","Underline","Line Through"},"None");
    }
    if(!multi && o->surfaceType()=="Image"){
        auto *image=section("IMAGE");auto *source=new QPushButton("Choose image…");image->addRow(source);
        connect(source,&QPushButton::clicked,this,[this]{QString path=QFileDialog::getOpenFileName(this,"Image",{},"Images (*.png *.jpg *.jpeg *.webp *.bmp)");if(!QImage(path).isNull())apply([&]{m->active()->image=path;});});
        choice(image,"Fit","image.fit",{"Fill","Contain","Cover","None","Scale Down"},"Contain");number(image,"Horizontal","image.x",0,1,.05);number(image,"Vertical","image.y",0,1,.05);
    }
    if(!multi&&(o->surfaceType()=="Shape"||o->surfaceType()=="Text"||o->surfaceType()=="Image")){
        textureControls(*o);
        auto *overflow=section("OVERFLOW",false);choice(overflow,"X","overflow.x",{"Visible","Hidden"},"Visible");choice(overflow,"Y","overflow.y",{"Visible","Hidden"},"Visible");
    }
    if(!multi && o->type=="Mesh" && !o->hasSurface()){
        auto *material=section("MATERIAL");color(material,"Base color","fill.color","#c8ccd4");number(material,"Roughness","material.roughness",0,1,.05);number(material,"Emission","material.emission",0,5,.1);number(material,"Alpha","material.alpha",0,1,.05);
        number(material,"Metallic","material.metallic",0,1,.05);
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
    if (o->surfaceType()=="Text" && !multi) {
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
