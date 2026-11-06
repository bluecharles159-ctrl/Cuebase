#pragma once
#include "AudioDsp.h"
#include "UiControls.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

inline bool editAudioAutomation(QWidget *parent,QJsonObject &effect){
    const auto options=AudioDsp::commands(effect.value("type").toString());
    if(options.isEmpty())return false;
    std::unique_ptr<QDialog> dialog(roundedModal(parent,"Processor automation"));
    dialog->setObjectName("audioAutomation");dialog->setMinimumWidth(350);
    auto *body=new QVBoxLayout(dialog.get());body->addWidget(new QLabel("Processor automation · "+effect.value("type").toString()));
    auto *keys=new QListWidget;body->addWidget(keys);
    auto *form=new QFormLayout;body->addLayout(form);auto *field=new QComboBox;form->addRow("Parameter",field);
    for(const auto &c:options)field->addItem(c.label,c.key);
    auto *time=new ValueInput;time->setObjectName("automationTime");time->setDecimals(3);time->setRange(0,86400);time->setSuffix(" s");form->addRow("Time",time);
    auto *value=new ValueInput;value->setObjectName("automationValue");value->setDecimals(3);form->addRow("Value",value);
    auto updateRange=[&]{const auto &c=options[field->currentIndex()];value->setRange(c.min,c.max);value->setValue(effect.value(c.key).toDouble(c.min));};
    QObject::connect(field,&QComboBox::currentIndexChanged,dialog.get(),updateRange);updateRange();
    auto *actions=new QHBoxLayout;body->addLayout(actions);auto *set=new QPushButton("Set key"),*remove=new QPushButton("Remove key");
    set->setObjectName("setAudioKey");actions->addWidget(set);actions->addWidget(remove);
    auto events=effect.value("automation").toArray();
    auto refresh=[&]{keys->clear();for(const auto e:events){const auto key=e.toObject();const int i=field->findData(key.value("field").toString());
        keys->addItem(QString("%1 s    %2    %3").arg(key.value("ms").toInteger()/1000.,0,'f',3).arg(field->itemText(i)).arg(key.value("value").toDouble()));}};
    refresh();
    QObject::connect(keys,&QListWidget::currentRowChanged,dialog.get(),[&](int i){if(i<0)return;auto key=events.at(i).toObject();field->setCurrentIndex(field->findData(key.value("field").toString()));time->setValue(key.value("ms").toInteger()/1000.);value->setValue(key.value("value").toDouble());});
    QObject::connect(set,&QPushButton::clicked,dialog.get(),[&]{
        const auto ms=qRound64(time->value()*1000);const auto name=field->currentData().toString();
        QJsonObject key{{"ms",ms},{"field",name},{"value",value->value()}};
        for(int i=0;i<events.size();++i){auto old=events.at(i).toObject();if(old.value("ms").toInteger()==ms && old.value("field").toString()==name){events[i]=key;refresh();return;}}
        if(events.size()<128){int pos=0;while(pos<events.size()&&events.at(pos).toObject().value("ms").toInteger()<=ms)++pos;events.insert(pos,key);refresh();}
    });
    QObject::connect(remove,&QPushButton::clicked,dialog.get(),[&]{int i=keys->currentRow();if(i>=0){events.removeAt(i);refresh();}});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);body->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog.get(),&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,dialog.get(),&QDialog::reject);
    if(dialog->exec()!=QDialog::Accepted)return false;effect["automation"]=events;return true;
}
