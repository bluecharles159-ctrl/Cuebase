#include "Mesh.h"

#include <QJsonArray>
#include <QStringList>
#include <QtMath>
#include <algorithm>

// ------------------------------------------------------------------ primitives

QStringList Mesh::primitiveKinds() { return {"Plane", "Cube", "Circle", "Sphere", "Cylinder", "Cone", "Torus", "Grid"}; }

Mesh Mesh::primitive(const QString &kind) {
    Mesh m;
    const int N = 32;
    auto ring = [&](double z, double r) {
        const int base = m.v.size();
        for (int i = 0; i < N; ++i) {
            const double a = 2 * M_PI * i / N;
            m.v.append({float(r * qCos(a)), float(r * qSin(a)), float(z)});
        }
        return base;
    };
    if (kind == "Plane") {
        m.v = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
        m.f = {{0, 1, 2, 3}};
    } else if (kind == "Cube") {
        m.v = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
        m.f = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    } else if (kind == "Circle") {
        ring(0, 1);
        QVector<int> l;
        for (int i = 0; i < N; ++i) l << i;
        m.f.append(l);
    } else if (kind == "Cylinder") {
        const int b = ring(-1, 1), t = ring(1, 1);
        for (int i = 0; i < N; ++i) m.f.append({b + i, b + (i + 1) % N, t + (i + 1) % N, t + i});
        QVector<int> top, bot;
        for (int i = 0; i < N; ++i) {
            top << t + i;
            bot << b + N - 1 - i;
        }
        m.f.append(top);
        m.f.append(bot);
    } else if (kind == "Cone") {
        const int b = ring(-1, 1);
        m.v.append({0, 0, 1});
        const int apex = m.v.size() - 1;
        for (int i = 0; i < N; ++i) m.f.append({b + i, b + (i + 1) % N, apex});
        QVector<int> bot;
        for (int i = 0; i < N; ++i) bot << b + N - 1 - i;
        m.f.append(bot);
    } else if (kind == "Sphere") {
        const int S = 32, R = 16;
        m.v.append({0, 0, 1});
        for (int r = 1; r < R; ++r) {
            const double th = M_PI * r / R;
            for (int i = 0; i < S; ++i) {
                const double a = 2 * M_PI * i / S;
                m.v.append({float(qSin(th) * qCos(a)), float(qSin(th) * qSin(a)), float(qCos(th))});
            }
        }
        m.v.append({0, 0, -1});
        const int bottom = m.v.size() - 1;
        auto P = [&](int r, int i) { return 1 + (r - 1) * S + (i % S); };
        for (int i = 0; i < S; ++i) m.f.append({0, P(1, i), P(1, i + 1)});
        for (int r = 1; r < R - 1; ++r)
            for (int i = 0; i < S; ++i) m.f.append({P(r, i), P(r + 1, i), P(r + 1, i + 1), P(r, i + 1)});
        for (int i = 0; i < S; ++i) m.f.append({P(R - 1, i), bottom, P(R - 1, i + 1)});
    } else if (kind == "Torus") {
        const int U = 32, V = 12;
        const double R = 1.0, r = 0.25;
        for (int i = 0; i < U; ++i)
            for (int j = 0; j < V; ++j) {
                const double u = 2 * M_PI * i / U, w = 2 * M_PI * j / V;
                m.v.append({float((R + r * qCos(w)) * qCos(u)), float((R + r * qCos(w)) * qSin(u)), float(r * qSin(w))});
            }
        auto P = [&](int i, int j) { return (i % U) * V + (j % V); };
        for (int i = 0; i < U; ++i)
            for (int j = 0; j < V; ++j) m.f.append({P(i, j), P(i + 1, j), P(i + 1, j + 1), P(i, j + 1)});
    } else if (kind == "Grid") {
        const int G = 10;
        for (int j = 0; j <= G; ++j)
            for (int i = 0; i <= G; ++i) m.v.append({float(-1 + 2.0 * i / G), float(-1 + 2.0 * j / G), 0});
        for (int j = 0; j < G; ++j)
            for (int i = 0; i < G; ++i) {
                const int a = j * (G + 1) + i;
                m.f.append({a, a + 1, a + G + 2, a + G + 1});
            }
    }
    return m;
}

// ------------------------------------------------------------------ queries

