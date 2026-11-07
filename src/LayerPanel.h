#pragma once
#include <QTreeWidget>
#include <QVariantAnimation>
#include <QSet>
class BuildModel;
class LayerPanel : public QTreeWidget {
public:
    LayerPanel(BuildModel *model, QWidget *parent = nullptr);
    void shiftOrder(int direction, bool toEdge=false);
    void filterByName(const QString &text);
    void revealActive();
protected:
    void paintEvent(QPaintEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    void refresh();
    void updateFilter();
    QString filter;
    QSet<QString> collapsedBeforeSearch;
    BuildModel *m;
    bool syncing=false;
    QString signature;
    QVariantAnimation pulse;
    int boundary=0;
    bool ready=false;
};
