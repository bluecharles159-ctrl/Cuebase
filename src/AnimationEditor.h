#pragma once
#include <QWidget>
#include <QHash>
class BuildModel;
class QTreeWidget;
class CurveView;
class AnimationEditor : public QWidget {
public:
    AnimationEditor(BuildModel *model, bool graph, QWidget *parent = nullptr);
private:
    void rebuild();
    BuildModel *m_model;
    QTreeWidget *m_channels;
    CurveView *m_curve;
    QString m_signature;
    QHash<QString,bool> m_expanded;
};
