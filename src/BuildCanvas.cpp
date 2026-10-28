#include "BuildCanvas.h"
#include "ColorManagement.h"
#include "SceneEffects.h"
#include "TextureLibrary.h"
#include "AudioLibrary.h"
#include "Toast.h"
#include <QDragEnterEvent>
#include <QMimeData>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QQuaternion>
#include <QPlainTextEdit>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QtMath>
#include <algorithm>

#include "Data.h"

namespace {
const QColor kOrange("#ff9f43"), kOrangeLight("#ffd08a"), kGizmo("#e6c84a");

QPointF projectView(const ViewCam &cam, const QRectF &r, const QVector3D &c) {
    const double d = -c.z();
    double nx, ny;
    if (cam.ortho) {
        nx = c.x() / (cam.orthoScale / 2);
        ny = c.y() / (cam.orthoScale / 2) * cam.aspect;
    } else {
        nx = c.x() / d * cam.f;
        ny = c.y() / d * cam.f * cam.aspect;
    }
    return {r.left() + (nx * 0.5 + 0.5) * r.width(), r.top() + (0.5 - ny * 0.5) * r.height()};
}

double segDist(const QPointF &p, const QPointF &a, const QPointF &b) {
    const QPointF ab = b - a;
    const double l2 = ab.x() * ab.x() + ab.y() * ab.y();
    double t = l2 < 1e-9 ? 0 : ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / l2;
    t = qBound(0.0, t, 1.0);
    const QPointF c = a + ab * t;
    return QLineF(p, c).length();
}

QVector3D axisOf(int a) { return a == 0 ? QVector3D(1, 0, 0) : a == 1 ? QVector3D(0, 1, 0) : QVector3D(0, 0, 1); }
} // namespace

BuildCanvas::BuildCanvas(BuildModel *m, Library *lib, QWidget *parent) : QWidget(parent), m_m(m), m_lib(lib) {
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(380, 240);
    auto loadGuides=[this]{
        m_verticalGuides.clear();m_horizontalGuides.clear();m_dragGuide=-1;
        if(const auto *camera=m_m->design.camera()){
            auto load=[](const QJsonArray &items,QVector<double> &values){for(auto item:items){const double value=item.toDouble(-1);if(value>=0&&value<=1)values.append(value);}};
            load(camera->style["editor.guides.vertical"].toArray(),m_verticalGuides);
            load(camera->style["editor.guides.horizontal"].toArray(),m_horizontalGuides);
        }
        update();
    };
    connect(m, &BuildModel::changed, this, loadGuides);loadGuides();
    connect(lib,&Library::displayChanged,this,qOverload<>(&QWidget::update));
    connect(m, &BuildModel::timeChanged, this, [this] { update(); });
    connect(m, &BuildModel::selectionChanged, this, [this] { update(); });
    connect(m, &BuildModel::modeChanged, this, [this] {
        if (m_loopCut) cancelModal();
        update();
    });
}

void BuildCanvas::dragEnterEvent(QDragEnterEvent *event){
    if(!modalActive()&&event->mimeData()->hasFormat(AudioLibrary::mimeType)&&!AudioLibrary::asset(*m_lib,QString::fromUtf8(event->mimeData()->data(AudioLibrary::mimeType))).isEmpty()){event->acceptProposedAction();return;}
    if(!modalActive()&&!m_m->editMode&&event->mimeData()->hasFormat(TextureLibrary::mimeType)&&
       !TextureLibrary::entry(*m_lib,QString::fromUtf8(event->mimeData()->data(TextureLibrary::mimeType))).isEmpty())event->acceptProposedAction();
}
void BuildCanvas::dragMoveEvent(QDragMoveEvent *event){
    if(!modalActive()&&event->mimeData()->hasFormat(AudioLibrary::mimeType)){event->acceptProposedAction();return;}
    const auto *object=m_m->byId(pickObject(event->position()));
    if(!modalActive()&&!m_m->editMode&&event->mimeData()->hasFormat(TextureLibrary::mimeType)&&object&&!object->locked&&TextureLibrary::compatible(*object))event->acceptProposedAction();else event->ignore();
}
void BuildCanvas::dropEvent(QDropEvent *event){
    if(!modalActive()&&event->mimeData()->hasFormat(AudioLibrary::mimeType)){const auto id=AudioLibrary::addToScene(*m_m,*m_lib,QString::fromUtf8(event->mimeData()->data(AudioLibrary::mimeType)));if(!id.isEmpty()){event->acceptProposedAction();ToastHost::notify(this,"audio","Audio added successfully",m_m->byId(id)->name);emit message("Audio added successfully {"+m_m->byId(id)->name+"}");}return;}
    if(modalActive()||m_m->editMode||!event->mimeData()->hasFormat(TextureLibrary::mimeType))return;
    const auto target=pickObject(event->position());if(target.isEmpty())return;
    QString error;if(TextureLibrary::apply(*m_m,*m_lib,QString::fromUtf8(event->mimeData()->data(TextureLibrary::mimeType)),target,&error)){
        event->acceptProposedAction();emit message("Texture applied · view in Material or Final mode");
    }else emit message(error);
}

// ------------------------------------------------------------------ view

ViewCam BuildCanvas::userCam() const {
    // Kept as a compatibility helper for older code paths. The primary Build
    // workspace never orbits this camera; it always uses the scene camera.
    return m_m->design.sceneCamera(m_m->t, double(width()) / qMax(1, height()));
}

QRectF BuildCanvas::cameraFrame() const {
    const double asp = m_lib->display.aspect();
    const QRectF avail = QRectF(rect()).adjusted(34, 34, -34, -34);
    double w = avail.width(), h = w / asp;
    if (h > avail.height()) {
        h = avail.height();
        w = h * asp;
    }
    return QRectF(avail.center().x() - w / 2, avail.center().y() - h / 2, w, h);
}

ViewCam BuildCanvas::activeCam(QRectF *r) const {
    QRectF base = cameraFrame();
    const QPointF c = base.center() + m_editorPan;
    const double z = qBound(0.02, m_editorZoom, 64.0);
    *r = QRectF(c.x() - base.width() * z / 2, c.y() - base.height() * z / 2, base.width() * z, base.height() * z);
    // The real broadcast camera is always used. Editor zoom/pan only changes
    // the screen rectangle used to display that camera's composition.
    return m_m->design.sceneCamera(m_m->t, base.width() / qMax(1.0, base.height()));
}

bool BuildCanvas::projectWorld(const ViewCam &cam, const QRectF &r, const QVector3D &w, QPointF *out, double *depth) const {
    return cam.toScreen(w, r, out, depth);
}

void BuildCanvas::setShading(int s) {
    s=qBound(0,s,3);
    if(m_shading==s)return;
    m_shading=s;
    emit shadingChanged(s);
    update();
}

void BuildCanvas::setCameraView(bool on) {
    Q_UNUSED(on);
    m_cameraView = true;
    m_viewName = "2D Composition  ·  Fixed Camera";
    update();
}

void BuildCanvas::viewFront() { m_cameraView = true; m_viewName = "2D Composition  ·  Fixed Camera"; update(); }
void BuildCanvas::viewRight() { m_cameraView = true; m_viewName = "2D Composition  ·  Fixed Camera"; update(); }
void BuildCanvas::viewTop() { m_cameraView = true; m_viewName = "2D Composition  ·  Fixed Camera"; update(); }

void BuildCanvas::toggleOrtho() { m_cameraView = true; update(); }

void BuildCanvas::frameAll() {
    m_editorZoom = 1.0;
    m_editorPan = {0, 0};
    update();
}

QRectF BuildCanvas::objectBounds(const DObject &source, const ViewCam &cam, const QRectF &frame) const {
    const DObject o = source.evaluated(m_m->t);
    const auto world = m_m->design.worldMatrix(o, m_m->t);
    QVector<QVector3D> points;
    if (o.type == "Mesh") points = m_m->design.evalMesh(o,m_m->t).v;
    else if (o.isRenderable()) points = {{float(-o.w/2),float(-o.h/2),0},{float(o.w/2),float(-o.h/2),0},{float(o.w/2),float(o.h/2),0},{float(-o.w/2),float(o.h/2),0}};
    else points = {{-.05f,0,-.05f},{.05f,0,.05f}};
    double left=1e100,top=1e100,right=-1e100,bottom=-1e100;
    for (const auto &v:points) { QPointF q; if (!cam.toScreen(world.map(v),frame,&q)) continue;left=qMin(left,q.x());top=qMin(top,q.y());right=qMax(right,q.x());bottom=qMax(bottom,q.y()); }
    return right>=left ? QRectF(QPointF(left,top),QPointF(right,bottom)) : QRectF();
}
QRectF BuildCanvas::selectionBounds(const ViewCam &cam,const QRectF &frame) const {
    QRectF result;
    for (const auto &o:m_m->design.objects) if(o.type!="Audio"&&m_m->sel.contains(o.id)&&o.visible&&!o.locked)result=result.united(objectBounds(o,cam,frame));
    return result;
}
void BuildCanvas::frameSelected() {
    QRectF frame;auto cam=activeCam(&frame);auto bounds=selectionBounds(cam,frame);
    if(bounds.isEmpty())return;
    double factor=qMin((width()-140)/qMax(1.0,bounds.width()),(height()-140)/qMax(1.0,bounds.height()));
    const double zoom=qBound(.02,m_editorZoom*factor,64.0);factor=zoom/m_editorZoom;
    m_editorPan=(frame.center()-bounds.center())*factor;
    m_editorZoom=zoom;update();
}
void BuildCanvas::actualSize(){m_editorZoom=qBound(.02,double(m_lib->display.resX())/cameraFrame().width(),64.0);m_editorPan={};update();}

