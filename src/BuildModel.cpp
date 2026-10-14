#include "BuildModel.h"
#include "UiControls.h"
#include "SurfaceEffects.h"
#include "CueSequence.h"

#include <QUuid>
#include <QtMath>
#include <algorithm>

namespace {
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8); }
} // namespace

BuildModel::BuildModel(){connect(uiGesture(),&UiGesture::finished,this,[this](quint64 token,bool cancel){if(token&&token==m_lastGesture&&cancel)cancelToUndo();});}

QVector<DObject *> BuildModel::selected() {
    QVector<DObject *> r;
    for (DObject &o : design.objects)
        if (sel.contains(o.id)) r.append(&o);
    return r;
}
QVector<DObject *> BuildModel::inspected(){auto result=selected();if(result.isEmpty())if(auto *o=active())result.append(o);return result;}

void BuildModel::reset(const ElementDesign &d) {
    design = d;
    design.ensureCamera();
    activeId.clear();
    sel.clear();
    selKey = -1;
    selChannel.clear();
    t = 0;
    playing = false;
    previewCueId.clear();previewCueElapsed=0;previewCueStep=0;cueRangeStart=cueRangeEnd=-1;
    autoKey = false;
    editMode = false;
    esel.clear();
    m_undo.clear();
    m_redo.clear();
    emit selectionChanged();
    emit modeChanged();
    emit timeChanged();
    emit changed();
}

// ------------------------------------------------------------------ undo

void BuildModel::pushUndo() {
    const auto token=uiGesture()->token;if(token&&m_lastGesture==token)return;m_lastGesture=token;
    m_undo.append(design.toJson());
    if (m_undo.size() > 80) m_undo.removeFirst();
    m_redo.clear();
}

void BuildModel::restore(const QString &json) {
    const bool wasEdit = editMode;
    design = ElementDesign::fromJson(json);
    design.ensureCamera();
    QSet<QString> keep;
    for (const QString &id : sel)
        if (design.find(id)) keep.insert(id);
    sel = keep;
    if (!design.find(activeId)) activeId = sel.isEmpty() ? QString() : *sel.begin();
    selKey = -1;
    selChannel.clear();
    editMode = false;
    esel.clear();
    if (wasEdit) emit modeChanged();
    emit selectionChanged();
    emit changed();
}

bool BuildModel::undo() {
    if (m_undo.isEmpty()) return false;
    m_redo.append(design.toJson());
    const QString s = m_undo.takeLast();
    restore(s);
    return true;
}

bool BuildModel::redo() {
    if (m_redo.isEmpty()) return false;
    m_undo.append(design.toJson());
    const QString s = m_redo.takeLast();
    restore(s);
    return true;
}

void BuildModel::dropUndo() {
    if (!m_undo.isEmpty()) m_undo.removeLast();
}

bool BuildModel::cancelToUndo() {
    if (m_undo.isEmpty()) return false;
    const bool wasEdit = editMode;
    const SelMode mode = selMode;
    const QString s = m_undo.takeLast();
    design = ElementDesign::fromJson(s);
    design.ensureCamera();
    QSet<QString> keep;
    for (const QString &id : sel)
        if (design.find(id)) keep.insert(id);
    sel = keep;
    if (!design.find(activeId)) activeId = sel.isEmpty() ? QString() : *sel.begin();
    if (wasEdit && active() && active()->type == "Mesh") {
        editMode = true;
        selMode = mode;
        if (Mesh *m = editMesh()) {
            QSet<int> keepV;
            for (int i : esel.v) if (i < m->v.size()) keepV.insert(i);
            esel.clear();
            esel.v = keepV;
            reconcile();
        }
    } else {
        editMode = false;
        esel.clear();
    }
    emit selectionChanged();
    emit changed();
    return true;
}

// ------------------------------------------------------------------ naming and adding

QString BuildModel::uniqueName(const QString &base0) const {
    QString base = base0;
    const int dot = base.lastIndexOf('.');
    if (dot > 0) {
        bool num = true;
        for (int i = dot + 1; i < base.size(); ++i) num = num && base[i].isDigit();
        if (num && dot + 1 < base.size()) base = base.left(dot);
    }
    QSet<QString> used;
    for (const DObject &o : design.objects) used.insert(o.name);
    if (!used.contains(base)) return base;
    for (int i = 1; i < 10000; ++i) {
        const QString n = QString("%1.%2").arg(base).arg(i, 3, 10, QChar('0'));
        if (!used.contains(n)) return n;
    }
    return base;
}

