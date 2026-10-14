#include "StudioElements.h"
#include "StudioDashboard.h"
#include "UiControls.h"
#include "AnimationCuePanel.h"
#include <QJsonDocument>
#include <QJsonArray>
#include "EditorPanel.h"
#include "Replay.h"
#include "SceneEffects.h"
#include "CrewWorkspace.h"
#include "BuildContextAction.h"
#include <QMenu>

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QAbstractSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <memory>

#include "ColorManagement.h"
#include "Data.h"
#include "DragDrop.h"
#include "Playout.h"

namespace {

const char *kMediaFilter = "Media (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;All files (*)";

QLabel *heading(const QString &title, const QString &sub) {
    auto *l = new QLabel(QString("<div style='font-size:12px; font-weight:700;'>%1</div>"
                                 "<div style='color:#94a3b8;'>%2</div>").arg(title, sub));
    l->setWordWrap(true);
    return l;
}

QLabel *messageLabel() {
    auto *l = new QLabel;
    l->setWordWrap(true);
    return l;
}
void say(QLabel *l, const QString &text, bool ok = false) {
    l->setStyleSheet(ok ? "color:#22c55e;" : "color:#f87171;");
    l->setText(text);
}

// Spin boxes and drop-downs swallow the mouse wheel, which made the page impossible to scroll while the
// pointer was over one. They now only react to the wheel once clicked into; otherwise the wheel scrolls the page.
class WheelGuard : public QObject {
public:
    WheelGuard(QScrollArea *sa) : QObject(sa), m_sa(sa) {}
protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() != QEvent::Wheel) return false;
        auto *w = qobject_cast<QWidget *>(o);
        if (!w || w->hasFocus()) return false;
        QApplication::sendEvent(m_sa->verticalScrollBar(), e);  // scroll the page instead (up and down)
        return true;
    }
private:
    QScrollArea *m_sa;
};

QWidget *scrollWrap(QWidget *page) {
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    sa->setWidget(page);
    auto *guard = new WheelGuard(sa);
    const auto spins = page->findChildren<QAbstractSpinBox *>();
    for (QAbstractSpinBox *w : spins) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(guard);
    }
    const auto combos = page->findChildren<QComboBox *>();
    for (QComboBox *w : combos) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(guard);
    }
    return sa;
}

// A blue strip shown while a "+" on the timeline is being filled in.
QFrame *pendingBanner(Library *lib, const QString &kind, const QString &sentence) {
    auto *f = new QFrame;
    f->setStyleSheet("QFrame { background:#12315f; border:1px solid #2563eb; border-radius:8px; }"
                     "QLabel { border:none; background:transparent; color:#bfdbfe; }");
    auto *h = new QHBoxLayout(f);
    h->setContentsMargins(10, 6, 6, 6);
    auto *label = new QLabel;
    label->setWordWrap(true);
    auto *cancel = new QPushButton("Cancel");
    cancel->setCursor(Qt::PointingHandCursor);
    h->addWidget(label, 1);
    h->addWidget(cancel);
    auto update = [=] {
        const bool on = lib->pendingKind == kind;
        f->setVisible(on);
        if (!on) return;
        const ProgramItem *p = lib->program(lib->pendingAfter);
        label->setText(sentence + (p ? QString(" after \u201c%1\u201d on the timeline.").arg(p->title) : "."));
    };
    QObject::connect(lib, &Library::pendingChanged, f, update);
    QObject::connect(lib, &Library::programsChanged, f, update);
    QObject::connect(cancel, &QPushButton::clicked, f, [lib] { lib->clearPending(); });
    update();
    return f;
}

// Is the "+" that was pressed still there?
bool slotPending(Library *lib, const QString &kind) {
    return lib->pendingKind == kind && lib->program(lib->pendingAfter);
}

// Puts an ad on the timeline as its own draggable block: at the "+" that was pressed, or at the end.
QString placeAd(Library *lib, Playout *play, const QString &adId) {
    const Ad *ad = lib->ad(adId);
    if (!ad) return {};
    if (lib->pendingKind == "Layer Ad") {  // from the + under the timeline: it goes on an extra timeline
        Placement pl;
        pl.id = Library::newId();
        pl.type = "Ad";
        pl.refId = ad->id;
        pl.name = ad->name;
        pl.file = ad->file;
        pl.seconds = ad->seconds;
        pl.layer = qBound(1, lib->pendingLayer, 3);
        pl.startMs = play->playheadMs();
        lib->placements.append(pl);
        lib->commitPlacements();
        lib->clearPending();
        return QString("Ad placed on timeline %1 at %2. Drag it to move it.").arg(pl.layer + 1).arg(formatDuration(int(pl.startMs / 1000)));
    }
    const bool slot = slotPending(lib, "Ad Center");
    const QString after = slot ? lib->pendingAfter : (lib->programs.isEmpty() ? QString() : lib->programs.last().id);
    const qint64 at = after.isEmpty() ? 0 : play->groupEndMs(after);
    ProgramItem item;
    item.id = Library::newId();
    item.title = ad->name;
    item.kind = "Ad";
    item.adId = ad->id;
    item.seconds = ad->seconds;
    lib->shiftPlacements(at, qint64(ad->seconds) * 1000);  // things placed later keep their place after the ad
    lib->insertAfter(after, item);
    lib->clearPending();
    return slot ? "Ad placed at the + on the timeline. Drag it to move it."
                : "Ad added to the end of the timeline. Drag it to move it.";
}

