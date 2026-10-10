#include "AnimationCuePanel.h"
#include "BuildModel.h"
#include "Data.h"
#include "UiControls.h"
#include "CueSequence.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalBlocker>
#include <memory>

QWidget *animationCueEditor(BuildModel *m){
    auto *page=new QWidget;auto *layout=new QVBoxLayout(page);layout->setContentsMargins(8,8,8,8);
    auto *heading=new QLabel("ANIMATION CUES");layout->addWidget(heading);
    auto *help=new QLabel("Select keys and choose Cue frames, or enter a range here. Each play includes its hold. Add these cues to a Studio cue board to preview, customize repetitions and render live.");help->setWordWrap(true);layout->addWidget(help);
    auto *list=new QComboBox;layout->addWidget(list);auto *name=new QLineEdit;name->setObjectName("cueName");
    auto *start=new FrameInput,*end=new FrameInput;start->setRange(0,10000000);end->setRange(1,10000000);
    start->setObjectName("cueStart");end->setObjectName("cueEnd");auto *hold=new QCheckBox("Keep visible after this animation");hold->setObjectName("cueHold");hold->setChecked(true);
    auto *form=new QFormLayout;form->addRow("Name",name);form->addRow("Start frame",start);form->addRow("End frame",end);form->addRow(hold);layout->addLayout(form);
    auto *repeat=new QSpinBox;repeat->setRange(1,100);repeat->setObjectName("cueRepeat");repeat->setToolTip("Total plays: 1 plays once; 4 plays four times.");auto *loop=new QCheckBox("Loop until stopped");loop->setObjectName("cueLoop");auto *manual=new QCheckBox("Hold until continued manually");manual->setObjectName("cueHoldManual");auto *holdTime=new ValueInput;holdTime->setRange(0,86400);holdTime->setDecimals(2);holdTime->setSuffix(" s");holdTime->setObjectName("cueHoldSeconds");form->addRow("Plays",repeat);form->addRow(loop);form->addRow(manual);form->addRow("Hold after each play",holdTime);
    auto timingEnabled=[=]{manual->setEnabled(hold->isChecked());holdTime->setEnabled(hold->isChecked()&&!manual->isChecked());};QObject::connect(hold,&QCheckBox::toggled,page,timingEnabled);QObject::connect(manual,&QCheckBox::toggled,page,timingEnabled);
    auto *row=new QHBoxLayout;auto *add=new QPushButton(uiIcon("add"),"Add"),*save=new QPushButton(uiIcon("save"),"Apply"),*remove=new QPushButton(uiIcon("delete"),"Remove");row->addWidget(add);row->addWidget(save);row->addWidget(remove);layout->addLayout(row);
    auto *jump=new QPushButton("Go to start frame");layout->addWidget(jump);layout->addStretch();
    auto read=[=]{const int i=list->currentIndex();const bool valid=i>=0&&i<m->design.animationCues.size();save->setEnabled(valid);remove->setEnabled(valid);jump->setEnabled(valid);if(!valid){name->clear();return;}auto cue=m->design.animationCues[i].toObject();name->setText(cue["name"].toString());start->setValue(qRound(cue["start"].toDouble()/m->frameMs()));end->setValue(qRound(cue["end"].toDouble()/m->frameMs()));hold->setChecked(cue["hold"].toBool(true));repeat->setValue(cue["repeat"].toInt(1));loop->setChecked(cue["loop"].toBool());manual->setChecked(cue["holdManual"].toBool(!cue.contains("holdMs")));holdTime->setValue(cue["holdMs"].toDouble()/1000.);timingEnabled();};
    auto last=std::make_shared<QByteArray>();auto refresh=[=]{const auto json=QJsonDocument(m->design.animationCues).toJson(QJsonDocument::Compact);if(json==*last)return;*last=json;const auto keep=list->currentData().toString();QSignalBlocker block(list);list->clear();for(auto value:m->design.animationCues){auto cue=value.toObject();list->addItem(cue["name"].toString(),cue["id"].toString());}if(list->findData(keep)>=0)list->setCurrentIndex(list->findData(keep));read();};
    QObject::connect(list,&QComboBox::currentIndexChanged,page,[=]{read();});QObject::connect(m,&BuildModel::changed,page,refresh);
    QObject::connect(add,&QPushButton::clicked,page,[=]{m->pushUndo();const auto id=Library::newId();m->design.animationCues.append(QJsonObject{{"id",id},{"name",QString("Animation %1").arg(m->design.animationCues.size()+1)},{"start",m->design.startMs},{"end",m->design.durationMs},{"hold",true},{"trigger","Manual"}});m->notify();list->setCurrentIndex(list->findData(id));});
    QObject::connect(m,&BuildModel::cueFramesRequested,page,[=](qint64 first,qint64 last){m->pushUndo();const auto id=Library::newId();m->design.animationCues.append(QJsonObject{{"id",id},{"name",QString("Cue %1").arg(m->design.animationCues.size()+1)},{"start",double(first)},{"end",double(last)},{"hold",false},{"repeat",1},{"holdManual",false},{"holdMs",0},{"trigger","Manual"}});m->notify();list->setCurrentIndex(list->findData(id));emit m->cueEditorRequested(id);name->setFocus();name->selectAll();});
    QObject::connect(save,&QPushButton::clicked,page,[=]{int i=list->currentIndex();if(i<0||name->text().trimmed().isEmpty())return;m->pushUndo();auto cue=m->design.animationCues[i].toObject();cue["name"]=name->text().trimmed();cue["start"]=start->value()*m->frameMs();const int finish=qMax(start->value()+1,end->value())*m->frameMs();cue["end"]=finish;cue["hold"]=hold->isChecked();cue["holdManual"]=manual->isChecked();cue["holdMs"]=holdTime->value()*1000.;cue["repeat"]=repeat->value();cue["loop"]=loop->isChecked();m->design.durationMs=qMax(m->design.durationMs,finish);m->design.animationCues[i]=cue;m->notify();});
    QObject::connect(remove,&QPushButton::clicked,page,[=]{if(list->currentIndex()<0)return;m->pushUndo();m->design.animationCues.removeAt(list->currentIndex());m->notify();});
    QObject::connect(jump,&QPushButton::clicked,page,[=]{m->setTime(start->value()*m->frameMs());});refresh();return page;
}

