#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

class Library;
class Playout;

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    TimelineWidget(Library *lib, Playout *play, QWidget *parent = nullptr);
    QSize sizeHint() const override { return {m_totalW, 148}; }
    QSize minimumSizeHint() const override { return {m_totalW, 148}; }
    int blockX(int index) const { return index >= 0 && index < m_blocks.size() ? m_blocks[index].x : 0; }

signals:
    void segmentClicked(int index);
    void openProgram(const QString &id);
    void openAudio(const QString &id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    struct Block {
        int x = 0, w = 0, y = 0, h = 0;
        enum Kind { Program, Break, Ad, Audio } kind = Program;
        int segIndex = -1;
        QString id;
    };
    void relayout();
    int clipWidth(int seconds, int expanded) const;
    int hitBlock(const QPoint &pos) const;
    double ratioAt(const Block &b, int x) const;
    qint64 timeAt(const QPoint &pos) const;
    void cutHover();
    void trimHover();

    Library *m_lib;
    Playout *m_play;
    QVector<Block> m_blocks;
    int m_totalW = 400;
    int m_hover = -1;
    int m_resize = -1;
    int m_selected = -1;
    int m_mouseX = 0;
};
