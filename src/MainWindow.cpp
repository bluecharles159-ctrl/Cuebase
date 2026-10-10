#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFont>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMediaPlayer>
#include <QUrl>
#include <memory>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QDialog>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#endif

#include "AddMenu.h"
#include "AppLinks.h"
#include "BuildWorkspace.h"
#include "FileDropOverlay.h"
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QMediaMetaData>
#include "UiControls.h"
#include "ControlPanel.h"
#include "Data.h"
#include "DragDrop.h"
#include "EditorPanel.h"
#include "HelpCenter.h"
#include "Playout.h"
#include "QueueStrip.h"
#include "TimelineWidget.h"
#include "Replay.h"
#include "ViewportWidget.h"
#include "NetworkOutput.h"

namespace {

const char *kStyle = R"(
* { color:#e5e9f0; font-size:11px; }
QMainWindow, QDialog, QMessageBox { background:#0b1220; }
QFrame#header { background:#0f172a; border:none; border-bottom:1px solid #1e293b; border-radius:0; }
QMenuBar#headerBar { background:transparent; }
QMenuBar#headerBar::item { background:#020617; border-radius:6px; padding:4px 12px; margin:2px 3px; font-weight:600; }
QMenuBar#headerBar::item:selected { background:#1e293b; }
QMenu { background:#1c1c1e; border:1px solid #3a3a3c; border-radius:10px; padding:8px; }
QMenu::item { padding:7px 28px 7px 14px; border-radius:6px; }
QMenu::item:selected { background:#2563eb; }
QMenu::separator { height:1px; background:#3a3a3c; margin:6px 8px; }

QFrame#panel { background:#111a2b; border:1px solid #243044; border-radius:10px; }
QFrame#editorBg { background:#0c1322; border:1px solid #1b2740; border-radius:10px; }
QFrame#editorFg { background:#131c2f; border:1px solid #243044; border-radius:10px; }
QFrame#qcard { background:#0f172a; border:1px solid #243044; border-radius:8px; }
QFrame#qcard[state="now"] { border:2px solid #2563eb; background:#13213d; }
QFrame#qcard[state="next"] { border:1px solid #475569; }
QLabel { background:transparent; border:none; }

QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox { background:#1f2937; border:1px solid #334155; border-radius:6px; padding:5px 8px; }
QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled { color:#64748b; background:#182131; }
QComboBox QAbstractItemView { background:#1f2937; selection-background-color:#2563eb; }
QPlainTextEdit { background:#1f2937; border:1px solid #334155; border-radius:6px; padding:5px 8px; }
QFrame#tlBox { background:#070c18; border:1px solid #243044; border-radius:12px; }
QListWidget { background:#0f1729; border:1px solid #243044; border-radius:8px; padding:4px; }
QListWidget::item { padding:6px; border-radius:6px; }
QListWidget::item:selected { background:#1d4ed8; }
QCheckBox { spacing:8px; }

QPushButton { background:#1e293b; border:1px solid #334155; border-radius:6px; padding:7px 14px; font-weight:600; }
QPushButton:hover { background:#2b3a55; }
QPushButton:disabled { color:#64748b; background:#141d2e; }
QPushButton#save { background:#454545; border:none; }
QPushButton#save:hover { background:#565656; }
QPushButton#broadcast { background:#426387; border:none; font-size:11px; padding:5px; }
QPushButton#broadcast[live="true"] { background:#dc2626; }
QPushButton#broadcast:disabled { background:#393c42; color:#777d87; }
QToolButton { background:transparent; border:none; color:#94a3b8; }
QToolButton#rail { font-size:20px; border-radius:10px; }
QToolButton#rail:hover { background:#16213a; }
QToolButton#rail:checked { background:#131c2f; border:1px solid #243044; }
QToolButton#addBtn { background:#2563eb; color:white; border-radius:6px; font-size:14px; font-weight:700; }
QToolButton#addBtn:hover { background:#1d4ed8; }

QSlider::groove:horizontal { height:4px; background:#334155; border-radius:2px; }
QSlider::handle:horizontal { background:#60a5fa; width:14px; margin:-6px 0; border-radius:7px; }
QScrollArea, QScrollArea > QWidget > QWidget { background:transparent; border:none; }
QScrollBar:horizontal { height:12px; background:rgba(255,255,255,8); margin:2px 6px; border-radius:4px; }
QScrollBar::handle:horizontal { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #46567a,stop:1 #34425f); border-radius:4px; min-width:44px; }
QScrollBar::handle:horizontal:hover { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #5b7bb8,stop:1 #4663a0); }
QScrollBar::handle:horizontal:pressed { background:#2563eb; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; height:0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background:transparent; }
QScrollBar:vertical { width:12px; background:rgba(255,255,255,8); margin:6px 2px; border-radius:4px; }
QScrollBar::handle:vertical { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #46567a,stop:1 #34425f); border-radius:4px; min-height:44px; }
QScrollBar::handle:vertical:hover { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #5b7bb8,stop:1 #4663a0); }
QScrollBar::handle:vertical:pressed { background:#2563eb; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { width:0; height:0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
QSplitter::handle { background:transparent; }
QSplitter::handle:hover { background:#1e40af; border-radius:2px; }
QStatusBar { background:#0f172a; border-top:1px solid #1e293b; }
QStatusBar::item { border:none; }
QToolTip { background:#1c1c1e; color:#e5e9f0; border:1px solid #3a3a3c; padding:4px; }
)";

QPixmap makeLogo(int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, size, size);
    g.setColorAt(0, QColor("#3b82f6"));
    g.setColorAt(1, QColor("#1d4ed8"));
    p.setBrush(g);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, size, size);
    p.setPen(Qt::white);
    QFont f("Segoe UI");
    f.setBold(true);
    f.setPixelSize(size * 6 / 10);
    p.setFont(f);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, "C");
    return pm;
}

// The app's own memory use, in MB (-1 when the system won't say).
double memoryMb() {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return double(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    return -1;
#else
    QFile f("/proc/self/status");
    if (!f.open(QIODevice::ReadOnly)) return -1;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        if (line.startsWith("VmRSS:")) return line.mid(6).trimmed().split(' ').value(0).toDouble() / 1024.0;
    }
    return -1;
#endif
}

QString clockText(qint64 ms) {
    const int s = int(ms / 1000);
    return s >= 3600 ? QString("%1:%2:%3").arg(s / 3600).arg(s / 60 % 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'))
                     : QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

QLabel *caption(const QString &t) {
    auto *l = new QLabel(t);
    l->setStyleSheet("color:#94a3b8; font-weight:700; font-size:11px;");
    return l;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setAcceptDrops(true);
    setWindowTitle("Cuebase");
    resize(1400, 860);
    qApp->setStyleSheet(QString::fromUtf8(kStyle)+R"(
QWidget { font-size:11px; }
QMainWindow { background:#242528; }
QFrame#panel,QFrame#editorFg,QFrame#qcard { background:#303135; border:1px solid #414247; border-radius:6px; }
QFrame#editorBg,QFrame#tlBox { background:#28292c; border:1px solid #414247; border-radius:6px; }
QLineEdit,QComboBox,QDoubleSpinBox,QSpinBox,QPlainTextEdit { background:#45464b; border:1px solid #515359; border-radius:3px; padding:2px 5px; }
QPushButton { background:#424449; border:1px solid #53555b; border-radius:4px; padding:4px 8px; font-weight:400; }
QPushButton:hover { background:#51545b; }
QToolButton#rail { font-size:10px; border-radius:4px; padding:2px; }
QToolButton#addBtn { background:#494b50; font-size:12px; }
QListWidget { background:#2c2d31; border:1px solid #414247; padding:2px; }
QListWidget::item { padding:3px; }
QMenu::item { padding:4px 22px 4px 10px; }
QScrollBar:vertical { width:6px; margin:0; }
QScrollBar:horizontal { height:6px; margin:0; }
QScrollBar::handle:vertical,QScrollBar::handle:horizontal { background:#64666d; border-radius:3px; }
QCheckBox { spacing:4px; }
)");
    setWindowIcon(QIcon(makeLogo(64)));

    buildUi();
    buildMenus();
    buildStatusBar();
    qApp->installEventFilter(this);
    log("Cuebase started");
}

// --------------------------------------------------------------- header

void MainWindow::buildMenus() {
    auto *header = new QFrame;
    header->setObjectName("header");
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 4, 10, 4);
    hl->setSpacing(8);

    auto *logo = new QLabel;
    logo->setPixmap(makeLogo(26));
    logo->setToolTip("Cuebase");
    auto *divider = new QFrame;
    divider->setFrameShape(QFrame::VLine);
    divider->setFixedHeight(18);
    auto *bar = new QMenuBar;
    bar->setObjectName("headerBar");
    hl->addWidget(logo);
    hl->addWidget(divider);
    hl->addWidget(bar);
    hl->addStretch();
    auto *titleLabel = new QLabel("Cuebase");
    titleLabel->setStyleSheet("color:#94a3b8; font-weight:600;");
    hl->addWidget(titleLabel);
    hl->addStretch();
    hl->addSpacing(230);
    setMenuWidget(header);

    auto newMenu = [bar](const QString &title) {
        QMenu *m = bar->addMenu(title);
        m->setWindowFlags(m->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        m->setAttribute(Qt::WA_TranslucentBackground);
        return m;
    };
    auto add = [this](QMenu *menu, const QString &text, const QKeySequence &key, auto handler) {
        auto *a = new QAction(text, this);
        if (!key.isEmpty()) {
            a->setShortcut(key);
            addAction(a);
        }
        connect(a, &QAction::triggered, this, [this, text, handler] {
            const int before = m_reportSeq;
            handler();
            const bool keyResult = m_keysHeld && !m_pendingResult.isEmpty();  // e.g. "Copied" from Ctrl+C
            if (m_reportSeq == before && !keyResult) report(text);          // otherwise say which command ran
        });
        menu->addAction(a);
    };
    auto focusCall = [](const char *slot) {
        return [slot] {
            if (QWidget *w = QApplication::focusWidget()) QMetaObject::invokeMethod(w, slot);
        };
    };

    // File
    auto *file = newMenu("File");
    auto *importAction=file->addAction(uiIcon("add"),"Import…");importAction->setShortcut(QKeySequence("Ctrl+I"));
    connect(importAction,&QAction::triggered,this,[this]{const auto files=QFileDialog::getOpenFileNames(this,"Import into Cuebase",{},"Media and designs (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.svg *.ico *.mp4 *.mov *.mkv *.avi *.webm *.mxf *.wav *.mp3 *.flac *.ogg *.aac *.fbx *.obj *.gltf *.glb *.dae *.stl *.ply *.cuebuild)");importFiles(files);});
    auto *exportAction=file->addAction(uiIcon("save"),"Export Build…");connect(exportAction,&QAction::triggered,this,[this]{m_build->exportFile();});
    file->addSeparator();add(file, "Quit", QKeySequence("Ctrl+Q"), [this] { close(); });

    // Edit (acts on the text field that has focus)
    auto *edit = newMenu("Edit");
    add(edit, "Undo", QKeySequence("Ctrl+Z"), [this, focusCall] { if (m_build && m_stack->currentWidget() == m_build) m_build->undoEdit(); else focusCall("undo")(); });
    add(edit, "Redo", QKeySequence("Ctrl+Shift+Z"), [this, focusCall] { if (m_build && m_stack->currentWidget() == m_build) m_build->redoEdit(); else focusCall("redo")(); });
    edit->addSeparator();
    add(edit, "Cut", QKeySequence("Ctrl+X"), focusCall("cut"));
    add(edit, "Copy", QKeySequence("Ctrl+C"), focusCall("copy"));
    add(edit, "Paste", QKeySequence("Ctrl+V"), focusCall("paste"));

    // Help
    auto *help = newMenu("Help");
    add(help, "Welcome", QKeySequence(), [this] {
        HelpCenter::showWelcome(
            this, [this] { HelpCenter::showWalkthrough(this); },
            [this] { HelpCenter::showShortcuts(this, shortcutRows()); });
    });
    add(help, "Editor Playground", QKeySequence(), [this] { HelpCenter::openPlayground(this); });
    add(help, "Open Walkthrough", QKeySequence(), [this] { HelpCenter::showWalkthrough(this); });
    add(help, "Show Release Notes", QKeySequence(), [this] { HelpCenter::showReleaseNotes(this); });
    add(help, "Keyboard Shortcuts", QKeySequence(), [this] { HelpCenter::showShortcuts(this, shortcutRows()); });
    add(help, "Join Us", QKeySequence(), [this] { HelpCenter::openLink(this, "Join Us", AppLinks::joinUs); });
    add(help, "Report An Issue", QKeySequence(),
        [this] { HelpCenter::openLink(this, "Report An Issue", AppLinks::reportIssue); });
    add(help, "View License", QKeySequence(), [this] { HelpCenter::showLicense(this); });
    add(help, "Toggle Developer Tools", QKeySequence("Ctrl+Shift+I"), [this] {
        toggleDevTools();
        report(m_devTools && m_devTools->isVisible() ? "Developer tools opened" : "Developer tools closed");
    });
    add(help, "Check For Updates", QKeySequence(),
        [this] { HelpCenter::openLink(this, "Check For Updates", AppLinks::updates); });
    add(help, "Privacy Policy", QKeySequence(), [this] { HelpCenter::showPrivacy(this); });
    add(help, "Documentation", QKeySequence(),
        [this] { HelpCenter::showDocumentation(this); });
    add(help, "About", QKeySequence(), [this] { HelpCenter::showAbout(this); });

    // Window
    auto *window = newMenu("Window");bar->removeAction(help->menuAction());bar->addAction(help->menuAction());
    add(window, "Full Screen", QKeySequence(Qt::Key_F11), [this] {
        const bool on = !isFullScreen();
        if (on) showFullScreen(); else showNormal();
        report(on ? "Full screen on" : "Full screen off");
    });
    // the workspace where media is controlled is called Studio; Build is where screen elements are made.
    // The menu always offers the other one.
    add(window, "Build", QKeySequence("Ctrl+Shift+B"), [this] {
        if (m_stack->currentWidget() == m_build) showStudio();
        else showBuild(QString(), false);
    });
    m_workspaceAct = window->actions().last();
    window->addSeparator();
    add(window, "Toggle Control Panel", QKeySequence("Ctrl+B"), [this] {
        const bool show = !m_control->isVisible();
        m_control->setVisible(show);
        report(show ? "Control panel shown" : "Control panel hidden");
    });
    add(window, "Toggle Preview", QKeySequence("Ctrl+J"), [this] {
        const bool show = !m_preview->isVisible();
        m_preview->setVisible(show);
        report(show ? "Timeline preview shown" : "Timeline preview hidden");
    });
    add(window, "Reset Layout", QKeySequence(), [this] {
        resetLayout();
        report("Layout reset");
    });
    add(window, "Minimize", QKeySequence(), [this] { showMinimized(); });
}

QList<QPair<QString, QString>> MainWindow::shortcutRows() const {
    QList<QPair<QString, QString>> rows;
    for (QAction *a : findChildren<QAction *>())
        if (!a->shortcut().isEmpty())
            rows.append({a->text(), a->shortcut().toString(QKeySequence::NativeText)});
    rows.append(QPair<QString,QString>{"Build: add object", "Shift+A"});
    rows.append(QPair<QString,QString>{"Studio timeline: fit / reveal playhead", "Home / F"});
    rows.append(QPair<QString,QString>{"Studio timeline: zoom / pan / step", "Wheel / Middle-drag / Alt+wheel"});
    rows.append(QPair<QString,QString>{"Studio timeline: select replay range", "Right-drag"});
    rows.append(QPair<QString,QString>{"Build: set placement cursor", "Shift+right-click"});
    rows.append(QPair<QString,QString>{"Build: move / rotate / scale", "G / R / S"});
    rows.append(QPair<QString,QString>{"Build: constrain transform", "X / Y / Z"});
    rows.append(QPair<QString,QString>{"Build: duplicate independently", "Ctrl+D / Shift+D"});
    rows.append(QPair<QString,QString>{"Build: parent to last-selected layer (keep transform)", "Ctrl+P"});
    rows.append(QPair<QString,QString>{"Build: clear parent (keep transform)", "Alt+P"});
    rows.append(QPair<QString,QString>{"Build: frame selected / fit artboard", "F / Home"});
    rows.append(QPair<QString,QString>{"Build: play / pause", "Space"});
    rows.append(QPair<QString,QString>{"Build: insert transform keys / remove current keys", "K / Alt+K"});
    rows.append(QPair<QString,QString>{"Build: previous / next key", "Left / Right"});
    rows.append(QPair<QString,QString>{"Build: previous / next frame", "Ctrl+Left / Ctrl+Right"});
    rows.append(QPair<QString,QString>{"Build: playback range start / end", "Shift+Left / Shift+Right"});
    rows.append(QPair<QString,QString>{"Build: save Screen Element", "Ctrl+S"});
    rows.append(QPair<QString,QString>{"Build: mesh edit / vertex / edge / face", "Tab / 1 / 2 / 3"});
    rows.append(QPair<QString,QString>{"Build mesh: extrude / inset / loop cut / fill / flatten", "E / I / Ctrl+R / F / Shift+F"});
    rows.append(QPair<QString,QString>{"Graph: select multiple / delete keys", "Shift+click / Delete"});
    rows.append(QPair<QString,QString>{"Graph: pan / axis zoom", "Middle-drag / Ctrl+drag"});
    rows.append(QPair<QString,QString>{"Graph: move / scale / constrain", "G / S / X or Y"});
    rows.append(QPair<QString,QString>{"Graph: select all / insert", "Ctrl+A / I"});
    rows.append(QPair<QString,QString>{"Graph: copy / paste / duplicate", "Ctrl+C / Ctrl+V / Shift+D"});
    rows.append(QPair<QString,QString>{"Graph: step frame / cancel", "Alt+wheel / Esc"});
    rows.append(QPair<QString,QString>{"Values: cancel adjustment / commit typed value", "Right-click / Enter"});
    rows.append(QPair<QString,QString>{"Graph: align opposite Bézier handle while dragging", "Shift+drag"});
    rows.append(QPair<QString,QString>{"Values: horizontal scrub / fine adjust / type", "Drag / Shift+drag / double-click"});
    rows.append(QPair<QString,QString>{"Property key: insert or remove at playhead", "Click diamond"});
    return rows;
}

void MainWindow::log(const QString &msg) {
    const QString line = QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + "  " + msg;
    m_logLines << line;
    if (m_devLog) m_devLog->appendPlainText(line);
}

void MainWindow::toggleDevTools() {
    if (!m_devTools) {
        m_devTools = new QDialog(this);
        m_devTools->setWindowTitle("Developer Tools");
        m_devTools->resize(640, 360);
        auto *l = new QVBoxLayout(m_devTools);
        m_devLog = new QPlainTextEdit;
        m_devLog->setReadOnly(true);
        m_devLog->setStyleSheet("font-family:Consolas;");
        m_devLog->setPlainText(m_logLines.join("\n"));
        auto *clear = new QPushButton("Clear");
        auto *copy = new QPushButton("Copy All");
        auto *close = new QPushButton("Close");
        auto *row = new QHBoxLayout;
        row->addWidget(clear);
        row->addWidget(copy);
        row->addStretch();
        row->addWidget(close);
        l->addWidget(m_devLog, 1);
        l->addLayout(row);
        connect(clear, &QPushButton::clicked, this, [this] {
            m_logLines.clear();
            m_devLog->clear();
        });
        connect(copy, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(m_devLog->toPlainText());
            report("Developer log copied to clipboard");
        });
        connect(close, &QPushButton::clicked, m_devTools, &QDialog::hide);
    }
    if (m_devTools->isVisible()) {
        m_devTools->hide();
    } else {
        m_devTools->show();
        m_devTools->raise();
    }
}

// ------------------------------------------------------------------- UI

void MainWindow::buildUi() {
    m_lib = new Library(this);
    m_lib->load();
    m_play = new Playout(m_lib, this);

    // viewport | editor
    m_editor = new EditorPanel(m_lib, m_play);
    m_top = new QSplitter(Qt::Horizontal);
    auto *programView=new ViewportWidget(m_lib,m_play);m_top->addWidget(programView);
    m_network=new NetworkOutput(this);m_network->setFrameProvider([programView](QSize size){return programView->programFrame(size);});
    m_network->attachPlayer(m_play->primaryPlayer(),m_play->audio());
    connect(m_play,&Playout::audioSourceCreated,m_network,&NetworkOutput::attachPlayer);
    m_top->addWidget(m_editor);
    m_top->setChildrenCollapsible(false);
    m_top->setHandleWidth(4);

    // Program monitor and asset editor share the top row; the timeline spans the full width below.
    m_preview=buildPreview();m_control=new ControlPanel(m_lib,m_play,nullptr,m_network);
    m_left=new QSplitter(Qt::Horizontal);m_left->addWidget(m_top);m_left->addWidget(m_control);m_left->setChildrenCollapsible(false);m_left->setHandleWidth(4);m_left->setStretchFactor(0,1);
    m_root=new QSplitter(Qt::Vertical);m_root->addWidget(m_left);m_root->addWidget(m_preview);m_root->setChildrenCollapsible(false);m_root->setHandleWidth(4);m_root->setStretchFactor(0,1);

    auto *wrap = new QWidget;
    auto *wl = new QVBoxLayout(wrap);
    wl->setContentsMargins(6, 6, 6, 6);
    wl->addWidget(m_root);
    m_studio = wrap;

    m_build = new BuildWorkspace(m_lib);
    m_stack = new QStackedWidget;
    m_stack->addWidget(m_studio);
    m_stack->addWidget(m_build);
    setCentralWidget(m_stack);
    connect(m_build, &BuildWorkspace::closeRequested, this, [this] { showStudio(); });
    connect(m_lib, &Library::buildRequested, this, [this](const QString &id) { showBuild(id, true); });
    connect(m_lib,&Library::effectRequested,this,[this]{
        if(m_build->hasUnsavedChanges()&&!m_build->saveForClose())return;
        m_build->newEffect();showBuild({},false);
    });

    resetLayout();
    QSettings s;
    m_top->restoreState(s.value("layout/studio3/top").toByteArray());
    m_left->restoreState(s.value("layout/studio3/left").toByteArray());
    m_root->restoreState(s.value("layout/studio3/root").toByteArray());

    // activity log + window title follow the playout
    connect(m_play, &Playout::segmentChanged, this, [this] {
        const Segment *s = m_play->current();
        if (!s) {
            setWindowTitle("Cuebase");
            log("Off air");
            return;
        }
        const char *kind = s->type == Segment::Program ? "Program" : s->type == Segment::Ad ? "Ad" : "Break";
        log(QString("Now playing (%1): %2").arg(kind, s->title));
        setWindowTitle(s->type == Segment::Program ? "Cuebase — " + s->title : "Cuebase");
    });
    connect(m_play, &Playout::stateChanged, this, [this] {
        log(m_play->state() == Playout::Playing ? "Broadcast running"
            : m_play->state() == Playout::Paused ? "Broadcast paused" : "Broadcast stopped");
    });
    connect(m_lib, &Library::programsChanged, this, [this] { log("Program list updated"); });
    connect(m_lib, &Library::adsChanged, this, [this] { log("Ads updated"); });
    connect(m_lib, &Library::elementsChanged, this, [this] { log("Screen elements updated"); });
}

QWidget *MainWindow::buildPreview() {
    // the whole lower half scrolls as a page while the viewport above stays on screen
    auto *f = new QFrame;
    f->setObjectName("panel");
    auto *fl = new QVBoxLayout(f);
    fl->setContentsMargins(0, 0, 0, 0);
    auto *page = new QScrollArea;
    page->setWidgetResizable(true);
    page->setFrameShape(QFrame::NoFrame);
    page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fl->addWidget(page);
    auto *content = new QWidget;
    page->setWidget(content);
    auto *l = new QVBoxLayout(content);
    l->setContentsMargins(7, 6, 7, 6);
    l->setSpacing(8);

    // the timeline sits in a darker rounded container, like the tracks of a video editor
    auto *tl = new TimelineWidget(m_play, m_lib);
    connect(tl,&TimelineWidget::replayRequested,this,[this](qint64 from,qint64 to){showReplayDialog(m_play,m_lib,from,to,this);});
    connect(m_play->replay(),&Replay::failed,this,[this](const QString &reason){report(reason);});
    auto *box = new QFrame;
    box->setObjectName("tlBox");
    auto *bl = new QVBoxLayout(box);
    bl->setContentsMargins(6, 6, 6, 4);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(tl);
    bl->addWidget(scroll);
    auto fitHeight = [scroll](int h) { scroll->setFixedHeight(h + 16); };  // lanes appear and disappear
    connect(tl, &TimelineWidget::heightWanted, scroll, fitHeight);
    fitHeight(tl->wantedHeight());

    connect(tl, &TimelineWidget::segmentClicked, m_play, [this](int i) { m_play->jumpTo(i); });
    connect(tl, &TimelineWidget::cutRequested, this, &MainWindow::onCut);
    connect(tl, &TimelineWidget::addRequested, this, &MainWindow::showAddMenu);
    connect(tl, &TimelineWidget::dropped, this, &MainWindow::onTimelineDrop);
    connect(tl, &TimelineWidget::spotRemoveRequested, this, &MainWindow::onSpotRemoved);
    connect(tl, &TimelineWidget::placementRemoveRequested, this, &MainWindow::onPlacementRemoved);
    connect(tl, &TimelineWidget::placementLengthRequested, this, &MainWindow::onPlacementLength);
    connect(m_play, &Playout::segmentChanged, scroll, [this, tl, scroll] {
        const int i = m_play->index();
        scroll->horizontalScrollBar()->setValue(i < 0 ? 0 : qMax(0, tl->blockX(i) - 30));
    });

    auto *addBtn = new QToolButton;
    addBtn->setObjectName("addBtn");
    addBtn->setText("+");
    addBtn->setToolTip("Add an ad, a screen element or an audio file to the timeline");
    addBtn->setFixedSize(22, 22);
    addBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QToolButton::clicked, this, [this] { showAddModal(); });
    auto *row = new QHBoxLayout;
    row->setContentsMargins(4, 0, 0, 0);
    row->addWidget(addBtn);
    auto tool=[&](QString label,QString icon,auto fn){auto *b=new QToolButton;b->setIcon(uiIcon(icon));b->setToolTip(label);b->setFixedSize(24,22);connect(b,&QToolButton::clicked,this,fn);row->addWidget(b);};
    tool("Fit timeline · Home","fit",[=]{tl->fitView();});tool("Zoom out","remove",[=]{tl->zoomView(1/1.25);});tool("Zoom in","add",[=]{tl->zoomView(1.25);});tool("Reveal playhead · F","cursor",[=]{tl->revealPlayhead();});
    auto *timeLabel=new QLabel;auto showTime=[=,this]{timeLabel->setText(QString("%1 s").arg(m_play->playheadMs()/1000.,0,'f',2));};connect(m_play,&Playout::ticked,timeLabel,showTime);showTime();row->addWidget(timeLabel);row->addStretch();auto *navHint=new QLabel("Wheel zoom · Middle drag pan · Alt wheel step");navHint->setStyleSheet("color:#969ca5;font-size:10px;");row->addWidget(navHint);

    navHint->hide();
    l->addWidget(caption("TIMELINE"));
    l->addWidget(box);
    l->addLayout(row);
    auto *rundown=new QueueStrip(m_lib,m_play);rundown->hide();auto *toggle=new QToolButton;toggle->setIcon(uiIcon("down"));toggle->setText("Rundown cards");toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);toggle->setCheckable(true);connect(toggle,&QToolButton::toggled,rundown,&QWidget::setVisible);l->addWidget(toggle);l->addWidget(rundown);
    l->addStretch();
    return f;
}

// The + under the timeline: what do you want to put on it?
void MainWindow::showAddModal() {
    QDialog dlg(this);
    dlg.setWindowTitle("Add to timeline");
    dlg.setModal(true);
    dlg.setMinimumWidth(380);
    auto *l = new QVBoxLayout(&dlg);
    l->setContentsMargins(18, 16, 18, 18);
    l->setSpacing(8);
    auto *title = new QLabel("<div style='font-size:16px; font-weight:700;'>Add to timeline</div>"
                             "<div style='color:#94a3b8;'>These go on their own timeline under the main one.</div>");
    l->addWidget(title);
    QString chosen;
    auto option = [&](const QString &name, const QString &sub, const QString &key) {
        auto *b = new QPushButton(QString("%1\n%2").arg(name, sub));
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("QPushButton { text-align:left; padding:10px 14px; font-weight:600; }");
        connect(b, &QPushButton::clicked, &dlg, [&dlg, &chosen, key] {
            chosen = key;
            dlg.accept();
        });
        l->addWidget(b);
    };
    option("📣  Ad", "Opens the Ad Center. Pick one and it lands on the second timeline.", "ad");
    option("🧩  Screen element", "Opens Screen Elements. Pick a logo, banner or your own build.", "element");
    option("♪  Audio file", "Choose a sound from your computer.", "audio");
    auto *cancel = new QPushButton("Cancel");
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    l->addWidget(cancel, 0, Qt::AlignRight);
    if (dlg.exec() != QDialog::Accepted) return;

    m_lib->pendingLayer = 1;  // the second timeline
    if (chosen == "ad") {
        m_editor->showTool(1);
        m_lib->setPending("Layer Ad", QString());
        report("Ad Center opened. Pick an ad: it goes on the second timeline");
    } else if (chosen == "element") {
        m_editor->showTool(3);
        m_lib->setPending("Layer Element", QString());
        report("Screen Elements opened. Pick an element: it goes on the second timeline");
    } else if (chosen == "audio") {
        chooseSound(QString());
    }
}

// ---------------------------------------------------------- workspaces

void MainWindow::importFiles(const QStringList &paths){
    for(const auto &path:paths){const auto suffix=QFileInfo(path).suffix().toLower();
        if(QStringList{"mp4","mov","mkv","avi","webm","mxf","m4v","mpeg","mpg","ts"}.contains(suffix)){importVideo(path);continue;}
        showBuild({},false);m_build->importFile(path);
    }
}

void MainWindow::importVideo(const QString &path){
    QDialog dialog(this);dialog.setWindowTitle("Import video");dialog.setMinimumWidth(370);
    dialog.setStyleSheet("QDialog{background:#303030;border-radius:12px;} QLabel{font-size:11px;color:#ddd;} QLineEdit,QComboBox{background:#474747;border:0;border-radius:4px;padding:5px;color:white;} QPushButton{border:0;border-radius:4px;padding:6px 14px;background:#4c70a0;color:white;}");
    auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(20,18,20,18);auto *form=new QFormLayout;
    auto *name=new QLineEdit(QFileInfo(path).completeBaseName());form->addRow("Name",name);
    auto *format=new QLabel(QFileInfo(path).suffix().toUpper()+" · reading media…");form->addRow("Format",format);
    auto *placement=new QComboBox;placement->addItems({"End of timeline","Start of timeline","Select on timeline"});form->addRow("Insert at",placement);layout->addLayout(form);
    auto *status=new QLabel;status->setWordWrap(true);layout->addWidget(status);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText("Import");buttons->button(QDialogButtonBox::Ok)->setEnabled(false);layout->addWidget(buttons);
    QMediaPlayer probe;connect(&probe,&QMediaPlayer::durationChanged,&dialog,[&](qint64 duration){format->setText(QFileInfo(path).suffix().toUpper()+QString(" · %1 s").arg(duration/1000.,0,'f',2));buttons->button(QDialogButtonBox::Ok)->setEnabled(duration>0&&!name->text().trimmed().isEmpty());});
    connect(name,&QLineEdit::textChanged,&dialog,[&]{buttons->button(QDialogButtonBox::Ok)->setEnabled(probe.duration()>0&&!name->text().trimmed().isEmpty());});
    connect(&probe,&QMediaPlayer::errorOccurred,&dialog,[&](QMediaPlayer::Error,const QString &error){status->setText(error);buttons->button(QDialogButtonBox::Ok)->setEnabled(false);});
    connect(placement,&QComboBox::currentIndexChanged,&dialog,[=](int index){status->setText(index==2?"Move the clip over the timeline, then click to insert before or after a program. Esc keeps it at the end.":QString());});
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);probe.setSource(QUrl::fromLocalFile(path));
    if(dialog.exec()!=QDialog::Accepted)return;
    ProgramItem item;item.id=Library::newId();item.title=name->text().trimmed();item.file=path;item.kind="Lifestyle";item.seconds=qMax(1,int(qCeil(probe.duration()/1000.)));
    if(placement->currentIndex()==1)m_lib->programs.prepend(item);else m_lib->programs.append(item);m_lib->commitPrograms();showStudio();
    if(placement->currentIndex()==2){DragPayload payload;payload.kind="program";payload.id=item.id;payload.label=item.title;payload.clickToPlace=true;QTimer::singleShot(0,this,[payload]{DragController::instance()->begin(payload,QCursor::pos());});}
}

void MainWindow::showBuild(const QString &elementId, bool fresh) {
    if (fresh) {
        if(m_build->hasUnsavedChanges()){
            QMessageBox confirmation(QMessageBox::Question,"Open composition","Save the current Build composition before opening another?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,this);
            confirmation.setDefaultButton(QMessageBox::Save);confirmation.button(QMessageBox::Discard)->setText("Don't Save");const int choice=confirmation.exec();
            if(choice==QMessageBox::Cancel||(choice==QMessageBox::Save&&!m_build->saveForClose()))return;
        }
        if (elementId.isEmpty()) m_build->newElement();
        else m_build->editElement(elementId);
    }
    m_stack->setCurrentWidget(m_build);
    statusBar()->hide();
    if (m_workspaceAct) m_workspaceAct->setText("Studio");
    setWindowTitle("Cuebase — Build");
    report("Build opened");
}

void MainWindow::showStudio() {
    m_stack->setCurrentWidget(m_studio);
    statusBar()->show();
    if (m_workspaceAct) m_workspaceAct->setText("Build");
    setWindowTitle("Cuebase");
    report("Studio opened");
}

void MainWindow::resetLayout() {
    m_top->setSizes({640, 520});
    m_left->setSizes({1100, 250});
    m_root->setSizes({560, 240});
    m_control->show();
    m_preview->show();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QMessageBox confirmation(QMessageBox::Question,"Quit Cuebase","Save changes before closing?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,this);
    confirmation.setDefaultButton(QMessageBox::Save);confirmation.button(QMessageBox::Discard)->setText("Don't Save");
    confirmation.setInformativeText(m_build&&m_build->hasUnsavedChanges()?"The current Build composition has unsaved changes.":"Your library is saved automatically. Close Cuebase?");
    const int choice=confirmation.exec();
    if(choice==QMessageBox::Cancel){event->ignore();return;}
    if(choice==QMessageBox::Save&&m_build&&m_build->hasUnsavedChanges()&&!m_build->saveForClose()){event->ignore();return;}
    if(m_network)m_network->stop();
    QSettings s;
    s.setValue("layout/studio3/top", m_top->saveState());
    s.setValue("layout/studio3/left", m_left->saveState());
    s.setValue("layout/studio3/root", m_root->saveState());
    QMainWindow::closeEvent(event);
}

// ------------------------------------------------------------ status bar

void MainWindow::buildStatusBar() {
    QStatusBar *sb = statusBar();
    sb->setSizeGripEnabled(false);
    sb->setFixedHeight(23);

    auto *left = new QWidget;
    auto *ll = new QHBoxLayout(left);
    ll->setContentsMargins(10, 0, 0, 0);
    ll->setSpacing(8);
    m_keyChip = new QLabel;
    m_keyChip->setStyleSheet("color:#c2cedc;padding:1px 5px;font-size:10px;");
    m_keyChip->hide();
    m_msg = new QLabel("Wheel zoom · Middle drag pan · Right drag replay range");
    m_msg->setStyleSheet("color:#94a3b8; font-size:10px;");
    ll->addWidget(m_keyChip);
    ll->addWidget(m_msg, 1);
    sb->addWidget(left, 1);

    auto stat = [sb](QLabel *l) {
        l->setStyleSheet("color:#a3adba; font-size:10px; padding:0 7px;");
        sb->addPermanentWidget(l);
    };
    m_stState = new QLabel;
    m_stPos = new QLabel;
    m_stLeft = new QLabel;
    m_stItems = new QLabel;
    m_stMem = new QLabel;
    m_stPos->setToolTip("Playhead position / timeline length");
    m_stLeft->setToolTip("Total time left for the programs still in the queue");
    m_stMem->setToolTip("Memory used by Cuebase");
    m_stMem->setMinimumWidth(110);
    m_stMem->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    stat(m_stState);
    stat(m_stPos);
    stat(m_stLeft);
    stat(m_stItems);
    stat(m_stMem);  // last one: far right

    m_msgTimer.setSingleShot(true);
    connect(&m_msgTimer, &QTimer::timeout, this, [this] {
        m_msg->setText("Wheel zoom · Middle drag pan · Right drag replay range");
        m_msg->setStyleSheet("color:#94a3b8; font-size:10px;");
    });
    m_statTimer.setInterval(500);
    connect(&m_statTimer, &QTimer::timeout, this, &MainWindow::updateStats);
    m_statTimer.start();
    connect(m_play, &Playout::stateChanged, this, &MainWindow::updateStats);
    connect(m_lib, &Library::programsChanged, this, &MainWindow::updateStats);
    connect(m_lib, &Library::placementsChanged, this, &MainWindow::updateStats);
    m_memTick = 0;
    updateStats();
}

void MainWindow::updateStats() {
    if (!m_stState) return;
    const bool live = m_play->isLive();
    const bool paused = m_play->state() == Playout::Paused;
    const bool replay=m_play->replay()->active();
    m_stState->setText(replay?"REPLAY":paused ? "❚❚ PAUSED" : live ? "● ON AIR" : "● OFF AIR");
    m_stState->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600; padding:0 7px;")
                                 .arg(replay?"#e9b674":paused ? "#fbbf24" : live ? "#ef4444" : "#64748b"));
    m_stPos->setText(QString("Playhead %1 / %2").arg(clockText(m_play->playheadMs()), clockText(m_play->totalMs())));
    if(replay)m_stPos->setText(QString("Replay %1 · Main %2").arg(clockText(m_play->replay()->position),clockText(m_play->playheadMs())));
    const auto left=m_play->remainingMs();const auto days=left/86400000;
    m_stLeft->setText((days?QString("%1d ").arg(days):QString())+clockText(left%86400000)+" remaining");

    int programs = 0, ads = 0;
    for (const ProgramItem &p : std::as_const(m_lib->programs)) if(m_play->groupEndMs(p.id)>m_play->playheadMs()) (p.isAdSpot() ? ads : programs)++;
    QString items = QString("%1 program%2").arg(programs).arg(programs == 1 ? "" : "s");
    if (ads) items += QString(" · %1 ad%2").arg(ads).arg(ads == 1 ? "" : "s");
    const int placed = int(m_lib->placements.size());
    if (placed) items += QString(" · %1 overlays").arg(placed);
    m_stItems->setText(items);

    if (m_memTick++ % 4 == 0) {  // memory is only read every couple of seconds
        const double mb = memoryMb();
        m_stMem->setText(mb < 0 ? QString("Memory n/a") : QString("Memory %1 MB").arg(mb, 0, 'f', 1));
    }
}

void MainWindow::showResult(const QString &text) {
    ++m_reportSeq;
    m_msg->setText(text);
    m_msg->setStyleSheet("color:#e2e8f0; font-size:12px; font-weight:600;");
    m_msgTimer.start(4000);
    log(text);
}

// What just happened. Held back until the keys are let go, so the left side reads "Ctrl + B" while pressing
// and "Control panel hidden" afterwards.
void MainWindow::report(const QString &text) {
    ++m_reportSeq;
    if (m_keysHeld) m_pendingResult = text;
    else showResult(text);
}

QString MainWindow::comboText(QKeyEvent *k) {
    const Qt::KeyboardModifiers m = k->modifiers();
    const int key = k->key();
    const bool fkey = key >= Qt::Key_F1 && key <= Qt::Key_F12;
    const bool modKey = key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta;
    const bool command = m & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);

    QStringList parts;
    if (m & Qt::ControlModifier) parts << "Ctrl";
    if (m & Qt::AltModifier) parts << "Alt";
    if (m & Qt::ShiftModifier) parts << "Shift";
    if (m & Qt::MetaModifier) parts << "Win";
    if (modKey) return command ? parts.join(" + ") : QString();  // plain Shift while typing isn't a shortcut
    if (!command && !fkey) return {};                            // ordinary typing
    const QString name = QKeySequence(key).toString(QKeySequence::NativeText);
    if (name.isEmpty()) return {};
    parts << name;
    return parts.join(" + ");
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e) {
    auto *widget=qobject_cast<QWidget*>(obj);
    if(widget&&(widget==this||isAncestorOf(widget))){
        auto supportedFiles=[](const QMimeData *mime){QStringList files;for(const auto &url:mime->urls()){if(!url.isLocalFile())continue;QFileInfo file(url.toLocalFile());if(file.isFile()&&QStringList{"png","jpg","jpeg","bmp","gif","webp","tif","tiff","svg","ico","mp4","mov","mkv","avi","webm","m4v","mpeg","mpg","ts","wav","mp3","flac","ogg","aac","m4a","opus","aiff","fbx","obj","gltf","glb","dae","stl","ply","cuebuild","json"}.contains(file.suffix().toLower()))files.append(file.absoluteFilePath());}return files;};
        if(e->type()==QEvent::DragEnter||e->type()==QEvent::DragMove){auto *drop=static_cast<QDropEvent*>(e);if(!supportedFiles(drop->mimeData()).isEmpty()){drop->acceptProposedAction();if(!m_fileOverlay)m_fileOverlay=new FileDropOverlay(this);return true;}}
        if(e->type()==QEvent::DragLeave){if(m_fileOverlay){delete m_fileOverlay;m_fileOverlay=nullptr;}}
        if(e->type()==QEvent::Drop){auto *drop=static_cast<QDropEvent*>(e);const auto files=supportedFiles(drop->mimeData());if(!files.isEmpty()){drop->acceptProposedAction();if(m_fileOverlay){delete m_fileOverlay;m_fileOverlay=nullptr;}QTimer::singleShot(0,this,[this,files]{importFiles(files);});return true;}}
    }
    switch (e->type()) {
    case QEvent::KeyPress: {
        auto *k = static_cast<QKeyEvent *>(e);
        if (k->isAutoRepeat()) break;
        const QString combo = comboText(k);
        if (combo.isEmpty()) break;
        m_keysHeld = true;
        m_pendingResult.clear();
        m_keyChip->setText(combo);
        m_keyChip->show();
        m_msgTimer.stop();
        m_msg->setText("");

        // text boxes handle their own clipboard/undo keys; still say what they did
        if (qobject_cast<QWidget *>(obj) && (k->modifiers() & Qt::ControlModifier)) {
            const bool shift = k->modifiers() & Qt::ShiftModifier;
            switch (k->key()) {
            case Qt::Key_C: m_pendingResult = "Copied"; break;
            case Qt::Key_X: m_pendingResult = "Cut"; break;
            case Qt::Key_V: m_pendingResult = "Pasted"; break;
            case Qt::Key_A: m_pendingResult = "Selected all"; break;
            case Qt::Key_Z: m_pendingResult = shift ? "Redo" : "Undo"; break;
            case Qt::Key_Y: m_pendingResult = "Redo"; break;
            default: break;
            }
        }
        break;
    }
    case QEvent::KeyRelease: {
        auto *k = static_cast<QKeyEvent *>(e);
        if (k->isAutoRepeat() || !m_keysHeld) break;
        m_keysHeld = false;
        m_keyChip->hide();
        if (m_pendingResult.isEmpty()) {
            m_msg->setText("Ready");
            m_msg->setStyleSheet("color:#94a3b8; font-size:12px;");
        } else {
            showResult(m_pendingResult);
        }
        m_pendingResult.clear();
        break;
    }
    case QEvent::ApplicationDeactivate:  // focus left while keys were down: no release will arrive
        if (m_keysHeld) {
            m_keysHeld = false;
            m_keyChip->hide();
            m_msg->setText("Ready");
        }
        break;
    default:
        break;
    }
    return QMainWindow::eventFilter(obj, e);
}

// -------------------------------------------------------- timeline editing

void MainWindow::onCut(const QString &programId, int atSec) {
    const ProgramItem *p = m_lib->program(programId);
    if (!p) return;
    const QString title = p->title;
    const qint64 at = m_play->programStartMs(programId) + qint64(atSec) * 1000;
    if (m_lib->cutProgram(programId, atSec, true).isEmpty()) {
        report("Can't cut there");
        return;
    }
    report(QString("Cut \u201c%1\u201d at %2. Press + to add something in between").arg(title, clockText(at)));
}

void MainWindow::showAddMenu(const QString &afterId, const QPoint &globalPos) {
    auto *menu = new AddMenu(this);
    connect(menu, &AddMenu::chosen, this, [this, afterId](const QString &what) { onAddChosen(what, afterId); });
    menu->popupAbove(globalPos);
}

void MainWindow::onAddChosen(const QString &what, const QString &afterId) {
    if (what == "Sound") {
        chooseSound(afterId);
    } else if (what == "Clip") {
        m_editor->showTool(2);
        m_lib->setPending("Clip", afterId);
        m_editor->focusNewProgram();
        report("Programs opened. Fill in the clip to place it at the +");
    } else if (what == "Ad Center") {
        m_editor->showTool(1);
        m_lib->setPending("Ad Center", afterId);
        report("Ad Center opened. Pick an ad to place at the +");
    } else if (what == "Element") {
        m_editor->showTool(3);
        m_lib->setPending("Element", afterId);
        report("Screen Elements opened. Pick an element to place at the +");
    }
}

void MainWindow::chooseSound(const QString &afterId) {
    const QString path = QFileDialog::getOpenFileName(this, "Choose a sound", QString(),
                                                      "Audio (*.mp3 *.wav *.ogg *.flac *.m4a *.aac *.wma);;All files (*)");
    if (path.isEmpty()) return;

    const qint64 start = m_lib->program(afterId) ? m_play->groupEndMs(afterId) : m_play->playheadMs();
    auto *probe = new QMediaPlayer(this);  // reads how long the sound is
    auto done = std::make_shared<bool>(false);
    auto finish = [=, this](int secs) {
        if (*done) return;
        *done = true;
        probe->deleteLater();
        Placement pl;
        pl.id = Library::newId();
        pl.type = "Sound";
        pl.name = QFileInfo(path).completeBaseName();
        pl.file = path;
        pl.startMs = start;
        pl.seconds = qMax(1, secs);
        m_lib->placements.append(pl);
        m_lib->commitPlacements();
        report(QString("Sound \u201c%1\u201d placed at %2. Drag it to move it").arg(pl.name, clockText(start)));
    };
    connect(probe, &QMediaPlayer::durationChanged, this, [=](qint64 ms) {
        if (ms > 0) finish(int((ms + 500) / 1000));
    });
    connect(probe, &QMediaPlayer::errorOccurred, this, [=] { finish(5); });
    QTimer::singleShot(4000, this, [=] { finish(5); });
    probe->setSource(QUrl::fromLocalFile(path));
}

// Something was dropped on the timeline. lane 0 = the main timeline (in front of beforeId, "" = at the end);
// 1..3 = one of the extra timelines, starting at startMs.
void MainWindow::onTimelineDrop(const DragPayload &d, int lane, qint64 startMs, const QString &beforeId) {
    if (lane == 0) {
        if (d.kind == "program") {
            m_lib->moveItemBefore(d.id, beforeId);
            report(QString("Now starts at %1").arg(clockText(m_play->programStartMs(d.id))));
        } else if (d.kind == "ad") {
            const Ad *ad = m_lib->ad(d.id);
            if (!ad) return;
            const qint64 at = beforeId.isEmpty() ? m_play->totalMs() : m_play->programStartMs(beforeId);
            ProgramItem item;
            item.id = Library::newId();
            item.title = ad->name;
            item.kind = "Ad";
            item.adId = ad->id;
            item.seconds = ad->seconds;
            m_lib->shiftPlacements(at, qint64(ad->seconds) * 1000);  // things placed later keep their place after the ad
            m_lib->insertBefore(beforeId, item);
            report(QString("Ad placed at %1").arg(clockText(at)));
        }
        return;
    }

    startMs = qMax<qint64>(0, startMs);
    if (d.kind == "placement") {
        Placement *pl = m_lib->placement(d.id);
        if (!pl) return;
        pl->layer = lane;
        pl->startMs = startMs;
        const QString name = pl->name;
        m_lib->commitPlacements();
        report(QString("\u201c%1\u201d now on timeline %2 at %3").arg(name).arg(lane + 1).arg(clockText(startMs)));
        return;
    }

    Placement pl;
    pl.id = Library::newId();
    pl.layer = lane;
    pl.startMs = startMs;
    if (d.kind == "program") {  // a copy of the programme's media on that timeline (the queue keeps the original)
        const ProgramItem *p = m_lib->program(d.id);
        if (!p) return;
        pl.name = p->title;
        pl.seconds = p->seconds;
        if (p->isAdSpot()) {
            const Ad *ad = m_lib->ad(p->adId);
            pl.type = "Ad";
            pl.refId = p->adId;
            pl.file = ad ? ad->file : QString();
        } else {
            pl.type = "Clip";
            pl.refId = p->id;
            pl.file = p->file;
            pl.offsetMs = p->offsetMs;
            pl.kind = p->kind;
        }
    } else if (d.kind == "ad") {
        const Ad *ad = m_lib->ad(d.id);
        if (!ad) return;
        pl.type = "Ad";
        pl.refId = ad->id;
        pl.name = ad->name;
        pl.file = ad->file;
        pl.seconds = ad->seconds;
    } else if (d.kind == "element") {
        const Element *e = m_lib->element(d.id);
        if (!e || e->type == "Break") return;
        pl.type = "Element";
        pl.refId = e->id;
        pl.name = e->name;
        pl.seconds = e->type == "Custom" ? qMax(1, e->seconds) : 8;
        pl.durationMs=e->lengthMs();
    } else {
        return;
    }
    m_lib->placements.append(pl);
    m_lib->commitPlacements();
    report(QString("\u201c%1\u201d placed on timeline %2 at %3").arg(pl.name).arg(lane + 1).arg(clockText(startMs)));
}

void MainWindow::onSpotRemoved(const QString &spotId) {
    const ProgramItem *p = m_lib->program(spotId);
    if (!p) return;
    const QString name = p->title;
    const qint64 end = m_play->groupEndMs(spotId);
    const qint64 len = qint64(p->seconds)*1000;
    m_lib->removeItem(spotId);
    m_lib->shiftPlacements(end, -len);  // whatever was placed after it moves back with the timeline
    report(QString("Ad \u201c%1\u201d removed from the timeline").arg(name));
}

void MainWindow::onPlacementRemoved(const QString &id) {
    for (int i = 0; i < m_lib->placements.size(); ++i)
        if (m_lib->placements[i].id == id) {
            const QString name = m_lib->placements[i].name;
            m_lib->placements.removeAt(i);
            m_lib->commitPlacements();
            report(QString("\u201c%1\u201d removed from the timeline").arg(name));
            return;
        }
}

void MainWindow::onPlacementLength(const QString &id) {
    Placement *pl = m_lib->placement(id);
    if (!pl) return;
    bool ok = false;
    const int secs = QInputDialog::getInt(this, "Length on timeline", "How long should it stay (seconds)?", pl->seconds, 1, 3600, 1, &ok);
    if (!ok) return;
    pl->seconds = secs;
    pl->durationMs=qint64(secs)*1000;
    m_lib->commitPlacements();
    report(QString("\u201c%1\u201d now lasts %2 s").arg(pl->name).arg(secs));
}
