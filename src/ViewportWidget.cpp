#include "ViewportWidget.h"
#include <algorithm>
#include "Continuity.h"
#include "Replay.h"

#include <QEnterEvent>
#include <QDateTime>
#include "ColorManagement.h"
#include <QFile>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygon>
#include <QSet>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>
#include <cmath>
#include <QtMath>

#include "Playout.h"

namespace {

QFont uiFont(int px, bool bold = false) {
    QFont f("Segoe UI");
    f.setPixelSize(qMax(8, px));
    f.setBold(bold);
    return f;
}

QPoint anchorFor(const QRect &r, const QString &pos, const QSize &s, int margin) {
    int x = r.left() + margin, y = r.top() + margin;
    if (pos.contains("Right")) x = r.right() - margin - s.width();
    if (pos.contains("Center")) x = r.center().x() - s.width() / 2;
    if (pos.startsWith("Bottom")) y = r.bottom() - margin - s.height();
    return {x, y};
}

// Moves / fades / scales / wipes whatever is drawn next.
void applyFx(QPainter &p, const QRectF &r, const FxState &s) {
    if (s.opacity >= 1.0 && s.dx == 0.0 && s.dy == 0.0 && s.scale == 1.0 && s.reveal >= 1.0) return;
    p.setOpacity(p.opacity() * qBound(0.0, s.opacity, 1.0));
    p.translate(s.dx * r.width(), s.dy * r.height());
    const QPointF c = r.center();
    p.translate(c);
    p.scale(s.scale, s.scale);
    p.translate(-c);
    if (s.reveal < 1.0) p.setClipRect(QRectF(r.left(), r.top(), r.width() * s.reveal, r.height()), Qt::IntersectClip);
}

QColor segColor(const Segment &s) {
    return s.type == Segment::Program ? kindColor(s.kind)
         : s.type == Segment::Ad      ? QColor("#16a34a")
                                      : QColor("#f59e0b");
}

} // namespace

ViewportWidget::ViewportWidget(Library *lib, Playout *play, QWidget *parent)
    : QWidget(parent), m_lib(lib), m_play(play) {
    setMouseTracking(true);

    auto refresh = [this] { update(); };
    connect(play->replay(),&Replay::changed,this,[this,play]{if(play->replay()->active()&&play->replay()->elapsed==0)goCurrent();update();});
    connect(play, &Playout::ticked, this, refresh);
    connect(play, &Playout::segmentChanged, this, refresh);
    connect(play, &Playout::stateChanged, this, refresh);
    connect(lib, &Library::displayChanged, this, refresh);
    connect(lib, &Library::elementsChanged, this, refresh);
    connect(lib, &Library::elementLiveChanged, this, refresh);
    connect(lib, &Library::placementsChanged, this, refresh);
    connect(lib, &Library::layersChanged, this, refresh);
    connect(play->videoSink(), &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &f) {
        m_frame = f.isValid() ? f.toImage() : QImage();
        update();
    });

    // the preview playhead has its own silent player, so it never disturbs the broadcast
    m_vplayer = new QMediaPlayer(this);
    m_vsink = new QVideoSink(this);
    m_vplayer->setVideoSink(m_vsink);
    connect(m_vsink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &f) {
        if (!m_detached) return;
        m_vframe = f.isValid() ? f.toImage() : QImage();
        update();
    });
    connect(m_vplayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus st) {
        if ((st != QMediaPlayer::LoadedMedia && st != QMediaPlayer::BufferedMedia) || m_vpending < 0) return;
        const qint64 pos = m_vpending;
        m_vpending = -1;
        if (pos > 0) m_vplayer->setPosition(pos);
        if (m_scrub || !m_viewPlaying) m_vplayer->pause();
        else m_vplayer->play();
    });
    m_transitionTimer.setInterval(16);
    connect(&m_transitionTimer,&QTimer::timeout,this,[this]{update();if((!m_returnTransition.isValid()||m_returnTransition.elapsed()>420)&&(!m_monitorTransition.isValid()||m_monitorTransition.elapsed()>300))m_transitionTimer.stop();});
    m_vtimer.setInterval(40);
    connect(&m_vtimer, &QTimer::timeout, this, [this] { tickViewer(); });
    m_vclock.start();

    connect(play, &Playout::segmentsChanged, this, [this] {
        if (m_detached) {
            m_viewMs = qBound<qint64>(0, m_viewMs, qMax<qint64>(0, m_play->totalMs() - 1));
            syncViewer(false);
        }
        update();
    });
}