void BuildCanvas::alignSelection(const QString &operation){
    auto objects=m_m->selected();objects.removeIf([](auto *o){return o->locked||!o->isRenderable();});
    if(objects.isEmpty())return;
    const bool horizontal=operation.contains("Horizontal")||operation=="Left"||operation=="Right";
    const bool distribute=operation.startsWith("Distribute");
    if(distribute&&objects.size()<3)return;
    QRectF frame;auto cam=activeCam(&frame);QRectF total;
    for(auto *o:objects)total=total.united(objectBounds(*o,cam,frame));
    if(objects.size()==1)total=frame;
    auto coordinate=[&](DObject *o){auto r=objectBounds(*o,cam,frame);return horizontal?r.center().x():r.center().y();};
    if(distribute)std::sort(objects.begin(),objects.end(),[&](auto *a,auto *b){return coordinate(a)<coordinate(b);});
    const double first=coordinate(objects.first()),last=coordinate(objects.last());
    m_m->pushUndo();
    for(int i=0;i<objects.size();++i){
        auto *o=objects[i];auto r=objectBounds(*o,cam,frame);QPointF delta;
        if(distribute){double d=first+(last-first)*i/(objects.size()-1)-coordinate(o);delta=horizontal?QPointF(d,0):QPointF(0,d);}
        else if(operation=="Left")delta.setX(total.left()-r.left());
        else if(operation=="Right")delta.setX(total.right()-r.right());
        else if(operation=="Top")delta.setY(total.top()-r.top());
        else if(operation=="Bottom")delta.setY(total.bottom()-r.bottom());
        else if(horizontal)delta.setX(total.center().x()-r.center().x());
        else delta.setY(total.center().y()-r.center().y());
        DProps p=o->at(m_m->t);const auto basis=m_m->design.parentMatrix(*o,m_m->t);const auto world=basis.map(p.loc);QPointF origin;cam.toScreen(world,frame,&origin);bool ok=false;
        p.loc=basis.inverted().map(rayPlane(origin+delta,world,cam.forward(),&ok));if(ok)m_m->setProps(o->id,p);
    }
    m_m->notify();
}

void BuildCanvas::resizeEvent(QResizeEvent *) { update(); }

void BuildCanvas::enterEvent(QEnterEvent *) {
    QWidget *f = QApplication::focusWidget();
    if (!f || !(qobject_cast<QLineEdit *>(f) || qobject_cast<QAbstractSpinBox *>(f) || qobject_cast<QPlainTextEdit *>(f))) setFocus();
}

void BuildCanvas::leaveEvent(QEvent *) {
    m_hasHover = false;
    update();
}

// ------------------------------------------------------------------ painting helpers

void BuildCanvas::drawLine3D(QPainter &p, const ViewCam &cam, const QRectF &r, const QVector3D &A, const QVector3D &B) const {
    QVector3D a = cam.view.map(A), b = cam.view.map(B);
    if (!cam.ortho) {
        const bool ia = -a.z() >= cam.nearClip, ib = -b.z() >= cam.nearClip;
        if (!ia && !ib) return;
        if (!ia) a = a + (b - a) * float((-cam.nearClip - a.z()) / (b.z() - a.z()));
        if (!ib) b = b + (a - b) * float((-cam.nearClip - b.z()) / (a.z() - b.z()));
    }
    p.drawLine(projectView(cam, r, a), projectView(cam, r, b));
}

void BuildCanvas::drawGrid(QPainter &p, const ViewCam &cam, const QRectF &r) const {
    double step = 1;
    if (m_editorZoom < 0.5) step = 5;
    if (m_editorZoom < 0.2) step = 25;
    const int N = 20;
    const double ext = N * step;
    if (showGrid) {
        for (int i = -N; i <= N; ++i) {
            if (i == 0) continue;
            p.setPen(QPen(QColor(255, 255, 255, i % 5 == 0 ? 42 : 20), 0));
            drawLine3D(p, cam, r, QVector3D(float(i * step), float(-ext), 0), QVector3D(float(i * step), float(ext), 0));
            drawLine3D(p, cam, r, QVector3D(float(-ext), float(i * step), 0), QVector3D(float(ext), float(i * step), 0));
        }
    }
    if (showAxes) {
        p.setPen(QPen(QColor(226, 72, 72, 200), 1.3));
        drawLine3D(p, cam, r, QVector3D(float(-ext), 0, 0), QVector3D(float(ext), 0, 0));
        p.setPen(QPen(QColor(110, 205, 90, 200), 1.3));
        drawLine3D(p, cam, r, QVector3D(0, float(-ext), 0), QVector3D(0, float(ext), 0));
        if (m_cameraView) {
            p.setPen(QPen(QColor(80, 140, 240, 170), 1.3));
            drawLine3D(p, cam, r, QVector3D(0, 0, float(-ext)), QVector3D(0, 0, float(ext)));
        }
    }
}

void BuildCanvas::drawGizmos(QPainter &p, const ViewCam &cam, const QRectF &r) {
    const ElementDesign &d = m_m->design;

    for (const DObject &o : d.objects) {
        if (!o.visible || (o.type != "Light" && o.type != "Camera")) continue;
        if((o.type=="Light"&&!showLights)||(o.type=="Camera"&&!showCamera))continue;
        const bool isSel = m_m->sel.contains(o.id), isAct = o.id == m_m->activeId;
        const QColor col = isAct ? kOrangeLight : isSel ? kOrange : (o.type == "Camera" ? QColor(210, 214, 222) : kGizmo);
        const QMatrix4x4 W = d.worldMatrix(o, m_m->t);
        const QVector3D pos = W.map(QVector3D(0, 0, 0));
        p.setPen(QPen(col, isSel ? 2.0 : 1.4));
        p.setBrush(Qt::NoBrush);
        if (o.type == "Light") {
            QPointF c;
            if (!cam.toScreen(pos, r, &c)) continue;
            p.drawEllipse(c, 7.0, 7.0);
            p.setBrush(col);
            p.drawEllipse(c, 2.0, 2.0);
            p.setBrush(Qt::NoBrush);
            if (o.light.kind == "Sun") {
                for (int i = 0; i < 8; ++i) {
                    const double a = i * M_PI / 4;
                    p.drawLine(c + QPointF(qCos(a), qSin(a)) * 11, c + QPointF(qCos(a), qSin(a)) * 16);
                }
            }
            if (o.light.kind != "Point") {
                const QVector3D dir = W.mapVector(QVector3D(0, 0, -1)).normalized();
                p.setPen(QPen(col, 1, Qt::DashLine));
                drawLine3D(p, cam, r, pos, pos + dir * 1.8f);
                if (o.light.kind == "Spot") {
                    const float half = float(qDegreesToRadians(o.light.spotSize / 2)), len = 1.8f, rad = len * qTan(half);
                    const QVector3D u = W.mapVector(QVector3D(1, 0, 0)).normalized(), v = W.mapVector(QVector3D(0, 1, 0)).normalized();
                    p.setPen(QPen(col, 1));
                    for (int i = 0; i < 4; ++i) {
                        const float a = float(i * M_PI / 2);
                        drawLine3D(p, cam, r, pos, pos + dir * len + (u * qCos(a) + v * qSin(a)) * rad);
                    }
                }
            }
        } else if (!m_cameraView) {  // the scene camera: a pyramid showing what it sees
            const double asp = m_lib->display.aspect();
            const double dist = 2.0;
            const double hw = o.cam.ortho ? o.cam.orthoScale / 2 * 0.3 : dist * qTan(qDegreesToRadians(o.cam.fov / 2));
            const double hh = hw / asp;
            QVector3D corner[4] = {{float(-hw), float(hh), float(-dist)}, {float(hw), float(hh), float(-dist)},
                                   {float(hw), float(-hh), float(-dist)}, {float(-hw), float(-hh), float(-dist)}};
            QVector3D wc[4];
            for (int i = 0; i < 4; ++i) wc[i] = W.map(corner[i]);
            for (int i = 0; i < 4; ++i) {
                drawLine3D(p, cam, r, pos, wc[i]);
                drawLine3D(p, cam, r, wc[i], wc[(i + 1) % 4]);
            }
            const QVector3D top = W.map(QVector3D(0, float(hh * 1.4), float(-dist)));  // the "up" triangle
            drawLine3D(p, cam, r, wc[0], top);
            drawLine3D(p, cam, r, wc[1], top);
        }
    }
}

void BuildCanvas::drawSelection(QPainter &p,const ViewCam &cam,const QRectF &r){
    p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#60a5fa"),1));
    for(const auto &o:m_m->design.objects)if(o.type!="Audio"&&m_m->sel.contains(o.id)&&o.visible)p.drawRect(objectBounds(o,cam,r));
    const auto box=selectionBounds(cam,r);if(box.isEmpty())return;
    const QPointF handles[]={box.topLeft(),box.topRight(),box.bottomLeft(),box.bottomRight(),QPointF(box.center().x(),box.top()),QPointF(box.center().x(),box.bottom()),QPointF(box.left(),box.center().y()),QPointF(box.right(),box.center().y())};
    p.setBrush(QColor("#292929"));
    for(auto point:handles)p.drawRect(QRectF(point-QPointF(3,3),QSizeF(6,6)));
    auto top=QPointF(box.center().x(),box.top());p.drawLine(top,top-QPointF(0,24));p.drawEllipse(top-QPointF(0,24),4,4);
}

QMatrix4x4 BuildCanvas::editWorld() const {
    DObject *o = const_cast<BuildModel *>(m_m)->active();
    return o ? m_m->design.worldMatrix(*o,m_m->t) : QMatrix4x4();
}

