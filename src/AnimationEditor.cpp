#include "AnimationEditor.h"
#include "BuildModel.h"
#include "UiControls.h"
#include <QMenu>
#include <QToolButton>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QHeaderView>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>

class CurveView : public QWidget {
public:
    BuildModel *m;
    QString object, channel;
    bool graph, snap = true, normalized = false;
    QSet<int> selected;
    QTreeWidget *channelTree=nullptr;
    PropertyTrack summary;
    QHash<QString,QVector<PropertyTrack>> originalTracks;
    QVector<QPair<DObject*,PropertyTrack*>> targets(){
        QVector<QPair<DObject*,PropertyTrack*>> result;
        for(auto &o:m->design.objects){
            if(object=="@selected"&&!m->sel.contains(o.id)&&!(m->sel.isEmpty()&&o.id==m->activeId))continue;
            if(object!="@all"&&object!="@selected"&&o.id!=object)continue;
            for(auto &tr:o.tracks)if(channel=="*"||(channel.startsWith("group:")&&tr.property.section('.',0,0)==channel.mid(6))||channel==tr.property)result.append({&o,&tr});
        }return result;
    }
    bool aggregate() const{return channel=="*"||channel.startsWith("group:");}
    CurveView(BuildModel *model, bool asGraph) : m(model), graph(asGraph) {
        setObjectName("animationCurve");
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(250, 160);
        setMouseTracking(true);setToolTip("Drag keys to move. Drag the playhead head to scrub. Bézier handles: drag freely, Shift to align the opposite handle.");
        connect(m, &BuildModel::changed, this, [this] { update(); });
        connect(m, &BuildModel::timeChanged, this, [this] { update(); });
    }
    PropertyTrack *track() {
        if(aggregate()){summary.keys.clear();QSet<int> times;for(const auto &target:targets())for(const auto &key:target.second->keys)times.insert(key.ms);for(int time:times)summary.keys.append({time,0,"Linear"});std::sort(summary.keys.begin(),summary.keys.end(),[](const auto&a,const auto&b){return a.ms<b.ms;});return &summary;}
        auto *o = m->byId(object);
        return o ? o->track(channel) : nullptr;
    }
    void choose(const QString &id, const QString &path) {
        object = id; channel = path; selected.clear();
        if(!id.startsWith('@')&&m->activeId!=id)m->select(id); m->selChannel = path;
        frame(false);
    }
    void frame(bool onlySelected) {
        auto *tr = track();
        t0 = 0; t1 = qMax(100, m->design.durationMs); v0 = -1; v1 = 1;
        if (tr && !tr->keys.isEmpty()) {
            double lo = 1e100, hi = -1e100, first = 1e100, last = -1e100;
            for (const auto &k : tr->keys) {
                if (onlySelected && !selected.contains(k.ms)) continue;
                lo = qMin(lo, k.value); hi = qMax(hi, k.value);
                first = qMin(first, double(k.ms)); last = qMax(last, double(k.ms));
            }
            if (hi >= lo) {
                const double pad = qMax(0.1, (hi - lo) * .15);
                v0 = lo - pad; v1 = hi + pad;
                if (onlySelected) { const double tp = qMax(100.0, (last-first)*.15); t0=first-tp; t1=last+tp; }
            }
        }
        update();
    }
    void erase() {
        if(aggregate()){if(selected.isEmpty())return;m->pushUndo();QSet<QString> changed;for(const auto &target:targets())if(!target.first->locked){target.second->keys.removeIf([&](const auto &key){return selected.contains(key.ms);});changed.insert(target.first->id);}for(const auto &id:changed){auto *o=m->byId(id);o->tracks.removeIf([](const auto &tr){return tr.keys.isEmpty();});o->refreshLegacyKeys();}selected.clear();m->notify();return;}
        auto *tr = track();
        if (!tr || m->byId(object)->locked || selected.isEmpty()) return;
        m->pushUndo();
        tr->keys.removeIf([this](const auto &k) { return selected.contains(k.ms); });
        auto *o = m->byId(object);
        o->tracks.removeIf([](const auto &t) { return t.keys.isEmpty(); });
        o->refreshLegacyKeys(); selected.clear(); m->notify();
    }
    void interpolation(const QString &kind) {
        if(aggregate()){if(selected.isEmpty())return;m->pushUndo();for(const auto &target:targets())if(!target.first->locked){for(auto &key:target.second->keys)if(selected.contains(key.ms))key.interpolation=kind;target.first->refreshLegacyKeys();}m->notify();return;}
        auto *tr=track(); if (!tr || m->byId(object)->locked || selected.isEmpty()) return;
        m->pushUndo();
        for (auto &k : tr->keys) if (selected.contains(k.ms)) k.interpolation=kind;
        m->byId(object)->refreshLegacyKeys(); m->notify();
    }
    void scaleKeys() {
        auto *tr=track(); if (!tr || (!aggregate()&&m->byId(object)->locked) || selected.isEmpty()) return;
        bool ok=false;
        double factor=QInputDialog::getDouble(this,"Scale selected keys","Time scale around first selected key",1,.01,100,3,&ok);
        if (!ok) return;
        if(aggregate()){
            int pivot=*std::min_element(selected.begin(),selected.end());QHash<QString,QVector<PropertyTrack>> updates;QSet<int> next;
            for(auto target:targets()){if(target.first->locked)continue;if(!updates.contains(target.first->id))updates[target.first->id]=target.first->tracks;for(auto &track:updates[target.first->id])if(track.property==target.second->property){QSet<int> times;for(auto &key:track.keys){if(selected.contains(key.ms)){key.ms=boundedTime(pivot+(key.ms-pivot)*factor);key.leftHandle.rx()*=factor;key.rightHandle.rx()*=factor;next.insert(key.ms);}if(times.contains(key.ms))return;times.insert(key.ms);}std::sort(track.keys.begin(),track.keys.end(),[](auto&a,auto&b){return a.ms<b.ms;});}}
            m->pushUndo();for(auto it=updates.begin();it!=updates.end();++it){auto *o=m->byId(it.key());o->tracks=it.value();o->refreshLegacyKeys();}selected=next;m->notify();return;
        }
        const auto old=tr->keys;
        int pivot=*std::min_element(selected.begin(),selected.end());
        QVector<AnimationKey> result;
        QSet<int> times, next;
        for (auto k : old) {
            if (selected.contains(k.ms)) { k.ms=boundedTime(pivot+(k.ms-pivot)*factor); k.leftHandle.rx()*=factor; k.rightHandle.rx()*=factor; next.insert(k.ms); }
            if (times.contains(k.ms)) return;
            times.insert(k.ms); result.append(k);
        }
        m->pushUndo(); tr->keys=result;
        std::sort(tr->keys.begin(),tr->keys.end(),[](const auto&a,const auto&b){return a.ms<b.ms;});
        selected=next; m->byId(object)->refreshLegacyKeys(); m->notify();
    }
protected:
    QPointF graphPoint(QPointF value) const {auto r=area();return {r.left()+(value.x()-t0)/(t1-t0)*r.width(),r.bottom()-(value.y()-v0)/(v1-v0)*r.height()};}
    void playhead(QPainter &p){auto r=area();double x=r.left()+(m->t-t0)/(t1-t0)*r.width();p.setPen(QPen(QColor("#f69b53"),1));p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));p.setBrush(QColor("#f69b53"));p.drawPolygon(QPolygonF{{x-7,r.top()-16},{x+7,r.top()-16},{x+7,r.top()-6},{x,r.top()},{x-7,r.top()-6}});}
    QRectF area() const { return QRectF(rect()).adjusted(graph?62:8,24,-14,graph?-28:-8); }
    QPointF point(const AnimationKey &k) const {
        auto r=area();double y=r.center().y();
        if(!graph && channelTree)for(QTreeWidgetItemIterator it(channelTree);*it;++it)
            if((*it)->data(0,Qt::UserRole).toString()==object && (*it)->data(0,Qt::UserRole+1).toString()==channel)
                y=channelTree->visualItemRect(*it).center().y()+channelTree->header()->height();
        return {r.left()+(k.ms-t0)/(t1-t0)*r.width(), graph ? r.bottom()-(k.value-v0)/(v1-v0)*r.height() : y};
    }
    int boundedTime(double time) const {
        if(snap) time=qRound(time/m->frameMs())*m->frameMs();
        return qBound(0,qRound(time),m->design.durationMs);
    }
    bool editedKey(const QString &id,const QString &path,int time) const {
        if(qAbs(time-m->t)>0)return false;
        for(const auto &o:m->design.objects){
            if(id=="@selected"&&!m->sel.contains(o.id)&&!(m->sel.isEmpty()&&o.id==m->activeId))continue;
            if(id!="@all"&&id!="@selected"&&id!=o.id)continue;
            for(const auto &track:o.tracks){
                if(path!="*"&&path!=track.property&&!(path.startsWith("group:")&&track.property.section('.',0,0)==path.mid(6)))continue;
                if(!o.overrides.contains(track.property))continue;
                for(const auto &key:track.keys)if(key.ms==time&&qAbs(o.propertyValue(track.property,time)-key.value)>1e-6)return true;
            }
        }
        return false;
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(),QColor("#292929")); const auto r=area();
        p.fillRect(r,QColor("#303030"));
        auto f=font(); f.setPixelSize(10); p.setFont(f);
        for(int i=0;i<=5;++i) {
            double x=r.left()+r.width()*i/5, y=r.bottom()-r.height()*i/5;
            p.setPen(QColor("#3a3a3a")); p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));
            if(graph) p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));
            p.setPen(QColor("#94a3b8"));
            p.drawText(QRectF(x-26,graph?r.bottom()+4:2,70,20),QString::number(qRound((t0+(t1-t0)*i/5)/m->frameMs())));
            if(graph) p.drawText(QRectF(0,y-7,56,18),Qt::AlignRight,QString::number(normalized ? i/5.0 : v0+(v1-v0)*i/5,'g',4));
        }
        p.setPen(QColor("#cbd5e1")); if(graph)p.drawText(8,16,channel.isEmpty()?"Select an animated channel":channel);
        if(!graph && channelTree){
            p.save();p.setClipRect(r);
            for(QTreeWidgetItemIterator it(channelTree);*it;++it){
                const auto path=(*it)->data(0,Qt::UserRole+1).toString();
                const auto id=(*it)->data(0,Qt::UserRole).toString();
                QSet<int> times;
                for(const auto &o:m->design.objects){if(id=="@selected"&&!m->sel.contains(o.id)&&!(m->sel.isEmpty()&&o.id==m->activeId))continue;if(id!="@all"&&id!="@selected"&&id!=o.id)continue;
                    for(const auto &tr:o.tracks)if(path=="*"||(path.startsWith("group:")&&tr.property.section('.',0,0)==path.mid(6))||path==tr.property)for(const auto &key:tr.keys)times.insert(key.ms);}
                const auto row=channelTree->visualItemRect(*it);
                if(row.isEmpty())continue;
                const double y=row.center().y()+channelTree->header()->height();
                p.fillRect(QRectF(r.left(),y-10,r.width(),21),id==object&&path==channel?QColor(37,99,235,35):QColor(255,255,255,4));
                for(int time:times){
                    QPointF c(r.left()+(time-t0)/(t1-t0)*r.width(),y);
                    p.setPen(QColor("#292929"));p.setBrush(id==object&&path==channel&&selected.contains(time)?QColor("#fbbf24"):QColor("#60a5fa"));
                    p.drawPolygon(QPolygonF({c+QPointF(0,-5),c+QPointF(5,0),c+QPointF(0,5),c+QPointF(-5,0)}));
                    if(editedKey(id,path,time)){p.setPen(QPen(QColor("#ff9f43"),2));p.setBrush(Qt::NoBrush);p.drawEllipse(c,7,7);}
                }
            }
            const double x=r.left()+(m->t-t0)/(t1-t0)*r.width();p.setPen(QPen(QColor("#f87171"),1));p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));
            if(drag==Box){p.setPen(QColor("#60a5fa"));p.setBrush(QColor(96,165,250,30));p.drawRect(QRectF(press,last).normalized());}
            p.restore();playhead(p);return;
        }
        auto *tr=track();
        if(!tr || tr->keys.isEmpty()) { p.drawText(r,Qt::AlignCenter,"Use a property diamond to insert a key");playhead(p); return; }
        p.save(); p.setClipRect(r.adjusted(-5,-5,5,5));
        if(graph) {
            QPainterPath curve;
            for(int x=0;x<=int(r.width());++x) {
                const double t=t0+x/r.width()*(t1-t0);
                const double val=tr->evaluate(qRound64(t),0);
                QPointF q(r.left()+x,r.bottom()-(val-v0)/(v1-v0)*r.height());
                if(x==0) curve.moveTo(q); else curve.lineTo(q);
            }
            p.setPen(QPen(QColor("#60a5fa"),1.7));p.drawPath(curve);
        } else { p.setPen(QPen(QColor("#334e70"),3));p.drawLine(point(tr->keys.first()),point(tr->keys.last())); }
        for(const auto &k:tr->keys) {
            auto c=point(k); p.setPen(QPen(QColor("#292929"),1));p.setBrush(selected.contains(k.ms)?QColor("#fbbf24"):QColor("#60a5fa"));
            p.drawPolygon(QPolygonF({c+QPointF(0,-5),c+QPointF(5,0),c+QPointF(0,5),c+QPointF(-5,0)}));
            if(editedKey(object,channel,k.ms)){p.setPen(QPen(QColor("#ff9f43"),2));p.setBrush(Qt::NoBrush);p.drawEllipse(c,7,7);}
        }
        if(graph&&!aggregate())for(int i=0;i<tr->keys.size();++i){const auto &k=tr->keys[i];if(!selected.contains(k.ms))continue;
            for(bool right:{false,true}){if(right?(i+1>=tr->keys.size()||k.interpolation!="Bezier"):(i==0||tr->keys[i-1].interpolation!="Bezier"))continue;auto h=graphPoint(tr->handle(i,right));p.setPen(QPen(QColor("#d6ac68"),1));p.drawLine(point(k),h);p.setBrush(QColor("#192b40"));p.drawEllipse(h,4,4);}}
        double x=r.left()+(m->t-t0)/(t1-t0)*r.width();
        p.setPen(QPen(QColor("#f87171"),1));p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));
        if(drag==Box) {p.setPen(QColor("#60a5fa"));p.setBrush(QColor(96,165,250,30));p.drawRect(QRectF(press,last).normalized());}
        p.restore();playhead(p);
    }
    void mousePressEvent(QMouseEvent *e) override {
        setFocus(); press=last=e->position(); drag=None;
        if(e->button()==Qt::MiddleButton || e->button()==Qt::RightButton) {drag=Pan;return;}
        if(e->button()==Qt::LeftButton&&(press.y()<area().top()||press.y()>area().bottom())){drag=Scrub;scrub(press);return;}
        if(!graph && channelTree){
            auto *item=channelTree->itemAt(QPoint(10,qRound(press.y())-channelTree->header()->height()));
            if(item){const auto path=item->data(0,Qt::UserRole+1).toString(),id=item->data(0,Qt::UserRole).toString();
                if(!path.isEmpty()&&(id!=object||path!=channel)){object=id;channel=path;selected.clear();m->selChannel=path;channelTree->setCurrentItem(item);}
            }
        }
        auto *tr=track(); if(e->button()!=Qt::LeftButton || !tr)return;
        if(!object.startsWith('@')&&m->activeId!=object)m->select(object);
        m->selChannel=channel;
        if(graph&&!aggregate()&&!m->byId(object)->locked)for(int i=0;i<tr->keys.size();++i)if(selected.contains(tr->keys[i].ms))for(bool right:{false,true}){
            if(right?(i+1>=tr->keys.size()||tr->keys[i].interpolation!="Bezier"):(i==0||tr->keys[i-1].interpolation!="Bezier"))continue;
            if(QLineF(graphPoint(tr->handle(i,right)),press).length()<8){handleIndex=i;handleRight=right;drag=Handle;undoPushed=false;return;}}
        for(const auto &k:tr->keys) if(QLineF(point(k),press).length()<9) {
            if(e->modifiers()&Qt::ShiftModifier) {if(selected.contains(k.ms))selected.remove(k.ms);else selected.insert(k.ms);}
            else if(!selected.contains(k.ms)) selected={k.ms};
            originals=tr->keys; originalSelection=selected;drag=Keys; undoPushed=false;
            originalTracks.clear();for(auto target:targets())originalTracks[target.first->id]=target.first->tracks;
            m->selChannel=channel; m->selKey=tr->indexAt(k.ms);update();return;
        }
        if(press.y()>area().bottom() || press.y()<area().top()) {drag=Scrub;scrub(press);}
        else { if(!(e->modifiers()&Qt::ShiftModifier))selected.clear();drag=Box; }
        update();
    }
    void scrub(QPointF pos) {m->setTime(boundedTime(t0+(pos.x()-area().left())/area().width()*(t1-t0)));}
    void mouseMoveEvent(QMouseEvent *e) override {
        const auto pos=e->position(), delta=pos-last;last=pos;
        const auto r=area();
        if(drag==Pan) {
            double dt=delta.x()/r.width()*(t1-t0),dv=delta.y()/r.height()*(v1-v0);
            t0-=dt;t1-=dt;v0+=dv;v1+=dv;
        } else if(drag==Scrub) scrub(pos);
        else if(drag==Handle){auto *tr=track();if(!tr||handleIndex>=tr->keys.size())return;if(!undoPushed){m->pushUndo();undoPushed=true;}
            auto &k=tr->keys[handleIndex];if(!k.customHandles){k.leftHandle=tr->handle(handleIndex,false)-QPointF(k.ms,k.value);k.rightHandle=tr->handle(handleIndex,true)-QPointF(k.ms,k.value);k.customHandles=true;}
            double x=t0+(pos.x()-r.left())/r.width()*(t1-t0),y=v0+(r.bottom()-pos.y())/r.height()*(v1-v0);
            double lo=handleRight?k.ms:(handleIndex?tr->keys[handleIndex-1].ms:t0),hi=handleRight?(handleIndex+1<tr->keys.size()?tr->keys[handleIndex+1].ms:t1):k.ms;
            QPointF value(qBound(lo,x,hi)-k.ms,y-k.value);if(handleRight)k.rightHandle=value;else k.leftHandle=value;
            if(e->modifiers()&Qt::ShiftModifier){auto &opposite=handleRight?k.leftHandle:k.rightHandle;double length=QLineF(QPointF(),opposite).length(),now=QLineF(QPointF(),value).length();if(now>0)opposite=-value*(length/now);}
            m->byId(object)->refreshLegacyKeys();m->notify();
        }
        else if(drag==Keys) {
            auto *tr=track();if(!tr)return;
            if(!aggregate()&&m->byId(object)->locked)return;
            int dt=qRound((pos.x()-press.x())/r.width()*(t1-t0));
            if(snap)dt=qRound(double(dt)/m->frameMs())*m->frameMs();
            int earliest=m->design.durationMs,latest=0;
            for(const auto &k:originals)if(originalSelection.contains(k.ms)){earliest=qMin(earliest,k.ms);latest=qMax(latest,k.ms);}
            dt=qBound(-earliest,dt,m->design.durationMs-latest);
            const double dv=graph ? -(pos.y()-press.y())/r.height()*(v1-v0) : 0;
            if(aggregate()){
                auto updates=originalTracks;QSet<int> next;
                for(auto target:targets()){if(target.first->locked)continue;for(auto &tr:updates[target.first->id])if(tr.property==target.second->property){QSet<int> times;for(auto &k:tr.keys){if(originalSelection.contains(k.ms)){k.ms+=dt;next.insert(k.ms);}if(times.contains(k.ms))return;times.insert(k.ms);}std::sort(tr.keys.begin(),tr.keys.end(),[](const auto&a,const auto&b){return a.ms<b.ms;});}}
                if(!undoPushed){m->pushUndo();undoPushed=true;}for(auto it=updates.begin();it!=updates.end();++it){auto *o=m->byId(it.key());if(!o||o->locked)continue;o->tracks=it.value();o->refreshLegacyKeys();}selected=next;m->notify();update();return;
            }
            QVector<AnimationKey> next=originals;QSet<int> times,newSelection;
            for(auto &k:next) {
                if(originalSelection.contains(k.ms)){k.ms+=dt;k.value+=dv;newSelection.insert(k.ms);}
                if(times.contains(k.ms))return;
                times.insert(k.ms);
            }
            if(!undoPushed){m->pushUndo();undoPushed=true;}
            std::sort(next.begin(),next.end(),[](const auto&a,const auto&b){return a.ms<b.ms;});
            tr->keys=next;selected=newSelection;m->byId(object)->refreshLegacyKeys();m->notify();
        }
        setCursor(drag==Pan?Qt::ClosedHandCursor:drag==Keys||drag==Handle?Qt::SizeAllCursor:pos.y()<r.top()?Qt::SizeHorCursor:Qt::CrossCursor);update();
    }
    void mouseReleaseEvent(QMouseEvent*) override {
        if(drag==Box) {auto *tr=track();if(tr)for(const auto &k:tr->keys)if(QRectF(press,last).normalized().contains(point(k)))selected.insert(k.ms);}
        if(drag==Keys && !selected.isEmpty()) {
            m->selChannel=channel;
            if(auto *tr=track())m->selKey=tr->indexAt(*std::min_element(selected.begin(),selected.end()));
        }
        drag=None;update();
    }
    void wheelEvent(QWheelEvent *e) override {
        const double factor=qPow(1.15,-e->angleDelta().y()/120.0);
        const auto r=area();double tx=t0+(e->position().x()-r.left())/r.width()*(t1-t0);
        double vy=v0+(r.bottom()-e->position().y())/r.height()*(v1-v0);
        if(t1-t0>10 || factor>1){t0=tx+(t0-tx)*factor;t1=tx+(t1-tx)*factor;}
        if(graph && (e->modifiers()&Qt::ControlModifier)){v0=vy+(v0-vy)*factor;v1=vy+(v1-vy)*factor;}
        update();e->accept();
    }
    void keyPressEvent(QKeyEvent *e) override {
        if(e->key()==Qt::Key_Delete || e->key()==Qt::Key_Backspace)erase();
        else if(e->key()==Qt::Key_Home)frame(false);
        else if(e->key()==Qt::Key_F)frame(true);
        else if(e->key()==Qt::Key_A && (e->modifiers()&Qt::ControlModifier)){if(auto *tr=track())for(const auto &k:tr->keys)selected.insert(k.ms);update();}
        else if(e->key()==Qt::Key_Z && (e->modifiers()&Qt::ControlModifier)){(e->modifiers()&Qt::ShiftModifier)?m->redo():m->undo();selected.clear();}
        else QWidget::keyPressEvent(e);
    }
