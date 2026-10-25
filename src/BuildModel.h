#pragma once

#include <QObject>
#include <QSet>
#include <QVector>
#include <functional>

#include "ElementDesign.h"
#include "Mesh.h"

// The state and the operations of the Build workspace: the scene, what is selected, edit mode, undo and the
// animation clock. The viewport, the side panel and the timeline all work through it.
class BuildModel : public QObject {
    Q_OBJECT
public:
    BuildModel();
    ElementDesign design;
    QString activeId;
    QSet<QString> sel;
    qint64 t = 0;
    bool playing = false;
    QString previewCueId;
    int previewCueStep=0;
    bool cuePreviewWaiting() const;
    bool continueCuePreview();
    qint64 previewCueElapsed=0, cueRangeStart=-1, cueRangeEnd=-1;
    bool autoKey = false;              // OFF by default; explicit property key buttons are the normal workflow
    QString selChannel;
    bool isolateParents=false;
    int selKey = -1;  // the selected keyframe of the active object
    int fps = 25;
    double fpsBase = 1.0;

    bool editMode = false;
    SelMode selMode = SelMode::Vertex;
    EditSel esel;

    int frameMs() const { return qMax(1, qRound(1000.0 * fpsBase / qMax(1, fps))); }

    DObject *byId(const QString &id) { return design.find(id); }
    DObject *active() { return design.find(activeId); }
    bool isSelected(const QString &id) const { return sel.contains(id); }
    QVector<DObject *> selected();
    QVector<DObject *> inspected();

    // ---- scene
    void reset(const ElementDesign &d);
    QString uniqueName(const QString &base) const;
    QString addMesh(const QString &kind);
    QString addText();
    QString addImage(const QString &path);
    QString addLight(const QString &kind);
    QStringList duplicateSelected();
    void deleteSelected();
    bool joinSelected();
    bool setOrigin(const QString &kind);  // Geometry, World, Bottom, Selected
    bool separateLooseParts();
    bool applyModifier(const QString &objectId, int index);
    void rename(const QString &id, const QString &name);
    void hideSelected(bool hide);
    void showAll();
    void changeLightKind(const QString &kind);
    bool parentSelected(bool clear = false);

    // ---- selection
    void select(const QString &id, bool additive = false);  // additive: toggles
    void setSelection(const QSet<QString> &ids, const QString &active);
    void selectAll();
    void deselectAll();

    // ---- time and keys
    void setTime(qint64 ms);
    void editProps(const QString &id, const std::function<void(DProps &)> &f);
    void setProps(const QString &id, const DProps &p);
    void insertKey();
    void keyProperty(const QString &id, const QString &property);
    void togglePropertyKey(const QString &id, const QString &property);
    void resetProperty(const QString &id, const QString &property);
    void editProperty(const QString &id, const QString &property, double value);
    bool propertyIsKeyed(const DObject &o, const QString &property, qint64 atMs) const;
    void deleteKey();
    void setInterpolation(const QString &kind);
    void jumpKey(int dir);  // -1 / +1
    void stepFrame(int dir);

    // ---- undo
    void pushUndo(bool force = false);
    bool undo();
    bool redo();
    void dropUndo();   // forget the last snapshot (a cancelled modal operation)
    bool cancelToUndo();

    // ---- edit mode
    bool enterEdit();
    void exitEdit();
    void toggleEdit() { editMode ? exitEdit() : (void)enterEdit(); }
    void setSelMode(SelMode m);
    void editSelectAll(bool on);
    void editInvert();
    bool opSubdivide();
    bool opExtrude(QVector3D *dirWorld);
    bool opInset(QVector<std::pair<int, QVector3D>> *info);
    bool opLoopCut(EdgeKey e, int cuts);
    bool opFill();
    bool opFlatten();
    bool opMerge();
    bool opDelete();
    bool opFlip();
    bool opSeparate();
    bool opSplit();
    void reconcile();  // after an operation that set esel.v: derive the rest for the current mode
    Mesh *editMesh();

    static DProps decompose(const QMatrix4x4 &m);

    void notify() { emit changed(); }

signals:
    void componentRegistrationRequested(const QString &objectId,const QString &name);
    void cuePlaybackRequested();
    void cueFramesRequested(qint64 start,qint64 end);
    void cueEditorRequested(const QString &cueId);
    void changed();
    void timeChanged();
    void selectionChanged();
    void modeChanged();

private:
    quint64 m_lastGesture=0;
    QVector<QString> m_undo, m_redo;
    void restore(const QString &json);
    void shiftOrigin(DObject &o, const QVector3D &cLocal);
};