void BuildCanvas::drawEdit(QPainter &p, const ViewCam &cam, const QRectF &r) {
    Mesh *mesh = m_m->editMesh();
    if (!mesh) return;
    const QMatrix4x4 W = editWorld();
    const int n = mesh->v.size();
    QVector<QPointF> pt(n);
    QVector<char> ok(n);
    for (int i = 0; i < n; ++i) ok[i] = cam.toScreen(W.map(mesh->v[i]), r, &pt[i]);
    const EditSel &s = m_m->esel;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 159, 67, 80));
    for (int fi : s.f) {
        if (fi >= mesh->f.size()) continue;
        QPolygonF poly;
        bool all = true;
        for (int i : mesh->f[fi]) {
            all = all && ok[i];
            poly << pt[i];
        }
        if (all) p.drawPolygon(poly);
    }
    p.setBrush(Qt::NoBrush);
    for (EdgeKey k : mesh->edges()) {
        const int a = ekA(k), b = ekB(k);
        if (!ok[a] || !ok[b]) continue;
        const bool sel = s.e.contains(k);
        p.setPen(QPen(sel ? kOrange : QColor(18, 22, 30, 215), sel ? 2.2 : 1.0));
        p.drawLine(pt[a], pt[b]);
    }
    if (m_m->selMode == SelMode::Face) {
        for (int fi = 0; fi < mesh->f.size(); ++fi) {
            QPointF c(0, 0);
            bool all = true;
            for (int i : mesh->f[fi]) {
                all = all && ok[i];
                c += pt[i];
            }
            if (!all) continue;
            c /= mesh->f[fi].size();
            const bool sel = s.f.contains(fi);
            p.setPen(Qt::NoPen);
            p.setBrush(sel ? kOrange : QColor(18, 22, 30, 215));
            p.drawEllipse(c, sel ? 3.5 : 2.2, sel ? 3.5 : 2.2);
        }
    } else {
        for (int i = 0; i < n; ++i) {
            if (!ok[i]) continue;
            const bool sel = s.v.contains(i);
            p.setPen(Qt::NoPen);
            p.setBrush(sel ? kOrange : QColor(18, 22, 30, 230));
            p.drawEllipse(pt[i], sel ? 4.0 : 2.6, sel ? 4.0 : 2.6);
        }
    }
    // the loop a Ctrl+R cut would make
    if (m_loopCut && m_loopValid) {
        const QSet<EdgeKey> ring(m_loopRing.begin(), m_loopRing.end());
        p.setPen(QPen(QColor("#facc15"), 2.2));
        for (const QVector<int> &L : mesh->f) {
            if (L.size() != 4) continue;
            for (int q = 0; q < 2; ++q) {
                const EdgeKey e1 = ekey(L[q], L[q + 1]), e2 = ekey(L[q + 2], L[(q + 3) % 4]);
                if (!ring.contains(e1) || !ring.contains(e2)) continue;
                for (int c = 1; c <= m_cuts; ++c) {
                    const float f = float(c) / (m_cuts + 1);
                    const QVector3D a = mesh->v[L[q]] + (mesh->v[L[q + 1]] - mesh->v[L[q]]) * f;
                    const QVector3D b = mesh->v[L[(q + 3) % 4]] + (mesh->v[L[q + 2]] - mesh->v[L[(q + 3) % 4]]) * f;
                    drawLine3D(p, cam, r, W.map(a), W.map(b));
                }
                break;
            }
        }
    }
}

void BuildCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#292929"));

    QRectF r;
    const ViewCam cam = activeCam(&r);
    const ElementDesign &d = m_m->design;

    // Work/material views retain the off-camera workspace. Only the camera frame
    // is exported; the dimmed surround helps position incoming/outgoing objects.
    if(m_cameraView && m_shading!=Rendered && !r.contains(QRectF(rect()))){
        const double factor=qMin(1.,qSqrt(2073600./qMax(1.,double(width())*height())));
        QImage surround(QSize(qMax(2,int(width()*factor)),qMax(2,int(height()*factor))),QImage::Format_ARGB32_Premultiplied);surround.fill(Qt::transparent);
        if(!surround.isNull()){
            QPainter editor(&surround);PaintOpts options;options.shade=PaintOpts::Shade(m_shading);options.clip=false;options.background=false;
            d.paint(editor,QRectF(r.topLeft()*factor,r.size()*factor),cam,m_m->t,options);editor.end();
            QPainterPath outside;outside.addRect(rect());QPainterPath camera;camera.addRect(r);outside=outside.subtracted(camera);
            p.save();p.setClipPath(outside);p.drawImage(rect(),surround);p.fillRect(rect(),QColor(0,0,0,165));p.restore();
        }
    }

    // Checker-like dark editor surround, then the real fixed-camera composition.
    p.save();
    p.setClipRect(r);
    p.fillRect(r,QColor("#393939"));
    // Render into an image so backdrop effects can sample already drawn pixels.
    // Bound the buffer independently of editor zoom; previews never change output.
    const QString quality=property("previewQuality").toString();
    const double scale=quality=="Quarter"?.25:quality=="Half"?.5:
        (quality.isEmpty()||quality=="Auto")&&(m_m->playing||modalActive())?.5:1.;
    QSizeF requested=r.size()*devicePixelRatioF()*scale;
    const double limit=2073600.;
    if(requested.width()*requested.height()>limit)requested*=qSqrt(limit/(requested.width()*requested.height()));
    const QSize previewSize(qMax(2,int(requested.width())),qMax(2,int(requested.height())));
    setProperty("previewPixelSize",previewSize);
    QElapsedTimer renderClock;renderClock.start();
    QImage frame(previewSize,QImage::Format_ARGB32_Premultiplied);frame.fill(Qt::transparent);
    if(!frame.isNull()){
        if(m_shading==Rendered&&sceneEffect(d))frame=sceneEffectPreview(d,previewSize,m_m->t);
        else{
            QPainter render(&frame);render.setRenderHint(QPainter::Antialiasing);render.setRenderHint(QPainter::SmoothPixmapTransform);
            if(m_shading==Rendered){
                const auto &display=m_lib->display;RenderSettings settings;
                settings.motionBlur=display.motionBlur;settings.shutter=display.shutter;settings.samples=display.mbSamples;
                settings.frameMs=1000.*display.fpsBase/qMax(1,display.fps);
                d.render(render,frame.rect(),m_m->t,true,&settings);
            }else{PaintOpts options;options.shade=PaintOpts::Shade(m_shading);d.paint(render,frame.rect(),cam,m_m->t,options);}
        }
        const auto &display=m_lib->display;
        if(m_shading==Rendered&&colorManagementActive(display.colorView,display.colorLook,display.exposure,display.gamma))
            applyColorManagement(frame,display.colorView,display.colorLook,display.exposure,display.gamma);
        p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(r,frame);
    }
    setProperty("previewRenderMs",renderClock.elapsed());
    p.restore();

    // Broadcast-safe overlays are editor-only.
    if(!showOverlays)return;
    p.setPen(QPen(QColor(255,255,255,90), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);
    const auto guideStyle=d.camera()?d.camera()->style:QJsonObject{};
    const double titleMargin=qBound(0.,guideStyle["editor.titleSafe"].toDouble(.05),.45),actionMargin=qBound(0.,guideStyle["editor.actionSafe"].toDouble(.035),.45);
    const QRectF titleSafe = r.adjusted(r.width()*titleMargin, r.height()*titleMargin, -r.width()*titleMargin, -r.height()*titleMargin);
    const QRectF actionSafe = r.adjusted(r.width()*actionMargin, r.height()*actionMargin, -r.width()*actionMargin, -r.height()*actionMargin);
    p.setPen(QPen(QColor(96,165,250,45), 1, Qt::DashLine));
    if(showActionSafe) p.drawRect(actionSafe);
    p.setPen(QPen(QColor(250,204,21,38), 1, Qt::DashLine));
    if(showTitleSafe) p.drawRect(titleSafe);
    p.setPen(QPen(QColor(148,163,184,28), 1));
    if(showAxes) p.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x(), r.bottom()));
    if(showAxes) p.drawLine(QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y()));

    p.save();p.setClipRect(r);
    if(showGrid){
        p.setPen(QColor(148,163,184,25));
        const double step=qMax(12.,qBound(1.,guideStyle["editor.gridSpacing"].toDouble(120),4096.)*r.width()/m_lib->display.resX());
        for(double x=r.left();x<r.right();x+=step)p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));
        for(double y=r.top();y<r.bottom();y+=step)p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));
    }
    QColor guideColor(guideStyle["editor.guideColor"].toString("#22d3ee"));if(!guideColor.isValid())guideColor=QColor("#22d3ee");
    p.setPen(QPen(guideColor,1,Qt::DashLine));
    for(double guide:m_verticalGuides){double x=r.left()+guide*r.width();p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));}
    for(double guide:m_horizontalGuides){double y=r.top()+guide*r.height();p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));}
    p.restore();
    if(showRulers){
        p.fillRect(QRectF(0,0,width(),20),palette().color(QPalette::Window));p.fillRect(QRectF(0,0,20,height()),palette().color(QPalette::Window));
        QFont f=font();f.setPixelSize(9);p.setFont(f);p.setPen(palette().color(QPalette::PlaceholderText));
        const double sx=r.width()/qMax(1,m_lib->display.resX()),sy=r.height()/qMax(1,m_lib->display.resY());
        auto stepFor=[](double scale){const double raw=72./qMax(.0001,scale),base=qPow(10.,qFloor(qLn(raw)/qLn(10.)));for(double multiple:{1.,2.,5.,10.})if(base*multiple>=raw)return base*multiple;return base*10.;};
        const double stepX=stepFor(sx),stepY=stepFor(sy);setProperty("rulerSpacing",QPointF(stepX*sx,stepY*sy));
        p.save();p.setClipRect(QRectF(20,0,qMax(0,width()-20),20));
        for(double value=qCeil((20-r.left())/sx/stepX)*stepX;value<=(width()-r.left())/sx;value+=stepX){double x=r.left()+value*sx;p.drawLine(QPointF(x,14),QPointF(x,20));p.drawText(QPointF(x+3,11),QString::number(qRound64(value)));for(int n=1;n<5;++n){const double tick=x+n*stepX*sx/5;p.drawLine(QPointF(tick,17),QPointF(tick,20));}}
        p.restore();p.save();p.setClipRect(QRectF(0,20,20,qMax(0,height()-20)));
        for(double value=qCeil((20-r.top())/sy/stepY)*stepY;value<=(height()-r.top())/sy;value+=stepY){double y=r.top()+value*sy;p.drawLine(QPointF(14,y),QPointF(20,y));p.save();p.translate(10,y-3);p.rotate(-90);p.drawText(0,0,QString::number(qRound64(value)));p.restore();for(int n=1;n<5;++n){const double tick=y+n*stepY*sy/5;p.drawLine(QPointF(17,tick),QPointF(20,tick));}}
        p.restore();
    }
    setProperty("compositionFrame",r);
    if(m_modal.kind==Modal::Move){
        p.setPen(QPen(QColor("#c084fc"),1,Qt::DashLine));
        if(m_modal.snapX)p.drawLine(QPointF(m_modal.snapPoint.x(),r.top()),QPointF(m_modal.snapPoint.x(),r.bottom()));
        if(m_modal.snapY)p.drawLine(QPointF(r.left(),m_modal.snapPoint.y()),QPointF(r.right(),m_modal.snapPoint.y()));
    }
    if(showOrigins){
        for(const auto &object:d.objects){
            if(!object.isRenderable()||!d.effectiveVisible(object))continue;
            QPointF at;if(!cam.toScreen(d.worldMatrix(object,m_m->t).map(QVector3D()),r,&at))continue;
            p.setPen(QPen(QColor("#20242a"),1));p.setBrush(object.id==m_m->activeId?QColor("#ffc46b"):m_m->isSelected(object.id)?QColor("#e29657"):QColor("#939ba6"));p.drawEllipse(at,3,3);
        }
    }
    if (m_m->editMode) drawEdit(p, cam, r);
    else drawSelection(p, cam, r);
    if (showGizmos) drawGizmos(p, cam, r);

    if (m_boxing) {
        p.setPen(QPen(QColor(96,165,250,220), 1, Qt::DashLine));
        p.setBrush(QColor(96,165,250,28));
        p.drawRect(QRectF(m_boxStart, m_boxEnd).normalized());
    }
    p.setPen(QPen(QColor("#3a3a3a"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5,0.5,-0.5,-0.5),10,10);
    QPointF cursorPoint;if(showCursor&&cam.toScreen(d.cursor,r,&cursorPoint)){
        p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#eeeeee"),1));p.drawEllipse(cursorPoint,7,7);p.setPen(QPen(QColor("#d65e5e"),1.4));p.drawArc(QRectF(cursorPoint-QPointF(7,7),QSizeF(14,14)),0,180*16);p.drawLine(cursorPoint-QPointF(12,0),cursorPoint+QPointF(12,0));p.drawLine(cursorPoint-QPointF(0,12),cursorPoint+QPointF(0,12));
    }
    if(m_drawingShape){p.setPen(QPen(QColor("#71c4ff"),1.5));p.setBrush(QColor(113,196,255,50));if(m_drawShape=="Ellipse")p.drawEllipse(m_drawRect);else p.drawRect(m_drawRect);}
    drawHud(p, QRectF(rect()));
}

