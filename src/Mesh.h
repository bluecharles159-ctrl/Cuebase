#pragma once

#include <QHash>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QSet>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <utility>

// Edges are stored as one 64-bit key: the smaller vertex index in the high half.
using EdgeKey = quint64;
inline EdgeKey ekey(int a, int b) {
    if (a > b) std::swap(a, b);
    return (quint64(quint32(a)) << 32) | quint32(b);
}
inline int ekA(EdgeKey k) { return int(k >> 32); }
inline int ekB(EdgeKey k) { return int(k & 0xffffffffu); }

enum class SelMode { Vertex, Edge, Face };

// What is selected while editing a mesh (vertices, edges and faces are tracked separately, like Blender).
struct EditSel {
    QSet<int> v;
    QSet<EdgeKey> e;
    QSet<int> f;
    bool isEmpty() const { return v.isEmpty() && e.isEmpty() && f.isEmpty(); }
    void clear() { v.clear(); e.clear(); f.clear(); }
};

// A polygon mesh: vertices and faces (each face a loop of vertex indices, counter-clockwise seen from outside).
struct Mesh {
    QVector<QVector3D> v;
    QVector<QVector<int>> f;

    bool isEmpty() const { return f.isEmpty(); }
    static QStringList primitiveKinds();
    static Mesh primitive(const QString &kind);   // Plane Cube Circle Sphere Cylinder Cone Torus Grid

    QVector3D faceNormal(int i) const;
    QVector3D faceCenter(int i) const;
    QVector<QVector3D> vertexNormals() const;
    QVector<EdgeKey> edges() const;
    QHash<EdgeKey, QVector<int>> edgeFaces() const;
    bool isClosed() const;
    bool bounds(QVector3D &mn, QVector3D &mx) const;
    QVector3D boundsCenter() const;
    void transform(const QMatrix4x4 &m);
    void append(const Mesh &o);
    void flipNormals(const QSet<int> *faces = nullptr);
    Mesh triangulated() const;
    void compact(EditSel *sel = nullptr);          // drops vertices no face uses
    void normalizeSel(EditSel &s, SelMode m) const; // makes v / e / f agree with the mode's primary set

    // ---- edit-mode operations (each returns false when there was nothing to do)
    bool subdivide(EditSel &s, SelMode m);
    bool extrude(EditSel &s, SelMode m, QVector3D *dir);
    // Individual inset: new inner vertices start on the old ones; info gets (vertex, face centre) so the caller
    // can slide them towards the centre.
    bool inset(EditSel &s, SelMode m, QVector<std::pair<int, QVector3D>> *info);
    QVector<EdgeKey> ringOf(EdgeKey e) const;       // the edge ring a loop cut through e would follow
    bool loopCut(EdgeKey e, int cuts, EditSel &out);
    bool fill(EditSel &s, SelMode m);
    bool flatten(EditSel &s, SelMode m);
    bool mergeAtCenter(EditSel &s, SelMode m);
    bool deleteSel(EditSel &s, SelMode m);
    Mesh separateFaces(EditSel &s, SelMode m);     // the selected faces leave this mesh and are returned
    bool splitFaces(EditSel &s, SelMode m);        // the selected faces stay but are cut loose from the rest
    QVector<Mesh> looseParts() const;
    QVector3D selCenter(const EditSel &s) const;

    // ---- used by modifiers
    static Mesh mirrored(const Mesh &m, bool x, bool y, bool z);
    static Mesh arrayed(const Mesh &m, int count, const QVector3D &relative, const QVector3D &constant);
    static Mesh solidified(const Mesh &m, double thickness, double offset);
    static Mesh bevelled(const Mesh &m, double width);
    static Mesh boolean(const Mesh &a, const Mesh &b, const QString &op);  // "Union", "Difference", "Intersect"

    QJsonObject toJson() const;
    static Mesh fromJson(const QJsonObject &j);
};
