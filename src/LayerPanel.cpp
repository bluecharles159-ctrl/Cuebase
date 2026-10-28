#include "LayerPanel.h"
#include "BuildModel.h"
#include "UiControls.h"
#include <QDropEvent>
#include <QHeaderView>
#include <QTreeWidgetItemIterator>
#include <QStyledItemDelegate>
#include <QSpinBox>
#include <QMenu>
#include <QTimer>

namespace {
class LayerOrderDelegate final:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent,const QStyleOptionViewItem &,const QModelIndex &index)const override{if(index.data(Qt::UserRole).toBool())return nullptr;auto *input=new FrameInput(parent);input->setRange(-10000,10000);input->setMinimumWidth(48);return input;}
    void setEditorData(QWidget *editor,const QModelIndex &index)const override{static_cast<QSpinBox*>(editor)->setValue(index.data(Qt::EditRole).toInt());}
    void setModelData(QWidget *editor,QAbstractItemModel *model,const QModelIndex &index)const override{auto *input=static_cast<QSpinBox*>(editor);input->interpretText();model->setData(index,input->value(),Qt::EditRole);}
};
}

LayerPanel::LayerPanel(BuildModel *model,QWidget *parent):QTreeWidget(parent),m(model){
    setColumnCount(4);setHeaderLabels({"Layers","", "", "Z"});setRootIsDecorated(true);setIndentation(13);
    setItemDelegateForColumn(3,new LayerOrderDelegate(this));setColumnWidth(3,48);headerItem()->setToolTip(3,"Overlap order · higher values appear in front without changing size or position. Double-click to edit.");
    setStyleSheet("QTreeWidget{background:palette(window);color:palette(text);border:0;font-size:10px;} QTreeWidget::item{height:23px;} QTreeWidget::item:selected{background:palette(highlight);color:palette(highlighted-text);} QHeaderView::section{background:palette(button);color:palette(button-text);border:0;padding:4px;font-size:10px;}");
    setSelectionMode(QAbstractItemView::ExtendedSelection);setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);setEditTriggers(QAbstractItemView::EditKeyPressed);
    setMinimumWidth(175);setMaximumWidth(260);header()->setSectionResizeMode(0,QHeaderView::Stretch);setColumnWidth(1,26);setColumnWidth(2,26);
    headerItem()->setIcon(1,uiIcon("eye"));headerItem()->setIcon(2,uiIcon("lock"));
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this,&QWidget::customContextMenuRequested,this,[this](const QPoint &point){
        auto *row=itemAt(point);if(!row)return;const auto id=row->data(0,Qt::UserRole).toString();if(!m->sel.contains(id))m->select(id);
        QMenu menu(this);bool enabled=false;for(auto *object:m->selected())enabled|=!object->locked&&object->isRenderable();
        auto add=[&](const QString &label,int direction,bool edge){auto *action=menu.addAction(label);action->setEnabled(enabled);connect(action,&QAction::triggered,this,[=,this]{shiftOrder(direction,edge);});};
        add("Bring forward",1,false);add("Send backward",-1,false);menu.addSeparator();add("Bring to front",1,true);add("Send to back",-1,true);menu.exec(viewport()->mapToGlobal(point));
    });
    connect(m,&BuildModel::changed,this,[this]{refresh();});connect(m,&BuildModel::selectionChanged,this,[this]{refresh();});
    connect(this,&QTreeWidget::itemSelectionChanged,this,[this]{if(syncing)return;QSet<QString> ids;for(auto *item:selectedItems())ids.insert(item->data(0,Qt::UserRole).toString());auto *cur=currentItem();m->setSelection(ids,cur?cur->data(0,Qt::UserRole).toString():QString());});
    connect(this,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *item,int column){
        if(syncing)return;const QString id=item->data(0,Qt::UserRole).toString();auto *o=m->byId(id);if(!o)return;
        if(column==1||column==2){m->pushUndo();if(column==1)o->visible=!o->visible;else o->locked=!o->locked;m->notify();}
        else if(m->sel.contains(id))m->setSelection(m->sel,id);
    });
    connect(this,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item,int column){if(column==0||column==3)editItem(item,column);});
    connect(this,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem *item,int col){if(syncing)return;const auto id=item->data(0,Qt::UserRole).toString();if(col==0)m->rename(id,item->text(0));else if(col==3){if(auto *object=m->byId(id)){const int order=qBound(-10000,item->data(3,Qt::EditRole).toInt(),10000);if(!object->locked&&order!=object->style["zIndex"].toInt()){m->pushUndo(true);object->style["zIndex"]=order;QTimer::singleShot(0,m,[model=m]{model->notify();});}else{syncing=true;item->setData(3,Qt::EditRole,object->style["zIndex"].toInt());syncing=false;}}}});refresh();
}
void LayerPanel::shiftOrder(int direction,bool toEdge){
    if(direction!=1&&direction!=-1)return;
    int low=10000,high=-10000,otherLow=10000,otherHigh=-10000;QList<DObject*> selected;bool others=false;
    for(auto &object:m->design.objects){if(!object.isRenderable())continue;const int z=object.style["zIndex"].toInt();if(m->sel.contains(object.id)&&!object.locked){selected.append(&object);low=qMin(low,z);high=qMax(high,z);}else{others=true;otherLow=qMin(otherLow,z);otherHigh=qMax(otherHigh,z);}}
    if(selected.isEmpty())return;
    int delta=direction;if(toEdge){if(!others)return;delta=direction>0?qMax(0,otherHigh-low+1):qMin(0,otherLow-high-1);}
    delta=qBound(-10000-low,delta,10000-high);if(!delta)return;
    m->pushUndo(true);for(auto *object:selected)object->style["zIndex"]=object->style["zIndex"].toInt()+delta;m->notify();
}
void LayerPanel::refresh(){
    QString sig;for(const auto &o:m->design.objects)sig+=o.id+o.name+o.parentId+QString::number(o.visible)+QString::number(o.locked)+QString::number(o.style["zIndex"].toInt())+o.style["component.id"].toString()+o.style["component.role"].toString();
    syncing=true;
    if(sig!=signature){
        QSet<QString> closed;for(QTreeWidgetItemIterator it(this);*it;++it)if((*it)->childCount()&&!(*it)->isExpanded())closed.insert((*it)->data(0,Qt::UserRole).toString());
        signature=sig;clear();QHash<QString,QTreeWidgetItem*> items;
        for(const auto &o:m->design.objects){if(o.type=="Audio")continue;auto *item=new QTreeWidgetItem(QStringList{o.name});item->setData(0,Qt::UserRole,o.id);items[o.id]=item;
            item->setFlags(item->flags()|Qt::ItemIsEditable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled);
            item->setData(3,Qt::EditRole,o.style["zIndex"].toInt());item->setToolTip(3,"Higher Z draws in front · double-click to edit · undo supported");
            item->setData(3,Qt::UserRole,o.locked||!o.isRenderable());
            const bool component=!o.style["component.id"].toString().isEmpty();item->setIcon(0,uiIcon(component?"component":o.type));item->setIcon(1,uiIcon(o.visible?"eye":"hidden",o.visible));item->setIcon(2,uiIcon(o.locked?"lock":"unlock",o.locked));
            item->setToolTip(0,o.type+" · F2 to rename · Ctrl+P to parent to the active layer");item->setToolTip(1,o.visible?"Hide layer":"Show layer");item->setToolTip(2,o.locked?"Unlock layer":"Lock layer");
            if(component)item->setToolTip(0,o.style["component.role"].toString()+" · "+o.type+" · / opens Components");
        }
        for(const auto &o:m->design.objects){if(o.type=="Audio")continue;auto *item=items[o.id];QSet<QString> seen{o.id};auto *parent=m->byId(o.parentId);bool cycle=false;
            while(parent){if(seen.contains(parent->id)){cycle=true;break;}seen.insert(parent->id);parent=m->byId(parent->parentId);}
            if(!cycle&&items.contains(o.parentId))items[o.parentId]->addChild(item);else addTopLevelItem(item);
        }
        for(auto it=items.begin();it!=items.end();++it)it.value()->setExpanded(!closed.contains(it.key()));
    }
    for(QTreeWidgetItemIterator it(this);*it;++it){auto *item=*it;const auto id=item->data(0,Qt::UserRole).toString();item->setSelected(m->sel.contains(id));if(id==m->activeId)setCurrentItem(item,0,QItemSelectionModel::NoUpdate);}
    syncing=false;
}
void LayerPanel::dropEvent(QDropEvent *event){
    if(auto *target=itemAt(event->position().toPoint());target&&dropIndicatorPosition()==QAbstractItemView::OnItem){
        const QString parent=target->data(0,Qt::UserRole).toString();const auto children=m->sel;const auto active=m->activeId;
        if(children.contains(parent)){event->ignore();return;}auto selection=children;selection.insert(parent);m->setSelection(selection,parent);
        const bool changed=m->parentSelected();m->setSelection(children,active);if(changed)event->acceptProposedAction();else event->ignore();return;
    }
    m->pushUndo();syncing=true;QTreeWidget::dropEvent(event);QVector<DObject> objects;
    for(QTreeWidgetItemIterator it(this);*it;++it)if(auto *o=m->byId((*it)->data(0,Qt::UserRole).toString()))objects.append(*o);
    for(const auto &object:m->design.objects)if(object.type=="Audio")objects.append(object);
    if(objects.size()==m->design.objects.size())m->design.objects=objects;else m->dropUndo();
    syncing=false;signature.clear();m->notify();
}