void BuildCanvas::drawHud(QPainter &p, const QRectF &r) {
    QFont f = font();
    f.setPixelSize(12);
    p.setFont(f);
    auto text = [&](const QRectF &box, int flags, const QString &t, const QColor &c) {
        p.setPen(QColor(0, 0, 0, 170));
        p.drawText(box.translated(1, 1), flags, t);
        p.setPen(c);
        p.drawText(box, flags, t);
    };
    const DObject *a = const_cast<BuildModel *>(m_m)->active();
    QString mode = m_m->editMode ? QString("Edit Mode  ·  %1").arg(m_m->selMode == SelMode::Vertex ? "Vertex" : m_m->selMode == SelMode::Edge ? "Edge" : "Face")
                                 : "Object Mode";
    const bool cameraLocked=!m_m->design.camera() || m_m->design.camera()->locked;
    text(r.adjusted(28, 24, -14, 0), Qt::AlignLeft | Qt::AlignTop, QString("Broadcast %1 × %2  ·  %3  ·  %4").arg(m_lib->display.resX()).arg(m_lib->display.resY()).arg(cameraLocked?"Camera locked":"Camera editing enabled",mode), QColor(230, 236, 245, 220));
    text(r.adjusted(28, 42, -14, 0), Qt::AlignLeft | Qt::AlignTop,
         a ? QString("(%1)  %2").arg(m_m->t / 1000.0, 0, 'f', 2).arg(a->name) : QString("(%1)").arg(m_m->t / 1000.0, 0, 'f', 2), QColor(255, 208, 138, 230));
    if (m_m->editMode && m_m->editMesh()) {
        const Mesh *m = m_m->editMesh();
        text(r.adjusted(14, 46, -14, 0), Qt::AlignLeft | Qt::AlignTop,
             QString("Verts %1/%2   Edges %3   Faces %4/%5").arg(m_m->esel.v.size()).arg(m->v.size()).arg(m_m->esel.e.size()).arg(m_m->esel.f.size()).arg(m->f.size()),
             QColor(190, 200, 214, 200));
    }

    QString hint;
    if (m_modal.kind != Modal::None) {
        const char *names[] = {"", "Move", "Rotate", "Scale", "Inset"};
        const char *ax[] = {"free", "X", "Y", "Z", "normal"};
        hint = QString("%1   axis: %2   %3   ·   X Y Z constrain  ·  type a number  ·  Enter / click confirm  ·  Esc cancel")
                   .arg(names[m_modal.kind], ax[m_modal.axis + 1], m_modal.numeric.isEmpty() ? QString() : "value: " + m_modal.numeric);
    } else if (m_loopCut) {
        hint = QString("Loop Cut   cuts: %1   ·   wheel changes the number of cuts  ·  click to cut  ·  Esc cancel").arg(m_cuts);
    } else if (m_m->editMode) {
        hint = "G/M move  R rotate  S scale  E extrude  I inset  Ctrl+R loop cut  W subdivide  F fill  Shift+F flatten  X delete  Alt+M merge  P separate  Y split  1 2 3 modes  Tab leave";
    } else {
        hint = "G/M move  R rotate  S scale  Shift+D duplicate  Tab edit  Shift+A add  K keyframe  Space play  middle/right-drag pan  wheel zoom";
    }
    emit shortcutHint(hint);
}

// ------------------------------------------------------------------ picking

QString BuildCanvas::pickObject(const QPointF &pos, bool cycle) {
    QRectF r;
    const ViewCam cam = activeCam(&r);
    const ElementDesign &d = m_m->design;
    QVector<QPair<double,QString>> hits;
    QString best;
    double bestDepth = 1e18;
    for (const DObject &source : d.objects) {
        best.clear();bestDepth=1e18;
        const DObject o = source.evaluated(m_m->t);
        if (!d.effectiveVisible(o) || o.locked || !d.overflowClip(o,m_m->t,cam,r,m_shading==Rendered?r:QRectF(rect())).contains(pos)) continue;
        const QMatrix4x4 W = d.worldMatrix(o, m_m->t);
        if (o.type == "Light" || o.type == "Camera") {
            QPointF c;
            double depth;
            if (!showOverlays || !showGizmos || (o.type=="Light"&&!showLights) || (o.type=="Camera"&&!showCamera) || (o.type == "Camera" && m_cameraView) || !cam.toScreen(W.map(QVector3D(0, 0, 0)), r, &c, &depth)) continue;
            if (QLineF(c, pos).length() <= 13 && depth - 0.5 < bestDepth) {
                bestDepth = depth - 0.5;  // gizmos win over what is behind them
                best = o.id;
            }
            if(!best.isEmpty())hits.prepend({bestDepth,best});
            continue;
        }
        if (o.type == "Mesh") {
            const Mesh &mesh = d.evalMesh(o, m_m->t);
            QVector<QPointF> pt(mesh.v.size());
            QVector<double> dep(mesh.v.size());
            QVector<char> ok(mesh.v.size());
            for (int i = 0; i < mesh.v.size(); ++i) ok[i] = cam.toScreen(W.map(mesh.v[i]), r, &pt[i], &dep[i]);
            for (const QVector<int> &L : mesh.f) {
                QPolygonF poly;
                double dsum = 0;
                bool all = true;
                for (int i : L) {
                    all = all && ok[i];
                    poly << pt[i];
                    dsum += dep[i];
                }
                if (!all || !poly.containsPoint(pos, Qt::OddEvenFill)) continue;
                const double dv = dsum / L.size();
                if (dv <= bestDepth) {
                    bestDepth = dv;
                    best = o.id;
                }
            }
        } else if (o.isRenderable()) {
            QPolygonF poly;
            double dsum = 0;
            bool all = true;
            const QVector3D c[4] = {{float(-o.w / 2), float(o.h / 2), 0}, {float(o.w / 2), float(o.h / 2), 0}, {float(o.w / 2), float(-o.h / 2), 0}, {float(-o.w / 2), float(-o.h / 2), 0}};
            for (const QVector3D &q : c) {
                QPointF s;
                double dd;
                all = all && cam.toScreen(W.map(q), r, &s, &dd);
                poly << s;
                dsum += dd;
            }
            if (all && poly.containsPoint(pos, Qt::OddEvenFill) && dsum / 4 <= bestDepth) {
                bestDepth = dsum / 4;
                best = o.id;
            }
        }
        if(!best.isEmpty())hits.prepend({bestDepth,best});
    }
    std::stable_sort(hits.begin(),hits.end(),[&d](const auto &a,const auto &b){const int az=d.find(a.second)->style["zIndex"].toInt(),bz=d.find(b.second)->style["zIndex"].toInt();return az!=bz?az>bz:a.first<b.first;});
    if(hits.isEmpty())return {};
    best=hits.first().second;
    if(cycle){if(QLineF(pos,m_lastPick).length()<4){for(int i=0;i<hits.size();++i)if(hits[i].second==m_lastPicked){best=hits[(i+1)%hits.size()].second;break;}}m_lastPick=pos;m_lastPicked=best;}
    return best;
}

bool BuildCanvas::pickEditElement(const QPointF &pos, bool additive) {
    Mesh *mesh = m_m->editMesh();
    if (!mesh) return false;
    QRectF r;
    const ViewCam cam = activeCam(&r);
    const QMatrix4x4 W = editWorld();
    const int n = mesh->v.size();
    QVector<QPointF> pt(n);
    QVector<double> dep(n);
    QVector<char> ok(n);
    for (int i = 0; i < n; ++i) ok[i] = cam.toScreen(W.map(mesh->v[i]), r, &pt[i], &dep[i]);
    EditSel &s = m_m->esel;
    bool hit = false;
    if (m_m->selMode == SelMode::Vertex) {
        int best = -1;
        double bd = 13, bz = 1e18;
        for (int i = 0; i < n; ++i) {
            if (!ok[i]) continue;
            const double d = QLineF(pt[i], pos).length();
            if (d < bd - 1.0 || (d <= bd + 1.0 && dep[i] < bz && d < 13)) {
                bd = qMin(bd, d);
                bz = dep[i];
                best = i;
            }
        }
        if (best >= 0) {
            hit = true;
            if (additive) { if (s.v.contains(best)) s.v.remove(best); else s.v.insert(best); }
            else { s.clear(); s.v.insert(best); }
        }
    } else if (m_m->selMode == SelMode::Edge) {
        EdgeKey best = 0;
        double bd = 9;
        bool found = false;
        for (EdgeKey k : mesh->edges()) {
            if (!ok[ekA(k)] || !ok[ekB(k)]) continue;
            const double d = segDist(pos, pt[ekA(k)], pt[ekB(k)]);
            if (d < bd) { bd = d; best = k; found = true; }
        }
        if (found) {
            hit = true;
            if (additive) { if (s.e.contains(best)) s.e.remove(best); else s.e.insert(best); }
            else { s.clear(); s.e.insert(best); }
        }
    } else {
        int best = -1;
        double bz = 1e18;
        for (int fi = 0; fi < mesh->f.size(); ++fi) {
            QPolygonF poly;
            double dsum = 0;
            bool all = true;
            for (int i : mesh->f[fi]) {
                all = all && ok[i];
                poly << pt[i];
                dsum += dep[i];
            }
            if (!all || !poly.containsPoint(pos, Qt::OddEvenFill)) continue;
            if (dsum / mesh->f[fi].size() < bz) { bz = dsum / mesh->f[fi].size(); best = fi; }
        }
        if (best >= 0) {
            hit = true;
            if (additive) { if (s.f.contains(best)) s.f.remove(best); else s.f.insert(best); }
            else { s.clear(); s.f.insert(best); }
        }
    }
    if (!hit && !additive) s.clear();
    mesh->normalizeSel(s, m_m->selMode);
    emit m_m->changed();
    return hit;
}

