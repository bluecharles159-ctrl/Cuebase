#pragma once
#include "BuildModel.h"
#include "FormAnimation.h"
#include "UiControls.h"
#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCursor>
#include <QSignalBlocker>

class FormAnimationPanel final : public QWidget {
    BuildModel *model;
    QComboBox *forms;
    ValueInput *influence;
    QLineEdit *name;
    QPushButton *key,*remove,*bake;
    QLabel *empty;
    bool syncing=false;
    QString property() const { return forms->currentData().toString(); }
    void insert() {
        if(auto *object=model->active();object&&!object->locked&&!property().isEmpty())
            model->keyProperty(object->id,property());
    }
    void syncValue() {
        const auto *object=model->active();
        const bool enabled=object&&!object->locked&&!property().isEmpty();
        influence->setEnabled(enabled);key->setEnabled(enabled);name->setEnabled(enabled);bake->setEnabled(enabled);
        remove->setEnabled(enabled&&model->propertyIsKeyed(*object,property(),model->t));
        QSignalBlocker a(influence),b(name);
        influence->setValue(enabled?object->propertyValue(property(),model->t):0);
        name->setText(forms->currentText());
        empty->setVisible(forms->count()==0);
    }
    void refresh() {
        syncing=true;const auto selected=property();QSignalBlocker block(forms);forms->clear();
        if(const auto *object=model->active())for(const auto &entry:object->style["form.poses"].toArray()){
            const auto pose=entry.toObject();forms->addItem(pose["name"].toString(),"form."+pose["id"].toString());
        }
        const int index=forms->findData(selected);forms->setCurrentIndex(index>=0?index:forms->count()-1);
        syncValue();syncing=false;
    }
protected:
    bool eventFilter(QObject *,QEvent *event) override {
        if(isVisible()&&(event->type()==QEvent::KeyPress||event->type()==QEvent::ShortcutOverride)&&influence->isEnabled()){
            auto *pressed=static_cast<QKeyEvent*>(event);auto *hovered=QApplication::widgetAt(QCursor::pos());
            if(hovered&&(hovered==influence||influence->isAncestorOf(hovered))&&(pressed->key()==Qt::Key_I||pressed->key()==Qt::Key_K)&&pressed->modifiers()==Qt::NoModifier){
                if(event->type()==QEvent::KeyPress&&!pressed->isAutoRepeat())insert();event->accept();return true;
            }
        }
        return false;
    }
public:
    explicit FormAnimationPanel(BuildModel *m,QWidget *parent=nullptr):QWidget(parent),model(m){
        setObjectName("formAnimationPanel");auto *layout=new QFormLayout(this);layout->setContentsMargins(16,16,16,16);layout->setSpacing(10);
        empty=new QLabel("Move, rotate, scale or extrude vertices, edges or faces and confirm to capture your first form.");empty->setWordWrap(true);layout->addRow(empty);
        forms=new QComboBox;forms->setObjectName("formSelection");layout->addRow("Form",forms);
        name=new QLineEdit;name->setObjectName("formName");name->setMaxLength(100);layout->addRow("Name",name);
        influence=new ValueInput;influence->setObjectName("formInfluence");influence->setRange(0,1);influence->setDecimals(3);influence->setSingleStep(.05);influence->setToolTip("0: original pose. 1: full form. Hover and press I or K to key.");layout->addRow("Influence",influence);
        key=new QPushButton(uiIcon("key"),"Insert / replace key");key->setObjectName("formInsertKey");
        remove=new QPushButton(uiIcon("delete"),"Remove current key");remove->setObjectName("formRemoveKey");layout->addRow(key);layout->addRow(remove);
        bake=new QPushButton("Bake and remove form");bake->setObjectName("formBakeRemove");bake->setToolTip("Keep this form's current influence in the mesh and remove its animation. Undo restores it.");layout->addRow(bake);
        connect(key,&QPushButton::clicked,this,[this]{insert();});
        connect(remove,&QPushButton::clicked,this,[this]{
            auto *o=model->active();const auto path=property();if(!o||o->locked)return;
            auto *track=o->track(path);const int index=track?track->indexAt(model->t):-1;if(index<0)return;
            const double value=o->propertyValue(path,model->t);model->pushUndo(true);track->keys.removeAt(index);
            o->tracks.removeIf([](const auto &item){return item.keys.isEmpty();});o->style[path]=value;
            if(o->track(path))o->overrides[path]=value;else o->overrides.remove(path);
            o->refreshLegacyKeys();model->notify();
        });
        connect(forms,&QComboBox::currentIndexChanged,this,[this]{if(!syncing)syncValue();});
        connect(name,&QLineEdit::editingFinished,this,[this]{auto *o=model->active();const auto text=name->text().trimmed();if(!o||o->locked||text.isEmpty()||text==forms->currentText())return;auto poses=o->style["form.poses"].toArray();for(int i=0;i<poses.size();++i){auto pose=poses[i].toObject();if("form."+pose["id"].toString()!=property())continue;model->pushUndo(true);pose["name"]=text;poses[i]=pose;o->style["form.poses"]=poses;model->notify();break;}});
        connect(influence,&QDoubleSpinBox::valueChanged,this,[this](double value){if(syncing)return;if(auto *o=model->active();o&&!o->locked&&!property().isEmpty()){model->pushUndo();model->editProperty(o->id,property(),value);}});
        connect(bake,&QPushButton::clicked,this,[this]{
            auto *o=model->active();const auto path=property();if(!o||o->locked||path.isEmpty())return;
            auto isolated=*o;QJsonArray keep,chosen;
            for(const auto &entry:o->style["form.poses"].toArray())("form."+entry.toObject()["id"].toString()==path?chosen:keep).append(entry);
            if(chosen.isEmpty())return;isolated.style["form.poses"]=chosen;const Mesh baked=FormAnimation::evaluate(isolated,model->t);
            model->pushUndo(true);o->mesh=baked;o->style["form.poses"]=keep;o->style.remove(path);o->overrides.remove(path);o->defaults.remove(path);
            o->tracks.removeIf([&](const PropertyTrack &track){return track.property==path;});o->refreshLegacyKeys();model->notify();
        });
        connect(model,&BuildModel::changed,this,[this]{refresh();});connect(model,&BuildModel::selectionChanged,this,[this]{refresh();});
        connect(model,&BuildModel::timeChanged,this,[this]{syncValue();});qApp->installEventFilter(this);refresh();
    }
    ~FormAnimationPanel() override { qApp->removeEventFilter(this); }
};
