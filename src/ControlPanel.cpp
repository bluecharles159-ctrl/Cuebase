#include "ControlPanel.h"
#include "Continuity.h"
#include "UiControls.h"
#include "ElementLibrary.h"

#include <QAudioOutput>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QSettings>
#include <QVBoxLayout>
#include <cmath>

#include "Data.h"
#include "Playout.h"
#include "NetworkOutputPanel.h"
#include "NetworkOutput.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QWheelEvent>
#include "WorkspaceSetup.h"

namespace {
class VolumeFader final:public QSlider {
    double remainder=0.;
public:
    VolumeFader():QSlider(Qt::Horizontal){setSingleStep(1);setFocusPolicy(Qt::StrongFocus);}
protected:
    void wheelEvent(QWheelEvent *e) override {
        const double change=e->pixelDelta().y()!=0?e->pixelDelta().y()/10.:e->angleDelta().y()/120.;
        if(change==0){e->ignore();return;}
        remainder+=change*(e->modifiers()&Qt::ShiftModifier?1.:3.);
        const int steps=int(remainder);remainder-=steps;setValue(value()+steps);e->accept();
    }
};

QWidget *stereoMeter(NetworkOutput *network){
    auto *body=new QWidget;auto *layout=new QVBoxLayout(body);layout->setContentsMargins(0,0,0,0);
    auto *source=new QComboBox;source->addItems({"Program audio","Network mix"});source->setAccessibleName("Meter source");
    layout->addWidget(source);
    struct Channel {QProgressBar *bar;QLabel *level;};
    auto channel=[&](const QString &name){
        auto *row=new QHBoxLayout;auto *bar=new QProgressBar;auto *level=new QLabel("−∞");level->setFixedWidth(35);
        bar->setRange(-600,0);bar->setTextVisible(false);bar->setValue(-600);bar->setFixedHeight(8);
        bar->setAccessibleName(name+" peak level");bar->setObjectName("studioMeter"+name);
        bar->setStyleSheet("QProgressBar{border:0;border-radius:3px;background:palette(mid);}QProgressBar::chunk{background:#48b889;border-radius:3px;}");
        row->addWidget(new QLabel(name));row->addWidget(bar,1);row->addWidget(level);layout->addLayout(row);return Channel{bar,level};
    };
    const auto left=channel("L"),right=channel("R");source->setToolTip("Stereo sample peaks in dBFS, after volume and mute.");
    auto display=[](Channel c,double linear){
        const double db=qBound(-60.,20.*std::log10(qMax(linear,.001)),0.);
        c.bar->setValue(qRound(db*10));c.level->setText(db<=-60?"−∞":QString::number(db,'f',1));
    };
    auto reset=[=]{display(left,0);display(right,0);};
    auto *idle=new QTimer(body);idle->setSingleShot(true);idle->setInterval(250);
    QObject::connect(idle,&QTimer::timeout,body,reset);QObject::connect(source,&QComboBox::currentIndexChanged,body,reset);
    QObject::connect(network,&NetworkOutput::stereoPeak,body,[=](bool mix,double l,double r){
        if(mix!=(source->currentIndex()==1))return;
        display(left,l);display(right,r);idle->start();
    });
    return body;
}
QLabel *caption(const QString &t) {
    auto *l = new QLabel(t);
    l->setStyleSheet("color:palette(placeholder-text); font-weight:700; font-size:10px;");
    return l;
}
} // namespace

