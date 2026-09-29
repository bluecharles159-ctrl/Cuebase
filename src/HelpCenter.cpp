#include "HelpCenter.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const char *kBrowserStyle =
    "QTextBrowser { background:#111a2b; border:1px solid #243044; border-radius:8px; padding:8px; }";

QDialog *makeDialog(QWidget *parent, const QString &title, int w, int h) {
    auto *d = new QDialog(parent);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(title);
    d->resize(w, h);
    return d;
}

QTextBrowser *makeBrowser(const QString &html) {
    auto *view = new QTextBrowser;
    view->setOpenExternalLinks(true);
    view->setStyleSheet(kBrowserStyle);
    view->setHtml(html);
    return view;
}

void htmlDialog(QWidget *parent, const QString &title, const QString &html, int w = 560, int h = 420) {
    auto *d = makeDialog(parent, title, w, h);
    auto *l = new QVBoxLayout(d);
    auto *close = new QPushButton("Close");
    QObject::connect(close, &QPushButton::clicked, d, &QDialog::close);
    auto *row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(close);
    l->addWidget(makeBrowser(html), 1);
    l->addLayout(row);
    d->show();
}

QString dataFolder() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

} // namespace

namespace HelpCenter {

void showWelcome(QWidget *parent, const std::function<void()> &startWalkthrough,
                 const std::function<void()> &showShortcuts) {
    auto *d = makeDialog(parent, "Welcome to Cuebase", 520, 360);
    auto *l = new QVBoxLayout(d);
    l->addWidget(makeBrowser(
        "<h2 style='color:#60a5fa'>Welcome to Cuebase</h2>"
        "<p>Cuebase is your TV production manager: monitor the output, go on air, "
        "and keep your TV, ADS and BV schedules in one place.</p>"
        "<p><b>Start here</b></p>"
        "<ul><li>Take the walkthrough for a quick tour.</li>"
        "<li>Check the keyboard shortcuts to work faster.</li>"
        "<li>Use Help &rarr; Editor Playground for quick notes.</li></ul>"), 1);
    auto *tour = new QPushButton("Start Walkthrough");
    auto *keys = new QPushButton("Keyboard Shortcuts");
    auto *close = new QPushButton("Close");
    auto *row = new QHBoxLayout;
    row->addWidget(tour);
    row->addWidget(keys);
    row->addStretch();
    row->addWidget(close);
    l->addLayout(row);
    QObject::connect(tour, &QPushButton::clicked, d, [d, startWalkthrough] { d->close(); startWalkthrough(); });
    QObject::connect(keys, &QPushButton::clicked, d, [d, showShortcuts] { d->close(); showShortcuts(); });
    QObject::connect(close, &QPushButton::clicked, d, &QDialog::close);
    d->show();
}

void showWalkthrough(QWidget *parent) {
    static const QStringList steps = {
        "<h2 style='color:#60a5fa'>The layout</h2>"
        "<p>The <b>viewport</b> (live monitor) and the <b>editor</b> sit side by side. The <b>preview</b> "
        "(timeline and programs queue) is underneath, and the <b>control panel</b> is on the right. "
        "Drag the thin gaps between panels to resize them. Window &rarr; Reset Layout puts them back.</p>",

        "<h2 style='color:#60a5fa'>Program Planner</h2>"
        "<p>In the editor, open the calendar icon. Add programs with a title, type and length, and optionally a "
        "media file. Programs play one after the other in list order. Use the arrows to reorder them.</p>",

        "<h2 style='color:#60a5fa'>Ads and breaks</h2>"
        "<p>Create ads in the megaphone tool, then choose <b>Ad after</b> on a program. Around each ad "
        "Cuebase plays two <b>breaks (BR)</b>: <i>Break in</i> before the ad and <i>Break out</i> after it. "
        "Breaks are screen elements you can edit.</p>",

        "<h2 style='color:#60a5fa'>Screen Elements</h2>"
        "<p>In the puzzle-piece tool, create logos, banners, ad elements and breaks. Switch a logo or banner on "
        "and it appears on the viewport straight away. Ad elements only show while an ad is playing.</p>",

        "<h2 style='color:#60a5fa'>Going on air</h2>"
        "<p>Press <b>Start Broadcast</b> in the control panel. The timeline playhead moves, the viewport shows "
        "<i>Breaking in Ns</i> before each break, and finished programs slide off to the left of the queue. "
        "Click any block on the timeline to jump to it.</p>",

        "<h2 style='color:#60a5fa'>Your data</h2>"
        "<p>Programs, ads, elements and display settings are saved automatically on this computer. "
        "See Help &rarr; Privacy Policy for where.</p>"};

    auto *d = makeDialog(parent, "Cuebase Walkthrough", 560, 380);
    auto *l = new QVBoxLayout(d);
    auto *view = makeBrowser({});
    auto *counter = new QLabel;
    auto *back = new QPushButton("Back");
    auto *next = new QPushButton("Next");
    auto *row = new QHBoxLayout;
    row->addWidget(counter);
    row->addStretch();
    row->addWidget(back);
    row->addWidget(next);
    l->addWidget(view, 1);
    l->addLayout(row);
    d->setProperty("step", 0);

    auto render = [=] {
        const int i = d->property("step").toInt();
        view->setHtml(steps[i]);
        counter->setText(QString("Step %1 of %2").arg(i + 1).arg(steps.size()));
        back->setEnabled(i > 0);
        next->setText(i == steps.size() - 1 ? "Done" : "Next");
    };
    QObject::connect(back, &QPushButton::clicked, d, [=] {
        d->setProperty("step", d->property("step").toInt() - 1);
        render();
    });
    QObject::connect(next, &QPushButton::clicked, d, [=] {
        const int i = d->property("step").toInt() + 1;
        if (i >= steps.size()) {
            d->close();
            return;
        }
        d->setProperty("step", i);
        render();
    });
    render();
    d->show();
}

void showReleaseNotes(QWidget *parent) {
    htmlDialog(parent, "Release Notes",
        "<h2 style='color:#60a5fa'>Release Notes</h2>"
        "<h3>0.4.0</h3><ul>"
        "<li>New layout: viewport, editor, preview and control panel, all resizable.</li>"
        "<li>Editor tools: Display, Ads, Program Planner and Screen Elements.</li>"
        "<li>Timeline with breaks (BR) around ads, and a programs queue that slides played items away.</li>"
        "<li>Live monitor with a moving timeline bar and a Breaking in countdown.</li>"
        "<li>Optional media file playback for programs and ads.</li></ul>"
        "<h3>0.3.0</h3><ul>"
        "<li>New header with File, Edit, Window and Help menus.</li>"
        "<li>Help center: welcome, walkthrough, release notes, shortcuts, license, privacy and about.</li>"
        "<li>Developer Tools window with a live activity log.</li>"
        "<li>Editor Playground scratchpad with open and save.</li></ul>"
        "<h3>0.2.0</h3><ul>"
        "<li>First Cuebase window: live monitor with timecode and Start/Stop Broadcast.</li>"
        "<li>Output and graphics settings that are remembered between launches.</li>"
        "<li>Scheduler with TV, ADS and BV lists saved to disk.</li></ul>"
        "<h3>0.1.0</h3><ul><li>Project created and build environment verified.</li></ul>");
}

void showShortcuts(QWidget *parent, const QList<QPair<QString, QString>> &rows) {
    QString html = "<h2 style='color:#60a5fa'>Keyboard Shortcuts</h2><table cellpadding='6' width='100%'>";
    for (const auto &r : rows)
        html += "<tr><td>" + r.first.toHtmlEscaped() + "</td><td align='right'><b>" +
                r.second.toHtmlEscaped() + "</b></td></tr>";
    html += "</table>";
    htmlDialog(parent, "Keyboard Shortcuts", html, 480, 420);
}

void showLicense(QWidget *parent) {
    htmlDialog(parent, "License",
        "<h2 style='color:#60a5fa'>License</h2>"
        "<p>Cuebase &copy; 2026. All rights reserved.</p>"
        "<p>Cuebase is built with the Qt toolkit, used under the GNU Lesser General Public License "
        "version 3 (LGPLv3). Qt and the Qt logo are trademarks of The Qt Company Ltd. "
        "More information: qt.io/licensing</p>", 520, 300);
}

void showPrivacy(QWidget *parent) {
    htmlDialog(parent, "Privacy Policy",
        "<h2 style='color:#60a5fa'>Privacy Policy</h2>"
        "<p>Cuebase keeps everything on this computer and does not send your data anywhere.</p>"
        "<ul><li><b>Programs, ads, screen elements and display settings</b> are saved in <i>" + dataFolder().toHtmlEscaped() +
        "/cuebase.json</i>.</li>"
        "<li>Your panel sizes are remembered in your Windows user profile.</li>"
        "<li>Media files you choose are played from where they are. Cuebase never copies or uploads them.</li>"
        "<li>Links in the Help menu open in your web browser, which follows that site's own policy.</li></ul>",
        540, 340);
}

void showAbout(QWidget *parent) {
    QMessageBox::about(parent, "About Cuebase",
        QString("<h3>Cuebase %1</h3><p>TV production manager.</p><p>Qt %2 &middot; GCC %3</p>")
            .arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion()),
                 QString::fromLatin1(__VERSION__)));
}

