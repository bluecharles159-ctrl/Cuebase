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
    d->setStyleSheet("QDialog{background:palette(window);border-radius:10px;} QWidget{font-size:10px;} QPushButton{background:#484848;border:0;border-radius:4px;padding:5px 12px;} QScrollBar:vertical{width:4px;}");
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

        "<h2 style='color:#60a5fa'>Programs</h2>"
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
        "<li>Editor tools: Display, Ads, Programs and Screen Elements.</li>"
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
    QString html="<style>body{font-family:'Segoe UI';font-size:11px;color:#ddd}h3{font-size:12px;color:#9ebbe0;margin-top:18px}td{padding:5px}</style><h2>Keyboard shortcuts</h2>";
    const QStringList groups{"Application","Objects and navigation","Animation","Mesh editing","Graph editor","Property controls","Studio timeline"};
    for(int group=0;group<groups.size();++group){html+="<h3>"+groups[group]+"</h3><table width='100%' cellspacing='0'>";
        for(const auto &row:rows){const QString label=row.first;int category=0;
            if(label.startsWith("Studio timeline:"))category=6;
            else if(label.startsWith("Build mesh")||label.contains("mesh edit"))category=3;
            else if(label.startsWith("Graph:"))category=4;
            else if(label.startsWith("Values:")||label.startsWith("Property key:"))category=5;
            else if(label.startsWith("Build:"))category=(label.contains("key")||label.contains("frame")&&!label.contains("artboard")||label.contains("play"))?2:1;
            if(category!=group)continue;QString description=label;const int colon=description.indexOf(':');if(colon>=0)description=description.mid(colon+1).trimmed();
            html+="<tr><td>"+description.toHtmlEscaped()+"</td><td align='right' nowrap='nowrap' style='color:#eee'>"+row.second.toHtmlEscaped()+"</td></tr>";
        }html+="</table>";
    }
    htmlDialog(parent,"Keyboard Shortcuts",html,520,600);
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
        "<p>Cuebase stores your library on this computer. When you start network output, it sends the program picture and mixed audio to the RTMP or SRT destination you configure.</p>"
        "<ul><li><b>Programs, ads, screen elements and display settings</b> are saved in <i>" + dataFolder().toHtmlEscaped() +
        "/cuebase.json</i>.</li>"
        "<li>Your panel sizes are remembered in your Windows user profile.</li>"
        "<li>Imported media stays at its original file path. Broadcast output transmits the selected media as part of the live program.</li>"
        "<li>Links in the Help menu open in your web browser, which follows that site's own policy.</li></ul>",
        540, 340);
}

void showAbout(QWidget *parent) {
    htmlDialog(parent,"About Cuebase",QString("<h2>Cuebase %1</h2><p>Create television graphics, animate screen elements, assemble a rundown and send your program picture and mixed audio to an RTMP or SRT destination.</p><p><b>Build</b> is the graphics and animation workspace. <b>Studio</b> manages programs, timed graphics, audio and live output.</p><p>Projects and media references are stored locally. Network output uses the destination configured by the producer.</p><p>Qt %2 &middot; GCC %3</p>").arg(QApplication::applicationVersion(),QString::fromLatin1(qVersion()),QString::fromLatin1(__VERSION__)),480,360);
}

