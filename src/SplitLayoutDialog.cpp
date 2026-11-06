#include "SplitLayout.h"
#include "Data.h"
#include "UiControls.h"
#include <QCameraDevice>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QMediaDevices>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

void SplitLayout::edit(QWidget *parent,Library *library,const QString &programId,qint64 time){
    const auto *program=library->program(programId);if(!program)return;
    const qint64 offset=program->offsetMs,length=program->lengthMs();
    const auto original=program->splitShots;
    auto shots=original;
    std::unique_ptr<QDialog> dialog(roundedModal(parent,"Split screens"));
    dialog->setObjectName("splitLayoutDialog");dialog->setMinimumWidth(570);
    auto *root=new QVBoxLayout(dialog.get());root->setContentsMargins(20,18,20,18);
    auto *title=new QLabel("Split screens · "+program->title);
    QFont heading=title->font();heading.setBold(true);heading.setPointSize(heading.pointSize()+3);title->setFont(heading);root->addWidget(title);
    auto *body=new QHBoxLayout;root->addLayout(body);
    auto *left=new QVBoxLayout;body->addLayout(left,1);auto *list=new QListWidget;list->setObjectName("splitSchedule");left->addWidget(list);
    auto *actions=new QHBoxLayout;left->addLayout(actions);auto *add=new QPushButton("+ Layout"),*remove=new QPushButton("Remove");
    add->setObjectName("splitAdd");actions->addWidget(add);actions->addWidget(remove);
    auto *settings=new QWidget;auto *fields=new QFormLayout(settings);body->addWidget(settings,2);
    auto seconds=[&](const QString &label,const QString &name){
        auto *s=new ValueInput;s->setDecimals(3);s->setRange(0,double(length)/1000);s->setSuffix(" s");
        s->setObjectName(name);s->setSingleStep(.1);fields->addRow(label,s);return s;
    };
    auto *start=seconds("Start","splitStart"),*end=seconds("End","splitEnd");
    auto *layout=new QComboBox;layout->setObjectName("splitLayout");layout->addItems(layouts());fields->addRow("Layout",layout);
    auto *transition=new QComboBox;transition->addItems(transitions());fields->addRow("Transition",transition);
    auto *fade=seconds("Transition time","splitFade");fade->setRange(0,3);
    QVector<QComboBox*> panes;
    for(int i=0;i<4;++i){
        auto *s=new QComboBox;s->setObjectName("splitPane"+QString::number(i+1));s->addItem("Current program","program");
        for(const auto &camera:QMediaDevices::videoInputs())s->addItem(camera.description(),"camera:"+QString::fromLatin1(camera.id().toBase64()));
        for(const auto &item:library->programs)if(!item.isAdSpot() && (!item.file.isEmpty() || !item.elementId.isEmpty()))
            s->addItem("Program · "+item.title,"clip:"+item.id);
        for(const auto &e:library->elements)if(e.type!="Break")s->addItem("Scene · "+e.name,"element:"+e.id);
        fields->addRow(QString("Pane %1").arg(i+1),s);panes.append(s);
    }
    auto *error=new QLabel;error->setWordWrap(true);root->addWidget(error);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);root->addWidget(buttons);
    bool loading=false;
    auto caption=[&](const QJsonObject &shot){
        const double a=(shot.value("start").toInteger()-offset)/1000.,b=(shot.value("end").toInteger()-offset)/1000.;
        return QString("%1 – %2 s\n%3").arg(a,0,'f',2).arg(b,0,'f',2).arg(shot.value("layout").toString());
    };
    auto load=[&](int index){
        loading=true;const bool valid=index>=0 && index<shots.size();settings->setEnabled(valid);remove->setEnabled(valid);
        if(valid){const auto s=shots.at(index).toObject();start->setValue((s.value("start").toInteger()-offset)/1000.);
            end->setValue((s.value("end").toInteger()-offset)/1000.);layout->setCurrentText(s.value("layout").toString());
            transition->setCurrentText(s.value("transition").toString());fade->setValue(s.value("fade").toInteger(350)/1000.);
            const auto sources=s.value("sources").toArray();
            for(int i=0;i<4;++i){const auto id=sources.at(i).toString("program");
                if(panes[i]->findData(id)<0)panes[i]->addItem("Unavailable · "+id,id);
                panes[i]->setCurrentIndex(panes[i]->findData(id));fields->setRowVisible(panes[i],i<paneCount(layout->currentText()));}
        }
        loading=false;
    };
    auto store=[&]{
        const int index=list->currentRow();if(loading||index<0||index>=shots.size())return;
        QJsonArray sources;
        for(int i=0;i<4;++i){const bool on=i<paneCount(layout->currentText());fields->setRowVisible(panes[i],on);if(on)sources.append(panes[i]->currentData().toString());}
        QJsonObject shot{{"start",offset+qRound64(start->value()*1000)},{"end",offset+qRound64(end->value()*1000)},
            {"fade",qRound64(fade->value()*1000)},{"layout",layout->currentText()},{"transition",transition->currentText()},{"sources",sources}};
        shots[index]=shot;list->item(index)->setText(caption(shot));error->clear();
    };
    for(auto *s:{start,end,fade})QObject::connect(s,&QDoubleSpinBox::valueChanged,dialog.get(),store);
    QObject::connect(layout,&QComboBox::currentIndexChanged,dialog.get(),store);
    QObject::connect(transition,&QComboBox::currentIndexChanged,dialog.get(),store);
    for(auto *s:panes)QObject::connect(s,&QComboBox::currentIndexChanged,dialog.get(),store);
    QObject::connect(list,&QListWidget::currentRowChanged,dialog.get(),load);
    QObject::connect(add,&QPushButton::clicked,dialog.get(),[&]{
        qint64 begin=qBound(offset,time,offset+length);
        // Find the next open interval without moving existing shots.
        qint64 stop=offset+length;
        for(int pass=0;pass<=shots.size();++pass){
            bool occupied=false;stop=offset+length;
            for(const auto value:shots){const auto s=value.toObject();const auto a=s.value("start").toInteger(),b=s.value("end").toInteger();
                if(a<=begin && begin<b){begin=b;occupied=true;break;}if(a>begin)stop=qMin(stop,a);}
            if(!occupied)break;
        }
        if(begin>=stop){error->setText("There is no room after this time. Shorten a layout or move the playhead.");return;}
        QJsonObject shot{{"start",begin},{"end",qMin(begin+10000,stop)},{"layout","Side by side"},{"transition","Dissolve"},
                         {"fade",350},{"sources",QJsonArray{"program","program"}}};
        shots.append(shot);list->addItem(caption(shot));list->setCurrentRow(list->count()-1);
    });
    QObject::connect(remove,&QPushButton::clicked,dialog.get(),[&]{
        int i=list->currentRow();if(i<0)return;shots.removeAt(i);delete list->takeItem(i);load(list->currentRow());
    });
    for(const auto value:shots)list->addItem(caption(value.toObject()));
    list->setCurrentRow(shots.isEmpty()?-1:0);load(list->currentRow());
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog.get(),&QDialog::reject);
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog.get(),[&]{
        if(shots==original){dialog->accept();return;}
        const auto invalid=validate(shots);if(!invalid.isEmpty()){error->setText(invalid);return;}
        for(auto &item:library->programs)if(item.id==programId){
            if(item.splitShots!=original || item.lengthMs()!=length || item.offsetMs!=offset){
                error->setText("The program changed. Reopen its layout schedule.");return;
            }
            const int oldOrigin=item.splitClockOrigin;
            if(item.splitClockOrigin<0)item.splitClockOrigin=item.offsetMs;
            item.splitShots=shots;QString reason;
            if(!library->saveChecked(&reason)){item.splitShots=original;item.splitClockOrigin=oldOrigin;error->setText(reason);return;}
            emit library->splitLayoutsChanged();dialog->accept();return;
        }
        error->setText("This program was removed.");
    });
    dialog->exec();
}