QPixmap ViewportWidget::cachedPixmap(const QString &path) {
    if (!m_pixmaps.contains(path)) m_pixmaps.insert(path, QPixmap(path));
    return m_pixmaps.value(path);
}

// ------------------------------------------------------- preview playhead

ViewportWidget::Geo ViewportWidget::viewGeo() const {
    Geo g;
    const int chromeH = 34;
    const QRect area(0, 0, width(), height() - chromeH);
    const double asp = m_lib->display.aspect();  // from the Display settings (resolution x pixel aspect)
    int w = area.width(), h = int(w / asp);
    if (h > area.height()) {
        h = area.height();
        w = int(h * asp);
    }
    g.screen = QRect(area.x() + (area.width() - w) / 2, area.y() + (area.height() - h) / 2, w, h);
    g.chrome = QRect(0, height() - chromeH, width(), chromeH);
    g.bar = QRect(g.screen.left(), g.screen.bottom() - 46, g.screen.width(), 47);
    const int cy = g.bar.bottom() - 18;
    const int left = g.bar.left() + 12, right = g.bar.right() - 12;
    g.playBtn = QRect(left, cy - 13, 26, 26);
    g.currentBtn = QRect(right - 104, cy - 12, 104, 24);
    g.track = QRect(left + 34, cy - 4, qMax(10, (right - 114) - (left + 34)), 8);
    g.trackHit = g.track.adjusted(-8, -14, 8, 14);
    return g;
}

ViewportWidget::View ViewportWidget::makeView() const {
    View v;
    const auto &segs = m_play->segments();
    if (m_detached) {
        qint64 off = 0;
        const int idx = m_play->segmentAtMs(m_viewMs, &off);
        v.seg = idx >= 0 ? &segs[idx] : nullptr;
        v.posMs = m_viewMs;
        v.segElapsed = off;
        v.tcMs = m_viewMs;
        v.frame = &m_vframe;
    } else {
        v.seg = m_play->current();
        v.posMs = m_play->timelinePosMs();
        v.segElapsed = m_play->segmentElapsed();
        v.tcMs = m_play->totalElapsed();
        v.paused = m_play->state() == Playout::Paused;
        v.onAir = m_play->isLive();
        v.frame = &m_frame;
    }
    return v;
}

// The scrubber shows one program only: the one on air (or the one the preview / the cue point is in), with its
// own break and ad. Nothing else of the queue is drawn on it.
ViewportWidget::Range ViewportWidget::scrubRange() const {
    int ref = -1;
    if (m_detached) ref = m_play->segmentAtMs(m_viewMs);
    else if (m_play->isLive()) ref = m_play->index();
    else ref = m_play->segmentAtMs(m_play->playheadMs());
    Range r;
    if(ref>=0&&ref<m_play->segments().size()){r.a=ref;r.b=ref;}
    return r;
}

double ViewportWidget::rangeUnit(qint64 ms, const Range &r) const {
    if(r.a<0)return 0.;const qint64 start=m_play->segmentStartMs(r.a),duration=m_play->segments()[r.a].ms;
    return duration>0?qBound(0.,double(ms-start)/duration,1.):0.;
}
qint64 ViewportWidget::rangeMs(double unit,const Range &r) const {
    if(r.a<0)return 0;const qint64 start=m_play->segmentStartMs(r.a),duration=m_play->segments()[r.a].ms;
    return start+qBound<qint64>(0,qRound64(qBound(0.,unit,1.)*duration),qMax<qint64>(0,duration-1));
}

void ViewportWidget::detachAt(qint64 ms) {
    if (m_play->segments().isEmpty()) return;
    const bool was = m_detached;
    m_detached = true;
    m_viewMs = qBound<qint64>(0, ms, qMax<qint64>(0, m_play->totalMs() - 1));
    if (!was) {
        m_monitorTransition.start();m_transitionTimer.start();
        m_viewPlaying = true;
        m_vlast = m_vclock.elapsed();
        m_vtimer.start();
        emit viewModeChanged();
    }
    syncViewer(true);
    update();
}

void ViewportWidget::scrubTo(int x) {
    const Geo g = viewGeo();
    const double u = qBound(0.0, double(x - g.track.left()) / g.track.width(), 1.0);
    detachAt(rangeMs(u, scrubRange()));
}

void ViewportWidget::goCurrent() {
    if (!m_detached) return;
    m_returnTransition.start();m_transitionTimer.start();
    m_detached = false;
    m_scrub = false;
    m_vtimer.stop();
    m_vpending = -1;
    m_vplayer->stop();
    m_vplayer->setSource(QUrl());
    m_vframe = QImage();
    m_viewSeg = -1;
    emit viewModeChanged();
    update();
}

