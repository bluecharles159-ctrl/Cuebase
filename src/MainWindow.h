#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>
#include <QPair>
#include <QStringList>
#include <QTimer>

class BuildWorkspace;
class EditorPanel;
struct DragPayload;
class Library;
class Playout;
class NetworkOutput;
class QCloseEvent;
class QDialog;
class QEvent;
class QKeyEvent;
class QLabel;
class QPlainTextEdit;
class QSplitter;
class QStackedWidget;
class QAction;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;  // watches shortcuts for the status bar

private:
    void buildMenus();
    void importFiles(const QStringList &paths);
    void importVideo(const QString &path);
    QWidget *m_fileOverlay = nullptr;
    void buildUi();
    QWidget *buildPreview();
    void resetLayout();
    void toggleDevTools();
    void log(const QString &msg);
    QList<QPair<QString, QString>> shortcutRows() const;

    // status bar (bottom): shortcuts and what they did on the left, stats in the middle, memory on the right
    void buildStatusBar();
    void updateStats();
    void report(const QString &text);      // "what just happened": shown after the keys are released
    void showResult(const QString &text);
    static QString comboText(QKeyEvent *e);

    // timeline editing
    void onCut(const QString &programId, int atSec);
    void showAddMenu(const QString &afterId, const QPoint &globalPos);
    void onAddChosen(const QString &what, const QString &afterId);
    void chooseSound(const QString &afterId);
    void onTimelineDrop(const DragPayload &payload, int lane, qint64 startMs, const QString &beforeId);
    void onSpotRemoved(const QString &spotId);
    void showAddModal();
    void showBuild(const QString &elementId, bool fresh);
    void showStudio();
    void onPlacementRemoved(const QString &id);
    void onPlacementLength(const QString &id);

    Library *m_lib = nullptr;
    Playout *m_play = nullptr;
    NetworkOutput *m_network = nullptr;
    EditorPanel *m_editor = nullptr;
    QSplitter *m_root = nullptr;
    QSplitter *m_left = nullptr;
    QSplitter *m_top = nullptr;
    QWidget *m_control = nullptr;
    QWidget *m_preview = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_studio = nullptr;
    BuildWorkspace *m_build = nullptr;
    QAction *m_workspaceAct = nullptr;

    QDialog *m_devTools = nullptr;
    QPlainTextEdit *m_devLog = nullptr;
    QStringList m_logLines;

    QLabel *m_keyChip = nullptr, *m_msg = nullptr;
    QLabel *m_stState = nullptr, *m_stPos = nullptr, *m_stLeft = nullptr, *m_stItems = nullptr, *m_stMem = nullptr;
    QTimer m_statTimer, m_msgTimer;
    QString m_pendingResult;
    bool m_keysHeld = false;
    int m_reportSeq = 0, m_memTick = 0;
};