QVector3D Mesh::faceNormal(int i) const {
    const QVector<int> &L = f[i];
    QVector3D n;
    for (int k = 0; k < L.size(); ++k) {
        const QVector3D &a = v[L[k]], &b = v[L[(k + 1) % L.size()]];
        n += QVector3D((a.y() - b.y()) * (a.z() + b.z()), (a.z() - b.z()) * (a.x() + b.x()), (a.x() - b.x()) * (a.y() + b.y()));
    }
    return n.normalized();
}

QVector3D Mesh::faceCenter(int i) const {
    QVector3D c;
    for (int k : f[i]) c += v[k];
    return f[i].isEmpty() ? c : c / float(f[i].size());
}

QVector<QVector3D> Mesh::vertexNormals() const {
    QVector<QVector3D> n(v.size());
    for (int i = 0; i < f.size(); ++i) {
        QVector3D fn = faceNormal(i);
        for (int k : f[i]) n[k] += fn;
    }
    for (QVector3D &x : n) x.normalize();
    return n;
}

QHash<EdgeKey, QVector<int>> Mesh::edgeFaces() const {
    QHash<EdgeKey, QVector<int>> m;
    for (int i = 0; i < f.size(); ++i)
        for (int k = 0; k < f[i].size(); ++k) m[ekey(f[i][k], f[i][(k + 1) % f[i].size()])].append(i);
    return m;
}

QVector<EdgeKey> Mesh::edges() const {
    QSet<EdgeKey> s;
    for (const QVector<int> &L : f)
        for (int k = 0; k < L.size(); ++k) s.insert(ekey(L[k], L[(k + 1) % L.size()]));
    QVector<EdgeKey> out(s.begin(), s.end());
    std::sort(out.begin(), out.end());
    return out;
}

bool Mesh::isClosed() const {
    if (f.isEmpty()) return false;
    const auto m = edgeFaces();
    for (auto it = m.begin(); it != m.end(); ++it)
        if (it.value().size() != 2) return false;
    return true;
}

bool Mesh::bounds(QVector3D &mn, QVector3D &mx) const {
    if (v.isEmpty()) return false;
    mn = mx = v[0];
    for (const QVector3D &p : v) {
        mn = QVector3D(qMin(mn.x(), p.x()), qMin(mn.y(), p.y()), qMin(mn.z(), p.z()));
        mx = QVector3D(qMax(mx.x(), p.x()), qMax(mx.y(), p.y()), qMax(mx.z(), p.z()));
    }
    return true;
}

QVector3D Mesh::boundsCenter() const {
    QVector3D a, b;
    return bounds(a, b) ? (a + b) / 2 : QVector3D();
}

void Mesh::transform(const QMatrix4x4 &m) {
    for (QVector3D &p : v) p = m.map(p);
    if (m.determinant() < 0) flipNormals();  // a mirroring transform turns the faces inside out
}

void Mesh::append(const Mesh &o) {
    const int base = v.size();
    v += o.v;
    for (QVector<int> L : o.f) {
        for (int &i : L) i += base;
        f.append(L);
    }
}

void Mesh::flipNormals(const QSet<int> *faces) {
    for (int i = 0; i < f.size(); ++i)
        if (!faces || faces->contains(i)) std::reverse(f[i].begin(), f[i].end());
}

// Ear clipping, so concave faces survive being cut into triangles.
Mesh Mesh::triangulated() const {
    Mesh out;
    out.v = v;
    for (int fi = 0; fi < f.size(); ++fi) {
        QVector<int> L = f[fi];
        if (L.size() < 3) continue;
        if (L.size() == 3) {
            out.f.append(L);
            continue;
        }
        const QVector3D n = faceNormal(fi);
        int guard = L.size() * L.size();
        while (L.size() > 3 && guard-- > 0) {
            bool clipped = false;
            for (int i = 0; i < L.size(); ++i) {
                const int ia = L[(i + L.size() - 1) % L.size()], ib = L[i], ic = L[(i + 1) % L.size()];
                const QVector3D &a = v[ia], &b = v[ib], &c = v[ic];
                if (QVector3D::dotProduct(QVector3D::crossProduct(b - a, c - b), n) <= 1e-9f) continue;  // reflex corner
                bool inside = false;
                for (int k : L) {
                    if (k == ia || k == ib || k == ic) continue;
                    const QVector3D &p = v[k];
                    const float d1 = QVector3D::dotProduct(QVector3D::crossProduct(b - a, p - a), n);
                    const float d2 = QVector3D::dotProduct(QVector3D::crossProduct(c - b, p - b), n);
                    const float d3 = QVector3D::dotProduct(QVector3D::crossProduct(a - c, p - c), n);
                    if (d1 >= 0 && d2 >= 0 && d3 >= 0) {
                        inside = true;
                        break;
                    }
                }
                if (inside) continue;
                out.f.append({ia, ib, ic});
                L.removeAt(i);
                clipped = true;
                break;
            }
            if (!clipped) break;
        }
        if (L.size() == 3) out.f.append(L);
        else for (int i = 1; i + 1 < L.size(); ++i) out.f.append({L[0], L[i], L[i + 1]});  // fallback fan
    }
    return out;
}

