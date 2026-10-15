#pragma once
#include "ProductionPlan.h"
#include <QGraphicsView>
#include <QPixmap>
#include <functional>
#include <memory>
struct PlanItemPresentation {
    QString title,detail;
    QPixmap thumbnail;
    QColor color;
};
class PlanningCanvas:public QGraphicsView {
    Q_OBJECT
public:
    explicit PlanningCanvas(QWidget *parent=nullptr);
    ~PlanningCanvas()override;
    void setDocument(const ProductionPlan &document);
    void setPlayhead(qint64 milliseconds);
    void setTimelineScale(double scale);
    void finishEditing();
    std::function<PlanItemPresentation(const PlanEntry&)> presentation;
signals:
    void documentEdited(const ProductionPlan &document);
    void sourceDropped(const QString &kind,const QString &id,int before);
    void seekRequested(qint64 milliseconds);
    void addRequested(const QPoint &globalPosition);
    void entryMenuRequested(const QString &id,const QPoint &globalPosition);
protected:
    void drawBackground(QPainter *,const QRectF &)override;
    void mousePressEvent(QMouseEvent *)override;
    void mouseMoveEvent(QMouseEvent *)override;
    void mouseReleaseEvent(QMouseEvent *)override;
    void mouseDoubleClickEvent(QMouseEvent *)override;
    void keyPressEvent(QKeyEvent *)override;
    void contextMenuEvent(QContextMenuEvent *)override;
    void dragEnterEvent(QDragEnterEvent *)override;
    void dragMoveEvent(QDragMoveEvent *)override;
    void dropEvent(QDropEvent *)override;
    void wheelEvent(QWheelEvent *)override;
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