void openPlayground(QWidget *parent) {
    auto *d = makeDialog(parent, "Editor Playground", 640, 440);
    auto *l = new QVBoxLayout(d);
    auto *edit = new QPlainTextEdit;
    edit->setPlaceholderText("Scratchpad: jot cue notes, rundown ideas or scripts here.");
    auto *open = new QPushButton("Open...");
    auto *save = new QPushButton("Save As...");
    auto *clear = new QPushButton("Clear");
    auto *close = new QPushButton("Close");
    auto *row = new QHBoxLayout;
    row->addWidget(open);
    row->addWidget(save);
    row->addWidget(clear);
    row->addStretch();
    row->addWidget(close);
    l->addWidget(edit, 1);
    l->addLayout(row);

    QObject::connect(open, &QPushButton::clicked, d, [d, edit] {
        const QString path = QFileDialog::getOpenFileName(d, "Open text file", QString(),
                                                          "Text files (*.txt *.md);;All files (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(d, "Open", "Could not open " + path);
            return;
        }
        edit->setPlainText(QString::fromUtf8(f.readAll()));
    });
    QObject::connect(save, &QPushButton::clicked, d, [d, edit] {
        const QString path = QFileDialog::getSaveFileName(d, "Save text file", "notes.txt",
                                                          "Text files (*.txt *.md);;All files (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QMessageBox::warning(d, "Save", "Could not write " + path);
            return;
        }
        f.write(edit->toPlainText().toUtf8());
    });
    QObject::connect(clear, &QPushButton::clicked, edit, &QPlainTextEdit::clear);
    QObject::connect(close, &QPushButton::clicked, d, &QDialog::close);
    d->show();
}

void openLink(QWidget *parent, const QString &what, const char *url) {
    const QString u = QString::fromUtf8(url).trimmed();
    if (u.isEmpty()) {
        QMessageBox::information(parent, what,
                                 what + " has no address set yet.\nAdd it in src/AppLinks.h and rebuild.");
        return;
    }
    if (!QDesktopServices::openUrl(QUrl(u)))
        QMessageBox::warning(parent, what, "Could not open " + u);
}

} // namespace HelpCenter