// Puts a screen element on the timeline: at the cut point of the "+" that was pressed, or at the playhead.
QString placeElement(Library *lib, Playout *play, const QString &elementId) {
    const Element *e = lib->element(elementId);
    if (!e || e->type == "Break") return {};
    const bool slot = slotPending(lib, "Element");
    Placement pl;
    pl.layer = lib->pendingKind == "Layer Element" ? qBound(1, lib->pendingLayer, 3) : 1;
    pl.id = Library::newId();
    pl.type = "Element";
    pl.refId = e->id;
    pl.name = e->name;
    pl.seconds = e->type == "Custom" ? qMax(1, e->seconds) : 8;
    pl.durationMs=e->lengthMs();
    pl.startMs = slot ? play->groupEndMs(lib->pendingAfter) : play->playheadMs();
    lib->placements.append(pl);
    lib->commitPlacements();
    lib->clearPending();
    return QString("\u201c%1\u201d placed on timeline %2 at %3. Drag it to move it; right-click it to change its length.")
        .arg(e->name).arg(pl.layer + 1).arg(formatDuration(int(pl.startMs / 1000)));
}

void selectById(QComboBox *box, const QString &id) {
    const int i = box->findData(id);
    box->setCurrentIndex(i >= 0 ? i : 0);
}

// A hidden player that reads a media file's length so the duration boxes fill themselves in.
QMediaPlayer *makeProbe(QObject *owner, std::function<void(int)> onSeconds) {
    auto *pl = new QMediaPlayer(owner);
    QObject::connect(pl, &QMediaPlayer::durationChanged, owner, [onSeconds](qint64 ms) {
        if (ms > 0) onSeconds(int((ms + 500) / 1000));
    });
    return pl;
}