static void finishAdd(BuildModel *m, const QString &id) {
    if(auto *o=m->byId(id)){o->base.loc=m->design.cursor;o->captureDefaults();}
    m->sel = {id};
    m->activeId = id;
    m->selKey = -1;
    m->editMode = false;
    m->esel.clear();
    emit m->selectionChanged();
    emit m->changed();
}

QString BuildModel::addMesh(const QString &kind) {
    pushUndo();
    DObject o;
    o.id = newId();
    o.name = uniqueName(kind);
    o.mesh = Mesh::primitive(kind);
    if(kind=="Cube")o.base.rot={20,0,25};
    if(kind=="Plane" || kind=="Circle" || kind=="Grid"){o.base.rot={90,0,0};o.mat.shadeless=true;}
    design.objects.append(o);
    finishAdd(this, o.id);
    return o.id;
}

QString BuildModel::addText() {
    pushUndo();
    DObject o;
    o.id = newId();
    o.name = uniqueName("Text");
    o.type = "Text";
    o.text = "Your text";
    o.fontPx = 150;
    o.w = 6;
    o.h = 1.5;
    o.mat.color = Qt::white;
    o.mat.shadeless = true;
    o.base.rot = {90, 0, 0};  // stands up facing the fixed broadcast camera
    design.objects.append(o);
    finishAdd(this, o.id);
    return o.id;
}

QString BuildModel::addImage(const QString &path) {
    const QImage img(path);
    if (img.isNull()) return {};
    pushUndo();
    DObject o;
    o.id = newId();
    o.name = uniqueName("Image");
    o.type = "Image";
    o.image = path;
    o.w = 4;
    o.h = 4.0 * img.height() / qMax(1, img.width());
    o.mat.shadeless = true;
    o.base.rot = {90, 0, 0};
    design.objects.append(o);
    finishAdd(this, o.id);
    return o.id;
}

QString BuildModel::addLight(const QString &kind) {
    pushUndo();
    DObject o;
    o.id = newId();
    o.name = uniqueName(kind == "Sun" ? "Sun" : kind == "Spot" ? "Spot" : kind == "Area" ? "Area" : "Point");
    o.type = "Light";
    o.light.kind = kind;
    o.light.energy = kind == "Sun" ? 3.0 : 1000.0;
    o.base.loc = kind == "Sun" ? QVector3D(0, 0, 6) : QVector3D(0, -2, 4);
    if (kind == "Sun") o.base.rot = {35, 15, 0};
    design.objects.append(o);
    finishAdd(this, o.id);
    return o.id;
}

void BuildModel::changeLightKind(const QString &kind) {
    DObject *o = active();
    if (!o || o->type != "Light" || o->light.kind == kind) return;
    pushUndo();
    o->light.energy = kind == "Sun" ? 3.0 : (o->light.kind == "Sun" ? 1000.0 : o->light.energy);
    o->light.kind = kind;
    emit changed();
}

QStringList BuildModel::duplicateSelected() {
    QStringList ids;
    const QVector<DObject *> src = selected();
    if (src.isEmpty()) return ids;
    pushUndo();
    QVector<DObject> copies;
    QHash<QString,QString> remap;
    for (DObject *s : src) {
        if (s->type == "Camera") continue;
        DObject c = *s;
        c.id = newId();
        remap.insert(s->id,c.id);
        c.name = uniqueName(s->name);
        copies.append(c);
    }
    for(auto &copy:copies){if(remap.contains(copy.parentId))copy.parentId=remap.value(copy.parentId);for(auto &modifier:copy.mods)if(remap.contains(modifier.target))modifier.target=remap.value(modifier.target);}
    const auto copiedActive=remap.value(activeId);
    sel.clear();
    for (const DObject &c : copies) {
        design.objects.append(c);
        sel.insert(c.id);
        ids << c.id;
    }
    if (!ids.isEmpty()) activeId = copiedActive.isEmpty()?ids.last():copiedActive;
    emit selectionChanged();
    emit changed();
    return ids;
}

