#include "EditorPanel.h"

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
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <memory>

#include "Data.h"
#include "Playout.h"

namespace {

const char *kMediaFilter = "Media (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;All files (*)";

QLabel *heading(const QString &title, const QString &sub) {
    auto *l = new QLabel(QString("<div style='font-size:16px; font-weight:700;'>%1</div>"
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

QWidget *scrollWrap(QWidget *page) {
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(page);
    return sa;
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

// ------------------------------------------------------------------ Display

QWidget *makeDisplayPage(Library *lib, Playout *play) {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(heading("Display", "Output and on-screen settings for the viewport."));

    auto *channel = new QComboBox;
    channel->addItems({"TV1 — Main", "TV2", "TV3"});
    auto *res = new QComboBox;
    res->addItems({"1920x1080", "1280x720", "3840x2160"});
    auto *fps = new QComboBox;
    fps->addItems({"25", "30", "50"});
    auto *bitrate = new QDoubleSpinBox;
    bitrate->setRange(1.0, 50.0);
    bitrate->setSingleStep(0.5);
    bitrate->setSuffix(" Mbps");
    auto *wmOn = new QCheckBox("Show watermark");
    auto *wmText = new QLineEdit;
    auto *safe = new QCheckBox("Show safe-area guides");

    auto *form = new QFormLayout;
    form->addRow("Output channel", channel);
    form->addRow("Resolution", res);
    form->addRow("Frame rate", fps);
    form->addRow("Bitrate", bitrate);
    form->addRow(wmOn);
    form->addRow("Watermark text", wmText);
    form->addRow(safe);
    l->addLayout(form);
    auto *lockNote = new QLabel("Channel, resolution, frame rate and bitrate are locked while you are on air.");
    lockNote->setWordWrap(true);
    lockNote->setStyleSheet("color:#f59e0b;");
    lockNote->hide();
    l->addWidget(lockNote);
    l->addStretch();

    auto loading = std::make_shared<bool>(true);
    const DisplaySettings &d = lib->display;
    channel->setCurrentText(d.channel);
    res->setCurrentText(d.resolution);
    fps->setCurrentText(QString::number(d.fps));
    bitrate->setValue(d.bitrate);
    wmOn->setChecked(d.watermarkOn);
    wmText->setText(d.watermarkText);
    safe->setChecked(d.safeArea);
    *loading = false;

    auto apply = [=] {
        if (*loading) return;
        DisplaySettings &s = lib->display;
        s.channel = channel->currentText();
        s.resolution = res->currentText();
        s.fps = fps->currentText().toInt();
        s.bitrate = bitrate->value();
        s.watermarkOn = wmOn->isChecked();
        s.watermarkText = wmText->text();
        s.safeArea = safe->isChecked();
        lib->commitDisplay();
    };
    QObject::connect(channel, &QComboBox::currentTextChanged, page, apply);
    QObject::connect(res, &QComboBox::currentTextChanged, page, apply);
    QObject::connect(fps, &QComboBox::currentTextChanged, page, apply);
    QObject::connect(bitrate, &QDoubleSpinBox::valueChanged, page, apply);
    QObject::connect(wmOn, &QCheckBox::toggled, page, apply);
    QObject::connect(wmText, &QLineEdit::textChanged, page, apply);
    QObject::connect(safe, &QCheckBox::toggled, page, apply);

    QObject::connect(play, &Playout::stateChanged, page, [=] {
        const bool ok = !play->isLive();
        for (QWidget *w : {static_cast<QWidget *>(channel), static_cast<QWidget *>(res),
                           static_cast<QWidget *>(fps), static_cast<QWidget *>(bitrate)})
            w->setEnabled(ok);
        lockNote->setVisible(!ok);
    });
    return page;
}

// ---------------------------------------------------------------------- Ads

QWidget *makeAdsPage(Library *lib, Playout *) {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(heading("Ads", "Create ads and store them here. Attach them to programs in the Program Planner."));

    auto *list = new QListWidget;
    list->setMinimumHeight(120);
    auto *name = new QLineEdit;
    name->setPlaceholderText("Ad name");
    auto *secs = new QSpinBox;
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
    l->addWidget(list);
    l->addLayout(form);
    l->addLayout(buttons);
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
                updated = true;
            }
        if (!updated) lib->ads.append(Ad{Library::newId(), n, file->text().trimmed(), secs->value()});
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
        for (ProgramItem &p : lib->programs)
            if (p.adId == id) {
                p.adId.clear();
                touched = true;
            }
        for (int i = 0; i < lib->ads.size(); ++i)
            if (lib->ads[i].id == id) lib->ads.removeAt(i);
        if (touched) lib->commitPrograms();
        lib->commitAds();
        clearForm();
        say(msg, "Ad removed.", true);
    });
    QObject::connect(lib, &Library::adsChanged, page, refresh);
    refresh();
    return page;
}

// -------------------------------------------------------- Program planner

QWidget *makePlannerPage(Library *lib, Playout *play) {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(heading("Program Planner", "Add programs and set how they play, one after the other."));

    auto *list = new QListWidget;
    list->setMinimumHeight(150);
    auto *title = new QLineEdit;
    title->setObjectName("programTitle");
    title->setPlaceholderText("Program title");
    auto *kind = new QComboBox;
    kind->addItems(programKinds());
    auto *mins = new QSpinBox;
    mins->setRange(0, 600);
    mins->setSuffix(" min");
    auto *secs = new QSpinBox;
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

    auto *form = new QFormLayout;
    form->addRow("Title", title);
    form->addRow("Type", kind);
    form->addRow("Length", lenRow);
    form->addRow("Media", fileRow);
    form->addRow("Ad after", adBox);
    form->addRow("Break in", inBox);
    form->addRow("Break out", outBox);

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
            if (lib->ad(p.adId)) text += "   ·   + ad";
            auto *it = new QListWidgetItem(text);
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
        title->setText(p->title);
        kind->setCurrentText(p->kind);
        mins->setValue(p->seconds / 60);
        secs->setValue(p->seconds % 60);
        file->setText(p->file);
        selectById(adBox, p->adId);
        selectById(inBox, p->breakIn);
        selectById(outBox, p->breakOut);
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
                updated = true;
                item.id = p.id;
            }
        if (!updated) item.id = Library::newId();
        item.title = t;
        item.kind = kind->currentText();
        item.seconds = total;
        item.file = file->text().trimmed();
        item.adId = adBox->currentData().toString();
        item.breakIn = inBox->currentData().toString();
        item.breakOut = outBox->currentData().toString();
        if (updated) {
            for (ProgramItem &p : lib->programs) if (p.id == item.id) p = item;
        } else {
            lib->programs.append(item);
        }
        lib->commitPrograms();
        if (!updated) clearForm();
        say(msg, updated ? "Program updated." : "Program added to the end of the queue.", true);
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
        lib->programs.removeAt(idx);
        lib->commitPrograms();
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

QWidget *makeElementsPage(Library *lib, Playout *) {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(heading("Screen Elements",
                         "Create logos, banners, ad elements and breaks (BR) to use on screen."));

    auto *list = new QListWidget;
    list->setMinimumHeight(120);
    auto *name = new QLineEdit;
    name->setPlaceholderText("Element name");
    auto *type = new QComboBox;
    type->addItems(elementTypes());
    auto *text = new QLineEdit;
    auto *position = new QComboBox;
    position->addItems(screenPositions());
    auto *colorBtn = smallButton("Choose colour");
    colorBtn->setProperty("color", "#2563eb");
    auto *image = new QLineEdit;
    image->setPlaceholderText("Logo image (optional)");
    auto *browse = smallButton("Browse...");
    auto *imgRow = new QHBoxLayout;
    imgRow->addWidget(image, 1);
    imgRow->addWidget(browse);
    auto *seconds = new QSpinBox;
    seconds->setRange(1, 60);
    seconds->setValue(4);
    seconds->setSuffix(" sec");
    auto *live = new QCheckBox("Show on screen now");
    auto *hint = new QLabel;
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#94a3b8;");

    auto *form = new QFormLayout;
    form->addRow("Name", name);
    form->addRow("Type", type);
    form->addRow("Text", text);
    form->addRow("Position", position);
    form->addRow("Colour", colorBtn);
    form->addRow("Image", imgRow);
    form->addRow("Shown for", seconds);
    form->addRow(live);
    auto *save = smallButton("Add Element");
    save->setObjectName("save");
    auto *fresh = smallButton("New");
    auto *remove = smallButton("Remove");
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(save);
    buttons->addWidget(fresh);
    buttons->addStretch();
    buttons->addWidget(remove);
    auto *msg = messageLabel();
    l->addWidget(list);
    l->addLayout(form);
    l->addWidget(hint);
    l->addLayout(buttons);
    l->addWidget(msg);
    l->addStretch();

    auto loading = std::make_shared<bool>(false);
    auto selId = [=] { auto *it = list->currentItem(); return it ? it->data(Qt::UserRole).toString() : QString(); };
    auto setColor = [=](const QString &c) {
        colorBtn->setProperty("color", c);
        const QColor col(c);
        colorBtn->setStyleSheet(QString("background:%1; color:%2;").arg(c, col.lightness() > 140 ? "#000" : "#fff"));
        colorBtn->setText(c);
    };
    auto adaptToType = [=] {
        const QString t = type->currentText();
        const bool isBreak = t == "Break";
        position->setEnabled(!isBreak);
        image->setEnabled(t == "Logo");
        browse->setEnabled(t == "Logo");
        seconds->setEnabled(isBreak);
        live->setEnabled(!isBreak);
        if (isBreak) live->setChecked(false);
        text->setPlaceholderText(isBreak ? "e.g. Now showing: {title}" : "Text shown on screen");
        hint->setText(isBreak ? "A break (BR) plays before and after an ad. Use {title} for the program, {next} for "
                                "the next program and {ad} for the ad name."
                              : t == "Ad Element" ? "Ad elements only appear on screen while an ad is playing."
                                                  : "Logos and banners appear on the viewport while switched on.");
    };
    auto refresh = [=] {
        const QString keep = selId();
        QSignalBlocker b(list);
        list->clear();
        for (const Element &e : lib->elements) {
            auto *it = new QListWidgetItem(QString("%1   ·   %2%3").arg(e.name, e.type, e.live ? "   ·   ● on screen" : ""));
            it->setData(Qt::UserRole, e.id);
            list->addItem(it);
            if (e.id == keep) {
                list->setCurrentItem(it);
                *loading = true;
                live->setChecked(e.live);
                *loading = false;
            }
        }
    };
    auto clearForm = [=] {
        *loading = true;
        list->clearSelection();
        list->setCurrentItem(nullptr);
        name->clear();
        text->clear();
        image->clear();
        type->setCurrentIndex(0);
        position->setCurrentIndex(0);
        seconds->setValue(4);
        live->setChecked(false);
        setColor("#2563eb");
        save->setText("Add Element");
        *loading = false;
        adaptToType();
    };

    QObject::connect(type, &QComboBox::currentTextChanged, page, adaptToType);
    QObject::connect(colorBtn, &QPushButton::clicked, page, [=] {
        const QColor c = QColorDialog::getColor(QColor(colorBtn->property("color").toString()), page, "Element colour");
        if (c.isValid()) setColor(c.name());
    });
    QObject::connect(browse, &QPushButton::clicked, page, [=] {
        const QString path = QFileDialog::getOpenFileName(page, "Choose logo image", QString(),
                                                          "Images (*.png *.jpg *.jpeg *.bmp *.svg);;All files (*)");
        if (!path.isEmpty()) image->setText(path);
    });
    QObject::connect(list, &QListWidget::currentItemChanged, page, [=](QListWidgetItem *it) {
        if (!it) return;
        const Element *e = lib->element(it->data(Qt::UserRole).toString());
        if (!e) return;
        *loading = true;
        name->setText(e->name);
        type->setCurrentText(e->type);
        text->setText(e->text);
        position->setCurrentText(e->position);
        setColor(e->color);
        image->setText(e->image);
        seconds->setValue(e->seconds);
        live->setChecked(e->live);
        save->setText("Update Element");
        *loading = false;
        adaptToType();
        msg->clear();
    });
    QObject::connect(live, &QCheckBox::toggled, page, [=](bool on) {
        if (*loading) return;
        for (Element &e : lib->elements)
            if (e.id == selId()) {
                e.live = on;
                lib->commitElementLive();
            }
    });
    QObject::connect(fresh, &QPushButton::clicked, page, clearForm);
    QObject::connect(save, &QPushButton::clicked, page, [=] {
        const QString n = name->text().trimmed();
        if (n.isEmpty()) {
            say(msg, "Enter a name for the element.");
            name->setFocus();
            return;
        }
        Element el;
        el.name = n;
        el.type = type->currentText();
        el.text = text->text();
        el.position = position->currentText();
        el.color = colorBtn->property("color").toString();
        el.image = image->text().trimmed();
        el.seconds = seconds->value();
        el.live = live->isChecked();
        bool updated = false;
        for (Element &e : lib->elements)
            if (e.id == selId()) {
                el.id = e.id;
                e = el;
                updated = true;
            }
        if (!updated) {
            el.id = Library::newId();
            lib->elements.append(el);
        }
        lib->commitElements();
        if (!updated) clearForm();
        say(msg, updated ? "Element updated." : "Element added.", true);
    });
    QObject::connect(remove, &QPushButton::clicked, page, [=] {
        const QString id = selId();
        if (id.isEmpty()) {
            say(msg, "Select an element to remove.");
            return;
        }
        bool touched = false;
        for (ProgramItem &p : lib->programs) {
            if (p.breakIn == id) { p.breakIn.clear(); touched = true; }
            if (p.breakOut == id) { p.breakOut.clear(); touched = true; }
        }
        for (int i = 0; i < lib->elements.size(); ++i)
            if (lib->elements[i].id == id) lib->elements.removeAt(i);
        if (touched) lib->commitPrograms();
        lib->commitElements();
        clearForm();
        say(msg, "Element removed.", true);
    });
    QObject::connect(lib, &Library::elementsChanged, page, refresh);
    QObject::connect(lib, &Library::elementLiveChanged, page, refresh);

    setColor("#2563eb");
    adaptToType();
    refresh();
    return page;
}

} // namespace

// ------------------------------------------------------------- the panel

EditorPanel::EditorPanel(Library *lib, Playout *play, QWidget *parent) : QFrame(parent) {
    setObjectName("editorBg");
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 6, 6, 6);
    root->setSpacing(0);

