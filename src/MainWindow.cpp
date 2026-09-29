#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QFont>
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
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "AppLinks.h"
#include "ControlPanel.h"
#include "Data.h"
#include "EditorPanel.h"
#include "HelpCenter.h"
#include "Playout.h"
#include "QueueStrip.h"
#include "TimelineWidget.h"
#include "ViewportWidget.h"

namespace {

const char *kStyle = R"(
* { color:#e5e9f0; font-size:13px; }
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
QListWidget { background:#0f1729; border:1px solid #243044; border-radius:8px; padding:4px; }
QListWidget::item { padding:6px; border-radius:6px; }
QListWidget::item:selected { background:#1d4ed8; }
QCheckBox { spacing:8px; }

QPushButton { background:#1e293b; border:1px solid #334155; border-radius:6px; padding:7px 14px; font-weight:600; }
QPushButton:hover { background:#2b3a55; }
QPushButton:disabled { color:#64748b; background:#141d2e; }
QPushButton#save { background:#16a34a; border:none; }
QPushButton#save:hover { background:#15803d; }
QPushButton#broadcast { background:#16a34a; border:none; font-size:15px; padding:12px; }
QPushButton#broadcast[live="true"] { background:#dc2626; }
QPushButton#broadcast:disabled { background:#14532d; color:#86efac; }
QToolButton { background:transparent; border:none; color:#94a3b8; }
QToolButton#rail { font-size:20px; border-radius:10px; }
QToolButton#rail:hover { background:#16213a; }
QToolButton#rail:checked { background:#131c2f; border:1px solid #243044; }
QToolButton#addBtn { background:#2563eb; color:white; border-radius:8px; font-size:18px; font-weight:700; }
QToolButton#addBtn:hover { background:#1d4ed8; }

QSlider::groove:horizontal { height:4px; background:#334155; border-radius:2px; }
QSlider::handle:horizontal { background:#60a5fa; width:14px; margin:-6px 0; border-radius:7px; }
QScrollArea, QScrollArea > QWidget > QWidget { background:transparent; border:none; }
QScrollBar:horizontal { height:8px; background:transparent; margin:0; }
QScrollBar::handle:horizontal { background:#334155; border-radius:4px; min-width:30px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
QScrollBar:vertical { width:8px; background:transparent; margin:0; }
QScrollBar::handle:vertical { background:#334155; border-radius:4px; min-height:30px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
QSplitter::handle { background:transparent; }
QSplitter::handle:hover { background:#1e40af; border-radius:2px; }
QStatusBar { background:#0f172a; }
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

QLabel *caption(const QString &t) {
    auto *l = new QLabel(t);
    l->setStyleSheet("color:#94a3b8; font-weight:700; font-size:11px;");
    return l;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Cuebase");
    resize(1400, 860);
    qApp->setStyleSheet(kStyle);
    setWindowIcon(QIcon(makeLogo(64)));

    buildUi();
    buildMenus();
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
        connect(a, &QAction::triggered, this, handler);
        menu->addAction(a);
    };
    auto focusCall = [](const char *slot) {
        return [slot] {
            if (QWidget *w = QApplication::focusWidget()) QMetaObject::invokeMethod(w, slot);
        };
    };

    // File
    auto *file = newMenu("File");
    add(file, "Exit", QKeySequence("Ctrl+Q"), [this] { close(); });

    // Edit (acts on the text field that has focus)
    auto *edit = newMenu("Edit");
    add(edit, "Undo", QKeySequence("Ctrl+Z"), focusCall("undo"));
    add(edit, "Redo", QKeySequence("Ctrl+Shift+Z"), focusCall("redo"));
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
    add(help, "Toggle Developer Tools", QKeySequence("Ctrl+Shift+I"), [this] { toggleDevTools(); });
    add(help, "Check For Updates", QKeySequence(),
        [this] { HelpCenter::openLink(this, "Check For Updates", AppLinks::updates); });
    add(help, "Privacy Policy", QKeySequence(), [this] { HelpCenter::showPrivacy(this); });
    add(help, "Documentation", QKeySequence(),
        [this] { HelpCenter::openLink(this, "Documentation", AppLinks::documentation); });
    add(help, "About", QKeySequence(), [this] { HelpCenter::showAbout(this); });

    // Window
    auto *window = newMenu("Window");
    add(window, "Full Screen", QKeySequence(Qt::Key_F11), [this] {
        if (isFullScreen()) showNormal(); else showFullScreen();
    });
    add(window, "Toggle Control Panel", QKeySequence("Ctrl+B"),
        [this] { m_control->setVisible(!m_control->isVisible()); });
    add(window, "Toggle Preview", QKeySequence("Ctrl+J"),
        [this] { m_preview->setVisible(!m_preview->isVisible()); });
    add(window, "Reset Layout", QKeySequence(), [this] { resetLayout(); });
    add(window, "Minimize", QKeySequence(), [this] { showMinimized(); });
}

QList<QPair<QString, QString>> MainWindow::shortcutRows() const {
    QList<QPair<QString, QString>> rows;
    for (QAction *a : findChildren<QAction *>())
        if (!a->shortcut().isEmpty())
            rows.append({a->text(), a->shortcut().toString(QKeySequence::NativeText)});
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
            statusBar()->showMessage("Log copied to clipboard", 3000);
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
    m_top->addWidget(new ViewportWidget(m_lib, m_play));
    m_top->addWidget(m_editor);
    m_top->setChildrenCollapsible(false);
    m_top->setHandleWidth(8);

    // (viewport | editor) over preview
    m_preview = buildPreview();
    m_left = new QSplitter(Qt::Vertical);
    m_left->addWidget(m_top);
    m_left->addWidget(m_preview);
    m_left->setChildrenCollapsible(false);
    m_left->setHandleWidth(8);

    // everything | control panel (vertical, on the right)
    m_control = new ControlPanel(m_lib, m_play);
    m_root = new QSplitter(Qt::Horizontal);
    m_root->addWidget(m_left);
    m_root->addWidget(m_control);
    m_root->setChildrenCollapsible(false);
    m_root->setHandleWidth(8);
    m_root->setStretchFactor(0, 1);

    auto *wrap = new QWidget;
    auto *wl = new QVBoxLayout(wrap);
    wl->setContentsMargins(10, 10, 10, 10);
    wl->addWidget(m_root);
    setCentralWidget(wrap);

    resetLayout();
    QSettings s;
    m_top->restoreState(s.value("layout/top").toByteArray());
    m_left->restoreState(s.value("layout/left").toByteArray());
    m_root->restoreState(s.value("layout/root").toByteArray());

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
    auto *f = new QFrame;
    f->setObjectName("panel");
    auto *l = new QVBoxLayout(f);
    l->setContentsMargins(12, 10, 12, 10);
    l->setSpacing(6);

    auto *tl = new TimelineWidget(m_play);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(106);
    scroll->setWidget(tl);
    connect(tl, &TimelineWidget::segmentClicked, m_play, &Playout::jumpTo);
    connect(m_play, &Playout::segmentChanged, scroll, [this, tl, scroll] {
        const int i = m_play->index();
        scroll->horizontalScrollBar()->setValue(i < 0 ? 0 : qMax(0, tl->blockX(i) - 30));
    });

    auto *addBtn = new QToolButton;
    addBtn->setObjectName("addBtn");
    addBtn->setText("+");
    addBtn->setToolTip("Add a program");
    addBtn->setFixedSize(30, 30);
    addBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QToolButton::clicked, this, [this] { m_editor->focusNewProgram(); });
    auto *row = new QHBoxLayout;
    row->addWidget(addBtn);
    row->addWidget(caption("PROGRAMS"));
    row->addStretch();

    l->addWidget(caption("TIMELINE"));
    l->addWidget(scroll);
    l->addLayout(row);
    l->addWidget(new QueueStrip(m_lib, m_play), 1);
    return f;
}

void MainWindow::resetLayout() {
    m_top->setSizes({640, 520});
    m_left->setSizes({470, 310});
    m_root->setSizes({1080, 260});
    m_control->show();
    m_preview->show();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QSettings s; // remember the panel sizes; closing never asks for confirmation
    s.setValue("layout/top", m_top->saveState());
    s.setValue("layout/left", m_left->saveState());
    s.setValue("layout/root", m_root->saveState());
    QMainWindow::closeEvent(event);
}