QPushButton *smallButton(const QString &text) {
    auto *b = new QPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

// "Transition": how something moves in when it starts and moves out when it ends.
struct FxBox : QFrame {
    QComboBox *in, *out;
    QSpinBox *inMs, *outMs;
    explicit FxBox(QWidget *parent = nullptr) : QFrame(parent) {
        setObjectName("fxBox");
        setStyleSheet("QFrame#fxBox { background:palette(window); border:1px solid palette(mid); border-radius:8px; }");
        auto *l = new QVBoxLayout(this);
        l->setContentsMargins(10, 8, 10, 10);
        l->setSpacing(6);
        auto *cap = new QLabel("TRANSITION");
        cap->setStyleSheet("color:palette(placeholder-text); font-weight:700; font-size:10px;");
        l->addWidget(cap);
        auto make = [this](QComboBox *&box, QSpinBox *&ms) {
            box = new QComboBox;
            box->addItems(transitionKinds());
            ms = new FrameInput;
            ms->setRange(100, 5000);
            ms->setSingleStep(100);
            ms->setValue(600);
            ms->setSuffix(" ms");
            auto *row = new QHBoxLayout;
            row->addWidget(box, 1);
            row->addWidget(ms);
            return row;
        };
        auto *form = new QFormLayout;
        form->addRow("Move in", make(in, inMs));
        form->addRow("Move out", make(out, outMs));
        l->addLayout(form);
        auto sync = [this] {
            inMs->setEnabled(in->currentText() != "None");
            outMs->setEnabled(out->currentText() != "None");
        };
        QObject::connect(in, &QComboBox::currentTextChanged, this, sync);
        QObject::connect(out, &QComboBox::currentTextChanged, this, sync);
        sync();
    }
    Transition get() const {
        Transition t;
        t.in = in->currentText();
        t.out = out->currentText();
        t.inMs = inMs->value();
        t.outMs = outMs->value();
        return t;
    }
    void set(const Transition &t) {
        in->setCurrentText(t.in);
        out->setCurrentText(t.out);
        inMs->setValue(t.inMs);
        outMs->setValue(t.outMs);
    }
};

// ------------------------------------------------------------------ Display

QWidget *makeDisplayPage(Library *lib, Playout *play) {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(heading("Display", "Output, format and render settings for the viewport and the broadcast."));

    auto section = [&](const QString &name) {
        auto *cap = new QLabel(name);
        cap->setStyleSheet("color:palette(placeholder-text); font-weight:700; font-size:10px; padding-top:8px;");
        l->addWidget(cap);
    };
    auto dsp = [](double lo, double hi, double step, int dec, const QString &suffix = QString()) {
        auto *s = new ValueInput;
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setDecimals(dec);
        s->setSuffix(suffix);
        return s;
    };

    // ---- output
    auto *channel = new QComboBox;
    channel->addItems({"TV1 — Main", "TV2", "TV3"});
    auto *bitrate = dsp(1.0, 50.0, 0.5, 2, " Mbps");
    auto *wmOn = new QCheckBox("Show watermark");
    auto *wmText = new QLineEdit;
    auto *safe = new QCheckBox("Show safe-area guides");
    auto *outForm = new QFormLayout;
    outForm->addRow("Output channel", channel);
    outForm->addRow("Bitrate", bitrate);
    outForm->addRow(wmOn);
    outForm->addRow("Watermark text", wmText);
    outForm->addRow(safe);
    l->addLayout(outForm);

    // ---- format: resolution, aspect ratio, frame rate (Advanced: pixel aspect X / Y and frame rate base)
    section("FORMAT");
    auto *preset = new QComboBox;
    preset->addItems({"Custom", "1920 × 1080  Full HD", "1280 × 720  HD", "2560 × 1440  QHD", "3840 × 2160  4K UHD", "1080 × 1920  Vertical",
                      "1024 × 768  4:3", "2560 × 1080  21:9", "1080 × 1080  Square"});
    auto *rx = new FrameInput, *ry = new FrameInput, *pct = new FrameInput;
    rx->setRange(16, 8192);
    ry->setRange(16, 8192);
    pct->setRange(10, 400);
    pct->setSuffix(" %");
    auto *ratio = new QComboBox;
    ratio->addItems({"Custom", "16:9", "4:3", "21:9", "1:1", "9:16", "3:2"});
    auto *aspectLabel = new QLabel;
    aspectLabel->setStyleSheet("color:palette(placeholder-text);");
    auto *fps = new QComboBox;
    fps->addItems({"24", "25", "30", "50", "60"});
    auto *adv = new QToolButton;
    adv->setText("Advanced");
    adv->setCheckable(true);
    adv->setArrowType(Qt::RightArrow);
    adv->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    adv->setStyleSheet("QToolButton { border:none; color:palette(placeholder-text); font-weight:600; padding:2px 0; }");
    auto *advBox = new QWidget;
    auto *advForm = new QFormLayout(advBox);
    advForm->setContentsMargins(0, 0, 0, 0);
    auto *pax = dsp(0.1, 10, 0.1, 3), *pay = dsp(0.1, 10, 0.1, 3), *base = dsp(1.0, 1.01, 0.001, 3);
    advForm->addRow("Aspect X", pax);
    advForm->addRow("Aspect Y", pay);
    advForm->addRow("Frame rate base", base);
    advBox->hide();

    auto *fmt = new QFormLayout;
    fmt->addRow("Preset", preset);
    auto *resRow = new QHBoxLayout;
    resRow->addWidget(rx);
    resRow->addWidget(new QLabel("×"));
    resRow->addWidget(ry);
    fmt->addRow("Resolution", resRow);
    fmt->addRow("Scale", pct);
    fmt->addRow("Aspect ratio", ratio);
    fmt->addRow("", aspectLabel);
    fmt->addRow("Frame rate", fps);
    l->addLayout(fmt);
    l->addWidget(adv);
    l->addWidget(advBox);

    // ---- render region
    section("RENDER REGION");
    auto *regOn = new QCheckBox("Output only this region");
    auto *rx0 = dsp(0, 100, 1, 0, " %"), *rx1 = dsp(0, 100, 1, 0, " %"), *ry0 = dsp(0, 100, 1, 0, " %"), *ry1 = dsp(0, 100, 1, 0, " %");
    auto *regForm = new QFormLayout;
    regForm->addRow(regOn);
    regForm->addRow("X min", rx0);
    regForm->addRow("X max", rx1);
    regForm->addRow("Y min", ry0);
    regForm->addRow("Y max", ry1);
    l->addLayout(regForm);

    // ---- motion blur
    section("MOTION BLUR");
    auto *mb = new QCheckBox("Motion blur on 3D screen elements");
    auto *shutter = dsp(0.05, 2.0, 0.05, 2);
    auto *samples = new FrameInput;
    samples->setRange(2, 16);
    auto *mbForm = new QFormLayout;
    mbForm->addRow(mb);
    mbForm->addRow("Shutter", shutter);
    mbForm->addRow("Samples", samples);
    l->addLayout(mbForm);

    // ---- colour management
    section("COLOR MANAGEMENT");
    auto *view = new QComboBox;
    view->addItems(colorViews());
    auto *look = new QComboBox;
    look->addItems(colorLooks());
    auto *expo = dsp(-5, 5, 0.1, 2, " EV");
    auto *gam = dsp(0.2, 3.0, 0.05, 2);
    auto *cmForm = new QFormLayout;
    cmForm->addRow("View transform", view);
    cmForm->addRow("Look", look);
    cmForm->addRow("Exposure", expo);
    cmForm->addRow("Gamma", gam);
    l->addLayout(cmForm);

    auto *lockNote = new QLabel("Channel, resolution, aspect, frame rate and bitrate are locked while you are on air.");
    lockNote->setWordWrap(true);
    lockNote->setStyleSheet("color:#f59e0b;");
    lockNote->hide();
    l->addWidget(lockNote);
    l->addStretch();

    auto loading = std::make_shared<bool>(true);
    auto presetRes = [](const QString &p) -> QPair<int, int> {
        static const QHash<QString, QPair<int, int>> m{{"1920 × 1080  Full HD", {1920, 1080}}, {"1280 × 720  HD", {1280, 720}},
            {"2560 × 1440  QHD", {2560, 1440}}, {"3840 × 2160  4K UHD", {3840, 2160}}, {"1080 × 1920  Vertical", {1080, 1920}},
            {"1024 × 768  4:3", {1024, 768}}, {"2560 × 1080  21:9", {2560, 1080}}, {"1080 × 1080  Square", {1080, 1080}}};
        return m.value(p, {0, 0});
    };
    auto showAspect = [=] {
        const DisplaySettings &d = lib->display;
        aspectLabel->setText(QString("Picture aspect %1 : 1").arg(d.aspect(), 0, 'f', 3));
    };
    auto load = [=] {
        *loading = true;
        const DisplaySettings &d = lib->display;
        channel->setCurrentText(d.channel);
        bitrate->setValue(d.bitrate);
        wmOn->setChecked(d.watermarkOn);
        wmText->setText(d.watermarkText);
        safe->setChecked(d.safeArea);
        rx->setValue(d.resX());
        ry->setValue(d.resY());
        pct->setValue(d.resScale);
        fps->setCurrentText(QString::number(d.fps));
        pax->setValue(d.pixAspX);
        pay->setValue(d.pixAspY);
        base->setValue(d.fpsBase);
        regOn->setChecked(d.regionOn);
        rx0->setValue(d.rx0 * 100);
        rx1->setValue(d.rx1 * 100);
        ry0->setValue(d.ry0 * 100);
        ry1->setValue(d.ry1 * 100);
        mb->setChecked(d.motionBlur);
        shutter->setValue(d.shutter);
        samples->setValue(d.mbSamples);
        view->setCurrentText(d.colorView);
        look->setCurrentText(d.colorLook);
        expo->setValue(d.exposure);
        gam->setValue(d.gamma);
        // which presets match what is set?
        preset->setCurrentIndex(0);
        for (int i = 1; i < preset->count(); ++i)
            if (presetRes(preset->itemText(i)) == qMakePair(d.resX(), d.resY())) preset->setCurrentIndex(i);
        ratio->setCurrentIndex(0);
        static const QList<QPair<QString, double>> rs{{"16:9", 16.0 / 9}, {"4:3", 4.0 / 3}, {"21:9", 21.0 / 9}, {"1:1", 1.0}, {"9:16", 9.0 / 16}, {"3:2", 1.5}};
        for (const auto &r : rs)
            if (qAbs(d.aspect() - r.second) < 0.01) ratio->setCurrentText(r.first);
        showAspect();
        *loading = false;
    };
    load();

    auto apply = [=] {
        if (*loading) return;
        DisplaySettings &s = lib->display;
        s.channel = channel->currentText();
        s.bitrate = bitrate->value();
        s.watermarkOn = wmOn->isChecked();
        s.watermarkText = wmText->text();
        s.safeArea = safe->isChecked();
        s.resolution = QString("%1x%2").arg(rx->value()).arg(ry->value());
        s.resScale = pct->value();
        s.fps = fps->currentText().toInt();
        s.pixAspX = pax->value();
        s.pixAspY = pay->value();
        s.fpsBase = base->value();
        s.regionOn = regOn->isChecked();
        s.rx0 = qMin(rx0->value(), rx1->value() - 1) / 100.0;
        s.rx1 = qMax(rx1->value(), rx0->value() + 1) / 100.0;
        s.ry0 = qMin(ry0->value(), ry1->value() - 1) / 100.0;
        s.ry1 = qMax(ry1->value(), ry0->value() + 1) / 100.0;
        s.motionBlur = mb->isChecked();
        s.shutter = shutter->value();
        s.mbSamples = samples->value();
        s.colorView = view->currentText();
        s.colorLook = look->currentText();
        s.exposure = expo->value();
        s.gamma = gam->value();
        showAspect();
        lib->commitDisplay();
    };
    for (QComboBox *c : {channel, fps, view, look}) QObject::connect(c, &QComboBox::currentTextChanged, page, apply);
    for (QDoubleSpinBox *d : {bitrate, pax, pay, base, rx0, rx1, ry0, ry1, shutter, expo, gam}) QObject::connect(d, &QDoubleSpinBox::valueChanged, page, apply);
    for (QSpinBox *d : {rx, ry, pct, samples}) QObject::connect(d, &QSpinBox::valueChanged, page, apply);
    for (QCheckBox *c : {wmOn, safe, regOn, mb}) QObject::connect(c, &QCheckBox::toggled, page, apply);
    QObject::connect(wmText, &QLineEdit::textChanged, page, apply);

    QObject::connect(preset, &QComboBox::activated, page, [=](int i) {
        const auto r = presetRes(preset->itemText(i));
        if (r.first == 0) return;
        rx->setValue(r.first);  // each setValue applies
        ry->setValue(r.second);
        load();
    });
    QObject::connect(ratio, &QComboBox::activated, page, [=](int i) {
        static const QHash<QString, QPair<int, int>> r{{"16:9", {16, 9}}, {"4:3", {4, 3}}, {"21:9", {21, 9}}, {"1:1", {1, 1}}, {"9:16", {9, 16}}, {"3:2", {3, 2}}};
        const auto v = r.value(ratio->itemText(i), {0, 0});
        if (v.first == 0) return;
        // keep the width, change the height; square pixels
        *loading = true;
        pax->setValue(1);
        pay->setValue(1);
        *loading = false;
        ry->setValue(qMax(16, qRound(double(rx->value()) * v.second / v.first)));
        load();
    });
    QObject::connect(adv, &QToolButton::toggled, page, [=](bool on) {
        advBox->setVisible(on);
        adv->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });
    QObject::connect(regOn, &QCheckBox::toggled, page, [=](bool on) {
        for (QWidget *w : {static_cast<QWidget *>(rx0), static_cast<QWidget *>(rx1), static_cast<QWidget *>(ry0), static_cast<QWidget *>(ry1)}) w->setEnabled(on);
    });
    for (QWidget *w : {static_cast<QWidget *>(rx0), static_cast<QWidget *>(rx1), static_cast<QWidget *>(ry0), static_cast<QWidget *>(ry1)}) w->setEnabled(regOn->isChecked());
    QObject::connect(mb, &QCheckBox::toggled, page, [=](bool on) { shutter->setEnabled(on); samples->setEnabled(on); });
    shutter->setEnabled(mb->isChecked());
    samples->setEnabled(mb->isChecked());

    QObject::connect(play, &Playout::stateChanged, page, [=] {
        const bool ok = !play->isLive();
        for (QWidget *w : {static_cast<QWidget *>(channel), static_cast<QWidget *>(preset), static_cast<QWidget *>(rx), static_cast<QWidget *>(ry),
                           static_cast<QWidget *>(ratio), static_cast<QWidget *>(fps), static_cast<QWidget *>(bitrate), static_cast<QWidget *>(pax),
                           static_cast<QWidget *>(pay), static_cast<QWidget *>(base)})
            w->setEnabled(ok);
        lockNote->setVisible(!ok);
    });
    return page;
}

// ---------------------------------------------------------------------- Ads

QWidget *makeAdsPage(Library *lib, Playout *play) {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(heading("Ads", "Create ads and store them here. Drag one onto the timeline, attach it to a program "
                                "in Programs, or select it and press Place on timeline."));
    l->addWidget(pendingBanner(lib, "Ad Center", "Pick an ad and press Place on timeline to put it"));
    l->addWidget(pendingBanner(lib, "Layer Ad", "Pick an ad and press Place on timeline to put it on its own timeline"));

    auto *list = new QListWidget;
    list->setMinimumHeight(120);
    auto *name = new QLineEdit;
    name->setPlaceholderText("Ad name");
    auto *secs = new FrameInput;
    secs->setRange(1, 3600);
    secs->setValue(15);
    secs->setSuffix(" sec");
    auto *file = new QLineEdit;
    file->setPlaceholderText("Media file (optional)");
    auto *browse = smallButton("Browse...");
    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(file, 1);
    fileRow->addWidget(browse);
    auto *form = new QFormLayout;
    form->addRow("Name", name);
    form->addRow("Length", secs);
    form->addRow("Media", fileRow);
    auto *save = smallButton("Add Ad");
    save->setObjectName("save");
    auto *fresh = smallButton("New");
    auto *remove = smallButton("Remove");
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(save);
    buttons->addWidget(fresh);
    buttons->addStretch();
    buttons->addWidget(remove);
    auto *msg = messageLabel();
    auto *place = smallButton("Place on timeline");
    place->setToolTip("Adds the selected ad as its own block on the timeline. Drag it there to move it.");
    auto *fx = new FxBox;
    l->addWidget(list);
    l->addLayout(form);
    l->addWidget(fx);
    l->addLayout(buttons);
    l->addWidget(place);
    l->addWidget(msg);
    l->addStretch();

    auto selId = [=] { auto *it = list->currentItem(); return it ? it->data(Qt::UserRole).toString() : QString(); };
    auto refresh = [=] {
        const QString keep = selId();
        QSignalBlocker b(list);
        list->clear();
        for (const Ad &a : lib->ads) {
            auto *it = new QListWidgetItem(QString("%1   ·   %2%3").arg(a.name, formatDuration(a.seconds),
                                                                          a.file.isEmpty() ? "" : "   ·   video"));
            it->setData(Qt::UserRole, a.id);
            list->addItem(it);
            if (a.id == keep) list->setCurrentItem(it);
        }
    };
    auto clearForm = [=] {
        list->clearSelection();
        list->setCurrentItem(nullptr);
        name->clear();
        file->clear();
        secs->setValue(15);
        fx->set(Transition());
        save->setText("Add Ad");
    };
    auto probe = makeProbe(page, [=](int s) { secs->setValue(qBound(1, s, 3600)); });

    QObject::connect(browse, &QPushButton::clicked, page, [=] {
        const QString path = QFileDialog::getOpenFileName(page, "Choose ad media", QString(), kMediaFilter);
        if (path.isEmpty()) return;
        file->setText(path);
        probe->setSource(QUrl::fromLocalFile(path));
    });
    QObject::connect(list, &QListWidget::currentItemChanged, page, [=](QListWidgetItem *it) {
        if (!it) return;
        const Ad *a = lib->ad(it->data(Qt::UserRole).toString());
        if (!a) return;
        name->setText(a->name);
        secs->setValue(a->seconds);
        file->setText(a->file);
        fx->set(a->fx);
        save->setText("Update Ad");
        msg->clear();
    });
    QObject::connect(fresh, &QPushButton::clicked, page, clearForm);
    QObject::connect(save, &QPushButton::clicked, page, [=] {
        const QString n = name->text().trimmed();
        if (n.isEmpty()) {
            say(msg, "Enter a name for the ad.");
            name->setFocus();
            return;
        }
        const QString id = selId();
        bool updated = false;
        for (Ad &a : lib->ads)
            if (a.id == id) {
                a.name = n;
                a.seconds = secs->value();
                a.file = file->text().trimmed();
                a.fx = fx->get();
                updated = true;
            }
        if (!updated) lib->ads.append(Ad{Library::newId(), n, file->text().trimmed(), secs->value(), fx->get()});
        lib->commitAds();
        if (!updated) clearForm();
        say(msg, updated ? "Ad updated." : "Ad added. You can attach it to a program now.", true);
    });
    QObject::connect(remove, &QPushButton::clicked, page, [=] {
        const QString id = selId();
        if (id.isEmpty()) {
            say(msg, "Select an ad to remove.");
            return;
        }
        bool touched = false;
        for (int i = lib->programs.size() - 1; i >= 0; --i) {
            ProgramItem &p = lib->programs[i];
            if (p.adId != id) continue;
            if (p.isAdSpot()) lib->programs.removeAt(i);  // its block on the timeline goes too
            else p.adId.clear();
            touched = true;
        }
        for (int i = 0; i < lib->ads.size(); ++i)
            if (lib->ads[i].id == id) lib->ads.removeAt(i);
        if (touched) lib->commitPrograms();
        lib->commitAds();
        clearForm();
        say(msg, "Ad removed.", true);
    });
    QObject::connect(place, &QPushButton::clicked, page, [=] {
        const QString id = selId();
        if (id.isEmpty()) {
            say(msg, "Select an ad in the list first.");
            return;
        }
        const QString done = placeAd(lib, play, id);
        if (done.isEmpty()) say(msg, "That ad no longer exists.");
        else say(msg, done, true);
    });
    auto placeLabel = [=] {
        const ProgramItem *p = slotPending(lib, "Ad Center") ? lib->program(lib->pendingAfter) : nullptr;
        place->setText(p ? QString("Place after \u201c%1\u201d").arg(p->title) : "Place on timeline");
    };
    QObject::connect(lib, &Library::pendingChanged, page, placeLabel);
    QObject::connect(lib, &Library::adsChanged, page, refresh);
    makeListDraggable(list, [=](QListWidgetItem *it, DragPayload &d) {
        const Ad *a = lib->ad(it->data(Qt::UserRole).toString());
        if (!a) return false;
        d.kind = "ad";
        d.id = a->id;
        d.label = "AD · " + a->name;
        d.color = QColor("#16a34a");
        d.widthPx = qBound(90, a->seconds * 3, 240);
        d.grabDx = qMin(d.widthPx, 260) / 2;
        return true;
    });
    placeLabel();
    refresh();
    return page;
}

// -------------------------------------------------------- Program planner

QWidget *makePlannerPage(Library *lib, Playout *play) {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(heading("Programs", "Add programs, describe them and set how they play, one after the other. "
                                     "Click a program card to open it here, or drag it onto the timeline."));
    l->addWidget(pendingBanner(lib, "Clip", "Fill in the clip below and press Add Clip to insert it"));

    auto *list = new QListWidget;
    list->setMinimumHeight(210);
    list->setObjectName("programLibrary");list->setSpacing(8);list->setIconSize(QSize(104,60));
    list->setStyleSheet("QListWidget{background:palette(window);border:1px solid palette(mid);border-radius:10px;padding:8px;} QListWidget::item{padding:10px;border:1px solid palette(mid);border-radius:8px;} QListWidget::item:selected{background:#344c61;border-color:#719aca;} QListWidget::item:hover{background:#2c3d4c;}");
    auto *browseRow=new QHBoxLayout;auto *search=new QLineEdit;search->setPlaceholderText("Search programs…");auto *viewMode=new QComboBox;viewMode->addItems({"List","Grid"});browseRow->addWidget(search,1);browseRow->addWidget(viewMode);l->addLayout(browseRow);
    QObject::connect(viewMode,&QComboBox::currentIndexChanged,page,[=](int mode){list->setViewMode(mode?QListView::IconMode:QListView::ListMode);list->setFlow(mode?QListView::LeftToRight:QListView::TopToBottom);list->setWrapping(mode);list->setResizeMode(QListView::Adjust);list->setGridSize(mode?QSize(220,140):QSize());list->setIconSize(mode?QSize(180,90):QSize(104,60));});
    QObject::connect(search,&QLineEdit::textChanged,page,[=](QString term){for(int i=0;i<list->count();++i)list->item(i)->setHidden(!list->item(i)->text().contains(term,Qt::CaseInsensitive));});
    auto *title = new QLineEdit;
    title->setObjectName("programTitle");
    title->setPlaceholderText("Program title");
    auto *kind = new QComboBox;
    kind->addItems(programKinds());
    auto *mins = new FrameInput;
    mins->setRange(0, 600);
    mins->setSuffix(" min");
    auto *secs = new FrameInput;
    secs->setRange(0, 59);
    secs->setSuffix(" sec");
    secs->setValue(30);
    auto *lenRow = new QHBoxLayout;
    lenRow->addWidget(mins);
    lenRow->addWidget(secs);
    auto *file = new QLineEdit;
    file->setPlaceholderText("Media file (optional)");
    auto *browse = smallButton("Browse...");
    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(file, 1);
    fileRow->addWidget(browse);
    auto *adBox = new QComboBox;
    auto *inBox = new QComboBox;
    auto *outBox = new QComboBox;
    auto *desc = new QPlainTextEdit;
    desc->setPlaceholderText("What is this programme about? Shown with its card.");
    desc->setFixedHeight(78);
    auto *poster = new QLineEdit;
    poster->setPlaceholderText("Poster picture for the card (optional)");
    auto *posterBrowse = smallButton("Browse...");
    auto *posterRow = new QHBoxLayout;
    posterRow->addWidget(poster, 1);
    posterRow->addWidget(posterBrowse);
    auto *fx = new FxBox;

    auto *form = new QFormLayout;
    form->addRow("Title", title);
    form->addRow("Type", kind);
    form->addRow("Length", lenRow);
    form->addRow("Media", fileRow);
    form->addRow("Ad after", adBox);
    form->addRow("Break in", inBox);
    form->addRow("Break out", outBox);
    form->addRow("Description", desc);
    form->addRow("Poster", posterRow);
    auto *replayIn=new QComboBox,*replayOut=new QComboBox;
    auto fillReplay=[=]{for(auto *box:{replayIn,replayOut}){const auto keep=box->currentData();QSignalBlocker block(box);box->clear();for(const auto &name:sceneEffectPresets())box->addItem(name,"preset:"+name);for(const auto &e:lib->elements)if(e.type=="Effect")box->addItem(e.name,e.id);int at=box->findData(keep);box->setCurrentIndex(qMax(0,at));}};fillReplay();
    QObject::connect(lib,&Library::elementsChanged,page,fillReplay);
    form->addRow("Replay entry effect",replayIn);form->addRow("Replay return effect",replayOut);
    auto *createEffect=new QPushButton(uiIcon("add"),"Create effect in Build");form->addRow(createEffect);QObject::connect(createEffect,&QPushButton::clicked,page,[=]{lib->requestEffect();});
    auto *returnReplay=new QPushButton(uiIcon("back"),"Return from replay");returnReplay->setEnabled(false);form->addRow(returnReplay);QObject::connect(returnReplay,&QPushButton::clicked,page,[=]{play->replay()->returnToProgram();});QObject::connect(play->replay(),&Replay::changed,page,[=]{returnReplay->setEnabled(play->replay()->active());});

    auto *addBtn = smallButton("Add Program");
    addBtn->setObjectName("save");
    auto *fresh = smallButton("New");
    auto *up = smallButton("↑");
    auto *down = smallButton("↓");
    auto *playBtn = smallButton("▶ Play from here");
    auto *remove = smallButton("Remove");
    auto *row1 = new QHBoxLayout;
    row1->addWidget(addBtn);
    row1->addWidget(fresh);
    row1->addStretch();
    row1->addWidget(up);
    row1->addWidget(down);
    auto *row2 = new QHBoxLayout;
    row2->addWidget(playBtn);
    row2->addStretch();
    row2->addWidget(remove);
    auto *msg = messageLabel();
    l->addWidget(list);
    l->addLayout(form);
    l->addWidget(fx);
    l->addLayout(row1);
    l->addLayout(row2);
    l->addWidget(msg);
    l->addStretch();

    auto selId = [=] { auto *it = list->currentItem(); return it ? it->data(Qt::UserRole).toString() : QString(); };

    auto fillCombos = [=] {
        const QString a = adBox->currentData().toString(), i = inBox->currentData().toString(),
                      o = outBox->currentData().toString();
        QSignalBlocker b1(adBox), b2(inBox), b3(outBox);
        adBox->clear();
        inBox->clear();
        outBox->clear();
        adBox->addItem("None", QString());
        inBox->addItem("None", QString());
        outBox->addItem("None", QString());
        for (const Ad &ad : lib->ads) adBox->addItem(ad.name + " · " + formatDuration(ad.seconds), ad.id);
        for (const Element &e : lib->elements)
            if (e.type == "Break") {
                inBox->addItem(e.name, e.id);
                outBox->addItem(e.name, e.id);
            }
        selectById(adBox, a);
        selectById(inBox, i);
        selectById(outBox, o);
    };
    auto refreshList = [=] {
        const QString keep = selId();
        QSignalBlocker b(list);
        list->clear();
        const int cur = play->currentProgramIndex();
        for (int i = 0; i < lib->programs.size(); ++i) {
            const ProgramItem &p = lib->programs[i];
            const qint64 fin = play->finishedAt(p.id);
            const QString prefix = i == cur ? "▶  " : fin > 0 ? "✓  " : "     ";
            QString text = QString("%1%2. %3   ·   %4   ·   %5").arg(prefix).arg(i + 1).arg(p.title, p.kind, formatDuration(p.seconds));
            if (!p.isAdSpot() && lib->ad(p.adId)) text += "   ·   + ad";
            text=QString("%1%2\n%3 · %4%5").arg(i==cur?"ON AIR · ":fin>0?"PLAYED · ":"",p.title,p.kind,formatDuration(p.seconds),p.description.isEmpty()?QString():" · "+p.description.left(80));
            auto *it = new QListWidgetItem(text);QPixmap picture(p.image);if(!picture.isNull())it->setIcon(QIcon(picture));else it->setIcon(uiIcon("play"));it->setToolTip(p.title+"\n"+p.description);it->setHidden(!text.contains(search->text(),Qt::CaseInsensitive));
            it->setData(Qt::UserRole, p.id);
            if (i == cur) it->setForeground(QColor("#60a5fa"));
            else if (fin > 0) it->setForeground(QColor("#64748b"));
            list->addItem(it);
            if (p.id == keep) list->setCurrentItem(it);
        }
    };
    auto setDefaults = [=] {
        QString first, second;
        for (const Element &e : lib->elements)
            if (e.type == "Break") {
                if (first.isEmpty()) first = e.id;
                else if (second.isEmpty()) second = e.id;
            }
        selectById(adBox, QString());
        selectById(inBox, first);
        selectById(outBox, second.isEmpty() ? first : second);
    };
    auto clearForm = [=] {
        list->clearSelection();
        list->setCurrentItem(nullptr);
        title->clear();
        file->clear();
        kind->setCurrentIndex(0);
        mins->setValue(0);
        secs->setValue(30);
        desc->clear();
        poster->clear();
        fx->set(Transition());
        replayIn->setCurrentIndex(0);replayOut->setCurrentIndex(0);
        setDefaults();
        addBtn->setText("Add Program");
    };
    auto probe = makeProbe(page, [=](int s) {
        mins->setValue(s / 60);
        secs->setValue(s % 60);
    });

    QObject::connect(browse, &QPushButton::clicked, page, [=] {
        const QString path = QFileDialog::getOpenFileName(page, "Choose program media", QString(), kMediaFilter);
        if (path.isEmpty()) return;
        file->setText(path);
        probe->setSource(QUrl::fromLocalFile(path));
    });
    QObject::connect(list, &QListWidget::currentItemChanged, page, [=](QListWidgetItem *it) {
        if (!it) return;
        const ProgramItem *p = lib->program(it->data(Qt::UserRole).toString());
        if (!p) return;
        if (p->isAdSpot()) {  // ad blocks are moved and removed on the timeline itself
            say(msg, "This is an ad block. Drag it on the timeline to move it, or right-click it there to remove it.", true);
            return;
        }
        title->setText(p->title);
        kind->setCurrentText(p->kind);
        mins->setValue(p->seconds / 60);
        secs->setValue(p->seconds % 60);
        file->setText(p->file);
        selectById(adBox, p->adId);
        selectById(inBox, p->breakIn);
        selectById(outBox, p->breakOut);
        desc->setPlainText(p->description);
        poster->setText(p->image);
        fx->set(p->fx);
        replayIn->setCurrentIndex(qMax(0,replayIn->findData(p->replayInEffect)));replayOut->setCurrentIndex(qMax(0,replayOut->findData(p->replayOutEffect)));
        addBtn->setText("Update Program");
        msg->clear();
    });
    QObject::connect(fresh, &QPushButton::clicked, page, clearForm);
    QObject::connect(addBtn, &QPushButton::clicked, page, [=] {
        const QString t = title->text().trimmed();
        const int total = mins->value() * 60 + secs->value();
        if (t.isEmpty()) {
            say(msg, "Enter a program title.");
            title->setFocus();
            return;
        }
        if (total < 1) {
            say(msg, "The length must be at least 1 second.");
            return;
        }
        const QString id = selId();
        ProgramItem item;
        bool updated = false;
        for (ProgramItem &p : lib->programs)
            if (p.id == id) {
                if (p.isAdSpot()) {
                    say(msg, "Ad blocks can't be edited here. Drag or remove them on the timeline.");
                    return;
                }
                updated = true;
                item = p;  // keeps where a cut piece starts in its media and its "+" point
            }
        if (!updated) item.id = Library::newId();
        item.title = t;
        item.kind = kind->currentText();
        item.seconds = total;
        item.file = file->text().trimmed();
        item.adId = adBox->currentData().toString();
        item.breakIn = inBox->currentData().toString();
        item.breakOut = outBox->currentData().toString();
        item.description = desc->toPlainText().trimmed();
        item.image = poster->text().trimmed();
        item.fx = fx->get();
        item.replayInEffect=replayIn->currentData().toString();item.replayOutEffect=replayOut->currentData().toString();
        bool inserted = false;
        if (updated) {
            for (ProgramItem &p : lib->programs) if (p.id == item.id) p = item;
        } else if (slotPending(lib, "Clip")) {
            const QString after = lib->pendingAfter;
            lib->shiftPlacements(play->groupEndMs(after), qint64(item.seconds) * 1000);
            lib->insertAfter(after, item);
            lib->clearPending();
            inserted = true;
        } else {
            lib->programs.append(item);
        }
        if (!inserted) lib->commitPrograms();
        if (!updated) clearForm();
        say(msg, updated ? "Program updated."
                         : inserted ? "Clip inserted at the + on the timeline."
                                    : "Program added to the end of the queue.", true);
    });
    auto move = [=](int delta) {
        const int idx = lib->programIndex(selId());
        const int j = idx + delta;
        if (idx < 0 || j < 0 || j >= lib->programs.size()) return;
        lib->programs.swapItemsAt(idx, j);
        lib->commitPrograms();
    };
    QObject::connect(up, &QPushButton::clicked, page, [=] { move(-1); });
    QObject::connect(down, &QPushButton::clicked, page, [=] { move(1); });
    QObject::connect(remove, &QPushButton::clicked, page, [=] {
        const int idx = lib->programIndex(selId());
        if (idx < 0) {
            say(msg, "Select a program to remove.");
            return;
        }
        lib->removeItem(lib->programs[idx].id);
        clearForm();
        say(msg, "Program removed.", true);
    });
    QObject::connect(playBtn, &QPushButton::clicked, page, [=] {
        const QString id = selId();
        if (id.isEmpty()) {
            say(msg, "Select a program to play.");
            return;
        }
        play->playProgram(id);
    });

    QObject::connect(posterBrowse, &QPushButton::clicked, page, [=] {
        const QString path = QFileDialog::getOpenFileName(page, "Choose a poster picture", QString(),
                                                          "Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*)");
        if (!path.isEmpty()) poster->setText(path);
    });
    // a program card was pressed: open that program here
    QObject::connect(lib, &Library::openProgramRequested, page, [=](const QString &id) {
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toString() == id) {
                list->setCurrentRow(i);
                list->scrollToItem(list->item(i));
                break;
            }
    });
    makeListDraggable(list, [=](QListWidgetItem *it, DragPayload &d) {
        const ProgramItem *p = lib->program(it->data(Qt::UserRole).toString());
        if (!p) return false;
        d.kind = "program";
        d.id = p->id;
        d.label = p->title;
        d.color = kindColor(p->kind);
        d.widthPx = p->isAdSpot() ? qBound(90, p->seconds * 3, 240) : qBound(140, int(p->seconds * 1.2), 720);
        d.grabDx = qMin(d.widthPx, 260) / 2;
        return true;
    });
    QObject::connect(lib, &Library::pendingChanged, page, [=] {
        if (lib->pendingKind == "Clip") {
            clearForm();
            msg->clear();
            addBtn->setText("Add Clip to Timeline");
            title->setFocus();
        } else if (!list->currentItem()) {
            addBtn->setText("Add Program");
        }
    });
    QObject::connect(lib, &Library::programsChanged, page, refreshList);
    QObject::connect(lib, &Library::adsChanged, page, fillCombos);
    QObject::connect(lib, &Library::elementsChanged, page, fillCombos);
    QObject::connect(play, &Playout::segmentChanged, page, refreshList);
    QObject::connect(play, &Playout::programFinished, page, refreshList);
    fillCombos();
    setDefaults();
    refreshList();
    return page;
}

