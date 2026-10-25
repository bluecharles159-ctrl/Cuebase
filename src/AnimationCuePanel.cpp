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
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QMenu>


QWidget *animationCueEditor(BuildModel *m){
    auto *page=new QWidget;page->setObjectName("buildCueHome");auto *layout=new QVBoxLayout(page);layout->setContentsMargins(10,10,10,10);layout->setSpacing(8);
    auto *header=new QHBoxLayout;auto *back=new QPushButton(uiIcon("back"),"Animation cues");back->setObjectName("cueHome");auto *plus=new QPushButton(uiIcon("add"),"");plus->setToolTip("Create animation cue");plus->setFixedWidth(28);header->addWidget(back,1);header->addWidget(plus);layout->addLayout(header);
    auto *hint=new QLabel("Split your animation into independent sections. Each cue can play its own frames, repeat, or hold its final pose.");hint->setWordWrap(true);hint->setStyleSheet("color:palette(placeholder-text);");layout->addWidget(hint);
    auto *create=new QPushButton(uiIcon("cue"),"Create animation cue");create->setObjectName("createAnimationCue");create->setMinimumHeight(36);layout->addWidget(create);
    auto *list=new QListWidget;list->setObjectName("buildCueList");list->setSpacing(5);list->setStyleSheet("QListWidget{background:transparent;border:0;}QListWidget::item{background:palette(window);border:1px solid #405c57;border-radius:7px;padding:10px;}QListWidget::item:selected{background:#365a50;}");layout->addWidget(list);
    auto *detail=new QWidget;auto *body=new QVBoxLayout(detail);body->setContentsMargins(0,0,0,0);auto *title=new QLabel;title->setStyleSheet("font-weight:600;color:#b2e4cd;");auto *facts=new QLabel;facts->setWordWrap(true);body->addWidget(title);body->addWidget(facts);auto *edit=new QPushButton(uiIcon("edit"),"Edit cue behavior");edit->setObjectName("editSelectedCue");auto *play=new QPushButton(uiIcon("play"),"Play this cue");play->setObjectName("playSelectedCue");body->addWidget(play);body->addWidget(edit);body->addStretch();layout->addWidget(detail);detail->hide();
    auto *resume=new QPushButton(uiIcon("play"),"Continue");resume->setObjectName("continueSelectedCue");resume->setToolTip("Release the current manual hold and move to the next play.");body->insertWidget(3,resume);resume->setEnabled(false);
    auto updateContinue=[=]{resume->setEnabled(m->cuePreviewWaiting());};QObject::connect(m,&BuildModel::timeChanged,page,updateContinue);QObject::connect(m,&BuildModel::changed,page,updateContinue);QObject::connect(resume,&QPushButton::clicked,page,[m]{m->continueCuePreview();});
    auto selected=std::make_shared<QString>();auto refresh=std::make_shared<std::function<void()>>();
    *refresh=[=]{QSignalBlocker block(list);list->clear();QJsonObject active;for(const auto &v:m->design.animationCues){const auto c=v.toObject();auto *item=new QListWidgetItem(uiIcon("cue"),c["name"].toString()+"\n"+QString("Frames %1–%2 · ").arg(qRound(c["start"].toDouble()/m->frameMs())).arg(qRound(c["end"].toDouble()/m->frameMs()))+cueTimingLabel(c));item->setData(Qt::UserRole,c["id"]);list->addItem(item);if(c["id"].toString()==*selected)active=c;}if(active.isEmpty())selected->clear();const bool home=selected->isEmpty();list->setVisible(home&&!m->design.animationCues.isEmpty());create->setVisible(home&&m->design.animationCues.isEmpty());hint->setVisible(home);detail->setVisible(!home);plus->setVisible(home&&!m->design.animationCues.isEmpty());back->setIcon(uiIcon(home?"cue":"back"));if(!home){title->setText(active["name"].toString());facts->setText(QString("Frames %1–%2\n%3\n\nPlay is restricted to this cue until you return to Animation cues.").arg(qRound(active["start"].toDouble()/m->frameMs())).arg(qRound(active["end"].toDouble()/m->frameMs())).arg(cueTimingLabel(active)));}};
    auto open=[=](QString id){*selected=id;m->previewCueId=id;m->previewCueElapsed=0;m->previewCueStep=0;m->playing=false;for(auto v:m->design.animationCues)if(v.toObject()["id"].toString()==id)m->setTime(v.toObject()["start"].toDouble());(*refresh)();};
    auto dialog=[=](QString id,qint64 first,qint64 last){
        QJsonObject prior;for(auto v:m->design.animationCues)if(v.toObject()["id"].toString()==id)prior=v.toObject();
        auto *box=roundedModal(page,prior.isEmpty()?"Create animation cue":"Edit animation cue");box->setAttribute(Qt::WA_DeleteOnClose);box->setObjectName("cueRangeDialog");box->setMinimumWidth(380);
        box->setStyleSheet("QLabel,QCheckBox{background:transparent;}QLabel{font-size:10px;color:palette(text);}QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{background:palette(base);border:1px solid palette(mid);border-radius:5px;padding:4px;}QPushButton{padding:6px;border-radius:5px;background:palette(base);color:palette(text);}QPushButton:hover{background:#526a76;}QSpinBox:disabled,QDoubleSpinBox:disabled,QComboBox:disabled{background:#2c353b;color:#73818a;border-color:#39454c;}QCheckBox:disabled{color:#73818a;}QComboBox::drop-down{border:0;width:22px;}");
        auto *form=new QFormLayout(box);form->setContentsMargins(22,20,22,20);form->setSpacing(10);
        auto *heading=new QLabel(box->windowTitle());heading->setStyleSheet("font-size:15px;font-weight:600;color:#b2e4cd;");form->addRow(heading);
        auto *name=new QLineEdit(prior["name"].toString(QString("Cue %1").arg(m->design.animationCues.size()+1)));name->setObjectName("cueName");
        auto *start=new FrameInput,*end=new FrameInput;start->setObjectName("cueStart");end->setObjectName("cueEnd");start->setRange(0,10000000);end->setRange(1,10000001);start->setValue(qRound(prior["start"].toDouble(first)/m->frameMs()));end->setValue(qMax(start->value()+1,qRound(prior["end"].toDouble(last)/m->frameMs())));
        auto sequencePlays=std::make_shared<int>(qBound(1,prior["repeat"].toInt(1),100));
        auto *repeat=new FrameInput;repeat->setObjectName("cueRepeat");repeat->setRange(0,100);repeat->setValue(prior["loop"].toBool()?0:*sequencePlays);repeat->setToolTip("Total plays. Zero repeats the listed plays forever.");
        auto *loop=new QCheckBox("Loop");loop->setObjectName("cueLoop");loop->setChecked(repeat->value()>1||prior["loop"].toBool()||prior["repeatEnabled"].toBool());
        auto *plays=new QComboBox;plays->setObjectName("cuePlaySelector");
        auto *hold=new QCheckBox("Hold final pose");hold->setObjectName("cueHold");auto *manual=new QCheckBox("Continue manually");manual->setObjectName("cueHoldManual");
        auto *seconds=new ValueInput;seconds->setObjectName("cueHoldSeconds");seconds->setRange(0,86400);seconds->setDecimals(2);seconds->setSuffix(" s");
        form->addRow("Name",name);form->addRow("Start frame",start);form->addRow("End frame",end);form->addRow(loop);form->addRow("Loop N times",repeat);form->addRow("Playback settings",plays);form->addRow(hold);form->addRow(manual);form->addRow("Hold duration",seconds);
        auto *hint=new QLabel("Each play has its own hold. Choose a play above to adjust it independently. Zero repeats the listed plays forever.");hint->setWordWrap(true);hint->setStyleSheet("color:palette(placeholder-text);");form->addRow(hint);
        auto values=std::make_shared<QJsonArray>(prior["plays"].toArray());auto current=std::make_shared<int>(-1);auto loading=std::make_shared<bool>(false);
        auto savePlay=[=]{if(*loading||*current<0)return;while(values->size()<=*current)values->append(QJsonObject{});auto v=(*values)[*current].toObject();v["hold"]=hold->isChecked();v["holdManual"]=manual->isChecked();v["holdMs"]=seconds->value()*1000.;(*values)[*current]=v;};
        auto enabled=[=]{repeat->setEnabled(loop->isChecked());manual->setEnabled(hold->isChecked());seconds->setEnabled(hold->isChecked()&&!manual->isChecked());};
        auto readPlay=[=](int n){savePlay();*loading=true;*current=n;const auto v=n>=0&&n<values->size()?(*values)[n].toObject():prior;hold->setChecked(v["hold"].toBool(false));manual->setChecked(v["holdManual"].toBool(v["hold"].toBool()&&!v.contains("holdMs")));seconds->setValue(v["holdMs"].toDouble()/1000.);*loading=false;enabled();};
        auto refreshPlays=[=]{savePlay();if(repeat->value()>0)*sequencePlays=repeat->value();const int count=loop->isChecked()?*sequencePlays:1;const int n=qBound(0,*current,count-1);{QSignalBlocker block(plays);plays->clear();for(int i=0;i<count;++i)plays->addItem(QString("Play %1").arg(i+1));plays->setCurrentIndex(n);}readPlay(n);};
        QObject::connect(plays,&QComboBox::currentIndexChanged,box,readPlay);QObject::connect(loop,&QCheckBox::toggled,box,refreshPlays);QObject::connect(repeat,&QSpinBox::valueChanged,box,refreshPlays);QObject::connect(hold,&QCheckBox::toggled,box,enabled);QObject::connect(manual,&QCheckBox::toggled,box,enabled);refreshPlays();
        auto *error=new QLabel;error->setWordWrap(true);error->setStyleSheet("color:#efaa85");form->addRow(error);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form->addRow(buttons);
        for(auto *button:buttons->buttons())if(auto *push=qobject_cast<QPushButton*>(button)){push->setAutoDefault(false);push->setDefault(false);}
        auto highlight=[=]{m->cueRangeStart=qint64(start->value())*m->frameMs();m->cueRangeEnd=qint64(end->value())*m->frameMs();m->notify();};QObject::connect(start,&QSpinBox::valueChanged,box,highlight);QObject::connect(end,&QSpinBox::valueChanged,box,highlight);highlight();
        QObject::connect(buttons,&QDialogButtonBox::rejected,box,&QDialog::reject);QObject::connect(box,&QDialog::finished,page,[=]{m->cueRangeStart=m->cueRangeEnd=-1;m->notify();});
        QObject::connect(buttons,&QDialogButtonBox::accepted,box,[=]{
            if(name->text().trimmed().isEmpty()||end->value()<=start->value()){error->setText("Enter a name and an end frame after the start frame.");return;}
            savePlay();m->pushUndo();auto c=prior;const auto cueId=id.isEmpty()?Library::newId():id;c["id"]=cueId;c["name"]=name->text().trimmed();c["start"]=double(qint64(start->value())*m->frameMs());c["end"]=double(qint64(end->value())*m->frameMs());c["repeat"]=loop->isChecked()?*sequencePlays:1;c["loop"]=loop->isChecked()&&repeat->value()==0;c["repeatEnabled"]=loop->isChecked();c["hold"]=false;c["holdManual"]=false;c["holdMs"]=0;c["plays"]=*values;
            if(id.isEmpty())m->design.animationCues.append(c);else for(int i=0;i<m->design.animationCues.size();++i)if(m->design.animationCues[i].toObject()["id"].toString()==id)m->design.animationCues[i]=c;
            m->design.durationMs=qMax(m->design.durationMs,int(c["end"].toDouble()));box->accept();open(cueId);m->notify();
        });box->open();name->setFocus();name->selectAll();
    };
    QObject::connect(back,&QPushButton::clicked,page,[=]{selected->clear();m->previewCueId.clear();m->playing=false;m->notify();(*refresh)();});QObject::connect(create,&QPushButton::clicked,page,[=]{dialog({},m->design.startMs,m->design.durationMs);});QObject::connect(plus,&QPushButton::clicked,page,[=]{dialog({},m->design.startMs,m->design.durationMs);});QObject::connect(edit,&QPushButton::clicked,page,[=]{dialog(*selected,0,m->design.durationMs);});QObject::connect(play,&QPushButton::clicked,page,[=]{emit m->cuePlaybackRequested();});QObject::connect(list,&QListWidget::itemClicked,page,[=](QListWidgetItem *item){open(item->data(Qt::UserRole).toString());});
    list->setContextMenuPolicy(Qt::CustomContextMenu);QObject::connect(list,&QListWidget::customContextMenuRequested,page,[=](QPoint at){auto *item=list->itemAt(at);if(!item)return;const auto id=item->data(Qt::UserRole).toString();QMenu menu;auto *remove=menu.addAction(uiIcon("delete"),"Delete animation cue");if(menu.exec(list->mapToGlobal(at))==remove){m->pushUndo();for(int i=0;i<m->design.animationCues.size();++i)if(m->design.animationCues[i].toObject()["id"].toString()==id){m->design.animationCues.removeAt(i);break;}m->notify();}});
    QObject::connect(m,&BuildModel::cueFramesRequested,page,[=](qint64 first,qint64 last){emit m->cueEditorRequested({});dialog({},first,last);});QObject::connect(m,&BuildModel::changed,page,[=]{(*refresh)();});(*refresh)();return page;
}

