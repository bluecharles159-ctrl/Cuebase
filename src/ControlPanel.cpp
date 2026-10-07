#include "ControlPanel.h"

#include <QAudioOutput>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>
#include <cmath>

#include "Data.h"
#include "Playout.h"
#include "NetworkOutputPanel.h"

namespace {
QLabel *caption(const QString &t) {
    auto *l = new QLabel(t);
    l->setStyleSheet("color:#94a3b8; font-weight:700; font-size:11px;");
    return l;
}
} // namespace

ControlPanel::ControlPanel(Library *lib, Playout *play, QWidget *parent, NetworkOutput *output)
    : QFrame(parent), m_lib(lib), m_play(play) {
    setObjectName("panel");
    setMinimumWidth(220);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *inner = new QWidget;
    sa->setWidget(inner);
    outer->addWidget(sa);
    auto *l = new QVBoxLayout(inner);
    l->setContentsMargins(14, 12, 10, 12);
    l->setSpacing(8);

    l->addWidget(caption("CONTROL"));
    m_chip = new QLabel;
    l->addWidget(m_chip);

    l->addWidget(caption("NOW"));
    m_now = new QLabel;
    m_now->setWordWrap(true);
    m_now->setStyleSheet("font-weight:700; font-size:14px;");
    l->addWidget(m_now);
    m_left = new QLabel;
    m_left->setStyleSheet("color:#94a3b8;");
    l->addWidget(m_left);

    l->addWidget(caption("UP NEXT"));
    m_next = new QLabel;
    m_next->setWordWrap(true);
    l->addWidget(m_next);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    m_info->setStyleSheet("color:#64748b; font-size:11px;");
    l->addWidget(m_info);

    m_start = new QPushButton;
    m_start->setObjectName("broadcast");
    m_pause = new QPushButton("⏸  Pause");
    m_skip = new QPushButton("⏭  Skip to next program");
    m_break = new QPushButton("⏏  Break now");
    l->addSpacing(6);
    l->addWidget(m_start);
    l->addWidget(m_pause);
    l->addWidget(m_skip);
    l->addWidget(m_break);

    l->addSpacing(6);
    l->addWidget(caption("VOLUME"));
    auto *volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, 100);
    volume->setValue(80);
    auto *mute = new QCheckBox("Mute");
    l->addWidget(volume);
    l->addWidget(mute);
    play->setVolume(0.8);
    if(output)l->addWidget(new NetworkOutputPanel(output,lib,play));

    // ---- one set of controls per timeline: the main one is always shown and broadcast; the others can be
    // muted, turned down or hidden while they keep running
    l->addSpacing(6);
    l->addWidget(caption("TIMELINES"));
    for (int k = 0; k < 4; ++k) {
        auto *box = new QFrame;
        box->setStyleSheet("QFrame#tlRow { background:#0f172a; border:1px solid #243044; border-radius:8px; }");
        box->setObjectName("tlRow");
        auto *bl = new QVBoxLayout(box);
        bl->setContentsMargins(10, 6, 10, 8);
        bl->setSpacing(4);
        auto *top = new QHBoxLayout;
        auto *name = new QLabel(k == 0 ? "Main  ·  on air" : QString("Timeline %1").arg(k + 1));
        name->setStyleSheet(k == 0 ? "font-weight:700;" : "font-weight:600; color:#cbd5e1;");
        auto *mute = new QCheckBox("Mute");
        mute->setChecked(lib->layers[k].muted);
        top->addWidget(name);
        top->addStretch();
        top->addWidget(mute);
        QCheckBox *show = nullptr;
        if (k > 0) {
            show = new QCheckBox("Show");
            show->setToolTip("Show this timeline's screen elements in the viewport");
            show->setChecked(lib->layers[k].visible);
            top->addWidget(show);
        }
        auto *vol = new QSlider(Qt::Horizontal);
        vol->setRange(0, 100);
        vol->setValue(lib->layers[k].volume);
        bl->addLayout(top);
        bl->addWidget(vol);
        l->addWidget(box);

        connect(mute, &QCheckBox::toggled, this, [lib, k](bool on) {
            lib->layers[k].muted = on;
            lib->commitLayers();
        });
        if (show)
            connect(show, &QCheckBox::toggled, this, [lib, k](bool on) {
                lib->layers[k].visible = on;
                lib->commitLayers();
            });
        connect(vol, &QSlider::valueChanged, this, [lib, k](int v) {
            lib->layers[k].volume = v;
            emit lib->layersChanged();  // applied live; saved when the slider is let go
        });
        connect(vol, &QSlider::sliderReleased, this, [lib] { lib->save(); });
    }

    l->addSpacing(6);
    l->addWidget(caption("ON SCREEN"));
    auto *host = new QWidget;
    m_elementsLay = new QVBoxLayout(host);
    m_elementsLay->setContentsMargins(0, 0, 0, 0);
    m_elementsLay->setSpacing(4);
    l->addWidget(host);
    l->addStretch();

    connect(m_start, &QPushButton::clicked, this, [this] {
        if (m_play->isLive()) m_play->stop();
        else m_play->startFromCue();  // from the playhead when it was dragged, else the beginning
    });
    connect(m_pause, &QPushButton::clicked, m_play, &Playout::togglePause);
    connect(m_skip, &QPushButton::clicked, m_play, &Playout::skip);
    connect(m_break, &QPushButton::clicked, m_play, &Playout::breakNow);
    connect(volume, &QSlider::valueChanged, this, [play](int v) { play->setVolume(v / 100.0); });
    connect(mute, &QCheckBox::toggled, this, [play](bool m) { play->setMuted(m); });

    connect(play, &Playout::stateChanged, this, [this] { refresh(); });
    connect(play, &Playout::segmentChanged, this, [this] { refresh(); });
    connect(play, &Playout::segmentsChanged, this, [this] { refresh(); });
    connect(play, &Playout::ticked, this, [this] { refresh(); });
    connect(lib, &Library::displayChanged, this, [this] { refresh(); });
    connect(lib, &Library::programsChanged, this, [this] { refresh(); });
    connect(lib, &Library::elementsChanged, this, [this] { rebuildElements(); });
    connect(lib, &Library::elementLiveChanged, this, [this] { rebuildElements(); });
    rebuildElements();
    refresh();
}