ControlPanel::ControlPanel(Library *lib, Playout *play, QWidget *parent, NetworkOutput *output)
    : QFrame(parent), m_lib(lib), m_play(play) {
    setObjectName("panel");
    setMinimumWidth(220);
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);
    auto *rail=new QVBoxLayout;rail->setContentsMargins(2,5,0,0);rail->setSpacing(4);outer->addLayout(rail);
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *inner = new QWidget;
    sa->setWidget(inner);
    outer->addWidget(sa);
    auto *pages=new QStackedWidget;pages->setObjectName("studioControlPages");
    auto *innerLayout=new QVBoxLayout(inner);innerLayout->setContentsMargins(0,0,0,0);innerLayout->addWidget(pages);
    auto *buttons=new QButtonGroup(this);
    auto page=[&](QString title,QString icon,QString name){
        auto *widget=new QWidget;auto *layout=new QVBoxLayout(widget);layout->setContentsMargins(6,8,6,8);layout->setSpacing(5);
        const int index=pages->addWidget(widget);auto *b=new QToolButton;b->setObjectName(name);b->setAccessibleName(title);
        b->setIcon(uiIcon(icon));b->setToolTip(title);b->setCheckable(true);b->setFixedSize(28,26);buttons->addButton(b);
        rail->addWidget(b);b->setChecked(index==0);
        connect(b,&QToolButton::clicked,pages,[pages,index]{pages->setCurrentIndex(index);});
        return layout;
    };
    auto *l=page("Broadcast","broadcast","controlBroadcast");

    l->addWidget(caption("CONTROL"));
    m_chip = new QLabel;
    connect(lib,&Library::productionChanged,this,[this]{refresh();});
    l->addWidget(m_chip);

    l->addWidget(caption("NOW"));
    m_now = new QLabel;
    m_now->setWordWrap(true);
    m_now->setStyleSheet("font-weight:700; font-size:10px;");
    l->addWidget(m_now);
    m_left = new QLabel;
    m_left->setStyleSheet("color:palette(placeholder-text);");
    l->addWidget(m_left);

    l->addWidget(caption("UP NEXT"));
    m_next = new QLabel;
    m_next->setWordWrap(true);
    l->addWidget(m_next);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    m_info->setStyleSheet("color:palette(placeholder-text); font-size:10px;");
    l->addWidget(m_info);

    m_start = new QPushButton;
    m_start->setObjectName("broadcast");
    m_pause = new QPushButton(uiIcon("pause"),"Pause");
    m_skip = new QPushButton(uiIcon("step"),"Next program");
    m_break = new QPushButton(uiIcon("composition"),"Take break");
    l->addSpacing(6);
    l->addWidget(m_start);
    l->addWidget(m_pause);
    l->addWidget(m_skip);
    l->addWidget(m_break);
    l->addWidget(continuityControls(lib,play,output));
    if(output)l->addWidget(new NetworkOutputPanel(output,lib,play));
    l->addStretch();l=page("Audio controls","audio","controlAudio");
    if(output){
        output->setMeterProgram(play->primaryPlayer());l->addWidget(caption("LEVELS · dBFS"));l->addWidget(stereoMeter(output));
    }
    l->addWidget(caption("VOLUME"));
    auto *volume = new VolumeFader;
    volume->setRange(0, 100);volume->setSingleStep(1);volume->setPageStep(5);volume->setObjectName("masterVolume");volume->setToolTip("Broadcast volume · Scroll vertically to adjust");
    volume->setValue(80);
    auto *mute = new QCheckBox("Mute");
    l->addWidget(volume);
    l->addWidget(mute);
    play->setVolume(0.8);

    // ---- one set of controls per timeline: the main one is always shown and broadcast; the others can be
    // muted, turned down or hidden while they keep running
    l->addSpacing(6);
    auto *mixerToggle=new QToolButton;mixerToggle->setText("Layer mixer");mixerToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);mixerToggle->setIcon(uiIcon("right"));mixerToggle->setCheckable(true);l->addWidget(mixerToggle);
    auto *mixer=new QWidget;auto *mixerLayout=new QVBoxLayout(mixer);mixerLayout->setContentsMargins(0,0,0,0);mixerLayout->setSpacing(4);l->addWidget(mixer);mixer->setVisible(QSettings().value("studio/mixerExpanded",false).toBool());mixerToggle->setChecked(!mixer->isHidden());connect(mixerToggle,&QToolButton::toggled,this,[=](bool on){mixer->setVisible(on);mixerToggle->setIcon(uiIcon(on?"down":"right"));QSettings().setValue("studio/mixerExpanded",on);});
    for (int k = 0; k < 4; ++k) {
        auto *box = new QFrame;
        box->setStyleSheet("QFrame#tlRow { background:#35363a; border:1px solid #46484e; border-radius:5px; }");
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
        auto *vol = new VolumeFader;
        vol->setRange(0, 100);
        vol->setValue(lib->layers[k].volume);
        bl->addLayout(top);
        bl->addWidget(vol);
        mixerLayout->addWidget(box);

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
    l->addStretch();l=page("On-screen elements","composition","controlGraphics");rail->addStretch();
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
        auto *row=new QWidget;auto *layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);auto *thumb=new QLabel;thumb->setPixmap(QPixmap::fromImage(elementThumbnail(e)).scaled(48,27,Qt::KeepAspectRatio,Qt::SmoothTransformation));layout->addWidget(thumb);auto *name=new QLabel(e.name);name->setMinimumWidth(0);name->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);name->setToolTip(e.name);layout->addWidget(name,1);
        auto *show=new QPushButton(uiIcon("play"),QString());auto *hide=new QPushButton(uiIcon("hidden"),QString());show->setToolTip("Show / replay entrance");hide->setToolTip("Play exit / hide");for(auto *button:{show,hide}){button->setFixedSize(24,24);button->setIconSize(QSize(14,14));button->setStyleSheet("padding:2px;");}layout->addWidget(show);layout->addWidget(hide);const QString id=e.id;
        auto trigger=[this,id](bool on){for(auto &el:m_lib->elements)if(el.id==id){auto d=ElementDesign::fromJson(el.design);QString cue;for(auto value:d.animationCues){auto c=value.toObject();if(c["hold"].toBool(true)==on){cue=c["id"].toString();break;}}if(!d.animationCues.isEmpty())m_play->triggerElementAnimation(id,cue);else {el.live=on;m_lib->commitElementLive();}break;}};
        connect(show,&QPushButton::clicked,this,[trigger]{trigger(true);});connect(hide,&QPushButton::clicked,this,[trigger]{trigger(false);});m_elementsLay->addWidget(row);
    }
    if (!any) {
        auto *none = new QLabel("No elements yet. Create some in Screen Elements.");
        none->setWordWrap(true);
        none->setStyleSheet("color:palette(placeholder-text);");
        m_elementsLay->addWidget(none);
    }
}

