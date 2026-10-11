#include "Continuity.h"
#include "CueSequence.h"
#include "Playout.h"
#include "NetworkOutput.h"
#include "ElementLibrary.h"
#include "DesignAudio.h"
#include "SurfaceEffects.h"
#include "UiControls.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPainter>
#include <QFile>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QToolButton>
#include <QRegularExpression>
#include <QVariant>

namespace {
struct ContinuityScene {ElementDesign design;qint64 time=0;bool advancing=true;QString error;};
ContinuityScene continuityScene(const Element &element,qint64 elapsed){
    auto design=ElementDesign::fromJson(element.design);const auto plan=compileCueBoard(design,element.cueBoard,element.cueBoardSettings);
    if(!plan.error.isEmpty())return {design,0,false,plan.error};if(!element.cueBoard.isEmpty()&&plan.steps.isEmpty())return {design,0,false,"This standby board has no enabled cues."};
    if(!plan.steps.isEmpty()){const auto duration=plan.duration();const auto sample=plan.sample(duration>0?elapsed%duration:elapsed);if(sample.visible)return {ElementDesign::fromJson(plan.steps[sample.step].design),sample.frame,sample.advancing};}
    return {design,design.startMs+elapsed%qMax(1,design.durationMs-design.startMs),true};
}
QImage renderContinuityElement(const Element &element,qint64 time,QSize size){
    QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);
    if(element.type=="Custom"&&!element.design.isEmpty()){const auto scene=continuityScene(element,time);if(!scene.error.isEmpty())return image;scene.design.render(painter,image.rect(),scene.time,true,nullptr,false);}
    else painter.drawImage(image.rect(),elementThumbnail(element));return image;
}
}
ContinuityOutput::ContinuityOutput(Library *library,QObject *parent):QObject(parent),lib(library){player=new QMediaPlayer(this);audio=new QAudioOutput(this);sink=new QVideoSink(this);player->setAudioOutput(audio);player->setVideoSink(sink);player->setLoops(QMediaPlayer::Infinite);designAudio=new DesignAudio(this);designAudio->error=[this](QString reason){setProperty("designError",reason);};designAudio->created=[this](QMediaPlayer *p,QAudioOutput *a){emit audioSourceCreated(p,a);};connect(sink,&QVideoSink::videoFrameChanged,this,[this](const QVideoFrame &frame){video=frame.toImage();emit changed();});timer.setInterval(40);connect(&timer,&QTimer::timeout,this,[this]{if(enabled){syncAudio();emit changed();}});clock.start();}
Element ContinuityOutput::selectedElement()const{QString id=reference;if(kind=="Logo"||kind=="Standby"||kind=="Technical")id=lib->display.channelLogoElement;if(const auto *e=lib->element(id))return *e;return {};}
void ContinuityOutput::setActive(bool value){if(enabled==value)return;enabled=value;if(enabled){clock.restart();if(!media.isEmpty())player->play();timer.start();}else{player->stop();timer.stop();}syncAudio();emit changed();}
bool ContinuityOutput::configure(QString mode,QString ref,QString file,QString copy,QString *error){auto fail=[&](QString s){setProperty("configurationError",s);if(error)*error=s;return false;};if(error)error->clear();if(!QStringList{"Black","Logo","Standby","Technical","Element","Ad","Video"}.contains(mode))return fail("Choose a standby mode.");if((mode=="Video"||mode=="Ad")&&!QFile::exists(file))return fail("Choose an existing video or ad file.");if(mode=="Element"){const auto *element=lib->element(ref);if(!element)return fail("Choose a screen element.");const auto scene=continuityScene(*element,0);if(!scene.error.isEmpty())return fail(scene.error);}setProperty("configurationError",QVariant{});const QString nextMedia=(mode=="Video"||mode=="Ad")?file:QString();const bool reload=nextMedia!=media;if(reload||mode!=kind||ref!=reference)setProperty("designError",QVariant{});kind=mode;reference=ref;message=copy;media=nextMedia;if(reload){player->stop();video={};player->setSource(media.isEmpty()?QUrl():QUrl::fromLocalFile(media));}audio->setVolume(volume);audio->setMuted(muted);if(enabled&&!media.isEmpty())player->play();syncAudio();emit changed();return true;}
void ContinuityOutput::setAudio(double level,bool mute){volume=level;muted=mute;audio->setVolume(level);audio->setMuted(mute||!enabled);syncAudio();}
QString ContinuityOutput::mediaError()const{const auto error=player->errorString();if(!error.isEmpty())return error;const auto designError=property("designError").toString();return designError.isEmpty()?property("configurationError").toString():designError;}
void ContinuityOutput::syncAudio(){QList<DesignAudioCue> cues;if(enabled&&kind=="Element"){const auto e=selectedElement();if(!e.design.isEmpty()){const auto scene=continuityScene(e,clock.elapsed());if(scene.error.isEmpty())cues=designAudioCues(scene.design,scene.time,scene.advancing,"continuity/",muted?0:volume,false);else setProperty("designError",scene.error);}}designAudio->sync(cues);}
QImage ContinuityOutput::frame(QSize size){QImage result(size,QImage::Format_ARGB32_Premultiplied);result.fill(Qt::black);if(!enabled||kind=="Black")return result;QPainter painter(&result);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);if(kind=="Video"||kind=="Ad"){if(!video.isNull())painter.drawImage(result.rect(),video);return result;}const auto element=selectedElement();if(kind=="Element"){painter.drawImage(result.rect(),renderContinuityElement(element,clock.elapsed(),size));return result;}
    QImage logo;if(!element.id.isEmpty())logo=renderContinuityElement(element,clock.elapsed(),{640,360});else if(!lib->display.channelLogo.isEmpty())logo=QImage(lib->display.channelLogo);
    QLinearGradient backdrop(0,0,size.width(),size.height());backdrop.setColorAt(0,QColor("#172e46"));backdrop.setColorAt(1,QColor("#060c18"));painter.fillRect(result.rect(),backdrop);
    if(!logo.isNull()){auto full=logo.scaled(size,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);full=backdropImage(full,16,.65);painter.setOpacity(.38);painter.drawImage(QPoint((size.width()-full.width())/2,(size.height()-full.height())/2),full);painter.setOpacity(1);auto mark=logo.scaled(QSize(size.width()/4,size.height()/6),Qt::KeepAspectRatio,Qt::SmoothTransformation);painter.drawImage(QPoint((size.width()-mark.width())/2,size.height()-mark.height()-size.height()/14),mark);}
    auto font=painter.font();font.setFamily("Segoe UI");font.setPixelSize(qMax(12,size.height()/18));font.setWeight(QFont::DemiBold);painter.setFont(font);painter.setPen(QColor("#f2f5fa"));QString headline=message.trimmed();if(headline.isEmpty())headline=kind=="Technical"?"Please stand by":kind=="Logo"?lib->display.channel:"We’ll be right back";QRect box(size.width()/10,size.height()/3,size.width()*8/10,size.height()/3);painter.drawText(box,Qt::AlignCenter|Qt::TextWordWrap,headline);return result;
}
QWidget *continuityControls(Library *library,Playout *play,NetworkOutput *output){auto *page=new QWidget;page->setObjectName("continuityControls");auto *layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);auto *take=new QPushButton(uiIcon("broadcast"),"Black / standby");take->setObjectName("emergencyOutput");take->setCheckable(true);take->setToolTip("Override outgoing picture and audio immediately; return restores the current program.");layout->addWidget(take);auto *options=new QWidget;auto *form=new QFormLayout(options);form->setContentsMargins(4,0,4,0);auto *mode=new QComboBox;mode->addItems({"Black","Logo","Standby","Technical","Element","Ad","Video"});auto *source=new QComboBox;auto *file=new QLineEdit;auto *browse=new QPushButton(uiIcon("image"),"Choose media");auto *message=new QPlainTextEdit;message->setMaximumHeight(65);message->setPlaceholderText("Editable on-screen message");auto *errors=new QComboBox;errors->addItem("Use an active error message…");auto *errorStatus=new QLabel;errorStatus->setWordWrap(true);form->addRow("Quick content",mode);form->addRow("Source",source);form->addRow(file);form->addRow(browse);form->addRow(message);form->addRow(errors);form->addRow(errorStatus);auto *settings=new QToolButton;settings->setText("Standby settings");settings->setCheckable(true);settings->setArrowType(Qt::RightArrow);settings->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);layout->addWidget(settings);layout->addWidget(options);options->hide();QObject::connect(settings,&QToolButton::toggled,page,[=](bool expanded){settings->setArrowType(expanded?Qt::DownArrow:Qt::RightArrow);options->setVisible(expanded);});
    auto fill=[=]{QSignalBlocker block(source);source->clear();if(mode->currentText()=="Ad")for(const auto &ad:library->ads)source->addItem(ad.name,ad.id);else for(const auto &e:library->elements)source->addItem(e.name,e.id);source->setVisible(mode->currentText()=="Ad"||mode->currentText()=="Element");file->setVisible(mode->currentText()=="Video");browse->setVisible(mode->currentText()=="Video");};
    auto configure=[=]()->bool{QString selectedFile=file->text();if(mode->currentText()=="Ad")if(const auto *ad=library->ad(source->currentData().toString()))selectedFile=ad->file;QString error;const bool ok=play->continuity()->configure(mode->currentText(),source->currentData().toString(),selectedFile,message->toPlainText(),&error);errorStatus->setText(error);if(!ok)settings->setChecked(true);return ok;};QObject::connect(take,&QPushButton::toggled,page,[=](bool on){if(on&&!configure()){QSignalBlocker block(take);take->setChecked(false);return;}play->continuity()->setActive(on);take->setText(on?"Return to program":"Black / standby");});QObject::connect(mode,&QComboBox::currentTextChanged,page,[=](QString){fill();if(take->isChecked())configure();});QObject::connect(source,&QComboBox::currentIndexChanged,page,[=]{if(take->isChecked())configure();});QObject::connect(message,&QPlainTextEdit::textChanged,page,[=]{if(take->isChecked())configure();});QObject::connect(browse,&QPushButton::clicked,page,[=]{auto chosen=QFileDialog::getOpenFileName(page,"Loop standby video",{},"Video (*.mp4 *.mov *.mkv *.webm *.avi)");if(!chosen.isEmpty()){file->setText(chosen);if(take->isChecked())configure();}});auto refreshErrors=[=]{QSignalBlocker block(errors);errors->clear();errors->addItem("Use an active error message…");const auto main=play->primaryPlayer()->errorString();if(!main.isEmpty())errors->addItem("Program media: "+main);const auto standby=play->continuity()->mediaError();if(!standby.isEmpty())errors->addItem("Standby media: "+standby);const auto outputError=output?output->property("lastError").toString():QString();if(!outputError.isEmpty())errors->addItem("Output: "+outputError);};QObject::connect(errors,&QComboBox::currentIndexChanged,page,[=](int i){if(i>0){QString copy=errors->currentText();copy.replace(QRegularExpression("(?:rtmps?|srt|https?)://[^\\s]+"),"[destination]");message->setPlainText(copy);}});auto *timer=new QTimer(page);timer->setInterval(2000);QObject::connect(timer,&QTimer::timeout,page,refreshErrors);timer->start();QObject::connect(library,&Library::elementsChanged,page,fill);QObject::connect(library,&Library::adsChanged,page,fill);fill();return page;}
