#pragma once

#include <QFrame>

class Library;
class Playout;
class NetworkOutput;
class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

// The vertical control panel on the right: broadcast, transport, volume and live element toggles.
class ControlPanel : public QFrame {
    Q_OBJECT
public:
    ControlPanel(Library *lib, Playout *play, QWidget *parent = nullptr, NetworkOutput *output = nullptr);

private:
    void refresh();
    void rebuildElements();

    Library *m_lib;
    Playout *m_play;
    QLabel *m_chip, *m_now, *m_next, *m_left, *m_info;
    QPushButton *m_start, *m_pause, *m_skip, *m_break;
    QVBoxLayout *m_elementsLay;
};