private:
    enum Drag{None,Keys,Box,Pan,Scrub,Handle}drag=None;
    int handleIndex=-1;bool handleRight=false;
    double t0=0,t1=5000,v0=-1,v1=1;
    QPointF press,last;
    QVector<AnimationKey> originals;
    QSet<int> originalSelection;
    bool undoPushed=false;
};

AnimationEditor::AnimationEditor(BuildModel *model,bool graph,QWidget *parent):QWidget(parent),m_model(model){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(3);
    auto *bar=new QHBoxLayout;
    m_curve=new CurveView(model,graph);
    auto button=[&](const QString &label,auto fn){auto *b=new QPushButton;b->setToolTip(label);b->setAccessibleName(label);b->setIcon(uiIcon(label=="Delete"?"delete":label=="View All"?"fit":label=="Frame Selected"?"composition":"scale"));b->setIconSize({14,14});b->setFixedSize(25,22);bar->addWidget(b);connect(b,&QPushButton::clicked,this,fn);};
    button("View All",[this]{m_curve->frame(false);});
    button("Frame Selected",[this]{m_curve->frame(true);});
    button("Scale Keys",[this]{m_curve->scaleKeys();});
    button("Delete",[this]{m_curve->erase();});
    auto *snap=new QCheckBox("Snap");snap->setChecked(true);bar->addWidget(snap);
    connect(snap,&QCheckBox::toggled,this,[this](bool on){m_curve->snap=on;});
    if(graph){auto *norm=new QCheckBox("Normalize");bar->addWidget(norm);connect(norm,&QCheckBox::toggled,this,[this](bool on){m_curve->normalized=on;m_curve->frame(false);});}
    auto *interp=new QToolButton;interp->setText("Interpolation");interp->setToolTip("Interpolation of selected keys");interp->setPopupMode(QToolButton::InstantPopup);bar->addWidget(interp);
    auto *menu=new QMenu(interp);menu->setToolTipsVisible(true);menu->setStyleSheet("QMenu{background:#353535;border:1px solid #555;border-radius:7px;padding:5px;} QMenu::item{padding:6px 18px;color:#ddd;} QMenu::item:selected{background:#4b6281;border-radius:3px;}");
    for(const auto &kind:interpolationKinds()){auto *action=menu->addAction(uiIcon(kind=="Constant"?"step":kind=="Linear"?"linear":"graph"),kind);action->setCheckable(true);action->setToolTip(kind=="Constant"?"Hold the value until the next key":kind=="Bezier"?"Edit the curve with tangent handles":"Ease between the selected key and the next key");connect(action,&QAction::triggered,this,[this,interp,kind]{m_curve->interpolation(kind);interp->setText(kind);});}
    connect(menu,&QMenu::aboutToShow,this,[this,menu]{QSet<QString> kinds;for(const auto &target:m_curve->targets())for(const auto &key:target.second->keys)if(m_curve->selected.contains(key.ms))kinds.insert(key.interpolation);for(auto *action:menu->actions()){action->setEnabled(!m_curve->selected.isEmpty());action->setChecked(kinds.size()==1&&kinds.contains(action->text()));}});interp->setMenu(menu);
    bar->addStretch();layout->addLayout(bar);
    auto *split=new QSplitter;
    m_channels=new QTreeWidget;m_channels->setHeaderLabel("Animated channels");m_channels->setMinimumWidth(160);m_channels->setMaximumWidth(280);
    m_channels->setStyleSheet("QTreeWidget{background:#282828;color:#ccc;border:0;font-size:10px;} QTreeWidget::item{height:19px;} QTreeWidget::item:selected{background:#3d5b7d;} QHeaderView::section{background:#333;color:#bbb;border:0;padding:3px;}");
    m_curve->channelTree=m_channels;
    connect(m_channels,&QTreeWidget::itemExpanded,m_curve,[this]{m_curve->update();});
    connect(m_channels,&QTreeWidget::itemCollapsed,m_curve,[this]{m_curve->update();});
    connect(m_channels->verticalScrollBar(),&QScrollBar::valueChanged,m_curve,[this]{m_curve->update();});
    split->addWidget(m_channels);split->addWidget(m_curve);split->setStretchFactor(1,1);layout->addWidget(split);
    connect(m_channels,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *item,int){
        const auto path=item->data(0,Qt::UserRole+1).toString();
        if(!path.isEmpty()&&(!m_curve->graph||(!path.startsWith("group:")&&path!="*")))m_curve->choose(item->data(0,Qt::UserRole).toString(),path);
    });
    connect(model,&BuildModel::changed,this,[this]{rebuild();}); connect(model,&BuildModel::selectionChanged,this,[this]{rebuild();});
    rebuild();
}
void AnimationEditor::rebuild(){
    QString signature;
    for(const auto &o:m_model->design.objects){signature+=o.id+o.name+QString::number(m_model->sel.contains(o.id));for(const auto &tr:o.tracks)signature+=tr.property;}
    signature+=m_model->activeId;
    if(signature==m_signature)return;m_signature=signature;
    QHash<QString,bool> expanded;
    for(QTreeWidgetItemIterator it(m_channels);*it;++it)expanded[(*it)->data(0,Qt::UserRole).toString()+"/"+(*it)->data(0,Qt::UserRole+1).toString()]=(*it)->isExpanded();
    m_channels->clear();
    auto add=[&](QTreeWidgetItem *parent,const QString &name,const QString &id,const QString &path,bool open){
        auto *item=parent?new QTreeWidgetItem(parent,{name}):new QTreeWidgetItem(m_channels,{name});item->setData(0,Qt::UserRole,id);item->setData(0,Qt::UserRole+1,path);
        item->setExpanded(expanded.value(id+"/"+path,open));item->setToolTip(0,name+" · click to select keys; expand for individual channels");return item;
    };
    QTreeWidgetItem *selectedRoot=nullptr;
    if(!m_curve->graph){auto *summary=add(nullptr,"Summary","@all","*",true);selectedRoot=add(summary,"Selected objects","@selected","*",true);}
    for(const auto &o:m_model->design.objects){
        if(o.tracks.isEmpty())continue;
        if(!m_curve->graph&&!m_model->sel.contains(o.id)&&!(m_model->sel.isEmpty()&&o.id==m_model->activeId))continue;
        auto *object=add(selectedRoot,o.name,o.id,"*",m_curve->graph);
        QHash<QString,QTreeWidgetItem*> groups;
        for(const auto &tr:o.tracks){
            const QString group=tr.property.section('.',0,0);
            if(!groups.contains(group))groups[group]=add(object,group,o.id,"group:"+group,false);
            auto *item=add(groups[group],tr.property.section('.',1).isEmpty()?tr.property:tr.property.section('.',1),o.id,tr.property,false);
            if(o.id==m_curve->object&&tr.property==m_curve->channel)m_channels->setCurrentItem(item);
            if(m_curve->graph&&m_curve->channel.isEmpty()&&o.id==m_model->activeId){m_curve->object=o.id;m_curve->channel=tr.property;m_curve->frame(false);}
        }
    }
    if(!m_curve->graph&&m_curve->channel.isEmpty()){m_curve->object="@selected";m_curve->channel="*";}
    m_curve->update();
}
