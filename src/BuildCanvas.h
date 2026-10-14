#pragma once

#include <QHash>
#include <QPointF>
#include <QWidget>

#include "BuildModel.h"

class Library;

// The Build viewport: a fixed-camera 2D broadcast composition with real 3D objects, 2D pan/zoom, picking, and Blender-style tools:
//   G or M move, R rotate, S scale (X / Y / Z to constrain, type a number, Enter or click to confirm, Esc to cancel),
//   Tab edit mode, 1 2 3 vertex / edge / face, E extrude, I inset, Ctrl+R loop cut, F fill, Shift+D duplicate ...
// The editor never orbits the render camera. Middle/right drag pans the composition and the wheel zooms it.
class BuildCanvas : public QWidget {
    Q_OBJECT
public:
    enum Shading { Wire = 0, Solid = 1, Rendered = 2 };

    BuildCanvas(BuildModel *m, Library *lib, QWidget *parent = nullptr);

    int shading() const { return m_shading; }
    void setShading(int s);
    bool cameraView() const { return m_cameraView; }
    void setCameraView(bool on);
    void viewFront();
    void viewRight();
    void viewTop();
    void toggleOrtho();
    void frameAll();
    void frameSelected();
    void actualSize();
    void alignSelection(const QString &operation);
    void clearGuides() { m_verticalGuides.clear(); m_horizontalGuides.clear(); update(); }
    bool modalActive() const { return m_modal.kind != Modal::None || m_loopCut; }

    bool showGrid = true, showAxes = true, showGizmos = true, placeCursor = false;
    bool showOverlays=true,showCursor=true,showLights=true,showCamera=true,showOrigins=true;
    bool showTitleSafe = true, showActionSafe = true, showRulers = true, snapping = true;

    void startMove() { startModal(Modal::Move); }
    void startRotate() { startModal(Modal::Rotate); }
    void startScale() { startModal(Modal::Scale); }
    void startExtrude();
    void startInset();
    void startLoopCut();
    void cancelModal();
    void pickSceneObject(std::function<void(QString)> picked);

signals:
    void addMenuRequested(const QPoint &globalPos);
    void message(const QString &text);
    void shortcutHint(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    struct Modal {
        enum Kind { None, Move, Rotate, Scale, Inset } kind = None;
        int axis = -1;  // -1 free, 0 X, 1 Y, 2 Z, 3 custom (customAxis)
        QVector3D customAxis;
        QPointF startMouse;
        QVector3D pivot;
        QPointF pivotScreen;
        QString numeric;
        QHash<QString, DProps> origProps;
        QVector<QVector3D> origVerts;
        QSet<int> editVerts;
        QVector<std::pair<int, QVector3D>> insetInfo;
        double startAngle = 0;
        bool fromMouseButton = false;  // middle drag / tweak: ends when the button is let go
        int resizeHandle = -1;
        QRectF resizeBounds;
        bool snapX=false, snapY=false;
        QPointF snapPoint;
    };

    ViewCam userCam() const;
    ViewCam activeCam(QRectF *rect) const;
    QRectF cameraFrame() const;
    void drawLine3D(QPainter &p, const ViewCam &cam, const QRectF &r, const QVector3D &a, const QVector3D &b) const;
    void drawGrid(QPainter &p, const ViewCam &cam, const QRectF &r) const;
    void drawGizmos(QPainter &p, const ViewCam &cam, const QRectF &r);
    void drawSelection(QPainter &p, const ViewCam &cam, const QRectF &r);
    void drawEdit(QPainter &p, const ViewCam &cam, const QRectF &r);
    void drawHud(QPainter &p, const QRectF &r);
    QRectF objectBounds(const DObject &object, const ViewCam &cam, const QRectF &frame) const;
    QRectF selectionBounds(const ViewCam &cam, const QRectF &frame) const;

    QString pickObject(const QPointF &pos, bool cycle = false);
    QPointF m_lastPick{-10000,-10000};
    QString m_lastPicked;
    std::function<void(QString)> m_objectPicker;
    bool pickEditElement(const QPointF &pos, bool additive);
    void boxSelect(const QRectF &box, bool additive);
    bool projectWorld(const ViewCam &cam, const QRectF &r, const QVector3D &w, QPointF *out, double *depth = nullptr) const;
    QMatrix4x4 editWorld() const;

    void startModal(Modal::Kind k, bool fromButton = false, bool skipUndo = false);
    void applyModal(const QPointF &mouse);
    void confirmModal();
    QVector3D rayPlane(const QPointF &px, const QVector3D &point, const QVector3D &normal, bool *ok) const;
    double axisParam(const QPointF &px, const QVector3D &origin, const QVector3D &axis) const;
    QVector3D axisVector() const;
    bool numericValue(double *v) const;
    void orbit(const QPointF &d);
    void pan(const QPointF &d);
    void updateLoopCutHover(const QPointF &pos);

    BuildModel *m_m;
    Library *m_lib;
    int m_shading = Rendered;
    bool m_cameraView = true, m_ortho = false;
    // The editor camera is fixed. These values are only 2D canvas navigation.
    double m_editorZoom = 1.0;
    QPointF m_editorPan{0, 0};
    QVector<double> m_verticalGuides, m_horizontalGuides;
    int m_dragGuide = -1;
    bool m_guideVertical = false;
    QString m_viewName = "2D Composition";

    Modal m_modal;
    QPointF m_press, m_last;
    Qt::MouseButton m_btn = Qt::NoButton;
    bool m_pressed = false, m_boxing = false, m_shiftPress = false, m_pressedOnSelected = false, m_tweakPending = false;
    QString m_pressObj;
    QPointF m_boxStart, m_boxEnd, m_hover;
    bool m_hasHover = false;

    bool m_loopCut = false;
    EdgeKey m_loopEdge = 0;
    bool m_loopValid = false;
    int m_cuts = 1;
    QVector<EdgeKey> m_loopRing;
};