void ViewportWidget::syncViewer(bool forceSeek) {
    qint64 off = 0;
    const int idx = m_play->segmentAtMs(m_viewMs, &off);
    m_viewSeg = idx;
    if (idx < 0) {
        m_vpending = -1;
        m_vplayer->stop();
        m_vplayer->setSource(QUrl());
        m_vframe = QImage();
        return;
    }
    const Segment &s = m_play->segments()[idx];
    const bool media = !s.file.isEmpty() && QFile::exists(s.file) && s.type != Segment::Break;
    if (!media) {
        m_vpending = -1;
        m_vplayer->stop();
        m_vplayer->setSource(QUrl());
        m_vframe = QImage();
        return;
    }
    const QUrl url = QUrl::fromLocalFile(s.file);
    const qint64 pos = s.offsetMs + off;
    if (m_vplayer->source() != url) {
        m_vframe = QImage();
        m_vpending = pos;  // seeks and plays/pauses once the file has loaded
        m_vplayer->setSource(url);
    } else if (forceSeek) {
        m_vplayer->setPosition(pos);
    }
    if (m_vpending < 0) {
        if (m_scrub || !m_viewPlaying) m_vplayer->pause();
        else m_vplayer->play();
    }
}

void ViewportWidget::tickViewer() {
    const qint64 now = m_vclock.elapsed();
    const qint64 dt = qMin<qint64>(now - m_vlast, 500);
    m_vlast = now;
    if (!m_detached || !m_viewPlaying || m_scrub) return;
    m_viewMs += dt;
    if (m_viewMs >= m_play->totalMs()) {  // reached the end of the timeline: stop there
        m_viewMs = qMax<qint64>(0, m_play->totalMs() - 1);
        m_viewPlaying = false;
        syncViewer(false);
        update();
        return;
    }
    if (m_play->segmentAtMs(m_viewMs) != m_viewSeg) syncViewer(false);
    update();
}

// ----------------------------------------------------------------- mouse

void ViewportWidget::enterEvent(QEnterEvent *e) {
    m_hover = true;
    update();
    QWidget::enterEvent(e);
}

void ViewportWidget::leaveEvent(QEvent *e) {
    m_hover = false;
    m_hoverPt = QPoint(-1, -1);
    update();
    QWidget::leaveEvent(e);
}

void ViewportWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) return;
    const QPoint pos = e->position().toPoint();
    const Geo g = viewGeo();
    if (!barVisible()) return;
    if (m_detached && g.currentBtn.contains(pos)) {
        goCurrent();
        return;
    }
    if (m_detached && g.playBtn.contains(pos)) {
        m_viewPlaying = !m_viewPlaying;
        if (m_viewPlaying && m_play->totalMs() > 0 && m_viewMs >= m_play->totalMs() - 1) m_viewMs = 0;  // replay from the start
        m_vlast = m_vclock.elapsed();
        syncViewer(m_viewPlaying);
        update();
        return;
    }
    if (g.trackHit.contains(pos)) {
        m_scrub = true;
        scrubTo(pos.x());
    }
}

void ViewportWidget::mouseMoveEvent(QMouseEvent *e) {
    m_hoverPt = e->position().toPoint();
    if (m_scrub) {
        scrubTo(m_hoverPt.x());
        return;
    }
    const Geo g = viewGeo();
    const bool bar = barVisible();
    if (bar && g.trackHit.contains(m_hoverPt)) setCursor(Qt::PointingHandCursor);
    else if (bar && m_detached && (g.playBtn.contains(m_hoverPt) || g.currentBtn.contains(m_hoverPt))) setCursor(Qt::PointingHandCursor);
    else unsetCursor();
    update();
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || !m_scrub) return;
    m_scrub = false;
    if (m_detached) {
        m_vlast = m_vclock.elapsed();
        syncViewer(false);  // carries on playing from where the playhead was dropped
    }
    update();
}

// -------------------------------------------------------------- painting

void ViewportWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    p.setClipPath(clip);
    p.fillRect(rect(), QColor("#050912"));

    const Geo g = viewGeo();
    const View v = makeView();

    p.save();
    p.setClipRect(g.screen);
    const DisplaySettings &ds = m_lib->display;
    if(!m_detached&&(m_play->replay()->active()||m_play->continuity()->active())){
        p.drawImage(g.screen,programFrame(g.screen.size()));
    } else {
        // colour management is applied to the picture itself, not to the controls drawn over it
        QImage img(g.screen.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        QPainter q(&img);
        drawScreen(q, img.rect(), v);
        q.end();
        if(colorManagementActive(ds.colorView,ds.colorLook,ds.exposure,ds.gamma))applyColorManagement(img, ds.colorView, ds.colorLook, ds.exposure, ds.gamma);
        p.drawImage(g.screen.topLeft(), img);
    }
    if (ds.regionOn && (ds.rx0 > 0.001 || ds.ry0 > 0.001 || ds.rx1 < 0.999 || ds.ry1 < 0.999)) {
        // render region: only that part of the picture is output, the rest is black
        const QRect s = g.screen;
        const QRect keep(s.left() + int(s.width() * ds.rx0), s.top() + int(s.height() * ds.ry0),
                         int(s.width() * (ds.rx1 - ds.rx0)), int(s.height() * (ds.ry1 - ds.ry0)));
        QPainterPath all, hole;
        all.addRect(s);
        hole.addRect(keep);
        p.fillPath(all.subtracted(hole), Qt::black);
        p.setPen(QPen(QColor(255, 255, 255, 90), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(keep);
    }
    if(m_detached){
        const int width=qMax(100,g.screen.width()/4),height=width*9/16;
        const QRect target(g.screen.right()-width-12,g.screen.bottom()-height-72,width,height);
        const double t=m_monitorTransition.isValid()?qBound(0.,m_monitorTransition.elapsed()/300.,1.):1.;
        const double eased=1.-qPow(1.-t,3.);
        const QRect start(g.screen.topLeft()+QPoint(12,12),g.screen.size());
        const QRect liveRect(int(start.x()+(target.x()-start.x())*eased),int(start.y()+(target.y()-start.y())*eased),int(start.width()+(target.width()-start.width())*eased),int(start.height()+(target.height()-start.height())*eased));
        p.save();QPainterPath path;path.addRoundedRect(liveRect,7,7);p.setClipPath(path,Qt::IntersectClip);p.drawImage(liveRect,programFrame(target.size()));p.fillRect(QRect(liveRect.left(),liveRect.top(),liveRect.width(),20),QColor(0,0,0,140));p.setPen(QColor("#ff6767"));p.setFont(uiFont(9,true));p.drawText(liveRect.adjusted(7,3,-7,-3),Qt::AlignTop|Qt::AlignLeft,"LIVE OUTPUT");p.restore();p.setPen(QPen(QColor(255,255,255,95),1));p.setBrush(Qt::NoBrush);p.drawRoundedRect(liveRect,7,7);
    }
    if(!m_detached&&m_returnTransition.isValid()&&m_returnTransition.elapsed()<420){
        const double t=m_returnTransition.elapsed()/420.;const double opacity=qSin(t*3.141592653589793);
        p.fillRect(g.screen,QColor(0,0,0,int(140*opacity)));const QPointF center=g.screen.center();const double cell=12,gap=4;
        p.setPen(Qt::NoPen);for(int i=0;i<9;++i){const int destination=(i+3)%9;const double ease=t*t*(3-2*t);const QPointF from((i%3-1)*(cell+gap),(i/3-1)*(cell+gap)),to((destination%3-1)*(cell+gap),(destination/3-1)*(cell+gap));p.setBrush(QColor(160+i*7,190+i*5,240,int(220*opacity)));p.drawRoundedRect(QRectF(center+from*(1-ease)+to*ease-QPointF(cell/2,cell/2),QSizeF(cell,cell)),2,2);}
    }
    if (barVisible()) drawScrubber(p, g, v);
    p.restore();
    drawChrome(p, g.chrome, v);

    p.setClipping(false);
    p.setPen(QPen(QColor("#243044"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
}

QImage ViewportWidget::programFrame(QSize size){
    if(m_play->continuity()->active())return m_play->continuity()->frame(size);
    QImage image(size,QImage::Format_ARGB32);image.fill(Qt::black);
    View live;live.seg=m_play->current();live.posMs=m_play->timelinePosMs();live.segElapsed=m_play->segmentElapsed();live.tcMs=m_play->totalElapsed();live.onAir=m_play->isLive();live.frame=&m_frame;
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);drawScreen(p,image.rect(),live,false);p.end();
    if(auto *replay=m_play->replay();replay->active()){
        QImage picture(size,QImage::Format_ARGB32);picture.fill(Qt::black);
        View view;view.seg=&replay->segment;view.posMs=replay->position;view.segElapsed=replay->segmentElapsed();view.tcMs=replay->elapsed;view.onAir=true;view.frame=&replay->frame;
        QPainter painter(&picture);drawScreen(painter,picture.rect(),view,false);painter.end();image=replay->composite(image,picture);
    }
    const auto &d=m_lib->display;if(colorManagementActive(d.colorView,d.colorLook,d.exposure,d.gamma))applyColorManagement(image,d.colorView,d.colorLook,d.exposure,d.gamma);
    if(d.regionOn){QPainter crop(&image);QPainterPath all,keep;all.addRect(image.rect());keep.addRect(QRectF(size.width()*d.rx0,size.height()*d.ry0,size.width()*(d.rx1-d.rx0),size.height()*(d.ry1-d.ry0)));crop.fillPath(all.subtracted(keep),Qt::black);}
    return image;
}
void ViewportWidget::drawScreen(QPainter &p, const QRect &r, const View &v, bool guides) {
    const Segment *seg = v.seg;
    const int H = r.height();

    // ---- background + main content (moves in / out with the program's or ad's transition) ----
    p.fillRect(r, Qt::black);
    if(!seg&&!guides)return;
    p.save();
    if (seg && seg->type != Segment::Break) applyFx(p, r, transitionAt(seg->fx, v.segElapsed, seg->ms));
    if (!seg) {
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, QColor("#0b1220"));
        g.setColorAt(1, QColor("#020617"));
        p.fillRect(r, g);
        p.setPen(QColor("#475569"));
        p.setFont(uiFont(H * 9 / 100, true));
        p.drawText(r.adjusted(0, 0, 0, -H / 8), Qt::AlignCenter, "OFF AIR");
        p.setFont(uiFont(H * 4 / 100));
        p.drawText(r.adjusted(0, H / 6, 0, 0), Qt::AlignCenter, "Press Start playout to begin");
    } else if (!seg->file.isEmpty() && v.frame && !v.frame->isNull() && seg->type != Segment::Break) {
        p.fillRect(r, Qt::black);
        const QImage scaled = v.frame->scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawImage(r.center() - QPoint(scaled.width() / 2, scaled.height() / 2), scaled);
    } else if(((m_lib->property("planningPreview").toBool()&&seg->kind=="Plan element")||seg->kind=="Screen element")&&seg->type!=Segment::Break){
        p.fillRect(r,Qt::black);
    } else {
        QColor base = seg->type == Segment::Break ? QColor(seg->color) : segColor(*seg);
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, base.darker(260));
        g.setColorAt(1, QColor("#020617"));
        p.fillRect(r, g);

        if (seg->type == Segment::Break) {
            const QRect card(r.left() + r.width() / 8, r.center().y() - H / 6, r.width() * 3 / 4, H / 3);
            p.setPen(Qt::NoPen);
            p.setBrush(base);
            p.drawRoundedRect(card, H / 20, H / 20);
            p.setPen(Qt::white);
            p.setFont(uiFont(H * 6 / 100, true));
            p.drawText(card.adjusted(H / 20, 0, -H / 20, 0), Qt::AlignCenter | Qt::TextWordWrap, seg->text);
        } else {
            p.setPen(QColor(255, 255, 255, 150));
            p.setFont(uiFont(H * 35 / 1000, true));
            const QString label = seg->type == Segment::Ad ? "ADVERT" : "NOW PLAYING  ·  " + seg->kind.toUpper();
            p.drawText(r.adjusted(0, 0, 0, -H / 6), Qt::AlignCenter, label);
            p.setPen(Qt::white);
            p.setFont(uiFont(H * 10 / 100, true));
            p.drawText(r.adjusted(H / 10, H / 8, -H / 10, 0), Qt::AlignCenter | Qt::TextWordWrap, seg->title);
        }
    }

    p.restore();

    if (v.paused && guides) {
        p.fillRect(r, QColor(0, 0, 0, 140));
        p.setPen(Qt::white);
        p.setFont(uiFont(H * 8 / 100, true));
        p.drawText(r, Qt::AlignCenter, "PAUSED");
    }

    // ---- on-screen elements: switched-on ones, plus ones placed on the extra timelines for this moment ----
    struct GraphicLayer{Element element;qint64 time=0,elapsed=0,length=0;bool controlled=false,placed=false;};
    QVector<GraphicLayer> graphics;QSet<QString> drawn;const qint64 clock=v.posMs;
    if(seg&&seg->type!=Segment::Break)if(const auto *item=m_lib->program(seg->programId);item&&!item->elementId.isEmpty())if(const auto *e=m_lib->element(item->elementId)){
        Element instance=*e;instance.id="program:"+item->id;const auto playback=m_play->graphicPlayback(instance,clock,clock-v.segElapsed,item->offsetMs);
        drawn.insert(e->id);if(playback.visible)graphics.append({playback.resolved(instance),playback.time,v.segElapsed,seg->ms,playback.controlled,true});
    }
    for(const auto &e:m_lib->elements){if(drawn.contains(e.id))continue;const auto playback=m_play->graphicPlayback(e,clock);if(playback.controlled)drawn.insert(e.id);if(!playback.visible||e.type=="Break")continue;if(e.type=="Ad Element"&&!(seg&&seg->type==Segment::Ad))continue;graphics.append({playback.resolved(e),playback.time,0,0,playback.controlled,false});drawn.insert(e.id);}
    if(v.onAir||m_detached)for(const auto &pl:m_lib->placements){if(pl.type!="Element"||!m_lib->layers.value(qBound(0,pl.layer,3)).visible||clock<pl.startMs||clock>=pl.startMs+pl.lengthMs())continue;const auto e=m_lib->placementElement(pl);if(e.type=="Break"||drawn.contains(e.id))continue;drawn.insert(e.id);const auto playback=m_play->graphicPlayback(e,clock,pl.startMs,pl.offsetMs);if(!playback.visible)continue;graphics.append({playback.resolved(e),playback.time,clock-pl.startMs,pl.lengthMs(),playback.controlled,true});}
    std::stable_sort(graphics.begin(),graphics.end(),[](const GraphicLayer &a,const GraphicLayer &b){return a.element.cueBoardSettings["zIndex"].toInt()<b.element.cueBoardSettings["zIndex"].toInt();});
    for(const auto &graphic:graphics){p.save();if(graphic.placed)applyFx(p,r,transitionAt(graphic.element.fx,graphic.elapsed,graphic.length));drawElement(p,r,graphic.element,graphic.time,graphic.controlled);p.restore();}

    // ---- watermark ----
    const DisplaySettings &d = m_lib->display;
    if (d.watermarkOn && !d.watermarkText.isEmpty()) {
        p.setFont(uiFont(H * 35 / 1000, true));
        p.setPen(QColor(255, 255, 255, 95));
        const int m = r.width() * 2 / 100;
        p.drawText(r.adjusted(m, m, -m, -m), Qt::AlignRight | Qt::AlignBottom, d.watermarkText);
    }

    // ---- safe-area guides ----
    if (d.safeArea && guides) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 90), 1, Qt::DashLine));
        for (double f : {0.9, 0.8}) {
            const int w = int(r.width() * f), h = int(r.height() * f);
            p.drawRect(QRect(r.center().x() - w / 2, r.center().y() - h / 2, w, h));
        }
    }
}