void BuildModel::deleteSelected() {
    QVector<QString> kill;
    for (const DObject &o : design.objects)
        if (sel.contains(o.id) && o.type != "Camera" && !o.locked) kill << o.id;
    if (kill.isEmpty()) return;
    pushUndo();
    // Detach surviving children before removing a parent, preserving their world transform.
    for(auto &o:design.objects)if(kill.contains(o.parentId)&&!kill.contains(o.id)){o.parentInverse=design.parentMatrix(o,t);o.parentId.clear();}
    for (const QString &id : kill)
        for (int i = 0; i < design.objects.size(); ++i)
            if (design.objects[i].id == id) {
                design.objects.removeAt(i);
                break;
            }
    for (DObject &o : design.objects)  // a boolean that cut with it no longer has a tool
        for (Modifier &m : o.mods)
            if (kill.contains(m.target)) m.target.clear();
    sel.clear();
    if(kill.contains(activeId))activeId=design.objects.isEmpty()?QString():design.objects.last().id;
    editMode = false;
    esel.clear();
    emit modeChanged();
    emit selectionChanged();
    emit changed();
}

void BuildModel::rename(const QString &id, const QString &name) {
    DObject *o = byId(id);
    const QString n = name.trimmed();
    if (!o || n.isEmpty() || n == o->name) return;
    pushUndo();
    o->name = n;
    emit changed();
}

void BuildModel::hideSelected(bool hide) {
    const auto objs = selected();
    if (objs.isEmpty()) return;
    pushUndo();
    for (DObject *o : objs) o->visible = !hide;
    emit changed();
}

void BuildModel::showAll() {
    pushUndo();
    for (DObject &o : design.objects) o.visible = true;
    emit changed();
}

// ------------------------------------------------------------------ selection

void BuildModel::select(const QString &id, bool additive) {
    if (editMode && id != activeId) {
        editMode = false;
        esel.clear();
        emit modeChanged();
    }
    if (additive) {
        if (sel.contains(id)) {
            sel.remove(id);
            if (activeId == id && !sel.isEmpty()) activeId = *sel.begin();
        } else {
            sel.insert(id);
            activeId = id;
        }
    } else {
        sel = {id};
        activeId = id;
    }
    selKey = -1;
    selChannel.clear();
    emit selectionChanged();
    emit changed();
}

void BuildModel::setSelection(const QSet<QString> &ids, const QString &act) {
    if (editMode) {
        editMode = false;
        esel.clear();
        emit modeChanged();
    }
    sel = ids;
    if(ids.contains(act))activeId=act;else if(!ids.isEmpty())activeId=*ids.begin();
    selKey = -1;
    selChannel.clear();
    emit selectionChanged();
    emit changed();
}

void BuildModel::selectAll() {
    sel.clear();
    for (const DObject &o : design.objects) sel.insert(o.id);
    if (!sel.contains(activeId)) activeId = sel.isEmpty() ? QString() : *sel.begin();
    emit selectionChanged();
    emit changed();
}

void BuildModel::deselectAll() {
    sel.clear();
    selKey = -1;
    selChannel.clear();
    emit selectionChanged();
    emit changed();
}

// ------------------------------------------------------------------ time and keys

bool BuildModel::cuePreviewWaiting() const {
    if(!playing)return false;
    for(const auto &value:design.animationCues){
        const auto cue=value.toObject();if(cue["id"].toString()!=previewCueId)continue;
        const auto plan=compileAnimationCue(design,cue);
        const auto sample=plan.sample(previewCueElapsed,previewCueStep);
        return sample.visible&&!sample.advancing&&sample.step>=0&&plan.steps[sample.step].manualHold;
    }
    return false;
}

bool BuildModel::continueCuePreview() {
    if(!cuePreviewWaiting())return false;
    for(const auto &value:design.animationCues){
        const auto cue=value.toObject();if(cue["id"].toString()!=previewCueId)continue;
        const auto plan=compileAnimationCue(design,cue);
        const auto sample=plan.sample(previewCueElapsed,previewCueStep);
        const int next=plan.next(sample.step);
        if(next<0){playing=false;notify();return true;}
        previewCueStep=next;previewCueElapsed=0;setTime(plan.steps[next].start);notify();return true;
    }
    return false;
}

