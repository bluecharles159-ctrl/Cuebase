#include "CurveNoise.h"
#include "BuildModel.h"
#include "UiControls.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>

void editCurveNoise(BuildModel *model,const QVector<QPair<QString,QString>> &requested,QWidget *parent){
    QVector<QPair<QString,QString>> channels;QVector<QJsonObject> drafts;
    for(const auto &ref:requested){auto *object=model->byId(ref.first);auto *track=object?object->track(ref.second):nullptr;
        if(!object||object->locked||!track||channels.contains(ref))continue;
        channels.append(ref);drafts.append(track->noise.isEmpty()?QJsonObject{{"enabled",true},{"end",model->design.durationMs/1000.}}:track->noise);
    }
    if(channels.isEmpty())return;
    const auto original=model->design.toJson();
    auto *dialog=roundedModal(parent,"Animation noise");dialog->setObjectName("curveNoiseDialog");dialog->setMinimumWidth(440);
    auto *layout=new QVBoxLayout(dialog);layout->setContentsMargins(22,20,22,20);layout->setSpacing(10);
    auto *title=new QLabel("Animation noise");title->setStyleSheet("font-size:16px;font-weight:600;");layout->addWidget(title);
    auto *note=new QLabel("Modifies the entire curve. Original keyframes stay editable.\nChoose a curve below to adjust its own settings.");note->setWordWrap(true);layout->addWidget(note);
    auto *which=new QComboBox;which->setObjectName("noiseChannel");for(const auto &ref:channels)which->addItem(model->byId(ref.first)->name+" · "+QString(ref.second).replace("position.","translation."));layout->addWidget(which);
    auto *form=new QFormLayout;form->setContentsMargins(12,4,0,4);layout->addLayout(form);
    auto *enabled=new QCheckBox("Enabled");enabled->setObjectName("noiseEnabled");form->addRow(enabled);
    auto *blend=new QComboBox;blend->setObjectName("noiseBlend");blend->addItems({"Replace","Add","Subtract","Multiply"});form->addRow("Blend",blend);
    QMap<QString,ValueInput*> inputs;
    auto number=[&](QString key,QString label,double low,double high,double step){auto *input=new ValueInput;input->setObjectName("noise_"+key);input->setRange(low,high);input->setDecimals(3);input->setSingleStep(step);inputs[key]=input;form->addRow(label,input);};
    number("scale","Scale · seconds",.001,100000,.1);number("strength","Strength",-100000,100000,.1);number("offset","Offset · seconds",-100000,100000,.1);
    number("seed","Seed",0,2147483647,1);inputs["seed"]->setDecimals(0);
    number("depth","Detail levels",0,8,1);inputs["depth"]->setDecimals(0);
    number("roughness","Roughness",0,1,.05);number("lacunarity","Lacunarity",1,4,.1);number("influence","Influence",0,1,.05);
    auto *restricted=new QCheckBox("Restrict time range");restricted->setObjectName("noiseRestricted");form->addRow(restricted);
    number("start","Start · seconds",0,100000,.1);number("end","End · seconds",0,100000,.1);number("fadeIn","Blend in · seconds",0,100000,.1);number("fadeOut","Blend out · seconds",0,100000,.1);
    auto *error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);auto *remove=buttons->addButton("Remove noise",QDialogButtonBox::ActionRole);remove->setObjectName("noiseRemove");layout->addWidget(buttons);
    int current=0;bool removed=false;
    auto saveDraft=[&]{if(removed){drafts[current]={};return;}auto &draft=drafts[current];draft["enabled"]=enabled->isChecked();draft["blend"]=blend->currentText();draft["restricted"]=restricted->isChecked();for(auto it=inputs.begin();it!=inputs.end();++it)draft[it.key()]=it.value()->value();};
    auto rangeState=[&]{for(const auto &key:QStringList{"start","end","fadeIn","fadeOut"})inputs[key]->setEnabled(restricted->isChecked());};
    auto load=[&]{const auto draft=drafts[current];removed=draft.isEmpty();enabled->setChecked(draft["enabled"].toBool(!removed));blend->setCurrentText(draft["blend"].toString("Replace"));restricted->setChecked(draft["restricted"].toBool());
        for(auto it=inputs.begin();it!=inputs.end();++it){double def=0;if(QStringList{"scale","strength","seed","influence"}.contains(it.key()))def=1;else if(it.key()=="roughness")def=.5;else if(it.key()=="lacunarity")def=2;it.value()->setValue(draft[it.key()].toDouble(def));}rangeState();error->clear();};
    QObject::connect(which,&QComboBox::currentIndexChanged,dialog,[&](int index){saveDraft();current=index;load();});
    QObject::connect(enabled,&QCheckBox::clicked,dialog,[&]{removed=false;});
    QObject::connect(restricted,&QCheckBox::toggled,dialog,rangeState);
    QObject::connect(remove,&QPushButton::clicked,dialog,[&]{removed=true;enabled->setChecked(false);error->setText("Noise will be removed from this curve when you save.");});
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog,[&]{saveDraft();for(const auto &draft:drafts)if(draft["restricted"].toBool()&&draft["end"].toDouble()<draft["start"].toDouble()){error->setText("End must be at or after start on every curve.");return;}
        if(model->design.toJson()!=original){error->setText("The scene changed. Cancel and reopen noise controls to use the latest curves.");return;}
        model->pushUndo(true);for(int i=0;i<channels.size();++i){auto *object=model->byId(channels[i].first);object->track(channels[i].second)->noise=drafts[i];object->refreshLegacyKeys();}model->notify();dialog->accept();});
    load();dialog->exec();delete dialog;
}