const ElementDesign &ViewportWidget::designFor(const Element &e) {
    auto it = m_designs.find(e.id);
    if (it == m_designs.end() || it->first != e.design)
        it = m_designs.insert(e.id, qMakePair(e.design, ElementDesign::fromJson(e.design)));
    return it->second;
}

void ViewportWidget::drawElement(QPainter &p, const QRect &r, const Element &e, qint64 tMs,bool controlled) {
    if (e.type == "Custom") {  // an animated design from the Build workspace
        if (!e.design.isEmpty()) {
            const DisplaySettings &ds = m_lib->display;
            RenderSettings rs;
            rs.motionBlur = ds.motionBlur;
            rs.shutter = ds.shutter;
            rs.samples = ds.mbSamples;
            rs.frameMs = 1000.0 * ds.fpsBase / qMax(1, ds.fps);
            designFor(e).render(p, r, tMs, true, &rs,!controlled);
        }
        return;
    }
    const int H = r.height();
    const int margin = r.width() * 3 / 100;
    const QColor color(e.color);

    if (e.type == "Logo" && !e.image.isEmpty() && QFile::exists(e.image)) {
        const QPixmap pm = cachedPixmap(e.image).scaledToHeight(H * 14 / 100, Qt::SmoothTransformation);
        p.drawPixmap(anchorFor(r, e.position, pm.size(), margin), pm);
        return;
    }

    const int px = e.type == "Logo" ? H * 7 / 100 : e.type == "Banner" ? H * 55 / 1000 : H * 45 / 1000;
    const QFont font = uiFont(px, true);
    const QFontMetrics fm(font);
    const int pad = px * 6 / 10;
    QSize size(fm.horizontalAdvance(e.text) + pad * 2, fm.height() + pad);
    if (e.type == "Banner" && e.position.contains("Center")) size.setWidth(r.width() * 6 / 10);

    const QRect box(anchorFor(r, e.position, size, margin), size);
    p.setFont(font);
    p.setPen(Qt::NoPen);
    if (e.type == "Logo") {
        p.setBrush(QColor(0, 0, 0, 120));
        p.drawRoundedRect(box, box.height() / 3.0, box.height() / 3.0);
        p.setPen(color);
    } else {
        p.setBrush(color);
        p.drawRoundedRect(box, box.height() / 4.0, box.height() / 4.0);
        p.setPen(Qt::white);
    }
    p.drawText(box, Qt::AlignCenter, e.text);
}

