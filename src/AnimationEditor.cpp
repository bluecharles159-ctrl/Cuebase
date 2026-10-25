#include "AnimationEditor.h"
#include "BuildModel.h"
#include "UiControls.h"
#include "CurveNoise.h"
#include <QMenu>
#include <QToolButton>
#include <QCheckBox>
#include <QHBoxLayout>
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
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <climits>
#include <QClipboard>
#include <QtMath>
#include <algorithm>

namespace {
QString channelId(const QString &id,const QString &path){return id+"|"+path;}
QString keyId(const QString &id,const QString &path,int time){return channelId(id,path)+"|"+QString::number(time);}
double niceStep(double span,double pixels){double raw=qMax(1e-9,span*64/qMax(1.,pixels)),base=std::pow(10.,std::floor(std::log10(raw)));for(double f:{1.,2.,2.5,5.,10.})if(base*f>=raw)return base*f;return base*10;}
QString tickText(double value){if(qAbs(value)<1e-9)value=0;return QString::number(value,'g',5);}
QColor channelColor(const QString &path){return path.endsWith(".x")?QColor("#df7777"):path.endsWith(".y")?QColor("#79bf85"):path.endsWith(".z")?QColor("#78a8e0"):QColor("#d9b674");}
struct KeyRef {QString object,path;int time;};
struct TrackRef {DObject *object;PropertyTrack *track;};
}