void Mesh::compact(EditSel *sel) {
    QVector<int> remap(v.size(), -1);
    for (const QVector<int> &L : f)
        for (int i : L) remap[i] = 0;
    QVector<QVector3D> nv;
    for (int i = 0; i < v.size(); ++i)
        if (remap[i] == 0) {
            remap[i] = nv.size();
            nv.append(v[i]);
        }
    for (QVector<int> &L : f)
        for (int &i : L) i = remap[i];
    v = nv;
    if (sel) {
        QSet<int> sv;
        for (int i : sel->v)
            if (i >= 0 && i < remap.size() && remap[i] >= 0) sv.insert(remap[i]);
        sel->v = sv;
        QSet<EdgeKey> se;
        for (EdgeKey k : sel->e) {
            const int a = ekA(k), b = ekB(k);
            if (a < remap.size() && b < remap.size() && remap[a] >= 0 && remap[b] >= 0) se.insert(ekey(remap[a], remap[b]));
        }
        sel->e = se;
    }
}

void Mesh::normalizeSel(EditSel &s, SelMode m) const {
    auto faceEdges = [&](int fi, QVector<EdgeKey> &out) {
        out.clear();
        const QVector<int> &L = f[fi];
        for (int k = 0; k < L.size(); ++k) out.append(ekey(L[k], L[(k + 1) % L.size()]));
    };
    QVector<EdgeKey> fe;
    if (m == SelMode::Vertex) {
        s.e.clear();
        s.f.clear();
        for (EdgeKey k : edges())
            if (s.v.contains(ekA(k)) && s.v.contains(ekB(k))) s.e.insert(k);
        for (int i = 0; i < f.size(); ++i) {
            bool all = true;
            for (int k : f[i]) all = all && s.v.contains(k);
            if (all) s.f.insert(i);
        }
    } else if (m == SelMode::Edge) {
        s.v.clear();
        s.f.clear();
        for (EdgeKey k : s.e) {
            s.v.insert(ekA(k));
            s.v.insert(ekB(k));
        }
        for (int i = 0; i < f.size(); ++i) {
            faceEdges(i, fe);
            bool all = !fe.isEmpty();
            for (EdgeKey k : fe) all = all && s.e.contains(k);
            if (all) s.f.insert(i);
        }
    } else {
        s.v.clear();
        s.e.clear();
        for (int i : s.f) {
            if (i < 0 || i >= f.size()) continue;
            faceEdges(i, fe);
            for (EdgeKey k : fe) s.e.insert(k);
            for (int k : f[i]) s.v.insert(k);
        }
    }
}

QVector3D Mesh::selCenter(const EditSel &s) const {
    QVector3D c;
    int n = 0;
    for (int i : s.v)
        if (i >= 0 && i < v.size()) {
            c += v[i];
            ++n;
        }
    return n ? c / float(n) : QVector3D();
}

// ------------------------------------------------------------------ json

QJsonObject Mesh::toJson() const {
    QJsonArray va, fa;
    for (const QVector3D &p : v) {
        va.append(double(p.x()));
        va.append(double(p.y()));
        va.append(double(p.z()));
    }
    for (const QVector<int> &L : f) {
        QJsonArray a;
        for (int i : L) a.append(i);
        fa.append(a);
    }
    return {{"v", va}, {"f", fa}};
}

Mesh Mesh::fromJson(const QJsonObject &j) {
    Mesh m;
    const QJsonArray va = j["v"].toArray();
    for (int i = 0; i + 2 < va.size(); i += 3) m.v.append({float(va[i].toDouble()), float(va[i + 1].toDouble()), float(va[i + 2].toDouble())});
    for (const QJsonValue &fv : j["f"].toArray()) {
        QVector<int> L;
        for (const QJsonValue &i : fv.toArray()) L.append(i.toInt());
        bool ok = L.size() >= 3;
        for (int i : L) ok = ok && i >= 0 && i < m.v.size();
        if (ok) m.f.append(L);
    }
    return m;
}