// The scrubber that appears while the mouse is over the monitor.
void ViewportWidget::drawScrubber(QPainter &p, const Geo &g, const View &) {
    QLinearGradient bg(g.bar.topLeft(), g.bar.bottomLeft());
    bg.setColorAt(0, QColor(0, 0, 0, 0));
    bg.setColorAt(1, QColor(0, 0, 0, 205));
    p.fillRect(g.bar, bg);

    const auto &segs = m_play->segments();
    if (segs.isEmpty()) {
        p.setPen(QColor(255, 255, 255, 170));
        p.setFont(uiFont(12));
        p.drawText(g.bar.adjusted(12, 0, -12, -8), Qt::AlignBottom | Qt::AlignLeft, "Nothing on the timeline to preview yet");
        return;
    }

    // the one program's timeline as a thin strip, one block per segment (program, break, ad, break)
    const Range rg = scrubRange();
    const double u0 = m_play->unitAt(rg.a, false), u1 = m_play->unitAt(rg.b, true);
    const double span = qMax(1e-9, u1 - u0);
    auto xOf = [&](double u) { return g.track.left() + int(g.track.width() * qBound(0.0, (u - u0) / span, 1.0)); };
    p.setPen(Qt::NoPen);
    for (int i = rg.a; i >= 0 && i <= rg.b; ++i) {
        const int x0 = xOf(m_play->unitAt(i, false)), x1 = xOf(m_play->unitAt(i, true));
        p.setBrush(segColor(segs[i]).darker(115));
        p.drawRoundedRect(QRect(x0, g.track.top(), qMax(2, x1 - x0 - 1), g.track.height()), 2, 2);
    }

    // hover guide
    if (!m_scrub && g.trackHit.contains(m_hoverPt)) {
        const int hx = qBound(g.track.left(), m_hoverPt.x(), g.track.right());
        p.setPen(QPen(QColor(255, 255, 255, 110), 1));
        p.drawLine(hx, g.track.top() - 5, hx, g.track.bottom() + 5);
    }

    // where the broadcast is right now, when this program is the one on air (never moved by this scrubber)
    if (m_play->isLive() && m_play->index() >= rg.a && m_play->index() <= rg.b) {
        const int lx = g.track.left() + int(g.track.width() * rangeUnit(m_play->timelinePosMs(), rg));
        p.setPen(QPen(QColor("#ef4444"), 2));
        p.drawLine(lx, g.track.top() - 4, lx, g.track.bottom() + 4);
    }

    // the preview playhead
    const qint64 shown = m_detached ? m_viewMs : m_play->playheadMs();
    const int cx = g.track.left() + int(g.track.width() * rangeUnit(shown, rg));
    const QPoint c(cx, g.track.center().y());
    p.setPen(QPen(QColor("#0b1220"), 2));
    p.setBrush(Qt::white);
    p.drawEllipse(c, 7, 7);

    const qint64 gStart = rg.a >= 0 ? m_play->segmentStartMs(rg.a) : 0;
    const qint64 gLen = rg.a >= 0 ? m_play->segmentStartMs(rg.b) + segs[rg.b].ms - gStart : 0;
    QString label = formatDuration(int(qBound<qint64>(0, shown - gStart, gLen) / 1000)) + " / " + formatDuration(int(gLen / 1000));
    if (!m_scrub && g.trackHit.contains(m_hoverPt)) {
        const double u = qBound(0.0, double(m_hoverPt.x() - g.track.left()) / g.track.width(), 1.0);
        label = formatDuration(int((rangeMs(u, rg) - gStart) / 1000));
    }
    const QFont lf = uiFont(11, true);
    p.setFont(lf);
    const int lw = QFontMetrics(lf).horizontalAdvance(label) + 12;
    const QRect lr(qBound(g.bar.left() + 4, cx - lw / 2, g.bar.right() - lw - 4), g.track.top() - 26, lw, 17);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 170));
    p.drawRoundedRect(lr, 6, 6);
    p.setPen(Qt::white);
    p.drawText(lr, Qt::AlignCenter, label);

    if (m_detached) {
        // play / pause the preview
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 45));
        p.drawEllipse(g.playBtn);
        p.setBrush(Qt::white);
        const QPoint pc = g.playBtn.center();
        if (m_viewPlaying) {
            p.drawRect(QRect(pc.x() - 5, pc.y() - 6, 4, 12));
            p.drawRect(QRect(pc.x() + 2, pc.y() - 6, 4, 12));
        } else {
            p.drawPolygon(QPolygon({QPoint(pc.x() - 4, pc.y() - 7), QPoint(pc.x() - 4, pc.y() + 7), QPoint(pc.x() + 7, pc.y())}));
        }

        // back to what is on air
        const bool hot = g.currentBtn.contains(m_hoverPt);
        p.setBrush(hot ? QColor("#3b82f6") : QColor("#2563eb"));
        p.drawRoundedRect(g.currentBtn, 12, 12);
        p.setPen(Qt::white);
        p.setFont(uiFont(12, true));
        p.drawText(g.currentBtn, Qt::AlignCenter, "Return to live");
    }
}