void ControlPanel::refresh() {
    const bool live = m_play->isLive();
    const bool paused = m_play->state() == Playout::Paused;
    const bool editing=WorkspaceSetup::editingOnly(*m_lib);
    m_chip->setText(paused ? "● PAUSED" : editing ? (live?"● PREVIEWING":"● PREVIEW") : live ? "● ON AIR" : "● OFF AIR");
    m_chip->setStyleSheet(paused ? "color:#f59e0b; font-weight:700; font-size:10px;"
                          : live ? "color:#ef4444; font-weight:700; font-size:11px;"
                                 : "color:#64748b; font-weight:700; font-size:11px;");

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

    m_start->setText(editing?(live?"Stop preview":"Preview timeline"):(live?"Stop playout":"Start playout"));m_start->setIcon(uiIcon(live?"stop":"play"));
    m_start->setProperty("live", live);
    m_start->style()->unpolish(m_start);
    m_start->style()->polish(m_start);
    m_start->setEnabled(live || !m_lib->programs.isEmpty());
    m_pause->setText(paused ? "Resume" : "Pause");m_pause->setIcon(uiIcon(paused?"play":"pause"));
    m_pause->setEnabled(live);
    m_skip->setEnabled(live);
    m_break->setEnabled(m_play->canBreak());
}