void BuildModel::setTime(qint64 ms) {
    for (auto &o : design.objects) o.overrides.clear();
    t = qBound<qint64>(0, ms, qint64(design.durationMs));
    emit timeChanged();
}

void BuildModel::setProps(const QString &id, const DProps &p) {
    if (DObject *o = byId(id)) {
        if (o->locked) return;
        const DProps old = o->at(t);
        QHash<QString,QMatrix4x4> children;if(isolateParents)for(const auto &child:design.objects)if(child.parentId==id)children[child.id]=design.parentMatrix(child,t);
        for (const auto &path : transformChannels()) {
            const double v = channelValue(p, path);
            if (qAbs(v - channelValue(old, path)) < 0.000001) continue;
            if (autoKey) o->keyChannel(path, int(t), v);
            else {
                setChannelValue(o->base, path, v);
                if (o->track(path)) o->overrides[path] = v;
            }
        }
        const auto inverse=design.worldMatrix(*o,t).inverted();for(auto it=children.begin();it!=children.end();++it)if(auto *child=byId(it.key()))child->parentInverse=inverse*it.value();
    }
}

void BuildModel::editProps(const QString &id, const std::function<void(DProps &)> &f) {
    DObject *o = byId(id);
    if (!o) return;
    DProps p = o->at(t);
    f(p);
    setProps(id, p);
    emit changed();
}

void BuildModel::keyProperty(const QString &id, const QString &property) {
    DObject *o = byId(id);
    if (!o || o->locked) return;
    pushUndo();
    o->keyChannel(property, int(t), o->propertyValue(property, t));
    selChannel = property;
    selKey = o->track(property)->indexAt(t);
    emit changed();
}
void BuildModel::togglePropertyKey(const QString &id,const QString &property){
    auto *o=byId(id);if(!o||o->locked)return;
    auto *track=o->track(property);int index=track?track->indexAt(t):-1;
    if(index<0){keyProperty(id,property);return;}
    if(o->overrides.contains(property)&&qAbs(o->propertyValue(property,t)-track->keys[index].value)>1e-6){keyProperty(id,property);return;}
    const double value=o->propertyValue(property,t);pushUndo();track->keys.removeAt(index);
    o->tracks.removeIf([](const auto &tr){return tr.keys.isEmpty();});
    o->setPropertyBase(property,value);if(o->track(property))o->overrides[property]=value;else o->overrides.remove(property);o->refreshLegacyKeys();selChannel=property;selKey=-1;emit changed();
}
void BuildModel::resetProperty(const QString &id,const QString &property){
    auto *o=byId(id);if(!o||o->locked)return;
    const double value=o->propertyDefault(property);pushUndo();editProperty(id,property,value);
}
bool BuildModel::parentSelected(bool clear){
    auto *parent=active();if(!clear&&(!parent||sel.size()<2))return false;
    QVector<QString> children;
    for(const auto &id:sel){auto *o=byId(id);if(!o||o->locked||o->type=="Camera"||(!clear&&id==activeId))continue;
        if(!clear){QSet<QString> visited;auto *p=parent;bool cycle=false;while(p&&!visited.contains(p->id)){if(p->id==id){cycle=true;break;}visited.insert(p->id);p=byId(p->parentId);}if(cycle)return false;}
        children.append(id);
    }
    if(children.isEmpty())return false;
    bool invertible=true;QMatrix4x4 inverse;
    if(!clear)inverse=design.worldMatrix(*parent,t).inverted(&invertible);
    if(!invertible)return false;
    pushUndo();
    for(const auto &id:children){auto *o=byId(id);const auto oldBasis=design.parentMatrix(*o,t);o->parentInverse=inverse*oldBasis;o->parentId=clear?QString():activeId;}
    emit changed();return true;
}

bool BuildModel::propertyIsKeyed(const DObject &o, const QString &property, qint64 atMs) const {
    const auto *track = o.track(property);
    return track && track->indexAt(atMs) >= 0;
}

