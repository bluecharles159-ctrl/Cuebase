#pragma once

#include <QWidget>

class Library;

class PlanBoard : public QWidget {
    Q_OBJECT
public:
    explicit PlanBoard(Library *lib, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int hitTest(const QPoint &pos) const;

    Library *m_lib;
    int m_hit = -1;
    QPoint m_grab;
    bool m_dragging = false;
};