QWidget *animationCueControls(Library *lib,const QString &elementId,std::function<void(QString)> trigger){
    auto *page=new QWidget;auto *layout=new QVBoxLayout(page);layout->setContentsMargins(6,6,6,6);const auto *element=lib->element(elementId);if(!element)return page;
    auto design=ElementDesign::fromJson(element->design);if(design.animationCues.isEmpty()){auto *hint=new QLabel("Create named entrance and exit animations in Build → Animation Cues.");hint->setWordWrap(true);layout->addWidget(hint);return page;}
    auto *explain=new QLabel("Show plays the entrance once, then stays visible. Play exit animates it off screen. Hide removes it immediately.");explain->setWordWrap(true);layout->addWidget(explain);auto *title=new QHBoxLayout;title->addWidget(new QLabel("ON-SCREEN CONTROLS"));title->addStretch();auto *hide=new QPushButton(uiIcon("hidden"),"Hide");title->addWidget(hide);QObject::connect(hide,&QPushButton::clicked,page,[trigger]{trigger({});});layout->addLayout(title);
    for(auto value:design.animationCues){const auto cue=value.toObject();const auto cueId=cue["id"].toString();auto *card=new QWidget;card->setStyleSheet("QWidget{background:#34363b;border-radius:5px;}");auto *body=new QVBoxLayout(card);body->setContentsMargins(7,6,7,6);auto *row=new QHBoxLayout;
        auto *label=new QLabel(cue["name"].toString());row->addWidget(label,1);auto *play=new QPushButton(uiIcon("play"),cue["hold"].toBool(true)?"Replay cue":"Play exit cue");play->setToolTip("Play this animation from its first frame, replacing the current animation");row->addWidget(play);body->addLayout(row);QObject::connect(play,&QPushButton::clicked,page,[trigger,cueId]{trigger(cueId);});
        auto *timing=new QHBoxLayout;auto *mode=new QComboBox;mode->addItems({"Manual","Program time","Before ad","After ad"});mode->setCurrentText(cue["trigger"].toString("Manual"));auto *seconds=new ValueInput;seconds->setRange(0,86400);seconds->setDecimals(2);seconds->setSuffix(" s");seconds->setValue(cue["triggerMs"].toDouble()/1000.);seconds->setEnabled(mode->currentText()!="Manual");QObject::connect(mode,&QComboBox::currentTextChanged,seconds,[=](QString text){seconds->setEnabled(text!="Manual");});
        auto *save=new QPushButton("Set timer");timing->addWidget(mode,1);timing->addWidget(seconds);timing->addWidget(save);auto *timingBox=new QWidget;timingBox->setLayout(timing);timingBox->hide();auto *schedule=new QPushButton("Schedule…");schedule->setCheckable(true);body->addWidget(schedule);QObject::connect(schedule,&QPushButton::toggled,timingBox,&QWidget::setVisible);body->addWidget(timingBox);layout->addWidget(card);
        QObject::connect(save,&QPushButton::clicked,page,[=]{for(auto &e:lib->elements)if(e.id==elementId){auto d=ElementDesign::fromJson(e.design);for(int i=0;i<d.animationCues.size();++i){auto c=d.animationCues[i].toObject();if(c["id"].toString()==cueId){c["trigger"]=mode->currentText();c["triggerMs"]=seconds->value()*1000.;d.animationCues[i]=c;}}e.design=d.toJson();lib->commitElements();break;}});
    }return page;
}