void BuildModel::editProperty(const QString &id, const QString &property, double value) {
    auto *o = byId(id);
    if (!o || o->locked) return;
    if(transformChannels().contains(property)){auto props=o->at(t);setChannelValue(props,property,value);setProps(id,props);emit changed();return;}
    if ((property=="layout.width" || property=="layout.height") && o->style["layout.aspectLock"].toBool()) {
        const QString other=property=="layout.width"?"layout.height":"layout.width";
        double paired=o->propertyValue(other,t)*value/qMax(1.0,o->propertyValue(property,t));
        if(autoKey)o->keyChannel(other,int(t),paired);
        else {o->setPropertyBase(other,paired);if(o->track(other))o->overrides[other]=paired;}
    }
    if (autoKey) o->keyChannel(property, int(t), value);
    else {
        o->setPropertyBase(property, value);
        if (o->track(property)) o->overrides[property] = value;
    }
    emit changed();
}

void BuildModel::insertKey() {
    const auto objs = selected();
    if (objs.isEmpty()) return;
    pushUndo();
    selChannel.clear();
    for (DObject *o : objs) {
        if (o->locked) continue;
        o->setKey(t, o->at(t));
        const int i = o->keyIndexNear(t);
        if (i >= 0) o->keys[i].channels = {"position.x","position.y","position.z","rotation.x","rotation.y","rotation.z","scale.x","scale.y","scale.z","opacity"};
    }
    if (DObject *a = active()) selKey = a->keyIndexNear(t);
    emit changed();
}

void BuildModel::deleteKey() {
    DObject *o = active();
    if (!o || o->locked) return;
    bool found = false;
    for (const auto &track : o->tracks)
        if ((selChannel.isEmpty() || selChannel == track.property) && track.indexAt(t) >= 0) found = true;
    if (!found) return;
    pushUndo();
    for (auto &track : o->tracks) {
        if (!selChannel.isEmpty() && track.property != selChannel) continue;
        const int i = track.indexAt(t);
        if (i >= 0) track.keys.removeAt(i);
    }
    o->tracks.removeIf([](const auto &track) { return track.keys.isEmpty(); });
    o->refreshLegacyKeys();
    selKey = -1;
    selChannel.clear();
    emit changed();
}

void BuildModel::setInterpolation(const QString &kind) {
    DObject *o = active();
    if (!o || o->locked || !interpolationKinds().contains(kind)) return;
    pushUndo();
    for (auto &track : o->tracks) {
        if (!selChannel.isEmpty() && track.property != selChannel) continue;
        const int i = track.indexAt(t);
        if (i >= 0) track.keys[i].interpolation = kind;
    }
    o->refreshLegacyKeys();
    emit changed();
}

void BuildModel::jumpKey(int dir) {
    QVector<int> times;
    const auto objs = selected();
    for (const DObject &o : design.objects)
        if (objs.isEmpty() || sel.contains(o.id))
            for (const DKey &k : o.keys) times << k.ms;
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    if (times.isEmpty()) return;
    if (dir > 0) {
        for (int ms : times)
            if (ms > t + 1) {
                setTime(ms);
                return;
            }
        setTime(times.last());
    } else {
        for (int i = times.size() - 1; i >= 0; --i)
            if (times[i] < t - 1) {
                setTime(times[i]);
                return;
            }
        setTime(times.first());
    }
}

void BuildModel::stepFrame(int dir) { setTime(t + dir * frameMs()); }

// ------------------------------------------------------------------ matrices

DProps BuildModel::decompose(const QMatrix4x4 &m) {
    DProps p;
    p.loc = m.column(3).toVector3D();
    QVector3D c0 = m.column(0).toVector3D(), c1 = m.column(1).toVector3D(), c2 = m.column(2).toVector3D();
    float sx = c0.length(), sy = c1.length(), sz = c2.length();
    if (QVector3D::dotProduct(QVector3D::crossProduct(c0, c1), c2) < 0) sx = -sx;
    p.scl = QVector3D(sx, sy, sz);
    if (qAbs(sx) > 1e-8f) c0 /= sx;
    if (qAbs(sy) > 1e-8f) c1 /= sy;
    if (qAbs(sz) > 1e-8f) c2 /= sz;
    // R = Rz * Ry * Rx
    const float r20 = c0.z(), r21 = c1.z(), r22 = c2.z(), r10 = c0.y(), r00 = c0.x(), r11 = c1.y(), r12 = c2.y();
    float ex, ey, ez;
    ey = qAsin(qBound(-1.0f, -r20, 1.0f));
    if (qAbs(r20) < 0.99999f) {
        ex = qAtan2(r21, r22);
        ez = qAtan2(r10, r00);
    } else {  // gimbal lock
        ex = qAtan2(-r12, r11);
        ez = 0;
    }
    p.rot = QVector3D(qRadiansToDegrees(ex), qRadiansToDegrees(ey), qRadiansToDegrees(ez));
    return p;
}

