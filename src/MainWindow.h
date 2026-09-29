#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>
#include <QPair>
#include <QStringList>

class EditorPanel;
class Library;
class Playout;
class QCloseEvent;
class QDialog;
class QPlainTextEdit;
class QSplitter;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildMenus();
    void buildUi();
    QWidget *buildPreview();
    void resetLayout();
    void toggleDevTools();
    void log(const QString &msg);
    QList<QPair<QString, QString>> shortcutRows() const;

    Library *m_lib = nullptr;
    Playout *m_play = nullptr;
    EditorPanel *m_editor = nullptr;
    QSplitter *m_root = nullptr;
    QSplitter *m_left = nullptr;
    QSplitter *m_top = nullptr;
    QWidget *m_control = nullptr;
    QWidget *m_preview = nullptr;

    QDialog *m_devTools = nullptr;
    QPlainTextEdit *m_devLog = nullptr;
    QStringList m_logLines;
};