class CurveView:public QWidget {
public:
    BuildModel *m;bool graph,snap=true,normalized=false,selectedOnly=false;
    QTreeWidget *tree=nullptr;
    QSet<QString> channels,selected;
    QString primary,defaultInterpolation="Bezier";
    std::function<void()> selectionChanged;
    CurveView(BuildModel *model,bool asGraph):m(model),graph(asGraph){
        setObjectName("animationCurve");setMinimumSize(250,160);setFocusPolicy(Qt::StrongFocus);setMouseTracking(true);
        setToolTip("Middle-drag: pan · Ctrl-drag: axis zoom · wheel: zoom · Alt-wheel: step frame\nG: move, X/Y constrain · S: scale keys/handles · I: insert · Ctrl+C/V: copy/paste · Shift+D: duplicate · right-click: cancel");
        QSettings settings;normalized=graph&&settings.value("animation/normalized",false).toBool();const auto values=settings.value(settingsKey()).toList();
        if(values.size()==4){t0=values[0].toDouble();t1=values[1].toDouble();v0=values[2].toDouble();v1=values[3].toDouble();initialized=t1>t0&&v1>v0;}
        publishView();
        connect(m,&BuildModel::changed,this,[this]{publishView();update();if(selectionChanged)selectionChanged();});
        connect(m,&BuildModel::timeChanged,this,qOverload<>(&QWidget::update));
    }
    ~CurveView(){QSettings().setValue(settingsKey(),QVariantList{t0,t1,v0,v1});}
    QString settingsKey()const{return graph?"animation/graphView":"animation/timelineView";}
    void saveView(){QSettings().setValue(settingsKey(),QVariantList{t0,t1,v0,v1});publishView();}
    void publishView(){setProperty("viewRange",QVariantList{t0,t1,v0,v1});QStringList visible;for(auto ref:tracks())visible<<channelId(ref.object->id,ref.track->property);setProperty("visibleChannels",visible);setProperty("activeChannel",primary);}
    bool visibleObject(const DObject &o)const{return !selectedOnly||m->sel.contains(o.id)||(m->sel.isEmpty()&&m->activeId==o.id);}
    QVector<TrackRef> allTracks()const{
        QVector<TrackRef> result;for(auto &o:m->design.objects)if(visibleObject(o))for(auto &track:o.tracks)
            result.append({&o,&track});return result;
    }
    QVector<TrackRef> tracks(bool chosen=false)const{
        auto result=allTracks();if(graph||chosen)result.removeIf([&](const auto &ref){return !channels.contains(channelId(ref.object->id,ref.track->property));});return result;
    }
    void activateChannel(const QString &id,const QString &path){
        primary=channelId(id,path);m->selChannel=path;
        if(tree){QSignalBlocker block(tree);for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()==id&&(*it)->data(0,Qt::UserRole+1).toString()==path){tree->setCurrentItem(*it,0,QItemSelectionModel::NoUpdate);break;}}
        publishView();update();
    }
    QVector<KeyRef> keys(bool onlySelected=true)const{
        QVector<KeyRef> refs;for(auto ref:tracks())for(auto &key:ref.track->keys)
            if(!onlySelected||selected.contains(keyId(ref.object->id,ref.track->property,key.ms)))refs.append({ref.object->id,ref.track->property,key.ms});return refs;
    }
    PropertyTrack *track(const KeyRef &ref)const{auto *o=m->byId(ref.object);return o?o->track(ref.path):nullptr;}
    bool matches(const QString &id,const QString &path,const TrackRef &ref)const{
        if(id=="@selected"&&!m->sel.contains(ref.object->id)&&!(m->sel.isEmpty()&&m->activeId==ref.object->id))return false;
        if(!id.startsWith('@')&&id!=ref.object->id)return false;
        return path=="*"||path==ref.track->property||(path.startsWith("group:")&&ref.track->property.section('.',0,0)==path.mid(6));
    }
    void ensureFrame(){if(!initialized)frame(false);}
    void chooseRows(){
        const auto previous=channels;
        channels.clear();for(auto *item:tree->selectedItems()){auto id=item->data(0,Qt::UserRole).toString(),path=item->data(0,Qt::UserRole+1).toString();
            if(!path.isEmpty()&&path!="*"&&!path.startsWith("group:"))channels.insert(channelId(id,path));
            for(auto ref:allTracks())if(matches(id,path,ref))channels.insert(channelId(ref.object->id,ref.track->property));}
        if(auto *current=tree->currentItem()){primary=channelId(current->data(0,Qt::UserRole).toString(),current->data(0,Qt::UserRole+1).toString());m->selChannel=current->data(0,Qt::UserRole+1).toString();}
        if(!channels.contains(primary))primary=channels.isEmpty()?QString():*channels.begin();
        if(previous!=channels)selected.clear();if(!initialized)frame(false);changedSelection();
    }
    void frame(bool selectedFrame){
        if(normalized){normalizationRanges.clear();for(auto ref:tracks())normalizationRanges[channelId(ref.object->id,ref.track->property)]=valueRange(*ref.track);}
        double lo=1e100,hi=-1e100,first=1e100,last=-1e100;
        for(auto ref:tracks(graph&&!channels.isEmpty()))for(const auto &k:ref.track->keys){if(selectedFrame&&!selected.contains(keyId(ref.object->id,ref.track->property,k.ms)))continue;lo=qMin(lo,displayValue(*ref.track,k.value));hi=qMax(hi,displayValue(*ref.track,k.value));first=qMin(first,double(k.ms));last=qMax(last,double(k.ms));}
        if(!selectedFrame){t0=0;t1=qMax(100,m->design.durationMs);}else if(first<=last){double pad=qMax(100.,(last-first)*.15);t0=qMax(0.,first-pad);t1=last+pad;}
        if(lo<=hi){double pad=qMax(.1,(hi-lo)*.15);v0=lo-pad;v1=hi+pad;}else {v0=-1;v1=1;}
        initialized=true;saveView();update();
    }
    QPair<double,double> valueRange(const PropertyTrack &tr)const{double lo=1e100,hi=-1e100;for(auto &k:tr.keys){lo=qMin(lo,k.value);hi=qMax(hi,k.value);}return lo<=hi?qMakePair(lo,qMax(1e-6,hi-lo)):qMakePair(0.,1.);}
    double displayValue(const PropertyTrack &tr,double value)const{if(!normalized)return value;auto range=normalizationRange(tr);return (value-range.first)/range.second;}
    double rawValue(const PropertyTrack &tr,double value)const{if(!normalized)return value;auto range=normalizationRange(tr);return value*range.second+range.first;}
    QPair<double,double> normalizationRange(const PropertyTrack &tr)const{for(auto ref:tracks())if(ref.track==&tr){auto id=channelId(ref.object->id,tr.property);if(!normalizationRanges.contains(id))normalizationRanges[id]=valueRange(tr);return normalizationRanges[id];}return valueRange(tr);}
    QRectF area()const{return QRectF(rect()).adjusted(graph?62:8,28,-14,graph?-26:-8);}
    QPointF point(const PropertyTrack &tr,double time,double value)const{auto r=area();return {r.left()+(time-t0)/(t1-t0)*r.width(),r.bottom()-(displayValue(tr,value)-v0)/(v1-v0)*r.height()};}
    int timeAt(double x,bool doSnap=true)const{auto r=area();double time=t0+(x-r.left())/r.width()*(t1-t0);if(snap&&doSnap)time=qRound(time/m->frameMs())*m->frameMs();return doSnap?qMax(0,qRound(time)):qRound(time);}
    double valueAt(double y)const{auto r=area();return v0+(r.bottom()-y)/r.height()*(v1-v0);}
    void selectAll(){selected.clear();for(auto ref:tracks(graph&&!channels.isEmpty()))for(auto &k:ref.track->keys)selected.insert(keyId(ref.object->id,ref.track->property,k.ms));changedSelection();}
    void cueFrames(){const auto refs=keys();if(refs.isEmpty())return;qint64 first=refs.first().time,last=first;for(const auto &ref:refs){first=qMin(first,qint64(ref.time));last=qMax(last,qint64(ref.time));}emit m->cueFramesRequested(first,qMax(first+m->frameMs(),last));}
    void noise(){QVector<QPair<QString,QString>> refs;for(const auto &key:keys()){const auto ref=qMakePair(key.object,key.path);if(!refs.contains(ref))refs.append(ref);}if(refs.isEmpty())for(auto ref:tracks(true))refs.append(qMakePair(ref.object->id,ref.track->property));editCurveNoise(m,refs,this);changedSelection();}
    void setNormalized(bool on){double first=t0,last=t1;normalized=on;QSettings().setValue("animation/normalized",on);frame(false);t0=first;t1=last;saveView();update();}
    void changedSelection(){publishView();update();if(selectionChanged)selectionChanged();}
    QString interpolationLabel()const{QSet<QString> values;auto refs=keys();if(refs.isEmpty()){for(auto ref:tracks(true))for(const auto &key:ref.track->keys)values.insert(key.interpolation);}else for(auto ref:refs){auto *tr=track(ref);int i=tr?tr->indexAt(ref.time):-1;if(i>=0)values.insert(tr->keys[i].interpolation);}return values.isEmpty()?defaultInterpolation:values.size()==1?*values.begin():"Mixed";}
    void interpolation(const QString &kind){defaultInterpolation=kind;auto refs=keys();if(refs.isEmpty()){for(auto ref:tracks(true))for(auto &k:ref.track->keys)refs.append({ref.object->id,ref.track->property,k.ms});}
        if(refs.isEmpty()){changedSelection();return;}m->pushUndo();for(auto ref:refs)if(auto *o=m->byId(ref.object);o&&!o->locked)if(auto *tr=o->track(ref.path)){auto &key=tr->keys[tr->indexAt(ref.time)];key.interpolation=kind;}finishEdit();}
    void handles(const QString &type){auto refs=keys();if(refs.isEmpty())return;m->pushUndo();for(auto ref:refs)if(auto *o=m->byId(ref.object);o&&!o->locked)if(auto *tr=o->track(ref.path)){int i=tr->indexAt(ref.time);auto &k=tr->keys[i];auto l=tr->handle(i,false)-QPointF(k.ms,k.value),r=tr->handle(i,true)-QPointF(k.ms,k.value);k.handleType=type;k.customHandles=type=="Free"||type=="Aligned";k.leftHandle=l;k.rightHandle=r;k.interpolation="Bezier";}finishEdit();}
    void erase(){auto refs=keys();if(refs.isEmpty())return;m->pushUndo();for(auto ref:refs)if(auto *o=m->byId(ref.object);o&&!o->locked)if(auto *tr=o->track(ref.path))tr->keys.removeIf([&](const auto &key){return key.ms==ref.time;});selected.clear();finishEdit();}
    void finishEdit(){for(auto &o:m->design.objects){o.tracks.removeIf([](const auto &tr){return tr.keys.isEmpty();});o.refreshLegacyKeys();}m->notify();changedSelection();}
    void insert(){
        if(channels.isEmpty())return;m->pushUndo();selected.clear();
        for(auto id:channels){auto parts=id.split('|');if(parts.size()!=2)continue;auto *o=m->byId(parts[0]);if(!o||o->locked)continue;o->keyChannel(parts[1],int(m->t),o->propertyValue(parts[1],m->t));auto *tr=o->track(parts[1]);tr->keys[tr->indexAt(m->t)].interpolation=defaultInterpolation;selected.insert(keyId(o->id,parts[1],m->t));}finishEdit();
    }
    void copy(){QJsonArray array;for(auto ref:keys())if(auto *tr=track(ref)){auto &k=tr->keys[tr->indexAt(ref.time)];array.append(QJsonObject{{"object",ref.object},{"path",ref.path},{"time",k.ms},{"value",k.value},{"interpolation",k.interpolation},{"type",k.handleType},{"custom",k.customHandles},{"lx",k.leftHandle.x()},{"ly",k.leftHandle.y()},{"rx",k.rightHandle.x()},{"ry",k.rightHandle.y()}});}QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(QJsonObject{{"cuebaseKeys",array}}).toJson(QJsonDocument::Compact)));}
    void paste(bool duplicate=false){
        auto array=QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8()).object()["cuebaseKeys"].toArray();if(array.isEmpty())return;
        int first=INT_MAX;QSet<QString> sourceChannels;for(auto value:array){auto k=value.toObject();first=qMin(first,k["time"].toInt());sourceChannels.insert(k["path"].toString());}
        m->pushUndo();selected.clear();for(auto value:array){auto k=value.toObject();QStringList destinations;
            if(!duplicate&&sourceChannels.size()==1&&!channels.isEmpty())destinations=channels.values();else destinations={channelId(k["object"].toString(),k["path"].toString())};
            for(auto destination:destinations){auto parts=destination.split('|');auto *o=parts.size()==2?m->byId(parts[0]):nullptr;if(!o||o->locked)continue;
                int time=duplicate?k["time"].toInt()+m->frameMs():k["time"].toInt()-first+int(m->t);o->keyChannel(parts[1],time,k["value"].toDouble());auto *tr=o->track(parts[1]);auto &key=tr->keys[tr->indexAt(time)];key.interpolation=k["interpolation"].toString();key.handleType=k["type"].toString("Auto");key.customHandles=k["custom"].toBool();key.leftHandle={k["lx"].toDouble(),k["ly"].toDouble()};key.rightHandle={k["rx"].toDouble(),k["ry"].toDouble()};selected.insert(keyId(o->id,parts[1],time));m->design.durationMs=qMax(m->design.durationMs,time);}}
        finishEdit();
    }
    void duplicate(){copy();paste(true);beginTransform(ScaleMove,false);}
    void beginTransform(int operation,bool mouse){
        if(keys().isEmpty())return;drag=operation;mouseTransform=mouse;press=last=mapFromGlobal(QCursor::pos());axis=0;pushed=false;snapshot.clear();originalSelected=selected;baseDuration=m->design.durationMs;
        pivot={};int count=0;for(auto ref:tracks())snapshot[channelId(ref.object->id,ref.track->property)]=ref.track->keys;
        for(auto ref:keys())if(auto *tr=track(ref)){auto &k=tr->keys[tr->indexAt(ref.time)];pivot+=QPointF(k.ms,displayValue(*tr,k.value));++count;}if(count)pivot/=count;
    }
    void scaleKeys(){beginTransform(ScaleKeys,false);}
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),palette().color(QPalette::Window));auto r=area();p.fillRect(r,palette().color(QPalette::AlternateBase));QFont f=font();f.setPixelSize(10);p.setFont(f);
        double frames0=t0/m->frameMs(),frames1=t1/m->frameMs(),step=qMax(1.,niceStep(frames1-frames0,r.width()));step=qCeil(step);
        for(double frame=std::ceil(frames0/step)*step;frame<=frames1;frame+=step){double x=r.left()+(frame*m->frameMs()-t0)/(t1-t0)*r.width();p.setPen(QColor("#414247"));p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));p.setPen(QColor("#a7a9b0"));p.drawText(QRectF(x+3,1,62,17),tickText(frame));}
        if(m->cueRangeStart>=0){const double a=r.left()+(m->cueRangeStart-t0)/(t1-t0)*r.width(),b=r.left()+(m->cueRangeEnd-t0)/(t1-t0)*r.width();p.save();p.setClipRect(r);p.fillRect(QRectF(a,r.top(),qMax(1.,b-a),r.height()),QColor(82,207,151,48));p.setPen(QPen(QColor("#7ee5b1"),2));p.drawLine(QPointF(a,r.top()),QPointF(a,r.bottom()));p.drawLine(QPointF(b,r.top()),QPointF(b,r.bottom()));p.restore();}
        if(graph){double stepY=niceStep(v1-v0,r.height());for(double value=std::ceil(v0/stepY)*stepY;value<=v1;value+=stepY){double y=r.bottom()-(value-v0)/(v1-v0)*r.height();p.setPen(QColor(qAbs(value)<1e-8?"#73767e":"#414247"));p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));p.setPen(QColor("#b8bac2"));p.drawText(QRectF(0,y-7,55,16),Qt::AlignRight,tickText(value));}}
        p.save();p.setClipRect(r.adjusted(-5,0,5,0));
        if(graph)for(auto ref:tracks()){
            const auto &tr=*ref.track;bool chosen=primary==channelId(ref.object->id,tr.property);QColor color=channelColor(tr.property);color.setAlpha(chosen?255:95);QPainterPath curve;
            for(int x=0;x<=r.width();++x){auto pos=point(tr,t0+x/r.width()*(t1-t0),tr.evaluate(qRound64(t0+x/r.width()*(t1-t0)),0));if(!x)curve.moveTo(pos);else curve.lineTo(pos);}p.setPen(QPen(color,chosen?1.6:1));p.setBrush(Qt::NoBrush);p.drawPath(curve);
            for(int i=0;i<tr.keys.size();++i){const auto &k=tr.keys[i];auto c=point(tr,k.ms,k.value);bool sel=selected.contains(keyId(ref.object->id,tr.property,k.ms));
                if(sel&&(k.interpolation=="Bezier"||(i&&tr.keys[i-1].interpolation=="Bezier")))for(bool right:{false,true}){auto handle=tr.handle(i,right),h=point(tr,handle.x(),handle.y());p.setPen(QPen(QColor("#cdb68a"),1));p.drawLine(c,h);p.setBrush(QColor("#303135"));p.drawEllipse(h,3.5,3.5);}
                p.setPen(QColor("#24252a"));p.setBrush(sel?QColor("#f1c66b"):color);p.drawEllipse(c,3.8,3.8);if(edited(*ref.object,tr,k)){p.setPen(QPen(QColor("#ff9d53"),1.5));p.setBrush(Qt::NoBrush);p.drawEllipse(c,6,6);}}
        }else if(tree){
            for(QTreeWidgetItemIterator it(tree);*it;++it){auto row=tree->visualItemRect(*it);if(row.isEmpty())continue;double y=row.center().y()+tree->header()->height();if(y<r.top()-8||y>r.bottom()+8)continue;QString id=(*it)->data(0,Qt::UserRole).toString(),path=(*it)->data(0,Qt::UserRole+1).toString();QMap<int,QVector<KeyRef>> times;
                bool active=id==m->activeId||m->sel.contains(id);p.fillRect(QRectF(r.left(),y-9,r.width(),18),active?QColor(90,135,195,25):QColor(255,255,255,3));
                for(auto ref:tracks())if(matches(id,path,ref)){auto &tr=*ref.track;for(int k=0;k<tr.keys.size();++k){auto key=tr.keys[k];times[key.ms].append({ref.object->id,tr.property,key.ms});if(k+1<tr.keys.size()&&qAbs(key.value-tr.keys[k+1].value)<1e-8){double a=r.left()+(key.ms-t0)/(t1-t0)*r.width(),b=r.left()+(tr.keys[k+1].ms-t0)/(t1-t0)*r.width();p.fillRect(QRectF(a,y-3,b-a,6),QColor(214,176,88,42));}}}
                for(auto time=times.begin();time!=times.end();++time){bool sel=false,dirty=false;for(auto key:time.value()){sel|=selected.contains(keyId(key.object,key.path,key.time));if(auto *o=m->byId(key.object))if(auto *tr=o->track(key.path))dirty|=edited(*o,*tr,tr->keys[tr->indexAt(key.time)]);}
                    QPointF c(r.left()+(time.key()-t0)/(t1-t0)*r.width(),y);p.setPen(QColor("#292a2e"));p.setBrush(sel?palette().color(QPalette::Highlight):active?palette().color(QPalette::Highlight).lighter(130):QColor("#8b8e96"));p.drawPolygon(QPolygonF{{c.x(),c.y()-4},{c.x()+4,c.y()},{c.x(),c.y()+4},{c.x()-4,c.y()}});if(dirty){p.setPen(QPen(QColor("#ff9d53"),1.5));p.setBrush(Qt::NoBrush);p.drawEllipse(c,6,6);}}
            }
        }
        if(drag==Box){p.setPen(QColor("#74d6a3"));p.setBrush(QColor(75,190,135,38));p.drawRect(QRectF(press,last).normalized());}p.restore();
        double x=r.left()+(m->t-t0)/(t1-t0)*r.width();if(x>=r.left()&&x<=r.right()){p.setPen(QPen(QColor("#77a6d9"),1));p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));QString label=QString::number(qRound(double(m->t)/m->frameMs()));double w=qMax(24,p.fontMetrics().horizontalAdvance(label)+10);p.setPen(Qt::NoPen);p.setBrush(QColor("#426b9f"));p.drawRoundedRect(QRectF(qBound(0.,x-w/2,width()-w),1,w,18),3,3);p.setPen(Qt::white);p.drawText(QRectF(qBound(0.,x-w/2,width()-w),1,w,18),Qt::AlignCenter,label);p.setPen(Qt::NoPen);p.setBrush(QColor("#77a6d9"));p.drawPolygon(QPolygonF{{x-5,20},{x+5,20},{x,27}});}
        publishView();
    }
    bool edited(const DObject &o,const PropertyTrack &tr,const AnimationKey &key)const{return key.ms==m->t&&o.overrides.contains(tr.property)&&qAbs(o.propertyValue(tr.property,m->t)-key.value)>1e-6;}
    QVector<KeyRef> hit(QPointF pos)const{
        QVector<KeyRef> result;if(graph){double best=9;for(auto ref:tracks())for(auto &k:ref.track->keys){double distance=QLineF(pos,point(*ref.track,k.ms,k.value)).length();if(distance<best){best=distance;result={{ref.object->id,ref.track->property,k.ms}};}}}
        else if(tree){auto *item=tree->itemAt(QPoint(10,qRound(pos.y())-tree->header()->height()));if(!item)return result;auto id=item->data(0,Qt::UserRole).toString(),path=item->data(0,Qt::UserRole+1).toString();for(auto ref:tracks())if(matches(id,path,ref))for(auto &k:ref.track->keys)if(qAbs(area().left()+(k.ms-t0)/(t1-t0)*area().width()-pos.x())<8)result.append({ref.object->id,ref.track->property,k.ms});}return result;
    }
    void mousePressEvent(QMouseEvent *e)override{
        setFocus();if(e->button()==Qt::RightButton&&(drag==ScaleMove||drag==ScaleKeys||drag==Handle)){cancel();return;}
        if((drag==ScaleMove||drag==ScaleKeys)&&!mouseTransform&&e->button()==Qt::LeftButton){drag=None;pushed=false;return;}
        press=last=e->position();
        if(e->modifiers()&Qt::ControlModifier&&(e->button()==Qt::MiddleButton||e->button()==Qt::LeftButton)){drag=Zoom;zoomView={t0,t1,v0,v1};return;}
        if(e->button()==Qt::MiddleButton||e->button()==Qt::RightButton){drag=Pan;return;}
        if(e->button()!=Qt::LeftButton)return;
        if(press.y()<area().top()){drag=Scrub;m->setTime(timeAt(press.x()));return;}
        if(graph)for(auto ref:keys())if(auto *tr=track(ref)){int index=tr->indexAt(ref.time);if(index<0)continue;if(tr->keys[index].interpolation!="Bezier"&&!(index&&tr->keys[index-1].interpolation=="Bezier"))continue;
            if(m->byId(ref.object)->locked)continue;for(bool right:{false,true}){auto h=tr->handle(index,right);if(QLineF(point(*tr,h.x(),h.y()),press).length()<7){handleKey=ref;handleRight=right;drag=Handle;pushed=false;originalSelected=selected;return;}}}
        auto refs=hit(press);if(!refs.isEmpty()){if(graph)activateChannel(refs.first().object,refs.first().path);bool all=true;for(auto ref:refs)all&=selected.contains(keyId(ref.object,ref.path,ref.time));if(!(e->modifiers()&Qt::ShiftModifier)&&!all)selected.clear();
            for(auto ref:refs){auto key=keyId(ref.object,ref.path,ref.time);if((e->modifiers()&Qt::ShiftModifier)&&all)selected.remove(key);else selected.insert(key);}
            changedSelection();beginTransform(ScaleMove,true);press=last=e->position();return;}
        if(graph){double nearest=7;QString object,path;for(auto ref:tracks())for(int dx=-5;dx<=5;++dx){const double time=t0+(press.x()+dx-area().left())/area().width()*(t1-t0);const auto at=point(*ref.track,time,ref.track->evaluate(qRound64(time),0));const double distance=QLineF(at,press).length();if(distance<nearest){nearest=distance;object=ref.object->id;path=ref.track->property;}}if(!object.isEmpty()){activateChannel(object,path);return;}}
        if(!(e->modifiers()&Qt::ShiftModifier))selected.clear();originalSelected=selected;drag=Box;changedSelection();
    }
    void transform(QPointF pos){
        double dx=(pos.x()-press.x())/area().width()*(t1-t0),dy=(press.y()-pos.y())/area().height()*(v1-v0);
        if(axis=='X')dy=0;if(axis=='Y')dx=0;if(!graph)dy=0;
        if(!pushed&&QLineF(pos,press).length()<3)return;
        double fx=drag==ScaleKeys?std::exp(qBound(-6.,(pos.x()-press.x())*.01,6.)):1,fy=drag==ScaleKeys?std::exp(qBound(-6.,(press.y()-pos.y())*.01,6.)):1;
        if(axis=='X')fy=1;if(axis=='Y')fx=1;if(!graph)fy=1;
        QHash<QString,QVector<AnimationKey>> updated;QSet<QString> next;
        for(auto ref:tracks()){auto cid=channelId(ref.object->id,ref.track->property);if(ref.object->locked||!snapshot.contains(cid))continue;auto values=snapshot[cid];PropertyTrack original{ref.track->property,values};QSet<int> occupied;
            for(auto &key:values){if(originalSelected.contains(keyId(ref.object->id,ref.track->property,key.ms))){
                double time=drag==ScaleKeys?pivot.x()+(key.ms-pivot.x())*fx:key.ms+dx;time=qBound(0.,time,double(INT_MAX-m->frameMs()));if(snap)time=qRound(time/m->frameMs())*double(m->frameMs());key.ms=qRound(time);
                double value=displayValue(*ref.track,key.value);value=drag==ScaleKeys?pivot.y()+(value-pivot.y())*fy:value+dy;key.value=rawValue(*ref.track,value);
                if(drag==ScaleKeys){if(!key.customHandles){const int index=int(&key-values.data());auto old=original.keys[index];key.leftHandle=original.handle(index,false)-QPointF(old.ms,old.value);key.rightHandle=original.handle(index,true)-QPointF(old.ms,old.value);key.customHandles=true;key.handleType="Free";}key.leftHandle.rx()*=fx;key.rightHandle.rx()*=fx;key.leftHandle.ry()*=fy;key.rightHandle.ry()*=fy;}
                next.insert(keyId(ref.object->id,ref.track->property,key.ms));}
                if(occupied.contains(key.ms))return;occupied.insert(key.ms);}
            std::sort(values.begin(),values.end(),[](const auto &a,const auto &b){return a.ms<b.ms;});updated[cid]=values;}
        if(!pushed){m->pushUndo();pushed=true;}m->design.durationMs=baseDuration;for(auto ref:tracks()){auto cid=channelId(ref.object->id,ref.track->property);if(updated.contains(cid))ref.track->keys=updated[cid];if(!ref.track->keys.isEmpty())m->design.durationMs=qMax(m->design.durationMs,ref.track->keys.last().ms);ref.object->refreshLegacyKeys();}selected=next;m->notify();
    }
    void mouseMoveEvent(QMouseEvent *e)override{
        auto pos=e->position(),delta=pos-last;last=pos;
        if(drag==Pan){double dt=delta.x()/area().width()*(t1-t0),dv=delta.y()/area().height()*(v1-v0);t0-=dt;t1-=dt;if(graph){v0+=dv;v1+=dv;}saveView();}
        else if(drag==Zoom){auto d=pos-press;t0=zoomView[0];t1=zoomView[1];v0=zoomView[2];v1=zoomView[3];if(qAbs(d.x())>=qAbs(d.y())){double factor=qBound(m->frameMs()*2/(zoomView[1]-zoomView[0]),std::exp(qBound(-10.,-d.x()*.01,10.)),1e9/(zoomView[1]-zoomView[0])),anchor=zoomView[0]+(press.x()-area().left())/area().width()*(zoomView[1]-zoomView[0]);t0=anchor+(zoomView[0]-anchor)*factor;t1=anchor+(zoomView[1]-anchor)*factor;}
            else if(graph){double factor=qBound(1e-6/(zoomView[3]-zoomView[2]),std::exp(qBound(-10.,d.y()*.01,10.)),1e12/(zoomView[3]-zoomView[2])),anchor=zoomView[2]+(area().bottom()-press.y())/area().height()*(zoomView[3]-zoomView[2]);v0=anchor+(zoomView[2]-anchor)*factor;v1=anchor+(zoomView[3]-anchor)*factor;}saveView();}
        else if(drag==Scrub)m->setTime(timeAt(pos.x()));
        else if(drag==ScaleMove||drag==ScaleKeys)transform(pos);
        else if(drag==Handle){auto *tr=track(handleKey);if(!tr)return;int i=tr->indexAt(handleKey.time);if(i<0)return;if(!pushed){m->pushUndo();pushed=true;}auto &k=tr->keys[i];if(!k.customHandles){k.leftHandle=tr->handle(i,false)-QPointF(k.ms,k.value);k.rightHandle=tr->handle(i,true)-QPointF(k.ms,k.value);k.customHandles=true;k.handleType="Aligned";}
            double time=timeAt(pos.x(),false);if(handleRight)time=qBound(double(k.ms),time,i+1<tr->keys.size()?tr->handle(i+1,false).x():double(k.ms)+(t1-t0));else time=qBound(i?tr->handle(i-1,true).x():double(k.ms)-(t1-t0),time,double(k.ms));
            QPointF offset(time-k.ms,rawValue(*tr,valueAt(pos.y()))-k.value);auto &current=handleRight?k.rightHandle:k.leftHandle;auto &other=handleRight?k.leftHandle:k.rightHandle;double length=QLineF(QPointF(),other).length();current=offset;if(k.handleType=="Aligned"||(e->modifiers()&Qt::ShiftModifier)){double now=QLineF(QPointF(),offset).length();if(now>1e-8)other=-offset*(length/now);}m->byId(handleKey.object)->refreshLegacyKeys();m->notify();}
        else if(drag==Box){selected=originalSelected;auto box=QRectF(press,pos).normalized();if(graph){for(auto ref:tracks())for(auto &key:ref.track->keys)if(box.contains(point(*ref.track,key.ms,key.value)))selected.insert(keyId(ref.object->id,ref.track->property,key.ms));}
            else if(tree){for(QTreeWidgetItemIterator it(tree);*it;++it){auto row=tree->visualItemRect(*it);double y=row.center().y()+tree->header()->height();if(row.isEmpty()||y<box.top()||y>box.bottom())continue;for(auto ref:tracks())if(matches((*it)->data(0,Qt::UserRole).toString(),(*it)->data(0,Qt::UserRole+1).toString(),ref))for(auto &key:ref.track->keys){double x=area().left()+(key.ms-t0)/(t1-t0)*area().width();if(x>=box.left()&&x<=box.right())selected.insert(keyId(ref.object->id,ref.track->property,key.ms));}}}}
        update();
    }
    void mouseReleaseEvent(QMouseEvent *event)override{if((drag==ScaleMove||drag==ScaleKeys)&&!mouseTransform)return;const bool menu=drag==Pan&&event->button()==Qt::RightButton&&QLineF(press,event->position()).length()<3;drag=None;pushed=false;saveView();changedSelection();if(menu){QMenu popup;auto *cue=popup.addAction("Cue selected frames");cue->setEnabled(!keys().isEmpty());auto *modifier=popup.addAction("Noise modifier…");modifier->setEnabled(!keys().isEmpty()||!tracks(true).isEmpty());auto *action=popup.exec(event->globalPosition().toPoint());if(action==cue)cueFrames();else if(action==modifier)noise();}}
    void cancel(){if(drag==None)return;if(pushed&&(drag==ScaleMove||drag==ScaleKeys||drag==Handle))m->cancelToUndo();if(drag==ScaleMove||drag==ScaleKeys||drag==Handle||drag==Box)selected=originalSelected;drag=None;pushed=false;changedSelection();}
    void wheelEvent(QWheelEvent *e)override{
        if(e->modifiers()&Qt::AltModifier){if(drag==None){if(e->angleDelta().y()>0)m->undo();else if(e->angleDelta().y()<0)m->redo();selected.clear();changedSelection();}e->accept();return;}
        if(e->angleDelta().x()||(e->modifiers()&Qt::ShiftModifier)){double delta=(e->angleDelta().x()?e->angleDelta().x():e->angleDelta().y())/600.*(t1-t0);t0-=delta;t1-=delta;}
        else {double factor=std::pow(1.2,-e->angleDelta().y()/120.),anchor=timeAt(e->position().x(),false);if(e->modifiers()&Qt::ControlModifier&&graph){double v=valueAt(e->position().y());v0=v+(v0-v)*factor;v1=v+(v1-v)*factor;}else if((t1-t0)*factor>=m->frameMs()*2){t0=anchor+(t0-anchor)*factor;t1=anchor+(t1-anchor)*factor;}}
        saveView();update();e->accept();
    }
    void keyPressEvent(QKeyEvent *e)override{
        bool ctrl=e->modifiers()&Qt::ControlModifier,shift=e->modifiers()&Qt::ShiftModifier;
        if(e->key()==Qt::Key_Escape){cancel();return;}if((drag==ScaleMove||drag==ScaleKeys)&&(e->key()==Qt::Key_X||e->key()==Qt::Key_Y)){axis=e->key();return;}
        if(e->key()==Qt::Key_Return&&(drag==ScaleMove||drag==ScaleKeys)){drag=None;pushed=false;return;}
        if(ctrl&&e->key()==Qt::Key_A)selectAll();else if(ctrl&&e->key()==Qt::Key_C)copy();else if(ctrl&&e->key()==Qt::Key_V)paste();else if((ctrl||shift)&&e->key()==Qt::Key_D)duplicate();
        else if(ctrl&&e->key()==Qt::Key_Z){shift?m->redo():m->undo();selected.clear();}
        else if(e->key()==Qt::Key_G)beginTransform(ScaleMove,false);else if(e->key()==Qt::Key_S)scaleKeys();else if(e->key()==Qt::Key_I||e->key()==Qt::Key_K)insert();
        else if(e->key()==Qt::Key_Delete||e->key()==Qt::Key_Backspace)erase();else if(e->key()==Qt::Key_Home)frame(false);else if(e->key()==Qt::Key_F)frame(true);else QWidget::keyPressEvent(e);
    }