// ------------------------------------------------------------------ join / origin / separate

void BuildModel::shiftOrigin(DObject &o, const QVector3D &c) {
    const DObject original=o;
    for (QVector3D &v : o.mesh.v) v -= c;
    o.base.loc += ElementDesign::matrixOf(o.base).mapVector(c);
    bool changingBasis=false;for(const auto &track:original.tracks)if(track.property.startsWith("rotation.")||track.property.startsWith("scale."))changingBasis=true;
    if(changingBasis){
        // Changing the pivot under animated rotation/scale requires compensated positions at every output frame.
        QSet<int> times{0,design.durationMs};for(int ms=0;ms<design.durationMs;ms+=qMax(1,frameMs()))times.insert(ms);
        for(const auto &track:original.tracks)for(const auto &key:track.keys)times.insert(key.ms);
        QList<int> ordered=times.values();std::sort(ordered.begin(),ordered.end());
        for(int axis=0;axis<3;++axis){const QString path="position."+QString("xyz").mid(axis,1);if(!o.track(path)){PropertyTrack track;track.property=path;o.tracks.append(track);}auto *track=o.track(path);track->keys.clear();
            for(int ms:ordered){auto props=original.at(ms);auto location=props.loc+ElementDesign::matrixOf(props).mapVector(c);track->keys.append({ms,location[axis],"Linear"});}
        }
    }else{
        const auto delta=ElementDesign::matrixOf(original.base).mapVector(c);
        for(int axis=0;axis<3;++axis)if(auto *track=o.track("position."+QString("xyz").mid(axis,1)))for(auto &key:track->keys)key.value+=delta[axis];
    }
    o.overrides.clear();o.refreshLegacyKeys();
}

bool BuildModel::setOrigin(const QString &kind) {
    DObject *a = active();
    if (!a) return false;
    if (kind == "Selected" && !(editMode && a->type == "Mesh")) return false;
    pushUndo();
    bool any = false;
    for (DObject *o : selected()) {
        if (o->type != "Mesh" || o->mesh.v.isEmpty()) continue;
        QVector3D c;
        if (kind == "Geometry") {
            c = o->mesh.boundsCenter();
        } else if (kind == "Bottom") {
            QVector3D mn, mx;
            o->mesh.bounds(mn, mx);
            c = QVector3D((mn.x() + mx.x()) / 2, (mn.y() + mx.y()) / 2, mn.z());
        } else if (kind == "World"||kind=="Cursor") {
            c = design.worldMatrix(*o,t).inverted().map(kind=="Cursor"?design.cursor:QVector3D());
        } else if(kind.startsWith("Bounds:")){
            QVector3D low,high;o->mesh.bounds(low,high);c=(low+high)/2;
            const auto position=kind.mid(7);if(position.contains("Left"))c.setX(low.x());if(position.contains("Right"))c.setX(high.x());if(position.contains("Top"))c.setZ(high.z());if(position.contains("Bottom"))c.setZ(low.z());
        } else if (kind == "Selected") {
            if (o != a || esel.v.isEmpty()) continue;
            c = o->mesh.selCenter(esel);
        }
        shiftOrigin(*o, c);
        any = true;
    }
    if (!any) {
        dropUndo();
        return false;
    }
    emit changed();
    return true;
}

bool BuildModel::joinSelected() {
    DObject *a = active();
    if (!a || a->type != "Mesh") return false;
    QVector<DObject *> others;
    for (DObject *o : selected())
        if (o != a && o->type == "Mesh") others << o;
    if (others.isEmpty()) return false;
    pushUndo();
    const QMatrix4x4 inv = design.worldMatrix(*a,t).inverted();
    QSet<QString> gone;
    for (DObject *o : others) {
        Mesh m = o->mesh;
        m.transform(inv * design.worldMatrix(*o,t));
        a->mesh.append(m);
        gone.insert(o->id);
    }
    const QString keep = a->id;
    for (int i = design.objects.size() - 1; i >= 0; --i)
        if (gone.contains(design.objects[i].id)) design.objects.removeAt(i);
    sel = {keep};
    activeId = keep;
    editMode = false;
    esel.clear();
    emit modeChanged();
    emit selectionChanged();
    emit changed();
    return true;
}