void BuildCanvas::boxSelect(const QRectF &box, bool additive) {
    QRectF r;
    const ViewCam cam = activeCam(&r);
    if (m_m->editMode) {
        Mesh *mesh = m_m->editMesh();
        if (!mesh) return;
        const QMatrix4x4 W = editWorld();
        QSet<int> in;
        for (int i = 0; i < mesh->v.size(); ++i) {
            QPointF q;
            if (cam.toScreen(W.map(mesh->v[i]), r, &q) && box.contains(q)) in.insert(i);
        }
        EditSel &s = m_m->esel;
        if (!additive) s.clear();
        if (m_m->selMode == SelMode::Vertex) {
            s.v += in;
        } else {
            EditSel tmp;
            tmp.v = in;
            mesh->normalizeSel(tmp, SelMode::Vertex);
            if (m_m->selMode == SelMode::Edge) s.e += tmp.e;
            else s.f += tmp.f;
        }
        mesh->normalizeSel(s, m_m->selMode);
        emit m_m->changed();
        return;
    }
    QSet<QString> ids = additive ? m_m->sel : QSet<QString>();
    QString act = m_m->activeId;
    for (const DObject &o : m_m->design.objects) {
        if (!o.visible || o.locked || o.type == "Audio") continue;
        if (box.intersects(objectBounds(o, cam, r))) {
            ids.insert(o.id);
            act = o.id;
        }
    }
    m_m->setSelection(ids, act);
}

// ------------------------------------------------------------------ modal tools

QVector3D BuildCanvas::rayPlane(const QPointF &px, const QVector3D &point, const QVector3D &normal, bool *ok) const {
    QRectF r;
    const ViewCam cam = activeCam(&r);
    QVector3D o, d;
    cam.ray(px, r, &o, &d);
    const float den = QVector3D::dotProduct(normal, d);
    if (qAbs(den) < 1e-6f) {
        *ok = false;
        return point;
    }
    *ok = true;
    return o + d * (QVector3D::dotProduct(normal, point - o) / den);
}

double BuildCanvas::axisParam(const QPointF &px, const QVector3D &P, const QVector3D &A) const {
    QRectF r;
    const ViewCam cam = activeCam(&r);
    QVector3D o, d;
    cam.ray(px, r, &o, &d);
    const QVector3D w0 = P - o;
    const float b = QVector3D::dotProduct(A, d), dd = QVector3D::dotProduct(A, w0), e = QVector3D::dotProduct(d, w0);
    const float den = 1.0f - b * b;
    return qAbs(den) < 1e-5f ? 0.0 : double((b * e - dd) / den);
}

QVector3D BuildCanvas::axisVector() const { return m_modal.axis == 3 ? m_modal.customAxis : axisOf(m_modal.axis); }

bool BuildCanvas::numericValue(double *v) const {
    if (m_modal.numeric.isEmpty() || m_modal.numeric == "-" || m_modal.numeric == ".") return false;
    bool ok = false;
    const double x = m_modal.numeric.toDouble(&ok);
    if (ok) *v = x;
    return ok;
}

void BuildCanvas::startModal(Modal::Kind k, bool fromButton, bool skipUndo) {
    if (m_modal.kind != Modal::None || m_loopCut) return;
    Modal md;
    md.kind = k;
    md.fromMouseButton = fromButton;
    md.startMouse = fromButton ? m_press : (m_hasHover ? m_hover : QPointF(width() / 2.0, height() / 2.0));
    QRectF r;
    const ViewCam cam = activeCam(&r);
    if (m_m->editMode) {
        Mesh *mesh = m_m->editMesh();
        if (!mesh || m_m->esel.v.isEmpty()) {
            emit message("Select something to edit first");
            return;
        }
        if (!skipUndo) m_m->pushUndo();
        md.origVerts = mesh->v;
        md.editVerts = m_m->esel.v;
        md.pivot = editWorld().map(mesh->selCenter(m_m->esel));
    } else {
        auto objs = m_m->selected();
        objs.removeIf([](auto *o){return o->locked||o->type=="Audio";});
        if (objs.isEmpty()) {
            emit message("Select an object first");
            return;
        }
        if (!skipUndo) m_m->pushUndo();
        QVector3D c;
        for (DObject *o : objs) {
            bool inherited=false;auto *ancestor=m_m->byId(o->parentId);QSet<QString> visited;
            while(ancestor&&!visited.contains(ancestor->id)){visited.insert(ancestor->id);if(m_m->sel.contains(ancestor->id)&&!ancestor->locked){inherited=true;break;}ancestor=m_m->byId(ancestor->parentId);}if(inherited)continue;
            md.origProps[o->id] = o->at(m_m->t);
            c += m_m->design.worldMatrix(*o,m_m->t).map(QVector3D());
        }
        md.pivot = c / float(qMax(1,md.origProps.size()));
    }
    QPointF ps;
    md.pivotScreen = cam.toScreen(md.pivot, r, &ps) ? ps : QPointF(width() / 2.0, height() / 2.0);
    md.startAngle = qAtan2(md.startMouse.y() - md.pivotScreen.y(), md.startMouse.x() - md.pivotScreen.x());
    md.moveBounds=selectionBounds(cam,r);
    m_modal = md;
    setFocus();
    if(!fromButton)grabMouse();
    update();
}

void BuildCanvas::startExtrude() {
    if (!m_m->editMode || m_modal.kind != Modal::None) return;
    QVector3D dir;
    if (!m_m->opExtrude(&dir)) {
        emit message("Select faces or edges to extrude");
        return;
    }
    startModal(Modal::Move, false, true);
    if (m_modal.kind == Modal::Move && !dir.isNull()) {
        m_modal.axis = 3;
        m_modal.customAxis = dir;
    }
}

void BuildCanvas::startInset() {
    if (!m_m->editMode || m_modal.kind != Modal::None) return;
    QVector<std::pair<int, QVector3D>> info;
    if (!m_m->opInset(&info)) {
        emit message("Select faces to inset");
        return;
    }
    startModal(Modal::Inset, false, true);
    if (m_modal.kind == Modal::Inset) m_modal.insetInfo = info;
}

void BuildCanvas::startLoopCut() {
    if (!m_m->editMode || m_modal.kind != Modal::None) return;
    m_loopCut = true;
    m_loopValid = false;
    m_cuts = 1;
    setMouseTracking(true);
    updateLoopCutHover(m_hasHover ? m_hover : QPointF(width() / 2.0, height() / 2.0));
    update();
}

void BuildCanvas::updateLoopCutHover(const QPointF &pos) {
    Mesh *mesh = m_m->editMesh();
    m_loopValid = false;
    if (!mesh) return;
    QRectF r;
    const ViewCam cam = activeCam(&r);
    const QMatrix4x4 W = editWorld();
    double bd = 30;
    for (EdgeKey k : mesh->edges()) {
        QPointF a, b;
        if (!cam.toScreen(W.map(mesh->v[ekA(k)]), r, &a) || !cam.toScreen(W.map(mesh->v[ekB(k)]), r, &b)) continue;
        const double d = segDist(pos, a, b);
        if (d < bd) { bd = d; m_loopEdge = k; m_loopValid = true; }
    }
    if (m_loopValid) m_loopRing = mesh->ringOf(m_loopEdge);
}

void BuildCanvas::cancelModal() {
    if (m_loopCut) {
        m_loopCut = false;
        m_loopValid = false;
        update();
        return;
    }
    if (m_modal.kind == Modal::None) return;
    m_modal = Modal();
    if(QWidget::mouseGrabber()==this)releaseMouse();
    m_m->cancelToUndo();
    emit message("Cancelled");
    update();
}

void BuildCanvas::confirmModal() {
    if (m_modal.kind == Modal::None) return;
    m_modal = Modal();
    if(QWidget::mouseGrabber()==this)releaseMouse();
    m_m->notify();
    update();
}

