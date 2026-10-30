#include "AudioLibrary.h"
#include "BuildModel.h"
#include "Data.h"
#include "Toast.h"
#include "UiControls.h"
#include <QDrag>
#include <QCheckBox>
#include <QAudioOutput>
#include <QHideEvent>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

namespace {
class AudioList final:public QListWidget {
public:
    std::function<void(QString)> dropped;
    explicit AudioList(QWidget *parent=nullptr):QListWidget(parent){setDragEnabled(true);setSelectionMode(SingleSelection);setMinimumHeight(105);setStyleSheet("QListWidget{background:palette(base);border:1px solid palette(mid);border-radius:6px;padding:4px;}QListWidget::item{padding:6px;}");}
protected:
    void startDrag(Qt::DropActions)override{
        if(!currentItem()||currentItem()->data(Qt::UserRole).toString().isEmpty())return;
        auto *drag=new QDrag(this);auto *mime=new QMimeData;mime->setData(AudioLibrary::mimeType,currentItem()->data(Qt::UserRole).toString().toUtf8());drag->setMimeData(mime);
        QPixmap ghost(180,64);ghost.fill(Qt::transparent);QPainter p(&ghost);p.setRenderHint(QPainter::Antialiasing);p.setBrush(QColor(25,40,55,235));p.setPen(QColor("#72c6ff"));p.drawRoundedRect(ghost.rect().adjusted(1,1,-1,-1),8,8);uiIcon("audio",true).paint(&p,QRect(76,7,28,28));p.setPen(Qt::white);p.drawText(QRect(8,37,164,20),Qt::AlignCenter,fontMetrics().elidedText(currentItem()->text(),Qt::ElideRight,164));p.end();drag->setPixmap(ghost);drag->setHotSpot({90,22});drag->exec(Qt::CopyAction);
    }
    void dragEnterEvent(QDragEnterEvent *e)override{if(dropped&&e->mimeData()->hasFormat(AudioLibrary::mimeType))e->acceptProposedAction();}
    void dragMoveEvent(QDragMoveEvent *e)override{if(dropped&&e->mimeData()->hasFormat(AudioLibrary::mimeType))e->acceptProposedAction();}
    void dropEvent(QDropEvent *e)override{if(dropped&&e->mimeData()->hasFormat(AudioLibrary::mimeType)){dropped(QString::fromUtf8(e->mimeData()->data(AudioLibrary::mimeType)));e->acceptProposedAction();}}
};
qint64 duration(const QJsonObject &a){return qMax<qint64>(1,qint64(a["durationMs"].toDouble(a["seconds"].toInt(8)*1000.)));}
QJsonObject defaults(){return {{"speed",1.},{"volume",1.},{"fadeIn",0.},{"fadeOut",0.},{"offset",0.}};}
void extendSceneForAudio(BuildModel &model,const DObject &audio){
    const double rate=qBound(.1,audio.propertyValue("audio.speed",model.t),4.);
    const double available=qMax(0.,audio.style["audio.duration"].toDouble()-audio.propertyValue("audio.offset",model.t));
    const double specified=audio.propertyValue("audio.length",model.t);
    const auto end=qRound64(audio.propertyValue("audio.start",model.t)+(specified>0?specified:available/rate));
    model.design.durationMs=qMax(model.design.durationMs,int(qBound(qint64(0),qint64(end),qint64(2147483647))));
}
}
QJsonObject AudioLibrary::asset(const Library &library,const QString &id){for(const auto &value:library.production["audioLibrary"].toArray())if(value.toObject()["id"].toString()==id)return value.toObject();return {};}
QJsonObject AudioLibrary::placementSettings(const Library &library,const QString &id){auto settings=library.production["audioPlacementSettings"].toObject()[id].toObject();return settings.isEmpty()?defaults():settings;}
QString AudioLibrary::addToScene(BuildModel &model,const Library &library,const QString &id){
    const auto a=asset(library,id);if(a.isEmpty()||!QFileInfo(a["file"].toString()).isFile())return {};
    model.pushUndo(true);DObject audio;audio.id=Library::newId();audio.name=model.uniqueName(a["name"].toString());audio.type="Audio";
    audio.style["audio.file"]=a["file"];audio.style["audio.assetId"]=id;audio.style["audio.duration"]=double(duration(a));audio.style["audio.start"]=double(model.t);
    audio.style["audio.speed"]=a["speed"].toDouble(1);audio.style["audio.volume"]=a["volume"].toDouble(1);for(const auto &key:{"fadeIn","fadeOut","offset"})audio.style["audio."+QString(key)]=a[key].toDouble()*1000;
    audio.captureDefaults();model.design.objects.append(audio);const auto length=qMax<qint64>(1,qRound64(qMax(0.,duration(a)-a["offset"].toDouble()*1000)/qBound(.1,a["speed"].toDouble(1),4.)));model.design.durationMs=qMax(model.design.durationMs,int(qMin<qint64>(2147483647,model.t+length)));model.select(audio.id);model.notify();return audio.id;
}
QString AudioLibrary::addToTimeline(Library &library,const QString &id,qint64 start,int layer,QString *error){
    const auto a=asset(library,id);if(a.isEmpty()||!QFileInfo(a["file"].toString()).isFile())return {};
    Placement clip;clip.id=Library::newId();clip.type="Sound";clip.refId=id;clip.name=a["name"].toString();clip.file=a["file"].toString();clip.startMs=qMax<qint64>(0,start);clip.layer=qBound(1,layer,3);clip.offsetMs=qBound<qint64>(0,qRound64(a["offset"].toDouble()*1000),duration(a)-1);clip.durationMs=qMax<qint64>(1,qRound64((duration(a)-clip.offsetMs)/qBound(.1,a["speed"].toDouble(1),4.)));clip.seconds=int((clip.durationMs+999)/1000);
    const auto previous=library.production;auto settings=library.production["audioPlacementSettings"].toObject();settings[clip.id]=a;library.production["audioPlacementSettings"]=settings;library.placements.append(clip);
    if(!library.saveChecked(error)){library.placements.removeLast();library.production=previous;return {};}
    emit library.placementsChanged();return clip.id;
}
AudioLibraryPanel::AudioLibraryPanel(Library *library,BuildModel *model,QWidget *parent):QWidget(parent),m_library(library),m_model(model){
    setObjectName(model?"buildAudioLibrary":"studioAudioLibrary");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(8,8,8,8);layout->setSpacing(8);
    auto *title=new QLabel("AUDIO");layout->addWidget(title);auto *import=new QPushButton(uiIcon("add"),"Import audio…");layout->addWidget(import);connect(import,&QPushButton::clicked,this,[this]{for(const auto &path:QFileDialog::getOpenFileNames(this,"Import audio",{},"Audio (*.wav *.mp3 *.flac *.ogg *.aac *.m4a *.opus *.aiff)"))importFile(path);});
    layout->addWidget(new QLabel("Stored audio · shared with Studio and Build"));m_assets=new AudioList;m_assets->setObjectName("audioStored");layout->addWidget(m_assets);
    m_preview=new QPushButton(uiIcon("play"),"Preview source");m_preview->setObjectName("previewStoredAudio");m_preview->setEnabled(false);m_preview->setToolTip("Listen to the original file locally. This does not start or change broadcast playback.");layout->addWidget(m_preview);
    m_previewPlayer=new QMediaPlayer(this);m_previewPlayer->setObjectName("audioLibraryPreviewPlayer");auto *previewOutput=new QAudioOutput(m_previewPlayer);previewOutput->setVolume(.7);m_previewPlayer->setAudioOutput(previewOutput);
    connect(m_preview,&QPushButton::clicked,this,[this]{if(m_previewPlayer->source().isValid()){stopPreview();return;}const auto audio=AudioLibrary::asset(*m_library,m_assetId);const auto file=audio["file"].toString();if(!QFileInfo(file).isFile()){ToastHost::notify(this,"audio","Audio source is unavailable",file);return;}m_previewPlayer->setSource(QUrl::fromLocalFile(file));m_preview->setText("Stop preview");m_preview->setIcon(uiIcon("stop"));m_previewPlayer->play();});
    connect(m_previewPlayer,&QMediaPlayer::mediaStatusChanged,this,[this](QMediaPlayer::MediaStatus status){if(status==QMediaPlayer::EndOfMedia)stopPreview();});
    connect(m_previewPlayer,&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString &error){stopPreview();ToastHost::notify(this,"audio","Unable to preview audio",error);});
    connect(m_assets,&QListWidget::currentItemChanged,this,[this](QListWidgetItem *item){if(m_syncing||!item)return;stopPreview();m_assetId=item->data(Qt::UserRole).toString();m_sceneId.clear();m_placementId.clear();loadControls();});
    if(model){layout->addWidget(new QLabel("In scene"));auto *scene=new AudioList;scene->setDragEnabled(false);scene->setAcceptDrops(true);scene->dropped=[this](const QString &id){const auto added=AudioLibrary::addToScene(*m_model,*m_library,id);if(!added.isEmpty())ToastHost::notify(this,"audio","Audio added successfully",m_model->byId(added)->name);};m_scene=scene;layout->addWidget(scene);
        auto *add=new QPushButton(uiIcon("audio"),"Add selected audio to scene");layout->addWidget(add);connect(add,&QPushButton::clicked,this,[this,scene]{if(!m_assetId.isEmpty())scene->dropped(m_assetId);});
        m_removeScene=new QPushButton(uiIcon("delete"),"Remove from scene");m_removeScene->setObjectName("removeSceneAudio");m_removeScene->setToolTip("Remove this scene instance; keep the stored audio and source file. Undo restores it.");layout->addWidget(m_removeScene);
        connect(m_removeScene,&QPushButton::clicked,this,[this]{auto *audio=m_model->byId(m_sceneId);if(!audio||audio->type!="Audio"||audio->locked)return;const auto name=audio->name;m_model->select(audio->id);m_model->deleteSelected();m_sceneId.clear();loadControls();ToastHost::notify(this,"audio","Audio removed from scene",name);});
        connect(scene,&QListWidget::currentItemChanged,this,[this](QListWidgetItem *item){if(m_syncing||!item)return;m_sceneId=item->data(Qt::UserRole).toString();m_placementId.clear();m_model->select(m_sceneId);loadControls();});connect(model,&BuildModel::changed,this,[this]{refresh();});
        connect(model,&BuildModel::selectionChanged,this,[this]{if(auto *o=m_model->active();o&&o->type=="Audio"){m_sceneId=o->id;refresh();}});
    }
    m_context=new QLabel("Select audio to edit");m_context->setWordWrap(true);layout->addWidget(m_context);auto *form=new QFormLayout;form->setContentsMargins(12,2,0,2);layout->addLayout(form);
    if(model){m_start=new ValueInput;m_start->setObjectName("audioSceneStart");m_start->setRange(0,86400);m_start->setDecimals(2);m_start->setSingleStep(.1);m_start->setKeyboardTracking(false);m_start->setToolTip("When this audio begins in the scene; source offset selects where it begins inside the file.");form->addRow("Scene start (s)",m_start);
        connect(m_start,&QDoubleSpinBox::valueChanged,this,[this](double seconds){if(m_syncing)return;auto *audio=m_model->byId(m_sceneId);if(!audio||audio->type!="Audio"||audio->locked)return;m_model->pushUndo(true);m_model->editProperty(audio->id,"audio.start",seconds*1000);extendSceneForAudio(*m_model,*audio);m_model->notify();});
        m_mute=new QCheckBox("Mute scene audio");m_mute->setObjectName("audioSceneMute");form->addRow(m_mute);
        connect(m_mute,&QCheckBox::toggled,this,[this](bool muted){if(m_syncing)return;auto *audio=m_model->byId(m_sceneId);if(!audio||audio->type!="Audio"||audio->locked)return;m_model->pushUndo(true);audio->style["audio.mute"]=muted?"On":"Off";m_model->notify();});}
    auto number=[&](const QString &label,double low,double high,double step){auto *spin=new ValueInput;spin->setRange(low,high);spin->setDecimals(2);spin->setSingleStep(step);spin->setKeyboardTracking(false);form->addRow(label,spin);connect(spin,&QDoubleSpinBox::valueChanged,this,[this,spin]{if(!m_syncing)saveControls(spin);});return spin;};
    m_speed=number("Speed",.1,4,.05);m_volume=number("Volume",0,1,.05);m_offset=number("Source offset (s)",0,86400,.1);m_fadeIn=number("Fade in (s)",0,3600,.1);m_fadeOut=number("Fade out (s)",0,3600,.1);m_speed->setObjectName("audioSpeed");m_volume->setObjectName("audioVolume");m_offset->setObjectName("audioOffset");m_fadeIn->setObjectName("audioFadeIn");m_fadeOut->setObjectName("audioFadeOut");layout->addStretch();
    connect(library,&Library::productionChanged,this,[this]{refresh();});connect(library,&Library::placementsChanged,this,[this]{if(!m_placementId.isEmpty())loadControls();});refresh();
}
void AudioLibraryPanel::refresh(){
    m_syncing=true;m_assets->clear();for(const auto &value:m_library->production["audioLibrary"].toArray()){const auto a=value.toObject();auto *item=new QListWidgetItem(uiIcon("audio"),a["name"].toString(),m_assets);item->setData(Qt::UserRole,a["id"].toString());item->setToolTip(a["file"].toString());if(a["id"].toString()==m_assetId)m_assets->setCurrentItem(item);}
    if(m_scene){m_scene->clear();for(const auto &audio:m_model->design.objects)if(audio.type=="Audio"){auto *item=new QListWidgetItem(uiIcon("audio"),audio.name,m_scene);item->setData(Qt::UserRole,audio.id);if(audio.id==m_sceneId)m_scene->setCurrentItem(item);}}
    m_syncing=false;loadControls();
}
void AudioLibraryPanel::stopPreview(){m_previewPlayer->stop();m_previewPlayer->setSource(QUrl());m_preview->setText("Preview source");m_preview->setIcon(uiIcon("play"));}
void AudioLibraryPanel::hideEvent(QHideEvent *event){stopPreview();QWidget::hideEvent(event);}
void AudioLibraryPanel::selectPlacement(const QString &id){stopPreview();m_placementId=id;m_sceneId.clear();loadControls();}
void AudioLibraryPanel::loadControls(){
    m_syncing=true;QJsonObject settings;QString name;
    m_preview->setEnabled(!AudioLibrary::asset(*m_library,m_assetId).isEmpty());
    bool locked=false;
    if(m_start){const auto *audio=m_model->byId(m_sceneId);locked=audio&&audio->locked;m_start->setEnabled(audio&&audio->type=="Audio"&&!locked);m_start->setValue(audio?audio->propertyValue("audio.start",m_model->t)/1000.:0.);}
    if(m_mute){const auto *audio=m_model->byId(m_sceneId);m_mute->setEnabled(audio&&audio->type=="Audio"&&!locked);m_mute->setChecked(audio&&audio->style["audio.mute"].toString()=="On");}
    if(m_removeScene){const auto *audio=m_model->byId(m_sceneId);m_removeScene->setEnabled(audio&&audio->type=="Audio"&&!audio->locked);}
    double sourceSeconds=86400.;
    if(!m_sceneId.isEmpty()&&m_model){if(auto *audio=m_model->byId(m_sceneId))sourceSeconds=audio->style.value("audio.duration").toDouble()/1000.;}
    else if(!m_placementId.isEmpty()){if(auto *clip=m_library->placement(m_placementId)){const auto source=AudioLibrary::asset(*m_library,clip->refId);if(!source.isEmpty())sourceSeconds=duration(source)/1000.;}}
    else{const auto source=AudioLibrary::asset(*m_library,m_assetId);if(!source.isEmpty())sourceSeconds=duration(source)/1000.;}
    // Keep at least one editable centisecond of source after the offset.
    m_offset->setMaximum(qMax(0.,sourceSeconds-.01));
    if(!m_sceneId.isEmpty()&&m_model){if(auto *audio=m_model->byId(m_sceneId);audio&&audio->type=="Audio"){name="In scene · "+audio->name;settings={{"speed",audio->propertyValue("audio.speed",m_model->t)},{"volume",audio->propertyValue("audio.volume",m_model->t)}};for(const auto &key:{"fadeIn","fadeOut","offset"})settings[key]=audio->propertyValue("audio."+QString(key),m_model->t)/1000.;}}
    else if(!m_placementId.isEmpty()){if(auto *clip=m_library->placement(m_placementId);clip&&clip->type=="Sound"){name="Timeline · "+clip->name;settings=AudioLibrary::placementSettings(*m_library,m_placementId);settings["offset"]=clip->offsetMs/1000.;}}
    else{settings=AudioLibrary::asset(*m_library,m_assetId);if(!settings.isEmpty())name="Stored audio · "+settings["name"].toString();}
    m_context->setText(name.isEmpty()?"Select audio to edit":name);for(auto *spin:{m_speed,m_volume,m_offset,m_fadeIn,m_fadeOut})spin->setEnabled(!name.isEmpty()&&!locked);m_speed->setValue(settings["speed"].toDouble(1));m_volume->setValue(settings["volume"].toDouble(1));m_offset->setValue(settings["offset"].toDouble());m_fadeIn->setValue(settings["fadeIn"].toDouble());m_fadeOut->setValue(settings["fadeOut"].toDouble());m_syncing=false;
}
void AudioLibraryPanel::saveControls(QDoubleSpinBox *changed){
    const auto previousProduction=m_library->production;
    const auto previousPlacements=m_library->placements;
    auto persist=[&]{QString error;if(m_library->saveChecked(&error))return true;m_library->production=previousProduction;m_library->placements=previousPlacements;loadControls();ToastHost::notify(this,"audio","Audio changes could not be saved",error);return false;};
    QJsonObject fields{{"speed",m_speed->value()},{"volume",m_volume->value()},{"offset",m_offset->value()},{"fadeIn",m_fadeIn->value()},{"fadeOut",m_fadeOut->value()}};
    if(!m_sceneId.isEmpty()){
        const QString key=changed==m_speed?"speed":changed==m_volume?"volume":changed==m_offset?"offset":changed==m_fadeIn?"fadeIn":"fadeOut";
        fields=QJsonObject{{key,fields[key]}};
    }
    if(!m_sceneId.isEmpty()&&m_model){if(auto *audio=m_model->byId(m_sceneId);audio&&audio->type=="Audio"&&!audio->locked){QMap<QString,double> changes;for(auto it=fields.begin();it!=fields.end();++it){const auto key="audio."+it.key();const double value=it.value().toDouble()*(it.key()=="speed"||it.key()=="volume"?1:1000);if(qAbs(audio->propertyValue(key,m_model->t)-value)>.000001)changes[key]=value;}if(changes.isEmpty())return;m_model->pushUndo();for(auto it=changes.begin();it!=changes.end();++it)m_model->editProperty(audio->id,it.key(),it.value());extendSceneForAudio(*m_model,*audio);m_model->notify();}return;}
    if(!m_placementId.isEmpty()){auto *clip=m_library->placement(m_placementId);if(!clip)return;const auto old=AudioLibrary::placementSettings(*m_library,m_placementId);const double span=clip->lengthMs()*qBound(.1,old["speed"].toDouble(1),4.);const auto offset=qRound64(m_offset->value()*1000);clip->durationMs=qMax<qint64>(1,qRound64(qMax(1.,span-(offset-clip->offsetMs))/m_speed->value()));auto all=m_library->production["audioPlacementSettings"].toObject();all[m_placementId]=fields;m_library->production["audioPlacementSettings"]=all;clip->offsetMs=offset;if(persist())emit m_library->placementsChanged();return;}
    auto all=m_library->production["audioLibrary"].toArray();for(int i=0;i<all.size();++i){auto a=all[i].toObject();if(a["id"].toString()!=m_assetId)continue;for(auto it=fields.begin();it!=fields.end();++it)a[it.key()]=it.value();all[i]=a;}m_library->production["audioLibrary"]=all;if(persist())emit m_library->productionChanged();
}
void AudioLibraryPanel::importFile(const QString &path){
    if(!QFileInfo(path).isFile())return;auto *probe=new QMediaPlayer(this);auto done=std::make_shared<bool>(false);
    connect(probe,&QMediaPlayer::durationChanged,this,[=,this](qint64 length){if(*done||length<=0)return;*done=true;auto all=m_library->production["audioLibrary"].toArray();QString id;for(const auto &value:all)if(value.toObject()["file"].toString()==QFileInfo(path).absoluteFilePath())id=value.toObject()["id"].toString();
        if(id.isEmpty()){const auto previous=m_library->production;id=Library::newId();auto a=defaults();a["id"]=id;a["name"]=QFileInfo(path).completeBaseName();a["file"]=QFileInfo(path).absoluteFilePath();a["seconds"]=int((length+999)/1000);a["durationMs"]=double(length);a["echo"]=0.;all.append(a);m_library->production["audioLibrary"]=all;QString error;if(!m_library->saveChecked(&error)){m_library->production=previous;ToastHost::notify(this,"audio","Audio import could not be saved",error);probe->deleteLater();return;}}m_assetId=id;emit m_library->productionChanged();ToastHost::notify(this,"audio","Audio imported",QFileInfo(path).completeBaseName());probe->deleteLater();});
    connect(probe,&QMediaPlayer::errorOccurred,this,[=,this](QMediaPlayer::Error,const QString &error){if(*done)return;*done=true;ToastHost::notify(this,"audio",error);probe->deleteLater();});QTimer::singleShot(30000,probe,[=,this]{if(!*done){*done=true;ToastHost::notify(this,"audio","Unable to read audio duration",QFileInfo(path).fileName());probe->deleteLater();}});probe->setSource(QUrl::fromLocalFile(path));
}