    // thin icon rail on the flat background
    auto *rail = new QWidget;
    rail->setMinimumWidth(50);
    rail->setMaximumWidth(72);
    auto *rl = new QVBoxLayout(rail);
    rl->setContentsMargins(6, 8, 6, 0);
    rl->setSpacing(8);
    rl->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *fg = new QFrame;
    fg->setObjectName("editorFg");
    auto *fl = new QVBoxLayout(fg);
    fl->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget;
    fl->addWidget(m_stack);

    struct Tool { const char *icon, *tip; QWidget *page; };
    const QList<Tool> tools = {
        {"🖥", "Display", makeDisplayPage(lib, play)},
        {"📣", "Ads", makeAdsPage(lib, play)},
        {"🗓", "Program Planner", makePlannerPage(lib, play)},
        {"🧩", "Screen Elements", makeElementsPage(lib, play)},
    };
    for (int i = 0; i < tools.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("rail");
        b->setText(tools[i].icon);
        b->setToolTip(tools[i].tip);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setFixedSize(40, 40);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [this, i] { showTool(i); });
        rl->addWidget(b);
        m_tools.append(b);
        m_stack->addWidget(scrollWrap(tools[i].page));
    }
    root->addWidget(rail, 1);
    root->addWidget(fg, 9);
    showTool(2); // open on the Program Planner
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