bool BuildModel::separateLooseParts() {
    DObject *a = active();
    if (!a || a->type != "Mesh") return false;
    const QVector<Mesh> parts = a->mesh.looseParts();
    if (parts.size() < 2) return false;
    pushUndo();
    const DObject base = *a;
    const QString baseName = a->name;
    a->mesh = parts[0];
    QSet<QString> ids{a->id};
    for (int i = 1; i < parts.size(); ++i) {
        DObject c = base;
        c.id = newId();
        c.name = uniqueName(baseName);
        c.mesh = parts[i];
        design.objects.append(c);
        ids.insert(c.id);
    }
    sel = ids;
    editMode = false;
    esel.clear();
    emit modeChanged();
    emit selectionChanged();
    emit changed();
    return true;
}

// ------------------------------------------------------------------ edit mode

Mesh *BuildModel::editMesh() {
    DObject *o = active();
    return (editMode && o && o->type == "Mesh") ? &o->mesh : nullptr;
}

bool BuildModel::applyModifier(const QString &objectId,int index){
    auto *object=byId(objectId);
    if(editMode||!object||object->locked||object->type!="Mesh"||index<0||index>=object->mods.size())return false;
    // Bake the ordered prefix: later modifiers depend on the geometry before them.
    auto snapshot=design;auto *copy=snapshot.find(objectId);copy->mods.resize(index+1);
    const Mesh baked=snapshot.evalMesh(*copy,t);
    pushUndo();object->mesh=baked;object->mods.remove(0,index+1);notify();return true;
}

bool BuildModel::enterEdit() {
    DObject *o = active();
    if(o&&o->type=="Shape"&&!o->locked){
        pushUndo();const QRectF bounds(-o->w/2,-o->h/2,o->w,o->h);
        auto outline=*o;if(outline.style["corner.unit"].toString()!="%")for(const auto &key:QStringList{"corner.radius","corner.tl","corner.tr","corner.br","corner.bl"})outline.style[key]=o->propertyBase(key)/kPxPerUnit;
        const QPainterPath path=surfaceOutline(bounds,outline);
        const auto polygon=path.toFillPolygon();Mesh mesh;QVector<int> face;
        for(int i=0;i<polygon.size();++i){if(i==polygon.size()-1&&polygon[i]==polygon[0])break;face.append(mesh.v.size());mesh.v.append({float(polygon[i].x()),float(polygon[i].y()),0});}
        mesh.f.append(face);if(mesh.faceNormal(0).z()<0)mesh.flipNormals();o->mesh=mesh;o->type="Mesh";
    }
    if (!o || o->type != "Mesh" || o->locked) return false;
    editMode = true;
    esel.clear();
    emit modeChanged();
    emit changed();
    return true;
}

void BuildModel::exitEdit() {
    if (!editMode) return;
    editMode = false;
    esel.clear();
    emit modeChanged();
    emit changed();
}

void BuildModel::setSelMode(SelMode m) {
    Mesh *mesh = editMesh();
    if (!mesh || m == selMode) {
        selMode = m;
        emit modeChanged();
        return;
    }
    mesh->normalizeSel(esel, selMode);
    EditSel next = esel;
    if (m == SelMode::Edge) next.f.clear();
    if (m == SelMode::Vertex) next.e.clear();
    if (m == SelMode::Face) next.e.clear();
    selMode = m;
    esel = next;
    EditSel tmp;
    tmp.v = esel.v;
    mesh->normalizeSel(tmp, SelMode::Vertex);
    if (m == SelMode::Edge) esel.e = tmp.e;
    if (m == SelMode::Face) esel.f = tmp.f;
    mesh->normalizeSel(esel, m);
    emit modeChanged();
    emit changed();
}

void BuildModel::reconcile() {
    Mesh *m = editMesh();
    if (!m) return;
    QSet<int> v;
    for (int i : esel.v)
        if (i >= 0 && i < m->v.size()) v.insert(i);
    esel.clear();
    esel.v = v;
    EditSel tmp;
    tmp.v = v;
    m->normalizeSel(tmp, SelMode::Vertex);
    if (selMode == SelMode::Vertex) {
        esel = tmp;
    } else if (selMode == SelMode::Edge) {
        esel.e = tmp.e;
        m->normalizeSel(esel, SelMode::Edge);
    } else {
        esel.f = tmp.f;
        m->normalizeSel(esel, SelMode::Face);
    }
}