private:
    enum {None,Pan,Zoom,Scrub,Box,ScaleMove,ScaleKeys,Handle};int drag=None,axis=0,baseDuration=5000;
    bool initialized=false,pushed=false,mouseTransform=false,handleRight=false;
    double t0=0,t1=5000,v0=-1,v1=1;
    QVector<double> zoomView;
    QPointF press,last,pivot;KeyRef handleKey;
    QHash<QString,QVector<AnimationKey>> snapshot;
    mutable QHash<QString,QPair<double,double>> normalizationRanges;
    QSet<QString> originalSelected;
};

AnimationEditor::AnimationEditor(BuildModel *model,bool graph,QWidget *parent):QWidget(parent),m_model(model){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(2);auto *bar=new QHBoxLayout;bar->setSpacing(3);m_curve=new CurveView(model,graph);
    auto button=[&](QString label,QString icon,auto fn){auto *b=new QToolButton;b->setIcon(uiIcon(icon));b->setToolTip(label);b->setAccessibleName(label);b->setIconSize({14,14});b->setFixedSize(24,22);bar->addWidget(b);connect(b,&QToolButton::clicked,this,fn);};
    button("View All","fit",[this]{m_curve->frame(false);});button("Frame Selected","composition",[this]{m_curve->frame(true);});button("Scale Keys / Handles · S","scale",[this]{m_curve->scaleKeys();});button("Insert key · I","key",[this]{m_curve->insert();});button("Delete","delete",[this]{m_curve->erase();});
    button("Cue selected frames","cue",[this]{m_curve->cueFrames();});
    button("Noise modifier · selected curves","effects",[this]{m_curve->noise();});
    auto *snap=new QCheckBox("Snap");snap->setChecked(true);bar->addWidget(snap);connect(snap,&QCheckBox::toggled,this,[this](bool on){m_curve->snap=on;});
    if(graph){auto *norm=new QCheckBox("Normalize");norm->setChecked(m_curve->normalized);bar->addWidget(norm);connect(norm,&QCheckBox::toggled,this,[this](bool on){m_curve->setNormalized(on);});}
    auto *visibility=new QToolButton;visibility->setIcon(uiIcon("eye"));visibility->setToolTip("Key visibility: all objects or selected only");visibility->setPopupMode(QToolButton::InstantPopup);auto *visMenu=new QMenu(visibility);for(bool only:{false,true}){auto *a=visMenu->addAction(only?"Selected objects only":"All objects");a->setCheckable(true);a->setChecked(!only);connect(a,&QAction::triggered,this,[this,only,visMenu]{m_curve->selectedOnly=only;for(auto *action:visMenu->actions())action->setChecked(action->text()==(only?"Selected objects only":"All objects"));m_signature.clear();rebuild();});}visibility->setMenu(visMenu);bar->addWidget(visibility);
    auto *interp=new QToolButton;interp->setPopupMode(QToolButton::InstantPopup);interp->setToolTip("Interpolation: selected keys, or all keys on selected channels when no keys are selected");auto *menu=new QMenu(interp);for(auto kind:interpolationKinds()){auto *a=menu->addAction(kind);connect(a,&QAction::triggered,this,[this,kind]{m_curve->interpolation(kind);});}interp->setMenu(menu);bar->addWidget(interp);
    if(graph){auto *handle=new QToolButton;handle->setText("Handles");handle->setPopupMode(QToolButton::InstantPopup);handle->setToolTip("Handle type for selected keys");auto *menu=new QMenu(handle);for(auto kind:QStringList{"Auto","Auto Clamped","Aligned","Vector","Free"}){auto *a=menu->addAction(kind);connect(a,&QAction::triggered,this,[this,kind]{m_curve->handles(kind);});}handle->setMenu(menu);bar->addWidget(handle);}
    m_curve->selectionChanged=[this,interp]{interp->setText(m_curve->interpolationLabel());};m_curve->selectionChanged();bar->addStretch();layout->addLayout(bar);
    auto *split=new QSplitter;m_channels=new QTreeWidget;m_channels->setHeaderLabel("Animated channels");m_channels->setMinimumWidth(150);m_channels->setMaximumWidth(280);m_channels->setSelectionMode(QAbstractItemView::ExtendedSelection);m_channels->setIndentation(12);
    m_channels->setStyleSheet("QTreeWidget{background:palette(window);color:palette(text);border:0;font-size:10px;}QTreeWidget::item{height:19px;}QTreeWidget::item:selected{background:#3c5878;}QHeaderView::section{background:palette(button);color:palette(text);border:0;padding:4px;}");
    m_curve->tree=m_channels;connect(m_channels,&QTreeWidget::itemSelectionChanged,this,[this]{m_curve->chooseRows();});connect(m_channels,&QTreeWidget::itemExpanded,m_curve,[this]{m_curve->update();});connect(m_channels,&QTreeWidget::itemCollapsed,m_curve,[this]{m_curve->update();});connect(m_channels->verticalScrollBar(),&QScrollBar::valueChanged,m_curve,[this]{m_curve->update();});
    split->addWidget(m_channels);split->addWidget(m_curve);split->setStretchFactor(1,1);layout->addWidget(split);
    connect(model,&BuildModel::changed,this,[this]{rebuild();});connect(model,&BuildModel::selectionChanged,this,[this]{rebuild();});rebuild();
}
void AnimationEditor::rebuild(){
    QString signature;for(const auto &o:m_model->design.objects){signature+=o.id+o.name+QString::number(m_model->sel.contains(o.id));for(const auto &tr:o.tracks)signature+=tr.property;}signature+=m_model->activeId+QString::number(m_curve->selectedOnly);
    if(signature==m_signature)return;m_signature=signature;QSignalBlocker block(m_channels);
    for(QTreeWidgetItemIterator it(m_channels);*it;++it)m_expanded[channelId((*it)->data(0,Qt::UserRole).toString(),(*it)->data(0,Qt::UserRole+1).toString())]=(*it)->isExpanded();
    m_channels->clear();
    auto add=[&](QTreeWidgetItem *parent,QString name,QString id,QString path,bool open){auto *item=parent?new QTreeWidgetItem(parent,{name}):new QTreeWidgetItem(m_channels,{name});item->setData(0,Qt::UserRole,id);item->setData(0,Qt::UserRole+1,path);item->setExpanded(m_expanded.value(channelId(id,path),open));item->setSelected(m_curve->channels.contains(channelId(id,path)));item->setToolTip(0,name+" · Shift/Ctrl select multiple channels");return item;};
    QTreeWidgetItem *root=nullptr;if(!m_curve->graph)root=add(nullptr,"Summary","@all","*",true);
    QTreeWidgetItem *first=nullptr;for(const auto &o:m_model->design.objects){if(!m_curve->visibleObject(o))continue;if(o.tracks.isEmpty()&&!m_model->sel.contains(o.id))continue;auto *object=add(root,o.name,o.id,"*",m_curve->graph);if(m_model->sel.contains(o.id))object->setForeground(0,QColor("#9fc7ef"));QHash<QString,QTreeWidgetItem*> groups;QStringList paths;for(auto &tr:o.tracks)paths<<tr.property;
        if(m_model->sel.contains(o.id))for(auto path:transformChannels())if(!paths.contains(path))paths<<path;
        for(auto path:paths){auto group=path.section('.',0,0);if(!groups.contains(group))groups[group]=add(object,group=="position"?QString("Translation"):group,o.id,"group:"+group,false);auto *item=add(groups[group],path.section('.',1),o.id,path,false);if(!first&&o.id==m_model->activeId)first=item;if(m_curve->primary==channelId(o.id,path))m_channels->setCurrentItem(item->parent()->isExpanded()?item:item->parent(),0,QItemSelectionModel::NoUpdate);}}
    if(m_curve->channels.isEmpty()&&first){first->setSelected(true);m_channels->setCurrentItem(first->parent(),0,QItemSelectionModel::NoUpdate);m_curve->channels.insert(channelId(first->data(0,Qt::UserRole).toString(),first->data(0,Qt::UserRole+1).toString()));m_curve->primary=*m_curve->channels.begin();}
    for(QTreeWidgetItemIterator it(m_channels);*it;++it)if((*it)->data(0,Qt::UserRole+1).toString().startsWith("group:"))(*it)->setExpanded(m_expanded.value(channelId((*it)->data(0,Qt::UserRole).toString(),(*it)->data(0,Qt::UserRole+1).toString()),false));
    m_curve->ensureFrame();m_curve->changedSelection();
}