// ---------------------------------------------------------- Screen elements

QWidget *makeElementsPage(Library *lib,Playout *play){return studioElementsPage(lib,play);}

} // namespace

// ------------------------------------------------------------- the panel

EditorPanel::EditorPanel(Library *lib, Playout *play, QWidget *parent) : QFrame(parent) {
    setObjectName("editorBg");
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 6, 6, 6);
    root->setSpacing(0);

    // thin icon rail on the flat background
    auto *rail = new QWidget;
    rail->setMinimumWidth(34);
    rail->setMaximumWidth(38);
    auto *rl = new QVBoxLayout(rail);
    rl->setContentsMargins(3, 5, 3, 0);
    rl->setSpacing(5);
    rl->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *fg = new QFrame;
    fg->setObjectName("editorFg");
    auto *fl = new QVBoxLayout(fg);
    fl->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget;
    fl->addWidget(m_stack);

    struct Tool { const char *icon, *tip; QWidget *page; };
    const QList<Tool> tools = {
        {"composition", "Display", makeDisplayPage(lib, play)},
        {"audio", "Ads", makeAdsPage(lib, play)},
        {"play", "Programs", makePlannerPage(lib, play)},
        {"image", "Screen Elements", makeElementsPage(lib, play)},
        {"graph", "Monitor", studioDashboard(play)},
        {"crew", "Production / Crew", crewWorkspace(lib)},
    };
    for (int i = 0; i < tools.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("rail");
        b->setIcon(uiIcon(tools[i].icon));b->setIconSize({16,16});
        b->setToolTip(tools[i].tip);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setFixedSize(28, 28);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [this, i] { showTool(i); });
        rl->addWidget(b);
        m_tools.append(b);
        m_stack->addWidget(scrollWrap(tools[i].page));
    }
    root->addWidget(rail, 1);
    root->addWidget(fg, 9);
    showTool(2); // open on Programs
    // pressing a program card opens that program here
    connect(lib, &Library::openProgramRequested, this, [this] { showTool(2); });
}

void EditorPanel::showTool(int index) {
    if (index < 0 || index >= m_tools.size()) return;
    m_stack->setCurrentIndex(index);
    m_tools[index]->setChecked(true);
}

void EditorPanel::focusNewProgram() {
    showTool(2);
    if (auto *t = m_stack->widget(2)->findChild<QLineEdit *>("programTitle")) t->setFocus();
}
