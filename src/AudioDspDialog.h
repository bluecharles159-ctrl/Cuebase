#pragma once
#include "AudioDsp.h"
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
#include <memory>

// The original source is never overwritten. Rendering uses explicit arguments,
// not a shell, and the resulting audio can enter the shared asset library.
inline void showAudioDsp(QWidget *parent,const QString &source,std::function<void(QString)> ready){
    auto *dialog=roundedModal(parent,"Audio effects");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(440,540);
    auto *layout=new QVBoxLayout(dialog);layout->setContentsMargins(18,18,18,18);
    auto *hint=new QLabel("Effects run from top to bottom. Uncheck an effect to bypass it. Render creates a separate WAV file.");hint->setWordWrap(true);layout->addWidget(hint);
    auto *types=new QComboBox;types->addItems({"Echo","Equalizer","High pass","Low pass","Compressor","Limiter","Gain"});layout->addWidget(types);
    auto *add=new QPushButton("Add effect");layout->addWidget(add);auto *list=new QListWidget;list->setDragDropMode(QAbstractItemView::InternalMove);layout->addWidget(list);
    auto *remove=new QPushButton("Remove selected effect");layout->addWidget(remove);
    auto *form=new QFormLayout;layout->addLayout(form);auto *error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText("Render processed copy…");layout->addWidget(buttons);
    auto *process=new QProcess(dialog);process->setProcessChannelMode(QProcess::SeparateChannels);
    auto output=std::make_shared<QString>();auto log=std::make_shared<QByteArray>();
    QObject::connect(add,&QPushButton::clicked,dialog,[=]{auto *item=new QListWidgetItem(types->currentText(),list);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Checked);item->setData(Qt::UserRole,QJsonObject{{"type",types->currentText()}});list->setCurrentItem(item);});
    QObject::connect(remove,&QPushButton::clicked,dialog,[=]{delete list->takeItem(list->currentRow());});
    QObject::connect(list,&QListWidget::currentItemChanged,dialog,[=](QListWidgetItem *item){
        while(form->rowCount())form->removeRow(0);if(!item)return;auto effect=item->data(Qt::UserRole).toJsonObject();const auto type=effect["type"].toString();
        auto field=[&](QString label,QString key,double value,double low,double high){auto *input=new ValueInput;input->setRange(low,high);input->setDecimals(3);input->setValue(effect[key].toDouble(value));form->addRow(label,input);QObject::connect(input,&QDoubleSpinBox::valueChanged,dialog,[=](double v){if(list->currentItem()!=item)return;auto current=item->data(Qt::UserRole).toJsonObject();current[key]=v;item->setData(Qt::UserRole,current);});};
        if(type=="Echo"){field("Delay (ms)","delayMs",250,1,90000);field("Echo decay","decay",.35,0,.95);}
        if(type=="Equalizer"){field("Frequency (Hz)","frequency",1000,20,20000);field("Q","q",1,.1,10);field("Gain (dB)","gainDb",0,-24,24);}
        if(type=="High pass"||type=="Low pass")field("Frequency (Hz)","frequency",type=="High pass"?80:12000,20,20000);
        if(type=="Compressor"){field("Threshold (linear)","threshold",.125,.001,1);field("Ratio","ratio",4,1,20);field("Attack (ms)","attackMs",20,.01,2000);field("Release (ms)","releaseMs",250,.01,9000);}
        if(type=="Limiter")field("Ceiling (linear)","limit",.95,.0625,1);
        if(type=="Gain")field("Gain (dB)","gainDb",0,-60,24);
    });
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    QObject::connect(dialog,&QDialog::finished,process,[=]{if(process->state()!=QProcess::NotRunning)process->kill();});
    QObject::connect(process,&QProcess::readyReadStandardError,dialog,[=]{*log=(*log+process->readAllStandardError()).right(8192);});
    QObject::connect(process,&QProcess::errorOccurred,dialog,[=](QProcess::ProcessError){error->setText(process->errorString());buttons->button(QDialogButtonBox::Save)->setEnabled(true);});
    QObject::connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),dialog,[=](int code,QProcess::ExitStatus status){buttons->button(QDialogButtonBox::Save)->setEnabled(true);if(code!=0||status!=QProcess::NormalExit){error->setText(QString::fromUtf8(*log));return;}if(!QFileInfo(*output).isFile()){error->setText("No processed audio was produced.");return;}ready(*output);dialog->accept();});
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog,[=]{
        QJsonArray chain;for(int i=0;i<list->count();++i){auto *item=list->item(i);auto effect=item->data(Qt::UserRole).toJsonObject();effect["enabled"]=item->checkState()==Qt::Checked;chain.append(effect);}QString reason;const auto graph=AudioDsp::filterGraph(chain,&reason);if(graph.isEmpty()){error->setText(reason);return;}
        const auto file=QFileDialog::getSaveFileName(dialog,"Processed audio",QFileInfo(source).dir().filePath(QFileInfo(source).completeBaseName()+"-processed.wav"),"WAV audio (*.wav)");if(file.isEmpty())return;
        if(QFileInfo(file).absoluteFilePath().compare(QFileInfo(source).absoluteFilePath(),Qt::CaseInsensitive)==0){error->setText("Choose a different file to preserve the original audio.");return;}
        *output=file;log->clear();error->setText("Rendering audio…");buttons->button(QDialogButtonBox::Save)->setEnabled(false);
        auto encoder=QCoreApplication::applicationDirPath()+"/ffmpeg.exe";if(!QFileInfo::exists(encoder))encoder=QStandardPaths::findExecutable("ffmpeg");
        if(encoder.isEmpty()){error->setText("FFmpeg is unavailable. Use the packaged Cuebase build or add FFmpeg to PATH.");buttons->button(QDialogButtonBox::Save)->setEnabled(true);return;}
        process->start(encoder,{"-hide_banner","-loglevel","error","-nostdin","-n","-i",source,"-vn","-af",graph,"-c:a","pcm_s24le",file});
    });dialog->open();
}
