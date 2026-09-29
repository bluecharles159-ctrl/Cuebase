#pragma once

#include <QFrame>

class Library;
class Playout;
class QStackedWidget;
class QToolButton;

// The editor: a thin icon rail on the left and a rounded work area with four tools:
// Display, Ads, Program Planner and Screen Elements.
class EditorPanel : public QFrame {
    Q_OBJECT
public:
    EditorPanel(Library *lib, Playout *play, QWidget *parent = nullptr);
    void showTool(int index);
    void focusNewProgram();

private:
    QStackedWidget *m_stack;
    QList<QToolButton *> m_tools;
};
