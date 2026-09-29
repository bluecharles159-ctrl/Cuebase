#pragma once

#include <QVector>
#include <QWidget>

class Playout;

// Paints the broadcast timeline: programs, break (BR) bumpers and ads, with a moving playhead.
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(Playout *play, QWidget *parent = nullptr);
    QSize sizeHint() const override { return {m_totalW, 84}; }
    QSize minimumSizeHint() const override { return {m_totalW, 84}; }
    int blockX(int index) const { return index >= 0 && index < m_blocks.size() ? m_blocks[index].x : 0; }
    int blockW(int index) const { return index >= 0 && index < m_blocks.size() ? m_blocks[index].w : 0; }

signals:
    void segmentClicked(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    struct Block { int x, w; };
    void relayout();

    Playout *m_play;
    QVector<Block> m_blocks;
    int m_totalW = 400;
};
