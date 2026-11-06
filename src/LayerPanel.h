#pragma once
#include <QTreeWidget>
#include <QVariantAnimation>
class BuildModel;
class LayerPanel : public QTreeWidget {
public:
    LayerPanel(BuildModel *model, QWidget *parent = nullptr);
    void shiftOrder(int direction, bool toEdge=false);
protected:
    void paintEvent(QPaintEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    void refresh();
    BuildModel *m;
    bool syncing=false;
    QString signature;
    QVariantAnimation pulse;
    int boundary=0;
    bool ready=false;
};
