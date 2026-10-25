#include "LayerPanel.h"
#include "BuildModel.h"
#include "UiControls.h"
#include <QDropEvent>
#include <QHeaderView>
#include <QTreeWidgetItemIterator>

LayerPanel::LayerPanel(BuildModel *model,QWidget *parent):QTreeWidget(parent),m(model){
    setColumnCount(3);setHeaderLabels({"Layers","", ""});setRootIsDecorated(true);setIndentation(13);
    setStyleSheet("QTreeWidget{background:palette(window);color:#b8c5d6;border:0;font-size:10px;} QTreeWidget::item{height:23px;} QTreeWidget::item:selected{background:#28466a;} QHeaderView::section{background:#343434;color:#8392a6;border:0;padding:4px;font-size:10px;}");
    setSelectionMode(QAbstractItemView::ExtendedSelection);setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);setEditTriggers(QAbstractItemView::EditKeyPressed);
    setMinimumWidth(175);setMaximumWidth(260);header()->setSectionResizeMode(0,QHeaderView::Stretch);setColumnWidth(1,26);setColumnWidth(2,26);
    headerItem()->setIcon(1,uiIcon("eye"));headerItem()->setIcon(2,uiIcon("lock"));
    connect(m,&BuildModel::changed,this,[this]{refresh();});connect(m,&BuildModel::selectionChanged,this,[this]{refresh();});
    connect(this,&QTreeWidget::itemSelectionChanged,this,[this]{if(syncing)return;QSet<QString> ids;for(auto *item:selectedItems())ids.insert(item->data(0,Qt::UserRole).toString());auto *cur=currentItem();m->setSelection(ids,cur?cur->data(0,Qt::UserRole).toString():QString());});
    connect(this,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *item,int column){
        if(syncing)return;const QString id=item->data(0,Qt::UserRole).toString();auto *o=m->byId(id);if(!o)return;
        if(column){m->pushUndo();if(column==1)o->visible=!o->visible;else o->locked=!o->locked;m->notify();}
        else if(m->sel.contains(id))m->setSelection(m->sel,id);
    });
    connect(this,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item,int column){if(column==0)editItem(item,0);});
    connect(this,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem *item,int col){if(!syncing&&col==0)m->rename(item->data(0,Qt::UserRole).toString(),item->text(0));});refresh();
}
void LayerPanel::refresh(){
    QString sig;for(const auto &o:m->design.objects)sig+=o.id+o.name+o.parentId+QString::number(o.visible)+QString::number(o.locked)+o.style["component.id"].toString()+o.style["component.role"].toString();
    syncing=true;
    if(sig!=signature){
        QSet<QString> closed;for(QTreeWidgetItemIterator it(this);*it;++it)if((*it)->childCount()&&!(*it)->isExpanded())closed.insert((*it)->data(0,Qt::UserRole).toString());
        signature=sig;clear();QHash<QString,QTreeWidgetItem*> items;
        for(const auto &o:m->design.objects){auto *item=new QTreeWidgetItem(QStringList{o.name});item->setData(0,Qt::UserRole,o.id);items[o.id]=item;
            item->setFlags(item->flags()|Qt::ItemIsEditable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled);
            const bool component=!o.style["component.id"].toString().isEmpty();item->setIcon(0,uiIcon(component?"component":o.type));item->setIcon(1,uiIcon(o.visible?"eye":"hidden",o.visible));item->setIcon(2,uiIcon(o.locked?"lock":"unlock",o.locked));
            item->setToolTip(0,o.type+" · F2 to rename · Ctrl+P to parent to the active layer");item->setToolTip(1,o.visible?"Hide layer":"Show layer");item->setToolTip(2,o.locked?"Unlock layer":"Lock layer");
            if(component)item->setToolTip(0,o.style["component.role"].toString()+" · "+o.type+" · / opens Components");
        }
        for(const auto &o:m->design.objects){auto *item=items[o.id];QSet<QString> seen{o.id};auto *parent=m->byId(o.parentId);bool cycle=false;
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
    if(objects.size()==m->design.objects.size())m->design.objects=objects;else m->dropUndo();
    syncing=false;signature.clear();m->notify();
}