void BuildModel::editSelectAll(bool on) {
    Mesh *m = editMesh();
    if (!m) return;
    esel.clear();
    if (on)
        for (int i = 0; i < m->v.size(); ++i) esel.v.insert(i);
    reconcile();
    emit changed();
}

void BuildModel::editInvert() {
    Mesh *m = editMesh();
    if (!m) return;
    QSet<int> v;
    for (int i = 0; i < m->v.size(); ++i)
        if (!esel.v.contains(i)) v.insert(i);
    esel.clear();
    esel.v = v;
    reconcile();
    emit changed();
}

#define EDIT_OP_BEGIN \
    Mesh *m = editMesh(); \
    if (!m) return false; \
    pushUndo();
#define EDIT_OP_FAIL \
    { dropUndo(); return false; }

bool BuildModel::opSubdivide() {
    EDIT_OP_BEGIN
    if (!m->subdivide(esel, selMode)) EDIT_OP_FAIL
    reconcile();
    emit changed();
    return true;
}

bool BuildModel::opExtrude(QVector3D *dirWorld) {
    EDIT_OP_BEGIN
    QVector3D dir;
    if (!m->extrude(esel, selMode, &dir)) EDIT_OP_FAIL
    if (dirWorld) {
        const QMatrix4x4 W = design.worldMatrix(*active(),t);
        *dirWorld = dir.isNull() ? QVector3D() : W.mapVector(dir).normalized();
    }
    reconcile();
    emit changed();
    return true;
}

bool BuildModel::opInset(QVector<std::pair<int, QVector3D>> *info) {
    EDIT_OP_BEGIN
    if (!m->inset(esel, selMode, info)) EDIT_OP_FAIL
    reconcile();
    emit changed();
    return true;
}

bool BuildModel::opLoopCut(EdgeKey e, int cuts) {
    EDIT_OP_BEGIN
    if (!m->loopCut(e, cuts, esel)) EDIT_OP_FAIL
    reconcile();
    emit changed();
    return true;
}

bool BuildModel::opFill() {
    EDIT_OP_BEGIN
    if (!m->fill(esel, selMode)) EDIT_OP_FAIL
    // select the new face
    esel.f = {int(m->f.size()) - 1};
    esel.v.clear();
    for (int i : m->f.last()) esel.v.insert(i);
    reconcile();
    emit changed();
    return true;
}

bool BuildModel::opFlatten() {
    EDIT_OP_BEGIN
    if (!m->flatten(esel, selMode)) EDIT_OP_FAIL
    emit changed();
    return true;
}

bool BuildModel::opMerge() {
    EDIT_OP_BEGIN
    if (!m->mergeAtCenter(esel, selMode)) EDIT_OP_FAIL
    reconcile();
    emit changed();
    return true;
}

bool BuildModel::opDelete() {
    EDIT_OP_BEGIN
    if (!m->deleteSel(esel, selMode)) EDIT_OP_FAIL
    esel.clear();
    emit changed();
    return true;
}

bool BuildModel::opFlip() {
    EDIT_OP_BEGIN
    m->normalizeSel(esel, selMode);
    if (esel.f.isEmpty()) {  // nothing selected: flip the whole mesh
        m->flipNormals();
    } else {
        const QSet<int> f = esel.f;
        m->flipNormals(&f);
    }
    emit changed();
    return true;
}

bool BuildModel::opSeparate() {
    EDIT_OP_BEGIN
    DObject *a = active();
    Mesh part = m->separateFaces(esel, selMode);
    if (part.isEmpty()) EDIT_OP_FAIL
    DObject c = *a;
    c.id = newId();
    c.name = uniqueName(a->name);
    c.mesh = part;
    c.mods.clear();
    design.objects.append(c);
    esel.clear();
    emit changed();
    return true;
}

bool BuildModel::opSplit() {
    EDIT_OP_BEGIN
    if (!m->splitFaces(esel, selMode)) EDIT_OP_FAIL
    reconcile();
    emit changed();
    return true;
}