void BuildCanvas::applyModal(const QPointF &mouse) {
    QRectF r;
    const ViewCam cam = activeCam(&r);
    const QVector3D fwd = cam.forward();
    double num = 0;
    const bool hasNum = numericValue(&num);
    const bool edit = m_m->editMode;
    Mesh *mesh = m_m->editMesh();
    const QMatrix4x4 W = edit ? editWorld() : QMatrix4x4();
    const QMatrix4x4 invW = edit ? W.inverted() : QMatrix4x4();

    if (!edit && m_modal.kind == Modal::Scale && m_modal.resizeHandle >= 0) {
        const int handle=m_modal.resizeHandle;
        const bool horizontal=handle<4 || handle>=6;
        const bool vertical=handle<6;
        double fx=horizontal ? (mouse.x()-m_modal.pivotScreen.x())/(m_modal.startMouse.x()-m_modal.pivotScreen.x()) : 1;
        double fy=vertical ? (mouse.y()-m_modal.pivotScreen.y())/(m_modal.startMouse.y()-m_modal.pivotScreen.y()) : 1;
        if(!qIsFinite(fx)||!qIsFinite(fy))return;
        bool keep=QApplication::keyboardModifiers()&Qt::ShiftModifier;
        for(auto it=m_modal.origProps.begin();it!=m_modal.origProps.end();++it)
            if(auto *o=m_m->byId(it.key()))keep=keep||o->style["layout.aspectLock"].toBool();
        if(keep){double f=horizontal?fx:fy;fx=f;fy=f;}
        fx=qMax(.01,fx);fy=qMax(.01,fy);
        const auto viewInverse=cam.view.inverted();
        const auto anchor=cam.view.map(m_modal.pivot);
        for(auto it=m_modal.origProps.begin();it!=m_modal.origProps.end();++it){
            auto *o=m_m->byId(it.key());if(!o)continue;
            auto p=it.value();const auto basis=m_m->design.parentMatrix(*o,m_m->t);auto position=cam.view.map(basis.map(p.loc));
            position.setX(anchor.x()+(position.x()-anchor.x())*float(fx));
            position.setY(anchor.y()+(position.y()-anchor.y())*float(fy));
            p.loc=basis.inverted().map(viewInverse.map(position));
            p.scl.setX(it.value().scl.x()*float(fx));
            if(o->type=="Mesh")p.scl.setZ(it.value().scl.z()*float(fy));
            else p.scl.setY(it.value().scl.y()*float(fy));
            m_m->setProps(it.key(),p);
        }
        m_m->notify();return;
    }

    // how each point moves, in world space
    std::function<QVector3D(const QVector3D &)> xf;
    QMatrix4x4 R;  // for object mode: the matrix that is applied on top of each object

    if (m_modal.kind == Modal::Move) {
        m_modal.snapX=m_modal.snapY=false;
        QVector3D delta;
        if (hasNum) {
            delta = (m_modal.axis < 0 ? axisOf(0) : axisVector()) * float(num);
        } else if (m_modal.axis < 0) {
            bool ok1, ok2;
            const QVector3D a = rayPlane(m_modal.startMouse, m_modal.pivot, fwd, &ok1), b = rayPlane(mouse, m_modal.pivot, fwd, &ok2);
            delta = (ok1 && ok2) ? b - a : QVector3D();
            if(snapping && !edit && !(QApplication::keyboardModifiers()&(Qt::AltModifier|Qt::ControlModifier))){
                QPointF target;cam.toScreen(m_modal.pivot+delta,r,&target);
                QVector<double> xs{r.left(),r.center().x(),r.right()},ys{r.top(),r.center().y(),r.bottom()};
                for(double g:m_verticalGuides)xs.append(r.left()+g*r.width());
                for(double g:m_horizontalGuides)ys.append(r.top()+g*r.height());
                for(const auto &o:m_m->design.objects)if(o.visible&&!m_m->sel.contains(o.id)&&o.isRenderable()){auto box=objectBounds(o,cam,r);xs<<box.left()<<box.center().x()<<box.right();ys<<box.top()<<box.center().y()<<box.bottom();}
                // Match the entire selection's edges and centre, using the gesture's
                // original bounds so snapping never accumulates on repeated moves.
                const auto moving=m_modal.moveBounds.translated(target-m_modal.pivotScreen);
                double dx=7,dy=7,gx=target.x(),gy=target.y();
                for(double x:xs)for(double anchor:{moving.left(),moving.center().x(),moving.right()})if(qAbs(x-anchor)<qAbs(dx)){dx=x-anchor;gx=x;}
                for(double y:ys)for(double anchor:{moving.top(),moving.center().y(),moving.bottom()})if(qAbs(y-anchor)<qAbs(dy)){dy=y-anchor;gy=y;}
                if(qAbs(dx)<7)target.rx()+=dx;if(qAbs(dy)<7)target.ry()+=dy;
                m_modal.snapX=qAbs(dx)<7;m_modal.snapY=qAbs(dy)<7;m_modal.snapPoint=QPointF(gx,gy);
                bool ok=false;auto snapped=rayPlane(target,m_modal.pivot,fwd,&ok);if(ok)delta=snapped-m_modal.pivot;
            }
        } else {
            const QVector3D A = axisVector().normalized();
            delta = A * float(axisParam(mouse, m_modal.pivot, A) - axisParam(m_modal.startMouse, m_modal.pivot, A));
        }
        xf = [delta](const QVector3D &v) { return v + delta; };
        R.translate(delta);
    } else if (m_modal.kind == Modal::Rotate) {
        double deg;
        const QVector3D A = m_modal.axis < 0 ? fwd : axisVector().normalized();
        if (hasNum) {
            deg = num;
        } else {
            double ang = qAtan2(mouse.y() - m_modal.pivotScreen.y(), mouse.x() - m_modal.pivotScreen.x()) - m_modal.startAngle;
            while (ang > M_PI) ang -= 2 * M_PI;
            while (ang < -M_PI) ang += 2 * M_PI;
            if (m_modal.axis >= 0 && QVector3D::dotProduct(A, fwd) < 0) ang = -ang;
            deg = qRadiansToDegrees(ang);
        }
        R.translate(m_modal.pivot);
        R.rotate(float(deg), A);
        R.translate(-m_modal.pivot);
        xf = [R](const QVector3D &v) { return R.map(v); };
    } else if (m_modal.kind == Modal::Scale) {
        double f;
        if (hasNum) {
            f = num;
        } else {
            const double d0 = qMax(8.0, QLineF(m_modal.startMouse, m_modal.pivotScreen).length());
            f = QLineF(mouse, m_modal.pivotScreen).length() / d0;
        }
        const QVector3D A = m_modal.axis >= 0 ? axisVector().normalized() : QVector3D();
        const QVector3D P = m_modal.pivot;
        xf = [A, P, f](const QVector3D &v) {
            const QVector3D d = v - P;
            return A.isNull() ? P + d * float(f) : P + d + A * float((f - 1) * QVector3D::dotProduct(d, A));
        };
    } else if (m_modal.kind == Modal::Inset && mesh) {
        double k = qBound(0.0, (mouse.x() - m_modal.startMouse.x()) / 220.0, 0.95);
        if (hasNum) k = qBound(0.0, num, 0.95);
        for (const auto &pr : m_modal.insetInfo)
            mesh->v[pr.first] = m_modal.origVerts[pr.first] + (pr.second - m_modal.origVerts[pr.first]) * float(k);
        m_m->notify();
        update();
        return;
    } else {
        return;
    }

    if (edit && mesh) {
        for (int i : m_modal.editVerts) mesh->v[i] = invW.map(xf(W.map(m_modal.origVerts[i])));
        if(const auto *object=m_m->active())for(const auto &modifier:object->mods){
            if(!modifier.on||modifier.type!="Mirror"||!modifier.options["clipping"].toBool())continue;
            QMatrix4x4 basis;if(const auto *target=m_m->byId(modifier.target))basis=m_m->design.worldMatrix(*object,m_m->t).inverted()*m_m->design.worldMatrix(*target,m_m->t);
            const auto inverse=basis.inverted();const bool axes[]={modifier.ax,modifier.ay,modifier.az};
            for(int i:m_modal.editVerts){const auto original=inverse.map(m_modal.origVerts[i]);auto point=inverse.map(mesh->v[i]);
                for(int axis=0;axis<3;++axis){const double offset=modifier.options["offset."+QString("xyz").mid(axis,1)].toDouble();if(axes[axis]&&qAbs(original[axis]-offset)<=modifier.options["mergeDistance"].toDouble(.001))point[axis]=offset;}mesh->v[i]=basis.map(point);
            }
        }
    } else {
        for (auto it = m_modal.origProps.begin(); it != m_modal.origProps.end(); ++it) {
            DProps p = it.value();
            auto *object=m_m->byId(it.key());if(!object)continue;
            const auto basis=m_m->design.parentMatrix(*object,m_m->t),inverse=basis.inverted();
            if (m_modal.kind == Modal::Move) {
                p.loc = inverse.map(xf(basis.map(p.loc)));
            } else if (m_modal.kind == Modal::Rotate) {
                DProps q = BuildModel::decompose(inverse * R * basis * ElementDesign::matrixOf(it.value()));
                for (int a = 0; a < 3; ++a) {  // keep the same turn count so the animation does not spin the long way
                    float v = a == 0 ? q.rot.x() : a == 1 ? q.rot.y() : q.rot.z();
                    const float o = a == 0 ? p.rot.x() : a == 1 ? p.rot.y() : p.rot.z();
                    v += 360.0f * qRound((o - v) / 360.0f);
                    if (a == 0) q.rot.setX(v); else if (a == 1) q.rot.setY(v); else q.rot.setZ(v);
                }
                p.loc = q.loc;
                p.rot = q.rot;
                p.scl = q.scl;
            } else {  // Scale
                p.loc = inverse.map(xf(basis.map(p.loc)));
                double f = hasNum ? num : 1.0;
                if (!hasNum) {
                    const double d0 = qMax(8.0, QLineF(m_modal.startMouse, m_modal.pivotScreen).length());
                    f = QLineF(mouse, m_modal.pivotScreen).length() / d0;
                }
                if (m_modal.axis < 0) p.scl = it.value().scl * float(f);
                else if (m_modal.axis == 0) p.scl.setX(it.value().scl.x() * float(f));
                else if (m_modal.axis == 1) p.scl.setY(it.value().scl.y() * float(f));
                else if (m_modal.axis == 2) p.scl.setZ(it.value().scl.z() * float(f));
            }
            m_m->setProps(it.key(), p);
        }
    }
    m_m->notify();
    update();
}

// ------------------------------------------------------------------ mouse

void BuildCanvas::orbit(const QPointF &d) {
    Q_UNUSED(d);
    // Orbiting is intentionally disabled in Cuebase Build. The camera is fixed.
}

void BuildCanvas::pan(const QPointF &d) {
    m_editorPan += d;
    update();
}

void BuildCanvas::pickSceneObject(std::function<void(QString)> picked){
    cancelModal();m_objectPicker=std::move(picked);setCursor(Qt::CrossCursor);setFocus();emit message("Pick a mirror reference object · Escape or right-click cancels");
}