void showDocumentation(QWidget *parent){
    htmlDialog(parent,"Cuebase Guide",
        "<h2>Produce a show with Cuebase</h2><h3>Build a screen element</h3>"
        "<p>Open Build to create a composition. Add shapes, text, media or a 3D object. Select a layer to edit its transform, background, material and effects. Save the composition to Screen Elements before scheduling it in Studio.</p>"
        "<p>Drag a layer onto another to parent it, or select children first and the parent last, then press Ctrl+P. Overflow X/Y controls clip children. Edit Parent Only holds children in place while the parent is adjusted.</p>"
        "<h3>Named entrance and exit animations</h3><p>In Build, open Animation Cues. Add a name and start/end frame range. Enable Hold on screen for an entrance, or disable it for an exit that hides the graphic when finished. Apply, then save the screen element.</p><p>In Studio, select the overlay to trigger its animations. Set a Program time timer, or an offset Before ad / After ad. For example, Exit with Before ad and 10 seconds begins ten seconds before each ad. A newer trigger replaces the current animation. Hide removes it immediately; a later timer may show it again.</p>"
        "<h3>Media and audio</h3><p>Shift+A → Media imports images, videos and audio. The World Background audio icon adds multiple tracks. Select a track to adjust volume, mute, start, source offset, playback speed, play length, looping and fades. Each track mixes independently.</p>"
        "<h3>Animate</h3><p>Click a property's diamond to insert a key at the current frame; click it again to remove that key. Auto Key records adjustments on animated channels. A changed value at a keyed frame is marked until keyed.</p>"
        "<p>In the graph, select channels on the left. Interpolation reports the selected keys, or selected channels when no keys are selected; Mixed means they differ. Choosing Bezier applies to that selection. Select keys to edit both handles and choose their handle type.</p>"
        "<p>Middle-drag pans. Wheel zooms time; Ctrl-drag zooms horizontally or vertically. G moves keys, S scales keys and handles, and X/Y constrains the operation. Right-click or Escape cancels. Ctrl+A selects keys, Ctrl+C/V copies and pastes at the playhead, and I inserts keys on selected channels. Alt-wheel steps frames. Home explicitly fits the view.</p>"
        "<h3>Assemble the rundown</h3><p>Import programs and arrange them in order. Drop screen elements onto an overlay lane at the intended time. Drag a clip to reposition it and drag its left or right edge to trim its on-screen duration. Wheel zooms the timeline; middle-drag pans.</p>"
        "<h3>Reusable Studio content</h3><p>Select text in Build and enable Studio Field &gt; Editable in Studio. Or type <code>{{studio:Headline}}Evening news</code> in Typography: the prefix becomes a field name and is removed from the visible text. In Screen Elements, edit Headline, inspect the private preview, and Apply content. This works for any text, including captions, names and sponsors. Animation and styling remain intact. Image/video objects can expose replaceable media fields too.</p>"
        "<h3>Replay and effects</h3><p>Right-drag the Studio timeline and choose Replay selected frames. Set the range and entry/return effects. Pause current scene starts unchecked: the program keeps advancing. Programs has default effects, Create effect in Build, and Return from replay. An effect starts without a rectangle; its Effects tab animates the real program picture with color, blur, zoom, pulse, grain and playback speed. Save it into Effects. Replay requires available timeline media; live sources without a recording cannot replay past footage.</p>"
        "<h3>Production and crew</h3><p>Production / Crew contains crew contacts, assignments, schedules, an audio/visual rundown and call sheets. Export a call sheet to HTML for printing. In Team session, start the host and create individual Viewer or Editor invitation codes. Crew use the HTTPS address on the studio network or VPN; verify the displayed certificate fingerprint first. Editors can update plans and exposed graphic text. Stale drafts are rejected; reload before retrying. Revoke an invitation or stop the session to remove access. Broadcast controls remain with the host.</p>"
        "<h3>Library and previews</h3><p>The Build name dropdown opens saved elements. New scene saves the current scene before starting fresh. First save asks for a thumbnail: scrub to a frame, drag to crop, and add an optional description and playlist. Preview reopens these settings. Cropping never changes broadcast output. Presets provide animated sponsor, news, break and ident designs.</p>"
        "<h3>Animation cue boards</h3><p>Create a cue in Build with its frame range and behavior, or select keys and choose Cue selected frames. The green range clears on Save or Cancel. A selected cue plays independently of the scene range. Studio previews run privately. Add authored cues to a board, reorder its cards, edit repeated text/media, and Modify keyframes independently. Open child cues to edit their own boards. Total duration includes holds and repetitions. Render places a baked independent copy at the playhead plus the delay, initially three seconds. Continue on air or Hide on air controls that copy. Timeline copies can be edited only with at least five minutes before the affected section while broadcasting. Extending a finite board repeats it.</p>"
        "<h3>Private preview and standby</h3><p>Scrub the viewport to look ahead privately; the bottom-right monitor continues showing live output. Return to live synchronizes the operator view without seeking the broadcast. Black / standby replaces outgoing picture and audio with black, branded editable messages, a saved element or looping media. Actual media and encoder errors can be copied explicitly into the message. Returning restores the existing program state. The header channel icon edits the station name, description and imported or Build-made logo.</p>"
        "<h3>Preview and broadcast</h3><p>Check the program preview, audio levels and scheduled graphics. Pausing holds the program clock and its animations. Configure RTMP or SRT output with your destination address, then start network output.</p>"
        "<h3>Local receiver and monitor</h3><p>Monitor &gt; Start local test receiver enables RTMP at <code>rtmp://127.0.0.1:1935/cuebase</code> or SRT at <code>srt://127.0.0.1:8890?streamid=publish:cuebase</code>. Leave the separate stream key empty. Watch at <code>http://127.0.0.1:8888/cuebase</code>. Connect metrics at <code>http://127.0.0.1:9998/metrics</code>, path cuebase. History records receiver connections, not unique people, and is available only for periods collected while Cuebase runs. Public distribution requires your own reachable server or CDN.</p>"
        "<h3>Save and recover</h3><p>The Studio library saves changes locally. Build compositions must be saved to update their Screen Element. Closing Cuebase offers Save, Don't Save and Cancel. Keep imported media at its original path or re-import it if moved.</p>",560,640);
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