void ViewportWidget::drawChrome(QPainter &p, const QRect &r, const View &v) {
    p.fillRect(r, QColor("#0a0f1c"));

    // thin timeline bar that moves as the video plays
    const QRect track(r.left(), r.top(), r.width(), 3);
    p.fillRect(track, QColor("#1e293b"));
    const Segment *seg = v.seg;
    if (seg && seg->ms > 0) {
        const double f = qBound(0.0, double(v.segElapsed) / seg->ms, 1.0);
        p.fillRect(QRect(track.left(), track.top(), int(track.width() * f), track.height()), segColor(*seg));
    }

    // left: ON AIR / PREVIEW + timecode
    const bool live = v.onAir;
    const int cy = r.top() + 3 + (r.height() - 3) / 2;
    const QColor chip = m_detached ? QColor("#f59e0b") : live ? QColor("#ef4444") : QColor("#64748b");
    p.setPen(Qt::NoPen);
    p.setBrush(m_detached ? QColor("#f59e0b") : live ? QColor("#ef4444") : QColor("#475569"));
    p.drawEllipse(QPoint(r.left() + 14, cy), 4, 4);
    p.setFont(uiFont(12, true));
    p.setPen(chip);
    p.drawText(QRect(r.left() + 24, r.top() + 3, 80, r.height() - 3), Qt::AlignVCenter | Qt::AlignLeft,
               m_detached ? "PREVIEW" : live ? "ON AIR" : "OFF AIR");

    const qint64 ms = v.tcMs;
    const int fps = qMax(1, m_lib->display.fps);
    const int s = int(ms / 1000), f = int((ms % 1000) * fps / 1000);
    auto two = [](int x) { return QString("%1").arg(x, 2, 10, QChar('0')); };
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    mono.setPixelSize(12);
    p.setFont(mono);
    p.setPen(QColor("#64748b"));
    p.drawText(QRect(r.left() + 96, r.top() + 3, 160, r.height() - 3), Qt::AlignVCenter | Qt::AlignLeft,
               QString("TC %1:%2:%3:%4").arg(two(s / 3600), two(s / 60 % 60), two(s % 60), two(f)));

    // right: "Next program in Ns" during the last 30 s of a program and all through its break / ads,
    // or what the broadcast is doing while previewing
    QString right;
    bool dot = false;
    if (m_detached) {
        right = m_play->isLive() ? "Broadcast is on air" : "Broadcast is off air";
    } else if (v.onAir && seg) {
        const qint64 next = seg->type==Segment::Program ? seg->ms-m_play->segmentElapsed() : m_play->nextProgramCountdownMs();
        const qint64 end = m_play->programEndCountdownMs();
        const bool show = next >= 0 && next <= 30000;
        if (next >= 0 && show) {
            const int secs = int(std::ceil(next / 1000.0));
            right = "Next program in " + (secs < 60 ? QString("%1s").arg(secs) : formatDuration(secs));
            dot = true;
        }
    }
    if (!right.isEmpty()) {
        p.setFont(uiFont(12, true));
        const int tw = QFontMetrics(p.font()).horizontalAdvance(right);
        p.setPen(m_detached ? QColor("#94a3b8") : QColor("#e2e8f0"));
        p.drawText(QRect(r.right() - tw - 14, r.top() + 3, tw + 4, r.height() - 3), Qt::AlignVCenter | Qt::AlignRight, right);
        if (dot) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#ef4444"));
            p.drawEllipse(QPoint(r.right() - tw - 24, cy), 4, 4);
        }
    }
}