void BuildCanvas::startDrawShape(const QString &kind){
    if(kind!="Rectangle"&&kind!="Ellipse")return;
    cancelModal();if(m_m->editMode)m_m->toggleEdit();
    m_drawShape=kind;m_drawingShape=false;setCursor(Qt::CrossCursor);setFocus();
    emit message("Drag to draw "+kind.toLower()+" · Shift: equal sides · Alt: from centre · Escape: cancel");update();
}
void BuildCanvas::updateDrawShape(QPointF point,Qt::KeyboardModifiers modifiers){
    QPointF delta=point-m_drawStart;
    if(modifiers&Qt::ShiftModifier){const auto side=qMax(qAbs(delta.x()),qAbs(delta.y()));delta=QPointF(delta.x()<0?-side:side,delta.y()<0?-side:side);}
    m_drawRect=QRectF((modifiers&Qt::AltModifier)?m_drawStart-delta:m_drawStart,m_drawStart+delta).normalized();update();
}
void BuildCanvas::mousePressEvent(QMouseEvent *e) {
    if(!m_drawShape.isEmpty()){
        if(e->button()==Qt::RightButton){m_drawShape.clear();m_drawingShape=false;releaseMouse();unsetCursor();update();return;}
        if(e->button()==Qt::LeftButton){setFocus();m_drawStart=e->position();m_drawingShape=true;updateDrawShape(e->position(),e->modifiers());grabMouse();}return;
    }
    if(m_objectPicker){
        if(e->button()==Qt::RightButton){m_objectPicker={};unsetCursor();emit message("Object picking cancelled");return;}
        if(e->button()==Qt::LeftButton){const auto id=pickObject(e->position(),true);if(!id.isEmpty()){auto callback=std::move(m_objectPicker);m_objectPicker={};unsetCursor();callback(id);}else emit message("No object here · click a visible object or Escape to cancel");return;}
    }
    setFocus();
    const QPointF pos = e->position();
    m_press = m_last = m_hover = pos;
    m_hasHover = true;
    m_btn = e->button();
    m_pressed = true;
    m_boxing = false;
    m_tweakPending = false;
    m_shiftPress = e->modifiers() & Qt::ShiftModifier;
    if(!modalActive()&&((e->button()==Qt::RightButton&&m_shiftPress)||(e->button()==Qt::LeftButton&&placeCursor))){
        QRectF frame;const auto camera=activeCam(&frame);bool valid=false;const auto point=rayPlane(pos,m_m->design.cursor,camera.forward(),&valid);
        if(valid){m_m->pushUndo();m_m->design.cursor=point;m_m->notify();emit message("Placement cursor set. New objects appear here.");}m_pressed=false;return;
    }

    if (m_modal.kind != Modal::None) {  // click confirms, right click cancels
        if (e->button() == Qt::LeftButton) confirmModal();
        else if (e->button() == Qt::RightButton) cancelModal();
        m_pressed = false;
        return;
    }
    if (m_loopCut) {
        if (e->button() == Qt::LeftButton && m_loopValid) {
            if (m_m->opLoopCut(m_loopEdge, m_cuts)) emit message(QString("Loop cut: %1").arg(m_cuts));
            m_loopCut = false;
            m_loopValid = false;
        } else if (e->button() == Qt::RightButton) {
            cancelModal();
        }
        m_pressed = false;
        update();
        return;
    }
    if (e->button() == Qt::MiddleButton) {
        const bool hasSel = m_m->editMode ? !m_m->esel.v.isEmpty() : !m_m->sel.isEmpty();
        Q_UNUSED(hasSel);
        return;
    }
    if (e->button() != Qt::LeftButton) return;

    if(!m_m->editMode){
        QRectF frame;auto cam=activeCam(&frame);
        if(showOverlays && showRulers && (pos.x()<20 || pos.y()<20)){
            m_m->pushUndo();
            m_guideVertical=pos.x()<20;
            auto &guides=m_guideVertical?m_verticalGuides:m_horizontalGuides;guides.append(0);m_dragGuide=int(guides.size())-1;return;
        }
        if(showOverlays){
            for(int i=0;i<m_verticalGuides.size();++i)if(qAbs(pos.x()-(frame.left()+m_verticalGuides[i]*frame.width()))<4){m_m->pushUndo();m_dragGuide=i;m_guideVertical=true;return;}
            for(int i=0;i<m_horizontalGuides.size();++i)if(qAbs(pos.y()-(frame.top()+m_horizontalGuides[i]*frame.height()))<4){m_m->pushUndo();m_dragGuide=i;m_guideVertical=false;return;}
        }
        const auto bounds=selectionBounds(cam,frame);
        if(!bounds.isEmpty()){
            QPointF rotation(bounds.center().x(),bounds.top()-24);
            if(QLineF(pos,rotation).length()<9){startModal(Modal::Rotate,true);return;}
            const QPointF handles[]={bounds.topLeft(),bounds.topRight(),bounds.bottomLeft(),bounds.bottomRight(),QPointF(bounds.center().x(),bounds.top()),QPointF(bounds.center().x(),bounds.bottom()),QPointF(bounds.left(),bounds.center().y()),QPointF(bounds.right(),bounds.center().y())};
            for(int i=0;i<8;++i)if(QLineF(pos,handles[i]).length()<8){
                startModal(Modal::Scale,true);m_modal.resizeHandle=i;m_modal.resizeBounds=bounds;
                m_modal.startMouse=handles[i];const int opposite[]={3,2,1,0,5,4,7,6};m_modal.pivotScreen=handles[opposite[i]];
                bool ok=false;auto anchor=rayPlane(m_modal.pivotScreen,m_modal.pivot,cam.forward(),&ok);if(ok)m_modal.pivot=anchor;
                return;
            }
        }
    }
    if (m_m->editMode) {
        const bool hit = pickEditElement(pos, m_shiftPress);
        m_tweakPending = hit && !m_m->esel.v.isEmpty();
        if (!hit && !m_shiftPress) m_tweakPending = false;
        return;
    }
    const QString id = pickObject(pos,true);
    m_pressObj = id;
    if (!id.isEmpty()) {
        if (m_shiftPress) m_m->select(id, true);
        else m_m->select(id);
        m_tweakPending = m_m->sel.contains(id);
    }
}

void BuildCanvas::mouseMoveEvent(QMouseEvent *e) {
    if(!m_drawShape.isEmpty()){if(m_drawingShape)updateDrawShape(e->position(),e->modifiers());return;}
    if(m_objectPicker){setCursor(Qt::CrossCursor);return;}
    const QPointF pos = e->position();
    const QPointF d = pos - m_last;
    m_last = m_hover = pos;
    m_hasHover = true;
    if(m_dragGuide>=0){QRectF frame;activeCam(&frame);auto &guides=m_guideVertical?m_verticalGuides:m_horizontalGuides;guides[m_dragGuide]=m_guideVertical?(pos.x()-frame.left())/frame.width():(pos.y()-frame.top())/frame.height();update();return;}
    if (m_modal.kind != Modal::None) {
        applyModal(pos);
        return;
    }
    if (m_loopCut) {
        updateLoopCutHover(pos);
        update();
        return;
    }
    if (!m_pressed) {
        QRectF frame;const auto cam=activeCam(&frame);const auto bounds=selectionBounds(cam,frame);
        Qt::CursorShape cursor=Qt::ArrowCursor;
        if(m_m->editMode)cursor=Qt::CrossCursor;
        else if(!bounds.isEmpty()){
            const QPointF handles[]={bounds.topLeft(),bounds.topRight(),bounds.bottomLeft(),bounds.bottomRight(),QPointF(bounds.center().x(),bounds.top()),QPointF(bounds.center().x(),bounds.bottom()),QPointF(bounds.left(),bounds.center().y()),QPointF(bounds.right(),bounds.center().y())};
            const Qt::CursorShape cursors[]={Qt::SizeFDiagCursor,Qt::SizeBDiagCursor,Qt::SizeBDiagCursor,Qt::SizeFDiagCursor,Qt::SizeVerCursor,Qt::SizeVerCursor,Qt::SizeHorCursor,Qt::SizeHorCursor};
            for(int i=0;i<8;++i)if(QLineF(pos,handles[i]).length()<8)cursor=cursors[i];
            if(QLineF(pos,QPointF(bounds.center().x(),bounds.top()-24)).length()<9)cursor=Qt::CrossCursor;
        }
        if(cursor==Qt::ArrowCursor&&!pickObject(pos).isEmpty())cursor=Qt::OpenHandCursor;
        setCursor(placeCursor ? Qt::CrossCursor : cursor);
        update();
        return;
    }
    if (m_btn == Qt::RightButton || m_btn == Qt::MiddleButton) {
        setCursor(Qt::ClosedHandCursor);
        pan(d);
    } else if (m_btn == Qt::LeftButton) {
        if (!m_boxing && !m_tweakPending && (pos - m_press).manhattanLength() > 4) {
            m_boxing = true;
            m_boxStart = m_press;
        }
        if (m_tweakPending && (pos - m_press).manhattanLength() > 4) {
            m_tweakPending = false;
            startModal(Modal::Move, true);
            if (m_modal.kind == Modal::Move) applyModal(pos);
            return;
        }
        if (m_boxing) {
            m_boxEnd = pos;
            update();
        }
    }
}

void BuildCanvas::mouseReleaseEvent(QMouseEvent *e) {
    if(!m_drawShape.isEmpty()){
        if(e->button()!=Qt::LeftButton||!m_drawingShape)return;
        updateDrawShape(e->position(),e->modifiers());releaseMouse();
        if(m_drawRect.width()>=3&&m_drawRect.height()>=3){
            QRectF frame;const auto camera=activeCam(&frame);bool aOk=false,bOk=false;
            const auto a=rayPlane(m_drawRect.topLeft(),m_m->design.cursor,camera.forward(),&aOk),b=rayPlane(m_drawRect.bottomRight(),m_m->design.cursor,camera.forward(),&bOk);
            if(aOk&&bOk){
                m_m->pushUndo(true);DObject object;object.id=Library::newId();object.name=m_m->uniqueName(m_drawShape);object.type="Shape";
                const auto local=camera.view.mapVector(b-a);object.w=qAbs(local.x());object.h=qAbs(local.y());object.base.loc=(a+b)*.5f;
                object.base.rot=QQuaternion::fromRotationMatrix(camera.view.inverted().normalMatrix()).toEulerAngles();object.mat.color=Qt::white;object.mat.shadeless=true;object.style["shape"]=m_drawShape;object.captureDefaults();
                m_m->design.objects.append(object);m_m->select(object.id);m_m->notify();emit message(object.name+" added");
            }
        }
        m_drawShape.clear();m_drawingShape=false;unsetCursor();update();return;
    }
    if(m_dragGuide>=0){
        QRectF frame;activeCam(&frame);if(!frame.contains(e->position())){auto &guides=m_guideVertical?m_verticalGuides:m_horizontalGuides;guides.removeAt(m_dragGuide);}
        if(auto *camera=m_m->design.camera()){
            auto json=[](const QVector<double> &values){QJsonArray array;for(double value:values)array.append(value);return array;};
            camera->style["editor.guides.vertical"]=json(m_verticalGuides);camera->style["editor.guides.horizontal"]=json(m_horizontalGuides);
        }
        m_dragGuide=-1;m_pressed=false;m_m->notify();update();return;
    }
    const bool wasPressed = m_pressed;
    m_pressed = false;
    if (m_modal.kind != Modal::None && m_modal.fromMouseButton && e->button() == m_btn) {
        confirmModal();
        return;
    }
    if (!wasPressed || e->button() != Qt::LeftButton) return;
    if (m_boxing) {
        m_boxing = false;
        boxSelect(QRectF(m_boxStart, e->position()).normalized(), e->modifiers() & Qt::ShiftModifier);
        update();
        return;
    }
    if ((e->position() - m_press).manhattanLength() <= 4 && !m_m->editMode && m_pressObj.isEmpty() && !m_shiftPress) m_m->deselectAll();
    m_pressObj.clear();
    m_tweakPending = false;
    update();
}

