#pragma once
#include "ParticlePath.h"
#include <QTransform>
#include <QWidget>
#include <functional>

class ParticlePathEditor final : public QWidget {
public:
    ParticlePathEditor(QWidget *canvas,const QJsonArray &initial,
                       std::function<QTransform()> projection,
                       std::function<void(QJsonArray)> save);
protected:
    bool eventFilter(QObject *,QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    void finish();
    void addSample(QPointF screen,bool last);
    std::function<QTransform()> project;
    std::function<void(QJsonArray)> commit;
    QVector<ParticlePath::Knot> knots;
    QVector<QPointF> stroke;
    bool draw=false;
    int active=-1,handle=0;
};
