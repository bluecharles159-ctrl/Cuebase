#pragma once
#include "AudioDsp.h"
#include "AudioAutomationDialog.h"
#include "Data.h"
#include "UiControls.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QTemporaryDir>
#include <QSaveFile>
#include <memory>

// The original source is never overwritten. Rendering uses explicit arguments,
// not a shell, and the resulting audio can enter the shared asset library.
inline void showAudioDsp(QWidget *parent,Library *library,const QString &source,const QJsonArray &initial,
                         std::function<bool(QJsonArray,QString*)> saveChain,std::function<void(QString)> ready){
    auto *dialog=roundedModal(parent,"Audio effects");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(440,540);
    auto *layout=new QVBoxLayout(dialog);layout->setContentsMargins(18,18,18,18);
    auto *hint=new QLabel("Effects run from top to bottom. Uncheck an effect to bypass it. Render creates a separate WAV file.");hint->setWordWrap(true);layout->addWidget(hint);
    dialog->setObjectName("audioDspDialog");
    auto *types=new QComboBox;types->setObjectName("dspTypes");types->addItems(AudioDsp::types());layout->addWidget(types);
    auto *add=new QPushButton("Add effect");layout->addWidget(add);auto *list=new QListWidget;list->setDragDropMode(QAbstractItemView::InternalMove);layout->addWidget(list);
    list->setObjectName("dspChain");add->setObjectName("dspAdd");
    auto *row=new QHBoxLayout;layout->addLayout(row);auto *up=new QPushButton("Earlier"),*down=new QPushButton("Later"),*remove=new QPushButton("Remove");
    up->setObjectName("dspEarlier");row->addWidget(up);row->addWidget(down);row->addWidget(remove);
    auto *form=new QFormLayout;layout->addLayout(form);auto *error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
    auto *audition=new QPushButton(uiIcon("play"),"Preview first 8 seconds");audition->setObjectName("dspAudition");layout->addWidget(audition);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText("Render processed copy…");layout->addWidget(buttons);
    auto *keep=buttons->addButton("Save chain",QDialogButtonBox::ActionRole);keep->setObjectName("saveDspChain");
    auto chain=[list]{QJsonArray result;for(int i=0;i<list->count();++i){auto *item=list->item(i);auto effect=item->data(Qt::UserRole).toJsonObject();effect["enabled"]=item->checkState()==Qt::Checked;result.append(effect);}return result;};
    auto fileFor=[library](const QString &id){for(auto entry:library->production.value("audioLibrary").toArray()){
        auto a=entry.toObject();if(a.value("id").toString()==id)return a.value("file").toString();}return QString();};
    auto load=[list](const QJsonArray &effects){
        list->clear();for(auto value:effects){auto e=value.toObject();auto *item=new QListWidgetItem(e.value("type").toString(),list);
            item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(e.value("enabled").toBool(true)?Qt::Checked:Qt::Unchecked);item->setData(Qt::UserRole,e);}
        if(list->count())list->setCurrentRow(0);
    };
    auto *presets=new QHBoxLayout;layout->insertLayout(1,presets);auto *preset=new QComboBox;preset->setEditable(true);preset->setObjectName("dspPresetName");
    preset->addItems(library->production.value("audioDspPresets").toObject().keys());preset->setCurrentIndex(-1);preset->lineEdit()->setPlaceholderText("Chain preset");
    auto *use=new QPushButton("Load"),*savePreset=new QPushButton("Save preset");presets->addWidget(preset,1);presets->addWidget(use);presets->addWidget(savePreset);
    use->setObjectName("dspLoadPreset");savePreset->setObjectName("dspSavePreset");
    QObject::connect(use,&QPushButton::clicked,dialog,[=]{const auto saved=library->production.value("audioDspPresets").toObject().value(preset->currentText().trimmed());
        if(saved.isArray()){load(saved.toArray());error->clear();}else error->setText("Select a saved preset.");});
    QObject::connect(savePreset,&QPushButton::clicked,dialog,[=]{
        QString reason;const auto effects=chain();if(AudioDsp::filterGraph(effects,&reason).isEmpty()){error->setText(reason);return;}
        const auto name=preset->currentText().trimmed();if(name.isEmpty()||name.size()>100){error->setText("Enter a preset name (up to 100 characters).");return;}
        const auto original=library->production;auto entries=original.value("audioDspPresets").toObject();entries[name]=effects;library->production["audioDspPresets"]=entries;
        if(!library->saveChecked(&reason)){library->production=original;error->setText(reason);return;}
        if(preset->findText(name)<0)preset->addItem(name);preset->setCurrentText(name);emit library->productionChanged();error->setText("Preset saved.");
    });
    QObject::connect(keep,&QPushButton::clicked,dialog,[=]{QString reason;const auto effects=chain();
        if(AudioDsp::filterGraph(effects,&reason).isEmpty() || !saveChain(effects,&reason)){error->setText(reason);return;}dialog->accept();
    });
    for(auto *button:{up,down})QObject::connect(button,&QPushButton::clicked,dialog,[=]{
        const int from=list->currentRow(),to=from+(button==up?-1:1);if(from<0||to<0||to>=list->count())return;
        auto *item=list->takeItem(from);list->insertItem(to,item);list->setCurrentRow(to);
    });
    auto *process=new QProcess(dialog);process->setProcessChannelMode(QProcess::SeparateChannels);
    auto output=std::make_shared<QString>();auto log=std::make_shared<QByteArray>();
    auto renderTemp=std::make_shared<QTemporaryDir>();
    QObject::connect(add,&QPushButton::clicked,dialog,[=]{auto *item=new QListWidgetItem(types->currentText(),list);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Checked);item->setData(Qt::UserRole,QJsonObject{{"type",types->currentText()}});list->setCurrentItem(item);});
    QObject::connect(remove,&QPushButton::clicked,dialog,[=]{delete list->takeItem(list->currentRow());});
    QObject::connect(list,&QListWidget::currentItemChanged,dialog,[=](QListWidgetItem *item){
        while(form->rowCount())form->removeRow(0);if(!item)return;auto effect=item->data(Qt::UserRole).toJsonObject();const auto type=effect["type"].toString();
        auto field=[&](QString label,QString key,double value,double low,double high){auto *input=new ValueInput;input->setRange(low,high);input->setDecimals(3);input->setValue(effect[key].toDouble(value));form->addRow(label,input);QObject::connect(input,&QDoubleSpinBox::valueChanged,dialog,[=](double v){if(list->currentItem()!=item)return;auto current=item->data(Qt::UserRole).toJsonObject();current[key]=v;item->setData(Qt::UserRole,current);});};
        if(type=="Echo"){field("Delay (ms)","delayMs",250,1,90000);field("Echo decay","decay",.35,0,.95);}
        if(type=="Equalizer"){field("Frequency (Hz)","frequency",1000,20,20000);field("Q","q",1,.1,10);field("Gain (dB)","gainDb",0,-24,24);}
        if(type=="High pass"||type=="Low pass")field("Frequency (Hz)","frequency",type=="High pass"?80:12000,20,20000);
        if(type=="Compressor"||type=="Sidechain compressor"){field("Threshold (linear)","threshold",.125,.001,1);field("Ratio","ratio",4,1,20);field("Attack (ms)","attackMs",20,.01,2000);field("Release (ms)","releaseMs",250,.01,9000);}
        if(type=="Sidechain compressor"){
            auto *trigger=new QComboBox;trigger->addItem("Choose a trigger",QString());for(auto entry:library->production.value("audioLibrary").toArray()){
                auto a=entry.toObject();trigger->addItem(a.value("name").toString(),a.value("id").toString());}
            trigger->setCurrentIndex(qMax(0,trigger->findData(effect.value("trigger").toString())));form->addRow("Trigger audio",trigger);
            QObject::connect(trigger,&QComboBox::activated,dialog,[=](int i){auto e=item->data(Qt::UserRole).toJsonObject();e["trigger"]=trigger->itemData(i).toString();item->setData(Qt::UserRole,e);});
        }
        if(type=="Pitch")field("Semitones","semitones",0,-24,24);
        if(type=="Tempo")field("Speed ratio","ratio",1,.25,4);
        if(type=="Balance")field("Left / right","balance",0,-1,1);
        if(type=="Limiter")field("Ceiling (linear)","limit",.95,.0625,1);
        if(type=="Gain")field("Gain (dB)","gainDb",0,-60,24);
        if(type=="Stereo width"){field("Delay (ms)","delayMs",20,1,100);field("Feedback","feedback",.3,0,.9);field("Crossfeed","crossfeed",.3,0,.8);field("Dry mix","dryMix",.8,0,1);}
        if(type=="Tremolo"||type=="Vibrato"){field("Rate (Hz)","rate",5,.1,20000);field("Depth","depth",.5,0,1);}
        if(type=="Flanger"){field("Base delay (ms)","delayMs",0,0,30);field("Sweep (ms)","sweepMs",2,0,10);field("Feedback (%)","feedback",0,-95,95);field("Wet mix (%)","wet",71,0,100);field("Rate (Hz)","rate",.5,.1,10);}
        if(!AudioDsp::commands(type).isEmpty()){
            auto *automate=new QPushButton("Automation…");automate->setObjectName("processorAutomation");form->addRow(automate);
            QObject::connect(automate,&QPushButton::clicked,dialog,[=]{auto e=item->data(Qt::UserRole).toJsonObject();if(editAudioAutomation(dialog,e))item->setData(Qt::UserRole,e);});
        }
    });
    load(initial);
    auto *previewProcess=new QProcess(dialog);auto *previewPlayer=new QMediaPlayer(dialog);
    auto *previewOutput=new QAudioOutput(previewPlayer);previewPlayer->setAudioOutput(previewOutput);
    auto storage=std::make_shared<QTemporaryDir>();
    QObject::connect(audition,&QPushButton::clicked,dialog,[=]{
        if(previewPlayer->playbackState()==QMediaPlayer::PlayingState){previewPlayer->stop();return;}
        if(previewProcess->state()!=QProcess::NotRunning)return;
        QString reason;auto args=AudioDsp::processingArgs(source,chain(),fileFor,&reason);
        if(args.isEmpty()||!storage->isValid()){error->setText(args.isEmpty()?reason:"Cannot create preview storage.");return;}
        auto encoder=QCoreApplication::applicationDirPath()+"/ffmpeg.exe";
        if(!QFileInfo::exists(encoder))encoder=QStandardPaths::findExecutable("ffmpeg");
        if(encoder.isEmpty()){error->setText("FFmpeg is unavailable.");return;}
        previewPlayer->setSource({});
        audition->setEnabled(false);error->setText("Rendering preview…");
        QStringList preview{"-hide_banner","-loglevel","error","-nostdin","-y"};preview+=args;preview<<"-t"<<"8"<<"-vn"<<"-c:a"<<"pcm_s24le"<<storage->filePath("audition.wav");
        previewProcess->start(encoder,preview);
    });
    QObject::connect(previewProcess,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),dialog,[=](int code,QProcess::ExitStatus status){
        audition->setEnabled(true);
        if(code!=0||status!=QProcess::NormalExit){error->setText(QString::fromUtf8(previewProcess->readAllStandardError()).right(2048));return;}
        error->clear();previewPlayer->setSource(QUrl::fromLocalFile(storage->filePath("audition.wav")));previewPlayer->play();
    });
    QObject::connect(previewProcess,&QProcess::errorOccurred,dialog,[=](QProcess::ProcessError){audition->setEnabled(true);error->setText(previewProcess->errorString());});
    QObject::connect(previewPlayer,&QMediaPlayer::playbackStateChanged,dialog,[=](auto state){audition->setText(state==QMediaPlayer::PlayingState?"Stop preview":"Preview first 8 seconds");});
    QObject::connect(dialog,&QDialog::finished,dialog,[=]{previewPlayer->stop();previewPlayer->setSource({});if(previewProcess->state()!=QProcess::NotRunning)previewProcess->kill();});
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    QObject::connect(dialog,&QDialog::finished,process,[=]{if(process->state()!=QProcess::NotRunning)process->kill();});
    QObject::connect(process,&QProcess::readyReadStandardError,dialog,[=]{*log=(*log+process->readAllStandardError()).right(8192);});
    QObject::connect(process,&QProcess::errorOccurred,dialog,[=](QProcess::ProcessError){error->setText(process->errorString());buttons->button(QDialogButtonBox::Save)->setEnabled(true);});
    QObject::connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),dialog,[=](int code,QProcess::ExitStatus status){
        buttons->button(QDialogButtonBox::Save)->setEnabled(true);
        if(code!=0||status!=QProcess::NormalExit){error->setText(QString::fromUtf8(*log));return;}
        QFile rendered(renderTemp->filePath("processed.wav"));QSaveFile destination(*output);
        if(!rendered.open(QIODevice::ReadOnly)){error->setText(rendered.errorString());return;}
        if(!destination.open(QIODevice::WriteOnly)){error->setText(destination.errorString());return;}
        while(!rendered.atEnd()){
            const auto bytes=rendered.read(1024*1024);
            if(rendered.error()!=QFileDevice::NoError){error->setText(rendered.errorString());return;}
            if(destination.write(bytes)!=bytes.size()){error->setText(destination.errorString());return;}
        }
        if(!destination.commit()){error->setText(destination.errorString());return;}
        ready(*output);dialog->accept();
    });
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog,[=]{
        QString reason;const auto args=AudioDsp::processingArgs(source,chain(),fileFor,&reason);if(args.isEmpty()){error->setText(reason);return;}
        const auto file=QFileDialog::getSaveFileName(dialog,"Processed audio",QFileInfo(source).dir().filePath(QFileInfo(source).completeBaseName()+"-processed.wav"),"WAV audio (*.wav)");if(file.isEmpty())return;
        if(QFileInfo(file).absoluteFilePath().compare(QFileInfo(source).absoluteFilePath(),Qt::CaseInsensitive)==0){error->setText("Choose a different file to preserve the original audio.");return;}
        *output=file;log->clear();error->setText("Rendering audio…");buttons->button(QDialogButtonBox::Save)->setEnabled(false);
        auto encoder=QCoreApplication::applicationDirPath()+"/ffmpeg.exe";if(!QFileInfo::exists(encoder))encoder=QStandardPaths::findExecutable("ffmpeg");
        if(encoder.isEmpty()){error->setText("FFmpeg is unavailable. Use the packaged Cuebase build or add FFmpeg to PATH.");buttons->button(QDialogButtonBox::Save)->setEnabled(true);return;}
        if(!renderTemp->isValid()){error->setText("Cannot create temporary audio storage.");buttons->button(QDialogButtonBox::Save)->setEnabled(true);return;}
        QStringList render{"-hide_banner","-loglevel","error","-nostdin","-y"};render+=args;render<<"-vn"<<"-c:a"<<"pcm_s24le"<<renderTemp->filePath("processed.wav");
        process->start(encoder,render);
    });dialog->open();
}
