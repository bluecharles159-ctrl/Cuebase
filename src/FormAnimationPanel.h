#pragma once
#include "BuildModel.h"
#include "UiControls.h"
#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QCursor>
#include <QSignalBlocker>

class FormAnimationPanel final:public QWidget {
    BuildModel *model;QComboBox *forms;ValueInput *influence;QPushButton *key,*remove;bool syncing=false;
    QString property()const{return forms->currentData().toString();}
    void insert(){if(auto *object=model->active();object&&!property().isEmpty())model->keyProperty(object->id,property());}
    void refresh(){
        syncing=true;const auto selected=property();forms->clear();auto *object=model->active();if(object)for(const auto &entry:object->style["form.poses"].toArray()){const auto pose=entry.toObject();forms->addItem(pose["name"].toString(),"form."+pose["id"].toString());}
        const int index=forms->findData(selected);forms->setCurrentIndex(index>=0?index:forms->count()-1);
        const bool enabled=object&&!object->locked&&!property().isEmpty();influence->setEnabled(enabled);key->setEnabled(enabled);remove->setEnabled(enabled&&model->propertyIsKeyed(*object,property(),model->t));
        influence->setValue(enabled?object->propertyValue(property(),model->t):0);syncing=false;
    }
protected:
    bool eventFilter(QObject *,QEvent *event)override{
        if(isVisible()&&(event->type()==QEvent::KeyPress||event->type()==QEvent::ShortcutOverride)&&influence->isEnabled()){
            auto *pressed=static_cast<QKeyEvent*>(event);auto *hovered=QApplication::widgetAt(QCursor::pos());
            if((hovered==influence||influence->isAncestorOf(hovered))&&(pressed->key()==Qt::Key_I||pressed->key()==Qt::Key_K)&&pressed->modifiers()==Qt::NoModifier){if(event->type()==QEvent::KeyPress)insert();event->accept();return true;}
        }return false;
    }
public:
    explicit FormAnimationPanel(BuildModel *m,QWidget *parent=nullptr):QWidget(parent),model(m){
        setObjectName("formAnimationPanel");auto *layout=new QFormLayout(this);layout->setContentsMargins(16,16,16,16);auto *help=new QLabel("Move, rotate, scale or extrude selected vertices, edges or faces. Confirm to capture a form. Influence 0 restores its start; 1 reaches the authored result.");help->setWordWrap(true);layout->addRow(help);
        forms=new QComboBox;forms->setObjectName("formSelection");layout->addRow("Form",forms);influence=new ValueInput;influence->setObjectName("formInfluence");influence->setRange(0,1);influence->setDecimals(3);influence->setSingleStep(.05);influence->setToolTip("Hover and press I or K to insert or replace a keyframe");layout->addRow("Influence",influence);
        key=new QPushButton(uiIcon("key"),"Insert / replace key");key->setObjectName("formInsertKey");remove=new QPushButton(uiIcon("delete"),"Remove current key");remove->setObjectName("formRemoveKey");layout->addRow(key);layout->addRow(remove);
        connect(key,&QPushButton::clicked,this,[this]{insert();});connect(remove,&QPushButton::clicked,this,[this]{if(auto *o=model->active();o&&model->propertyIsKeyed(*o,property(),model->t))model->togglePropertyKey(o->id,property());});
        connect(forms,&QComboBox::currentIndexChanged,this,[this]{if(!syncing){auto *o=model->active();QSignalBlocker block(influence);influence->setValue(o?o->propertyValue(property(),model->t):0);}});
        connect(influence,&QDoubleSpinBox::valueChanged,this,[this](double value){if(syncing)return;if(auto *o=model->active();o&&!property().isEmpty()){model->pushUndo();model->editProperty(o->id,property(),value);}});
        connect(model,&BuildModel::changed,this,[this]{refresh();});connect(model,&BuildModel::selectionChanged,this,[this]{refresh();});qApp->installEventFilter(this);refresh();
    }
};
