#include "ElementLibrary.h"
#include <QPainterPath>
#include "SceneEffects.h"
#include "UiControls.h"
#include <QBuffer>
#include <QDialog>
#include <QFrame>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QFileInfo>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QImageReader>
#include "MediaFrames.h"

bool parseStudioText(DObject &object,const QString &input){
    static const QRegularExpression prefix("^\\{\\{studio:([^{}\\r\\n]{1,64})\\}\\}");
    const auto match=prefix.match(input);object.text=input;
    if(!match.hasMatch()||match.captured(1).trimmed().isEmpty())return false;
    object.style["studio.editable"]=true;object.style["studio.label"]=match.captured(1).trimmed();
    object.style["studio.fit"]=true;object.text=input.mid(match.capturedLength());return true;
}
bool setStudioField(ElementDesign &design,const QString &id,const QString &value,QString *error){
    auto fail=[&](QString reason){if(error)*error=reason;return false;};auto *object=design.find(id);
    if(!object||!object->style["studio.editable"].toBool())return fail("This object is not exposed for Studio editing.");
    if(object->type=="Text"){if(value.size()>10000)return fail("Text must contain no more than 10,000 characters.");object->text=value;return true;}
    if(object->type=="Image"){
        if(!QFileInfo(value).isFile())return fail("Choose an existing image or video file.");
        const auto suffix=QFileInfo(value).suffix().toLower();
        if(!QStringList{"png","jpg","jpeg","webp","bmp","svg","gif","mp4","mov","mkv","webm","avi"}.contains(suffix))return fail("Choose a supported image or video.");
        if(QStringList{"mp4","mov","mkv","webm","avi"}.contains(suffix)){QString reason;if(MediaFrames::duration(value,&reason)<=0)return fail(reason.isEmpty()?"This video cannot be decoded.":reason);}
        else if(!QImageReader(value).canRead())return fail("This image cannot be decoded.");
        object->image=value;return true;
    }return fail("Only text and media objects can be edited here.");
}
static QImage renderElement(const Element &element,qint64 time,QSize size){
    const auto design=ElementDesign::fromJson(element.design);
    if(sceneEffect(design))return sceneEffectPreview(design,size,time);
    QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter p(&image);
    if(!element.design.isEmpty())ElementDesign::fromJson(element.design).render(p,QRectF(image.rect()),time,true,nullptr,false);
    else if(!element.image.isEmpty())p.drawImage(QRectF(image.rect()),QImage(element.image));
    else {p.setPen(QColor(element.color));QFont f("Segoe UI");f.setPixelSize(32);f.setBold(true);p.setFont(f);p.drawText(image.rect().adjusted(12,12,-12,-12),Qt::AlignCenter|Qt::TextWordWrap,element.text.isEmpty()?element.name:element.text);}
    return image;
}
QImage elementThumbnail(const Element &element){QImage image;image.loadFromData(QByteArray::fromBase64(element.thumbnail.toLatin1()),"PNG");if(!image.isNull())return image;return renderElement(element,element.previewMs,{480,270});}
void refreshElementThumbnail(Element &element){
    auto image=renderElement(element,element.previewMs,{960,540});
    QRectF crop(element.previewX,element.previewY,element.previewW,element.previewH);crop=crop.intersected(QRectF(0,0,1,1));if(crop.width()<.01||crop.height()<.01)crop={0,0,1,1};
    image=image.copy(QRect(qRound(crop.x()*image.width()),qRound(crop.y()*image.height()),qMax(1,qRound(crop.width()*image.width())),qMax(1,qRound(crop.height()*image.height())))).scaled(480,270,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");element.thumbnail=QString::fromLatin1(bytes.toBase64());
}
ElementPreview::ElementPreview(QWidget *parent):QWidget(parent){setMinimumSize(240,135);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);}
void ElementPreview::setElement(const Element &value){if(element.design!=value.design)design=ElementDesign::fromJson(value.design);element=value;update();}
void ElementPreview::setTime(qint64 value){time=value;update();}
QRectF ElementPreview::frame()const{QSizeF size(16,9);size.scale(width(),height(),Qt::KeepAspectRatio);return QRectF(QPointF((width()-size.width())/2,(height()-size.height())/2),size);}
void ElementPreview::paintEvent(QPaintEvent*){QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QPainterPath rounded;rounded.addRoundedRect(rect(),8,8);p.setClipPath(rounded);p.fillRect(rect(),QColor("#242527"));auto r=frame();p.save();p.setClipRect(r,Qt::IntersectClip);for(int y=r.top();y<r.bottom();y+=12)for(int x=r.left();x<r.right();x+=12)p.fillRect(QRect(x,y,12,12),((x-int(r.left()))/12+(y-int(r.top()))/12)%2?QColor("#35363a"):QColor("#3a3b3f"));
    if(sceneEffect(design))p.drawImage(r,sceneEffectPreview(design,r.size().toSize(),time));
    else if(!element.design.isEmpty())design.render(p,r,time,true,nullptr,false);else p.drawImage(r,renderElement(element,time,{480,270}));p.restore();
    if(cropping){QRectF selected(r.x()+crop.x()*r.width(),r.y()+crop.y()*r.height(),crop.width()*r.width(),crop.height()*r.height());p.setPen(QPen(QColor("#86b9ef"),2));p.setBrush(QColor(70,130,200,25));p.drawRect(selected);}
}
void ElementPreview::mousePressEvent(QMouseEvent *e){if(!cropping||e->button()!=Qt::LeftButton)return;auto r=frame();cropStart={(e->position().x()-r.x())/r.width(),(e->position().y()-r.y())/r.height()};cropStart.setX(qBound(0.,cropStart.x(),1.));cropStart.setY(qBound(0.,cropStart.y(),1.));}
void ElementPreview::mouseMoveEvent(QMouseEvent *e){if(!cropping||!(e->buttons()&Qt::LeftButton))return;auto r=frame();QPointF end(qBound(0.,(e->position().x()-r.x())/r.width(),1.),qBound(0.,(e->position().y()-r.y())/r.height(),1.));crop=QRectF(cropStart,end).normalized();update();}
void ElementPreview::mouseReleaseEvent(QMouseEvent*){if(crop.width()<.01||crop.height()<.01)crop={0,0,1,1};update();}
bool editElementPresentation(Element &element,QWidget *parent){
    QDialog dialog(parent);dialog.setWindowTitle("Screen element preview");dialog.setWindowFlag(Qt::FramelessWindowHint);dialog.setAttribute(Qt::WA_TranslucentBackground);dialog.resize(640,560);dialog.setObjectName("elementPreviewDialog");
    dialog.setStyleSheet("QWidget{background:transparent;font-size:11px;color:#dedee3;}QFrame#previewCard{background:#2b2c30;border:1px solid #55575d;border-radius:12px;}QLineEdit,QPlainTextEdit,QComboBox{background:#404248;border:0;border-radius:4px;padding:5px;}QPushButton{background:#474b54;border:0;border-radius:5px;padding:6px 12px;}");
    auto *outer=new QVBoxLayout(&dialog);outer->setContentsMargins(0,0,0,0);auto *card=new QFrame;card->setObjectName("previewCard");outer->addWidget(card);
    auto *layout=new QVBoxLayout(card);layout->setContentsMargins(16,16,16,16);auto *title=new QLabel("Choose how this element appears in the library");layout->addWidget(title);
    auto *preview=new ElementPreview;preview->setElement(element);preview->time=element.previewMs;preview->cropping=true;preview->crop={element.previewX,element.previewY,element.previewW,element.previewH};layout->addWidget(preview,1);
    auto *slider=new QSlider(Qt::Horizontal);slider->setRange(0,qMax<qint64>(1,element.lengthMs()));slider->setValue(element.previewMs);layout->addWidget(slider);QObject::connect(slider,&QSlider::valueChanged,preview,[=](int value){preview->setTime(value);});
    auto *hint=new QLabel("Drag a rectangle to crop the thumbnail. Scrub to choose a recognizable frame.");hint->setWordWrap(true);layout->addWidget(hint);auto *full=new QPushButton("Full screen");layout->addWidget(full);QObject::connect(full,&QPushButton::clicked,preview,[=]{preview->crop={0,0,1,1};preview->update();});
    auto *form=new QFormLayout;auto *name=new QLineEdit(element.name);auto *description=new QPlainTextEdit(element.description);description->setMaximumHeight(60);auto *group=new QComboBox;group->setEditable(true);group->addItems({"Overlays","Break screens","Logos","Effects"});group->setCurrentText(element.playlist);form->addRow("Name",name);form->addRow("Description",description);form->addRow("Playlist",group);layout->addLayout(form);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(name->text().trimmed().isEmpty()){name->setFocus();return;}dialog.accept();});
    if(dialog.exec()!=QDialog::Accepted)return false;element.name=name->text().trimmed();element.description=description->toPlainText();element.playlist=group->currentText().trimmed();if(element.playlist.isEmpty())element.playlist="Overlays";element.previewMs=slider->value();element.previewX=preview->crop.x();element.previewY=preview->crop.y();element.previewW=preview->crop.width();element.previewH=preview->crop.height();refreshElementThumbnail(element);return true;
}
QStringList elementPresetNames(){return {"Sponsor spotlight","News lower third","Be right back","Channel ident"};}
ElementDesign elementPreset(const QString &name){
    auto d=ElementDesign::defaultScene();d.durationMs=4000;d.loop=false;d.camera()->cam.orthoScale=16;
    const bool lower=name=="News lower third",br=name=="Be right back",logo=name=="Channel ident";
    DObject panel;panel.id=Library::newId();panel.type="Shape";panel.name="Panel";panel.w=logo?2.8:lower?12:13;panel.h=logo?2.8:lower?1.7:5;panel.base.rot={90,0,0};panel.base.loc={0,0,lower?-3.f:0.f};panel.mat.shadeless=true;panel.mat.color=QColor("#173047");panel.style["shape"]="Rectangle";panel.style["corner.radius"]=24;panel.style["background.type"]="Gradient";panel.style["fill.middle"]="#235776";panel.style["fill.end"]="#2778ab";
    panel.keyChannel("opacity",0,0);panel.keyChannel("opacity",650,1);panel.keyChannel("opacity",3000,1);panel.keyChannel("opacity",4000,0);panel.captureDefaults();d.objects.append(panel);
    DObject text;text.id=Library::newId();text.type="Text";text.name="Headline";text.text=logo?"CB":br?"WE’LL BE RIGHT BACK":lower?"The story starts here":"Sponsored by NVIDIA";text.w=panel.w-.7;text.h=panel.h-.4;text.fontPx=logo?130:lower?65:90;text.base=panel.base;text.base.loc.setY(-.02);text.mat.shadeless=true;text.mat.color=Qt::white;text.style["studio.editable"]=true;text.style["studio.label"]=logo?"Channel initials":br?"Break message":lower?"Headline":"Sponsor";text.style["studio.fit"]=true;text.style["text.weight"]=700;
    text.keyChannel("opacity",0,0);text.keyChannel("opacity",400,0);text.keyChannel("opacity",1000,1);text.keyChannel("opacity",3000,1);text.keyChannel("opacity",3600,0);text.keyChannel("position.x",0,-1);text.keyChannel("position.x",1000,0);text.captureDefaults();d.objects.append(text);
    d.animationCues=QJsonArray{QJsonObject{{"id",Library::newId()},{"name","Show"},{"start",0},{"end",1000},{"hold",true}},QJsonObject{{"id",Library::newId()},{"name","Hide"},{"start",3000},{"end",4000},{"hold",false}}};return d;
}