QWidget *animationCueControls(Library *lib,const QString &elementId,std::function<void(QString)> trigger){
    auto *page=new QWidget;auto *layout=new QVBoxLayout(page);layout->setContentsMargins(6,6,6,6);const auto *element=lib->element(elementId);if(!element)return page;
    auto design=ElementDesign::fromJson(element->design);if(design.animationCues.isEmpty()){auto *hint=new QLabel("Create named entrance and exit animations in Build → Animation Cues.");hint->setWordWrap(true);layout->addWidget(hint);return page;}
    auto *explain=new QLabel("Show plays the entrance once, then stays visible. Play exit animates it off screen. Hide removes it immediately.");explain->setWordWrap(true);layout->addWidget(explain);auto *title=new QHBoxLayout;title->addWidget(new QLabel("ON-SCREEN CONTROLS"));title->addStretch();auto *hide=new QPushButton(uiIcon("hidden"),"Hide");title->addWidget(hide);QObject::connect(hide,&QPushButton::clicked,page,[trigger]{trigger({});});layout->addLayout(title);
    for(auto value:design.animationCues){const auto cue=value.toObject();const auto cueId=cue["id"].toString();auto *card=new QWidget;card->setStyleSheet("QWidget{background:palette(window);border-radius:5px;}");auto *body=new QVBoxLayout(card);body->setContentsMargins(7,6,7,6);auto *row=new QHBoxLayout;
        auto *label=new QLabel(cue["name"].toString());row->addWidget(label,1);auto *play=new QPushButton(uiIcon("play"),cue["hold"].toBool(true)?"Replay cue":"Play exit cue");play->setToolTip("Play this animation from its first frame, replacing the current animation");row->addWidget(play);body->addLayout(row);QObject::connect(play,&QPushButton::clicked,page,[trigger,cueId]{trigger(cueId);});
        auto *timing=new QHBoxLayout;auto *mode=new QComboBox;mode->addItems({"Manual","Program time","Before ad","After ad"});mode->setCurrentText(cue["trigger"].toString("Manual"));auto *seconds=new ValueInput;seconds->setRange(0,86400);seconds->setDecimals(2);seconds->setSuffix(" s");seconds->setValue(cue["triggerMs"].toDouble()/1000.);seconds->setEnabled(mode->currentText()!="Manual");QObject::connect(mode,&QComboBox::currentTextChanged,seconds,[=](QString text){seconds->setEnabled(text!="Manual");});
        auto *save=new QPushButton("Set timer");timing->addWidget(mode,1);timing->addWidget(seconds);timing->addWidget(save);auto *timingBox=new QWidget;timingBox->setLayout(timing);timingBox->hide();auto *schedule=new QPushButton("Schedule…");schedule->setCheckable(true);body->addWidget(schedule);QObject::connect(schedule,&QPushButton::toggled,timingBox,&QWidget::setVisible);body->addWidget(timingBox);layout->addWidget(card);
        QObject::connect(save,&QPushButton::clicked,page,[=]{for(auto &e:lib->elements)if(e.id==elementId){auto d=ElementDesign::fromJson(e.design);for(int i=0;i<d.animationCues.size();++i){auto c=d.animationCues[i].toObject();if(c["id"].toString()==cueId){c["trigger"]=mode->currentText();c["triggerMs"]=seconds->value()*1000.;d.animationCues[i]=c;}}e.design=d.toJson();lib->commitElements();break;}});
    }return page;
}
