#pragma once
#include <QTreeWidget>
class BuildModel;
class LayerPanel : public QTreeWidget {
public:
    LayerPanel(BuildModel *model, QWidget *parent = nullptr);
    void shiftOrder(int direction, bool toEdge=false);
protected:
    void dropEvent(QDropEvent *event) override;
private:
    void refresh();
    BuildModel *m;
    bool syncing=false;
    QString signature;
};