void ControlPanel::rebuildElements() {
    while (QLayoutItem *it = m_elementsLay->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete it;
    }
    bool any = false;
    for (const Element &e : m_lib->elements) {
        if (e.type == "Break") continue;
        any = true;
        auto *cb = new QCheckBox(e.name + "  ·  " + e.type);
        cb->setChecked(e.live);
        const QString id = e.id;
        connect(cb, &QCheckBox::toggled, this, [this, id](bool on) {
            for (Element &el : m_lib->elements)
                if (el.id == id) el.live = on;
            m_lib->commitElementLive();
        });
        m_elementsLay->addWidget(cb);
    }
    if (!any) {
        auto *none = new QLabel("No elements yet. Create some in Screen Elements.");
        none->setWordWrap(true);
        none->setStyleSheet("color:#64748b;");
        m_elementsLay->addWidget(none);
    }
}

void ControlPanel::refresh() {
    const bool live = m_play->isLive();
    const bool paused = m_play->state() == Playout::Paused;
    m_chip->setText(paused ? "● PAUSED" : live ? "● ON AIR" : "● OFF AIR");
    m_chip->setStyleSheet(paused ? "color:#f59e0b; font-weight:700; font-size:14px;"
                          : live ? "color:#ef4444; font-weight:700; font-size:14px;"
                                 : "color:#64748b; font-weight:700; font-size:14px;");

    const Segment *seg = m_play->current();
    if (!seg) {
        m_now->setText(m_lib->programs.isEmpty() ? "Nothing planned" : "Off air");
        m_left->clear();
    } else {
        m_now->setText(seg->type == Segment::Ad ? "Ad: " + seg->title
                     : seg->type == Segment::Break ? "Break (" + seg->title + ")"
                                                   : seg->title);
        const int left = int(std::ceil((seg->ms - m_play->segmentElapsed()) / 1000.0));
        m_left->setText(formatDuration(qMax(0, left)) + " remaining");
    }

    // next program that has not played yet
    QString next = "—";
    const int cur = m_play->currentProgramIndex();
    for (int i = qMax(0, cur + 1); i < m_lib->programs.size(); ++i) {
        if (m_play->finishedAt(m_lib->programs[i].id) == 0 || !live) {
            next = m_lib->programs[i].title;
            break;
        }
    }
    if (!live && !m_lib->programs.isEmpty()) next = m_lib->programs.first().title;
    m_next->setText(next);
    const DisplaySettings &d = m_lib->display;
    m_info->setText(QString("%1 · %2 · %3 fps · %4 Mbps")
                        .arg(d.channel, d.resolution, QString::number(d.fps), QString::number(d.bitrate, 'f', 1)));

    m_start->setText(live ? "■  Stop Broadcast" : "▶  Start Broadcast");
    m_start->setProperty("live", live);
    m_start->style()->unpolish(m_start);
    m_start->style()->polish(m_start);
    m_start->setEnabled(live || !m_lib->programs.isEmpty());
    m_pause->setText(paused ? "▶  Resume" : "⏸  Pause");
    m_pause->setEnabled(live);
    m_skip->setEnabled(live);
    m_break->setEnabled(m_play->canBreak());
}