void BuildCanvas::wheelEvent(QWheelEvent *e) {
    if (m_loopCut) {
        m_cuts = qBound(1, m_cuts + (e->angleDelta().y() > 0 ? 1 : -1), 12);
        update();
        e->accept();
        return;
    }
    const double delta=e->angleDelta().y()?e->angleDelta().y()/120.:e->pixelDelta().y()/100.;
    const double factor = qPow(1.12, delta);
    QRectF oldFrame;activeCam(&oldFrame);
    double oldZoom=m_editorZoom;m_editorZoom=qBound(.02,m_editorZoom*factor,64.0);
    const QPointF center=e->position()+(oldFrame.center()-e->position())*(m_editorZoom/oldZoom);
    m_editorPan=center-cameraFrame().center();
    update();
    e->accept();
}

// ------------------------------------------------------------------ keyboard

void BuildCanvas::keyPressEvent(QKeyEvent *e) {
    if(!m_drawShape.isEmpty()){if(e->key()==Qt::Key_Escape){m_drawShape.clear();m_drawingShape=false;releaseMouse();unsetCursor();update();emit message("Drawing cancelled");}e->accept();return;}
    const int k = e->key();
    const Qt::KeyboardModifiers mod = e->modifiers();
    const bool ctrl = mod & Qt::ControlModifier, shift = mod & Qt::ShiftModifier, alt = mod & Qt::AltModifier, keypad = mod & Qt::KeypadModifier;
    const bool edit = m_m->editMode;

    if(m_objectPicker){if(k==Qt::Key_Escape){m_objectPicker={};unsetCursor();emit message("Object picking cancelled");}return;}

    // ---- while a tool is running
    if (m_modal.kind != Modal::None) {
        if (k == Qt::Key_Escape) cancelModal();
        else if (k == Qt::Key_Return || k == Qt::Key_Enter) confirmModal();
        else if (k == Qt::Key_X || k == Qt::Key_Y || k == Qt::Key_Z) {
            const int a = k == Qt::Key_X ? 0 : k == Qt::Key_Y ? 1 : 2;
            m_modal.axis = m_modal.axis == a ? -1 : a;
            applyModal(m_hasHover ? m_hover : m_modal.startMouse);
        } else if (k == Qt::Key_Backspace) {
            m_modal.numeric.chop(1);
            applyModal(m_hasHover ? m_hover : m_modal.startMouse);
        } else if ((k >= Qt::Key_0 && k <= Qt::Key_9) || k == Qt::Key_Period || k == Qt::Key_Minus) {
            if (k == Qt::Key_Minus && !m_modal.numeric.isEmpty()) m_modal.numeric = m_modal.numeric.startsWith('-') ? m_modal.numeric.mid(1) : "-" + m_modal.numeric;
            else m_modal.numeric += QChar(k == Qt::Key_Period ? '.' : k == Qt::Key_Minus ? '-' : char('0' + (k - Qt::Key_0)));
            applyModal(m_hasHover ? m_hover : m_modal.startMouse);
        }
        update();
        return;
    }
    if (m_loopCut) {
        if (k == Qt::Key_Escape) cancelModal();
        return;
    }

    if(!edit&&k==Qt::Key_Slash&&mod==Qt::NoModifier){emit componentBrowserRequested();return;}
    // ---- views (numpad)
    if (keypad) {
        if (k == Qt::Key_1) { viewFront(); return; }
        if (k == Qt::Key_3) { viewRight(); return; }
        if (k == Qt::Key_7) { viewTop(); return; }
        if (k == Qt::Key_5) { toggleOrtho(); return; }
        if (k == Qt::Key_0) { setCameraView(!m_cameraView); return; }
        if (k == Qt::Key_Period) { frameSelected(); return; }
    }

    if (ctrl && k == Qt::Key_D) { m_m->duplicateSelected(); return; }
    if (!edit && k == Qt::Key_F) { frameSelected(); return; }
    if (!edit && k == Qt::Key_V) { m_m->deselectAll(); return; }
    if (ctrl && k == Qt::Key_Z) { shift ? m_m->redo() : m_m->undo(); return; }
    if (ctrl && k == Qt::Key_Y) { m_m->redo(); return; }
    if (k == Qt::Key_Home) { frameAll(); return; }

    if(ctrl&&shift&&k==Qt::Key_M){auto *object=m_m->active();emit message(object&&m_m->applyModifier(object->id,0)?"Applied first modifier":"Select an unlocked mesh in Object Mode with a modifier");return;}
    if (k == Qt::Key_A) {
        if (shift) { emit addMenuRequested(mapToGlobal(m_hasHover ? m_hover.toPoint() : QPoint(width() / 2, height() / 2))); return; }
        if (alt) { edit ? m_m->editSelectAll(false) : m_m->deselectAll(); }
        else if(edit)m_m->editSelectAll(true);
        else {
            QSet<QString> ids;
            for(const auto &object:m_m->design.objects)if(!object.locked&&m_m->design.effectiveVisible(object)&&
                (object.isRenderable()||(showOverlays&&showGizmos&&((object.type=="Light"&&showLights)||(object.type=="Camera"&&showCamera&&!m_cameraView)))))ids.insert(object.id);
            m_m->setSelection(ids,m_m->activeId);
        }
        emit message(alt?"Selection cleared":edit?QString("Selected %1 vertices · handles use the selection center").arg(m_m->esel.v.size()):QString("Selected %1 objects · handles use the selection center").arg(m_m->sel.size()));
        return;
    }
    if (k == Qt::Key_Tab) {
        m_m->toggleEdit();
        emit message(m_m->editMode?"Edit Mode · select vertices, edges or faces · A selects all · Tab returns to Object Mode":edit?"Object Mode":"Select an unlocked mesh or shape to enter Edit Mode");
        return;
    }
    if (edit) {
        if (!keypad && !ctrl && k == Qt::Key_1) { m_m->setSelMode(SelMode::Vertex); return; }
        if (!keypad && !ctrl && k == Qt::Key_2) { m_m->setSelMode(SelMode::Edge); return; }
        if (!keypad && !ctrl && k == Qt::Key_3) { m_m->setSelMode(SelMode::Face); return; }
        if (ctrl && k == Qt::Key_R) { startLoopCut(); return; }
        if (ctrl && k == Qt::Key_I) { m_m->editInvert(); return; }
        if (k == Qt::Key_E) { startExtrude(); return; }
        if (k == Qt::Key_I) { startInset(); return; }
        if (k == Qt::Key_F && !shift) { emit message(m_m->opFill() ? "Filled" : "Select 3 or more vertices to fill"); return; }
        if (k == Qt::Key_P) { emit message(m_m->opSeparate() ? "Separated into a new object" : "Select faces to separate"); return; }
        if (k == Qt::Key_Y) { emit message(m_m->opSplit() ? "Split" : "Select faces to split"); return; }
        if (alt && k == Qt::Key_M) { emit message(m_m->opMerge() ? "Merged at center" : "Select 2 or more vertices"); return; }
        if (k == Qt::Key_X || k == Qt::Key_Delete) { m_m->opDelete(); return; }
        if (k == Qt::Key_W && !ctrl) { emit message(m_m->opSubdivide() ? "Subdivided" : "Select edges or faces to subdivide"); return; }
        if (shift && k == Qt::Key_F) { emit message(m_m->opFlatten() ? "Flattened" : "Select 3 or more vertices to flatten"); return; }
        if (alt && k == Qt::Key_N) { m_m->opFlip(); emit message("Normals flipped"); return; }
    } else {
        if (shift && k == Qt::Key_D) {
            if (!m_m->duplicateSelected().isEmpty()) startModal(Modal::Move, false, true);
            return;
        }
        if (ctrl && k == Qt::Key_J) { emit message(m_m->joinSelected() ? "Joined" : "Select two or more mesh objects (active one stays)"); return; }
        if (k == Qt::Key_X || k == Qt::Key_Delete) { m_m->deleteSelected(); return; }
        if (k == Qt::Key_H) { alt ? m_m->showAll() : m_m->hideSelected(true); return; }
        if (k == Qt::Key_0 && !keypad) { setCameraView(!m_cameraView); return; }
    }
    if (k == Qt::Key_G || (k == Qt::Key_M && !alt)) { startMove(); return; }
    if (k == Qt::Key_R && !ctrl) { startRotate(); return; }
    if (k == Qt::Key_S && !ctrl) { startScale(); return; }
    if (k == Qt::Key_K) { alt ? m_m->deleteKey() : m_m->insertKey(); return; }
    if (k == Qt::Key_I && !edit) { alt ? m_m->deleteKey() : m_m->insertKey(); return; }
    if (k == Qt::Key_Z && !ctrl) { setShading(m_shading==Wire?Solid:m_shading==Solid?MaterialPreview:m_shading==MaterialPreview?Rendered:Wire); return; }
    if (k == Qt::Key_Period) { frameSelected(); return; }
    QWidget::keyPressEvent(e);
}
