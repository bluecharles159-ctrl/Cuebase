#include "Mesh.h"

#include <QtMath>
#include <algorithm>
#include <memory>
#include <functional>

namespace {

QVector3D lerp3(const QVector3D &a, const QVector3D &b, float t) { return a + (b - a) * t; }

// Eigenvector of the smallest eigenvalue of a symmetric 3x3 matrix (Jacobi). Gives a plane's normal.
QVector3D smallestEigenvector(double a[3][3]) {
    double v[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int sweep = 0; sweep < 24; ++sweep) {
        int p = 0, q = 1;
        double big = qAbs(a[0][1]);
        if (qAbs(a[0][2]) > big) { p = 0; q = 2; big = qAbs(a[0][2]); }
        if (qAbs(a[1][2]) > big) { p = 1; q = 2; big = qAbs(a[1][2]); }
        if (big < 1e-12) break;
        const double th = 0.5 * qAtan2(2 * a[p][q], a[q][q] - a[p][p]);
        const double c = qCos(th), s = qSin(th);
        for (int k = 0; k < 3; ++k) {
            const double akp = a[k][p], akq = a[k][q];
            a[k][p] = c * akp - s * akq;
            a[k][q] = s * akp + c * akq;
        }
        for (int k = 0; k < 3; ++k) {
            const double apk = a[p][k], aqk = a[q][k];
            a[p][k] = c * apk - s * aqk;
            a[q][k] = s * apk + c * aqk;
        }
        for (int k = 0; k < 3; ++k) {
            const double vkp = v[k][p], vkq = v[k][q];
            v[k][p] = c * vkp - s * vkq;
            v[k][q] = s * vkp + c * vkq;
        }
    }
    int m = 0;
    for (int i = 1; i < 3; ++i)
        if (a[i][i] < a[m][m]) m = i;
    return QVector3D(float(v[0][m]), float(v[1][m]), float(v[2][m])).normalized();
}

} // namespace

// ============================================================== subdivide

bool Mesh::subdivide(EditSel &s, SelMode m) {
    normalizeSel(s, m);
    if (s.e.isEmpty()) return false;
    QHash<EdgeKey, int> mid;
    auto M = [&](int a, int b) {
        const EdgeKey k = ekey(a, b);
        auto it = mid.find(k);
        if (it != mid.end()) return it.value();
        v.append((v[a] + v[b]) / 2);
        mid[k] = v.size() - 1;
        return int(v.size() - 1);
    };
    QVector<QVector<int>> nf;
    QSet<int> newVerts;
    const QVector<QVector<int>> old = f;
    for (int fi = 0; fi < old.size(); ++fi) {
        const QVector<int> &L = old[fi];
        const int n = L.size();
        QVector<bool> sel(n);
        int cnt = 0;
        for (int k = 0; k < n; ++k) {
            sel[k] = s.e.contains(ekey(L[k], L[(k + 1) % n]));
            cnt += sel[k];
        }
        if (cnt == 0) {
            nf.append(L);
            continue;
        }
        QVector<int> mids(n, -1);
        for (int k = 0; k < n; ++k)
            if (sel[k]) {
                mids[k] = M(L[k], L[(k + 1) % n]);
                newVerts.insert(mids[k]);
            }
        if (cnt == n && n == 3) {
            nf.append({L[0], mids[0], mids[2]});
            nf.append({mids[0], L[1], mids[1]});
            nf.append({mids[2], mids[1], L[2]});
            nf.append({mids[0], mids[1], mids[2]});
        } else if (cnt == n) {
            QVector3D c;
            for (int i : L) c += v[i];
            v.append(c / float(n));
            const int ci = v.size() - 1;
            newVerts.insert(ci);
            for (int k = 0; k < n; ++k) nf.append({mids[(k + n - 1) % n], L[k], mids[k], ci});
        } else if (n == 4 && cnt == 2 && sel[0] == sel[2]) {  // two opposite edges: a clean split in two
            const int p = sel[0] ? 0 : 1;
            nf.append({L[p], mids[p], mids[(p + 2) % 4], L[(p + 3) % 4]});
            nf.append({mids[p], L[(p + 1) % 4], L[(p + 2) % 4], mids[(p + 2) % 4]});
        } else {  // otherwise the new points just join the outline (no cracks)
            QVector<int> out;
            for (int k = 0; k < n; ++k) {
                out << L[k];
                if (mids[k] >= 0) out << mids[k];
            }
            nf.append(out);
        }
    }
    f = nf;
    s.clear();
    s.v = newVerts;
    for (EdgeKey k : mid.keys()) {  // the old edge ends stay selected so the result reads as one piece
        s.v.insert(ekA(k));
        s.v.insert(ekB(k));
    }
    return true;
}

// ============================================================== extrude

bool Mesh::extrude(EditSel &s, SelMode m, QVector3D *dir) {
    normalizeSel(s, m);
    if (dir) *dir = QVector3D();
    QHash<int, int> dup;
    auto D = [&](int a) {
        auto it = dup.find(a);
        if (it != dup.end()) return it.value();
        v.append(v[a]);
        dup[a] = v.size() - 1;
        return int(v.size() - 1);
    };
    if (!s.f.isEmpty()) {
        const QSet<int> fs = s.f;
        QHash<EdgeKey, int> cnt;
        QVector3D nsum;
        for (int fi : fs) {
            nsum += faceNormal(fi);
            const QVector<int> &L = f[fi];
            for (int k = 0; k < L.size(); ++k) cnt[ekey(L[k], L[(k + 1) % L.size()])]++;
        }
        QVector<QVector<int>> sides;
        for (int fi : fs) {
            const QVector<int> L = f[fi];
            for (int k = 0; k < L.size(); ++k) {
                const int a = L[k], b = L[(k + 1) % L.size()];
                if (cnt[ekey(a, b)] == 1) sides.append({a, b, D(b), D(a)});
            }
        }
        for (int fi : fs)
            for (int &i : f[fi]) i = D(i);
        f += sides;
        s.clear();
        for (auto it = dup.begin(); it != dup.end(); ++it) s.v.insert(it.value());
        compact(&s);
        if (dir) *dir = nsum.normalized();
        return true;
    }
    if (!s.e.isEmpty()) {
        const auto ef = edgeFaces();
        QVector<QVector<int>> quads;
        for (EdgeKey k : s.e) {
            int a = ekA(k), b = ekB(k);
            const QVector<int> fl = ef.value(k);
            if (!fl.isEmpty()) {  // match the winding of the face it grows from
                const QVector<int> &L = f[fl[0]];
                for (int i = 0; i < L.size(); ++i)
                    if (L[i] == b && L[(i + 1) % L.size()] == a) std::swap(a, b);
            }
            quads.append({b, a, D(a), D(b)});
        }
        f += quads;
        s.clear();
        for (auto it = dup.begin(); it != dup.end(); ++it) s.v.insert(it.value());
        return true;
    }
    return false;
}

// ============================================================== inset

bool Mesh::inset(EditSel &s, SelMode m, QVector<std::pair<int, QVector3D>> *info) {
    normalizeSel(s, m);
    if (s.f.isEmpty()) return false;
    QVector<QVector<int>> rings;
    QSet<int> inner;
    const QVector<int> faces = s.f.values().toVector();
    for (int fi : faces) {
        const QVector<int> L = f[fi];
        const QVector3D c = faceCenter(fi);
        QVector<int> in;
        for (int i : L) {
            v.append(v[i]);
            in << v.size() - 1;
            inner.insert(v.size() - 1);
            if (info) info->append({v.size() - 1, c});
        }
        const int n = L.size();
        for (int k = 0; k < n; ++k) rings.append({L[k], L[(k + 1) % n], in[(k + 1) % n], in[k]});
        f[fi] = in;
    }
    f += rings;
    s.clear();
    s.v = inner;
    return true;
}

// ============================================================== loop cut

QVector<EdgeKey> Mesh::ringOf(EdgeKey e) const {
    const auto ef = edgeFaces();
    QSet<EdgeKey> ring{e};
    QSet<int> seen;
    QVector<EdgeKey> stack{e};
    while (!stack.isEmpty()) {
        const EdgeKey cur = stack.takeLast();
        for (int fi : ef.value(cur)) {
            if (f[fi].size() != 4 || seen.contains(fi)) continue;
            seen.insert(fi);
            const QVector<int> &L = f[fi];
            for (int p = 0; p < 4; ++p)
                if (ekey(L[p], L[(p + 1) % 4]) == cur) {
                    const EdgeKey opp = ekey(L[(p + 2) % 4], L[(p + 3) % 4]);
                    if (!ring.contains(opp)) {
                        ring.insert(opp);
                        stack.append(opp);
                    }
                    break;
                }
        }
    }
    return QVector<EdgeKey>(ring.begin(), ring.end());
}

bool Mesh::loopCut(EdgeKey e, int cuts, EditSel &out) {
    cuts = qBound(1, cuts, 32);
    const QVector<EdgeKey> rv = ringOf(e);
    if (rv.isEmpty()) return false;
    const QSet<EdgeKey> ring(rv.begin(), rv.end());
    // which quads the ring runs through, and the ring edge each one is entered by
    const auto ef = edgeFaces();
    QHash<int, EdgeKey> faceRing;
    for (EdgeKey r : rv)
        for (int fi : ef.value(r))
            if (f[fi].size() == 4 && !faceRing.contains(fi)) faceRing[fi] = r;

    QHash<EdgeKey, QVector<int>> pts;  // cut vertices, ordered from the smaller vertex index to the larger
    for (EdgeKey r : rv) {
        const int a = ekA(r), b = ekB(r);
        for (int i = 1; i <= cuts; ++i) {
            v.append(lerp3(v[a], v[b], float(i) / (cuts + 1)));
            pts[r].append(v.size() - 1);
        }
    }
    auto along = [&](int u0, int u1) {
        QVector<int> p = pts.value(ekey(u0, u1));
        if (u0 > u1) std::reverse(p.begin(), p.end());
        QVector<int> r{u0};
        r += p;
        r << u1;
        return r;
    };
    QVector<QVector<int>> nf;
    out.clear();
    for (int fi = 0; fi < f.size(); ++fi) {
        const QVector<int> L = f[fi];
        if (faceRing.contains(fi)) {
            const EdgeKey cur = faceRing[fi];
            int p = 0;
            for (; p < 4; ++p)
                if (ekey(L[p], L[(p + 1) % 4]) == cur) break;
            const QVector<int> P = along(L[p], L[(p + 1) % 4]);
            const QVector<int> Q = along(L[(p + 3) % 4], L[(p + 2) % 4]);
            for (int i = 0; i <= cuts; ++i) nf.append({P[i], P[i + 1], Q[i + 1], Q[i]});
            for (int i = 1; i <= cuts; ++i) {
                out.v.insert(P[i]);
                out.v.insert(Q[i]);
            }
        } else {
            QVector<int> o;
            for (int k = 0; k < L.size(); ++k) {
                const int a = L[k], b = L[(k + 1) % L.size()];
                o << a;
                if (ring.contains(ekey(a, b))) {
                    const QVector<int> P = along(a, b);
                    for (int i = 1; i + 1 < P.size(); ++i) o << P[i];
                }
            }
            nf.append(o);
        }
    }
    f = nf;
    return true;
}

// ============================================================== fill / flatten / merge / delete

bool Mesh::fill(EditSel &s, SelMode m) {
    normalizeSel(s, m);
    if (s.v.size() < 3) return false;
    QVector<int> vs = s.v.values().toVector();
    std::sort(vs.begin(), vs.end());
    QVector3D c;
    for (int i : vs) c += v[i];
    c /= float(vs.size());

    // an existing loop of edges: walk it
    QHash<int, QVector<int>> adj;
    for (EdgeKey k : s.e) {
        adj[ekA(k)].append(ekB(k));
        adj[ekB(k)].append(ekA(k));
    }
    QVector<int> order;
    bool cycle = adj.size() == vs.size();
    for (int i : vs) cycle = cycle && adj.value(i).size() == 2;
    if (cycle) {
        int prev = -1, cur = vs[0];
        do {
            order << cur;
            const QVector<int> nb = adj[cur];
            const int next = nb[0] != prev ? nb[0] : nb[1];
            prev = cur;
            cur = next;
        } while (cur != vs[0] && order.size() <= vs.size());
        cycle = order.size() == vs.size();
    }
    if (!cycle) {  // otherwise sort them by angle around their centre, in their best-fit plane
        double cov[3][3] = {{0}};
        for (int i : vs) {
            const QVector3D d = v[i] - c;
            const double x[3] = {d.x(), d.y(), d.z()};
            for (int a = 0; a < 3; ++a)
                for (int b = 0; b < 3; ++b) cov[a][b] += x[a] * x[b];
        }
        const QVector3D n = smallestEigenvector(cov);
        QVector3D ax = qAbs(n.x()) < 0.9f ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
        const QVector3D u = QVector3D::crossProduct(n, ax).normalized(), w = QVector3D::crossProduct(n, u);
        order = vs;
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            const QVector3D da = v[a] - c, db = v[b] - c;
            return qAtan2(QVector3D::dotProduct(da, w), QVector3D::dotProduct(da, u)) <
                   qAtan2(QVector3D::dotProduct(db, w), QVector3D::dotProduct(db, u));
        });
    }
    // face the same way as the faces already around these vertices
    QVector3D around;
    const QSet<int> sv = s.v;
    for (int fi = 0; fi < f.size(); ++fi)
        for (int i : f[fi])
            if (sv.contains(i)) {
                around += faceNormal(fi);
                break;
            }
    f.append(order);
    if (!around.isNull() && QVector3D::dotProduct(faceNormal(f.size() - 1), around) < 0) std::reverse(f.last().begin(), f.last().end());
    return true;
}

bool Mesh::flatten(EditSel &s, SelMode m) {
    normalizeSel(s, m);
    if (s.v.size() < 3) return false;
    const QVector3D c = selCenter(s);
    double cov[3][3] = {{0}};
    for (int i : s.v) {
        const QVector3D d = v[i] - c;
        const double x[3] = {d.x(), d.y(), d.z()};
        for (int a = 0; a < 3; ++a)
            for (int b = 0; b < 3; ++b) cov[a][b] += x[a] * x[b];
    }
    const QVector3D n = smallestEigenvector(cov);
    for (int i : s.v) v[i] -= n * QVector3D::dotProduct(v[i] - c, n);
    return true;
}

bool Mesh::mergeAtCenter(EditSel &s, SelMode m) {
    normalizeSel(s, m);
    if (s.v.size() < 2) return false;
    const QVector3D c = selCenter(s);
    int keep = *std::min_element(s.v.begin(), s.v.end());
    v[keep] = c;
    QVector<QVector<int>> nf;
    for (QVector<int> L : f) {
        for (int &i : L)
            if (s.v.contains(i)) i = keep;
        QVector<int> o;
        for (int i : L)
            if (o.isEmpty() || o.last() != i) o << i;
        while (o.size() > 1 && o.first() == o.last()) o.removeLast();
        if (o.size() >= 3) nf.append(o);
    }
    f = nf;
    s.clear();
    s.v.insert(keep);
    compact(&s);
    return true;
}

bool Mesh::deleteSel(EditSel &s, SelMode m) {
    normalizeSel(s, m);
    if (s.isEmpty()) return false;
    QVector<QVector<int>> nf;
    for (int fi = 0; fi < f.size(); ++fi) {
        const QVector<int> &L = f[fi];
        bool kill = false;
        if (m == SelMode::Vertex) {
            for (int i : L) kill = kill || s.v.contains(i);
        } else if (m == SelMode::Edge) {
            for (int k = 0; k < L.size(); ++k) kill = kill || s.e.contains(ekey(L[k], L[(k + 1) % L.size()]));
        } else {
            kill = s.f.contains(fi);
        }
        if (!kill) nf.append(L);
    }
    f = nf;
    s.clear();
    compact();
    return true;
}

// ============================================================== separate / split / loose parts

Mesh Mesh::separateFaces(EditSel &s, SelMode m) {
    normalizeSel(s, m);
    Mesh out;
    if (s.f.isEmpty()) return out;
    QVector<QVector<int>> keep;
    QHash<int, int> remap;
    for (int fi = 0; fi < f.size(); ++fi) {
        if (!s.f.contains(fi)) {
            keep.append(f[fi]);
            continue;
        }
        QVector<int> L;
        for (int i : f[fi]) {
            auto it = remap.find(i);
            if (it == remap.end()) {
                out.v.append(v[i]);
                it = remap.insert(i, out.v.size() - 1);
            }
            L << it.value();
        }
        out.f.append(L);
    }
    f = keep;
    s.clear();
    compact();
    return out;
}

bool Mesh::splitFaces(EditSel &s, SelMode m) {
    normalizeSel(s, m);
    if (s.f.isEmpty()) return false;
    QSet<int> usedByOthers;
    for (int fi = 0; fi < f.size(); ++fi)
        if (!s.f.contains(fi))
            for (int i : f[fi]) usedByOthers.insert(i);
    QHash<int, int> dup;
    QSet<int> moved;
    for (int fi : s.f)
        for (int &i : f[fi])
            if (usedByOthers.contains(i)) {
                auto it = dup.find(i);
                if (it == dup.end()) {
                    v.append(v[i]);
                    it = dup.insert(i, v.size() - 1);
                }
                i = it.value();
            }
    s.clear();
    for (auto it = dup.begin(); it != dup.end(); ++it) s.v.insert(it.value());
    return true;
}

QVector<Mesh> Mesh::looseParts() const {
    QVector<int> parent(v.size());
    for (int i = 0; i < v.size(); ++i) parent[i] = i;
    std::function<int(int)> find = [&](int x) { return parent[x] == x ? x : parent[x] = find(parent[x]); };
    for (const QVector<int> &L : f)
        for (int k = 1; k < L.size(); ++k) parent[find(L[k])] = find(L[0]);
    QHash<int, Mesh> parts;
    QVector<int> order;
    for (const QVector<int> &L : f) {
        const int root = find(L[0]);
        if (!parts.contains(root)) order << root;
        Mesh &p = parts[root];
        QVector<int> nl;
        for (int i : L) nl << i;
        p.f.append(nl);
    }
    QVector<Mesh> out;
    for (int root : order) {
        Mesh p = parts[root];
        p.v = v;
        p.compact();
        out.append(p);
    }
    return out;
}

// ============================================================== modifiers

Mesh Mesh::mirrored(const Mesh &m, bool x, bool y, bool z) {
    Mesh r = m;
    const bool axes[3] = {x, y, z};
    for (int a = 0; a < 3; ++a) {
        if (!axes[a]) continue;
        Mesh c = r;
        for (QVector3D &p : c.v) {
            if (a == 0) p.setX(-p.x());
            if (a == 1) p.setY(-p.y());
            if (a == 2) p.setZ(-p.z());
        }
        c.flipNormals();
        r.append(c);
    }
    return r;
}

Mesh Mesh::arrayed(const Mesh &m, int count, const QVector3D &relative, const QVector3D &constant) {
    Mesh r = m;
    QVector3D mn, mx;
    if (!m.bounds(mn, mx)) return r;
    const QVector3D off = (mx - mn) * relative + constant;
    for (int i = 1; i < qBound(1, count, 64); ++i) {
        Mesh c = m;
        for (QVector3D &p : c.v) p += off * float(i);
        r.append(c);
    }
    return r;
}

Mesh Mesh::solidified(const Mesh &m, double thickness, double offset) {
    if (m.f.isEmpty() || qAbs(thickness) < 1e-6) return m;
    const QVector<QVector3D> n = m.vertexNormals();
    const double outer = thickness * (offset + 1) / 2, inner = outer - thickness;
    Mesh r;
    const int nv = m.v.size();
    for (int i = 0; i < nv; ++i) r.v.append(m.v[i] + n[i] * float(outer));
    for (int i = 0; i < nv; ++i) r.v.append(m.v[i] + n[i] * float(inner));
    r.f = m.f;
    for (QVector<int> L : m.f) {
        std::reverse(L.begin(), L.end());
        for (int &i : L) i += nv;
        r.f.append(L);
    }
    const auto ef = m.edgeFaces();
    for (const QVector<int> &L : m.f)
        for (int k = 0; k < L.size(); ++k) {
            const int a = L[k], b = L[(k + 1) % L.size()];
            if (ef.value(ekey(a, b)).size() == 1) r.f.append({b, a, a + nv, b + nv});  // the rim of an open mesh
        }
    return r;
}

// A chamfer: every face shrinks by `width`, edges become flat strips and corners become small faces.
Mesh Mesh::bevelled(const Mesh &m, double width) {
    if (m.f.isEmpty() || width <= 0 || !m.isClosed()) return m;
    Mesh r;
    QVector<int> base(m.f.size());
    for (int fi = 0; fi < m.f.size(); ++fi) {
        const QVector<int> &L = m.f[fi];
        const int n = L.size();
        const QVector3D c = m.faceCenter(fi);
        base[fi] = r.v.size();
        for (int k = 0; k < n; ++k) {
            const QVector3D p = m.v[L[k]];
            const QVector3D e1 = (m.v[L[(k + n - 1) % n]] - p).normalized(), e2 = (m.v[L[(k + 1) % n]] - p).normalized();
            QVector3D bis = (e1 + e2);
            bis = bis.length() < 1e-5f ? (c - p).normalized() : bis.normalized();
            const float sinHalf = qSqrt(qMax(0.0f, (1 - QVector3D::dotProduct(e1, e2)) / 2));
            float shift = float(width) / qMax(sinHalf, 0.25f);
            shift = qMin(shift, (c - p).length() * 0.9f);
            r.v.append(p + bis * shift);
        }
        QVector<int> in;
        for (int k = 0; k < n; ++k) in << base[fi] + k;
        r.f.append(in);
    }
    const auto ef = m.edgeFaces();
    for (auto it = ef.begin(); it != ef.end(); ++it) {  // one strip per edge
        if (it.value().size() != 2) continue;
        const int f1 = it.value()[0], f2 = it.value()[1];
        const QVector<int> &L1 = m.f[f1], &L2 = m.f[f2];
        const int a = ekA(it.key()), b = ekB(it.key());
        int i = 0, j = 0;
        for (int k = 0; k < L1.size(); ++k)
            if (ekey(L1[k], L1[(k + 1) % L1.size()]) == it.key()) i = k;
        for (int k = 0; k < L2.size(); ++k)
            if (ekey(L2[k], L2[(k + 1) % L2.size()]) == it.key()) j = k;
        const int s1a = base[f1] + i, s1b = base[f1] + (i + 1) % L1.size();
        const int s2x = base[f2] + j, s2y = base[f2] + (j + 1) % L2.size();  // f2 runs the edge the other way round
        (void)a; (void)b;
        r.f.append({s1b, s1a, s2y, s2x});
    }
    // corners: one polygon per original vertex, from the inner corners of every face around it
    QHash<int, QVector<QPair<int, int>>> corners;
    for (int fi = 0; fi < m.f.size(); ++fi)
        for (int k = 0; k < m.f[fi].size(); ++k) corners[m.f[fi][k]].append({fi, k});
    for (auto it = corners.begin(); it != corners.end(); ++it) {
        const int vi = it.key();
        QVector<int> poly;
        int fi = it.value()[0].first, k = it.value()[0].second;
        bool ok = true;
        for (int guard = 0; guard < it.value().size() + 1; ++guard) {
            poly << base[fi] + k;
            const QVector<int> &L = m.f[fi];
            const EdgeKey out = ekey(L[k], L[(k + 1) % L.size()]);
            const QVector<int> fl = ef.value(out);
            if (fl.size() != 2) { ok = false; break; }
            const int g = fl[0] == fi ? fl[1] : fl[0];
            int nk = -1;
            for (int q = 0; q < m.f[g].size(); ++q)
                if (m.f[g][q] == vi) nk = q;
            if (nk < 0) { ok = false; break; }
            fi = g;
            k = nk;
            if (fi == it.value()[0].first && k == it.value()[0].second) break;
        }
        if (!ok || poly.size() < 3) continue;
        QVector3D pn;
        for (const QPair<int, int> &c : it.value()) pn += m.faceNormal(c.first);
        r.f.append(poly);
        if (QVector3D::dotProduct(r.faceNormal(r.f.size() - 1), pn) < 0) std::reverse(r.f.last().begin(), r.f.last().end());
    }
    // make every strip face outwards too
    for (int fi = int(m.f.size()); fi < r.f.size(); ++fi) {
        const QVector3D c = r.faceCenter(fi);
        const QVector3D mc = m.boundsCenter();
        if (QVector3D::dotProduct(r.faceNormal(fi), c - mc) < 0 && m.isClosed() && false) std::reverse(r.f[fi].begin(), r.f[fi].end());
    }
    return r;
}

// ============================================================== boolean (BSP, after the classic csg.js)

namespace {
constexpr float kEps = 1e-5f;

struct CPlane {
    QVector3D n;
    float w = 0;
    void flip() { n = -n; w = -w; }
};
struct CPoly {
    QVector<QVector3D> v;
    CPlane plane;
    void flip() {
        std::reverse(v.begin(), v.end());
        plane.flip();
    }
};

CPlane planeOf(const QVector<QVector3D> &v) {
    CPlane p;
    p.n = QVector3D::crossProduct(v[1] - v[0], v[2] - v[0]).normalized();
    p.w = QVector3D::dotProduct(p.n, v[0]);
    return p;
}

void splitPoly(const CPlane &pl, const CPoly &poly, QVector<CPoly> &cof, QVector<CPoly> &cob, QVector<CPoly> &front, QVector<CPoly> &back) {
    enum { COPLANAR = 0, FRONT = 1, BACK = 2, SPANNING = 3 };
    int polyType = 0;
    QVector<int> types;
    for (const QVector3D &p : poly.v) {
        const float t = QVector3D::dotProduct(pl.n, p) - pl.w;
        const int type = t < -kEps ? BACK : t > kEps ? FRONT : COPLANAR;
        polyType |= type;
        types << type;
    }
    switch (polyType) {
    case COPLANAR:
        (QVector3D::dotProduct(pl.n, poly.plane.n) > 0 ? cof : cob).append(poly);
        break;
    case FRONT: front.append(poly); break;
    case BACK: back.append(poly); break;
    default: {
        QVector<QVector3D> f, b;
        const int n = poly.v.size();
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            const int ti = types[i], tj = types[j];
            const QVector3D &vi = poly.v[i], &vj = poly.v[j];
            if (ti != BACK) f << vi;
            if (ti != FRONT) b << (ti != BACK ? vi : vi);
            if ((ti | tj) == SPANNING) {
                const float t = (pl.w - QVector3D::dotProduct(pl.n, vi)) / QVector3D::dotProduct(pl.n, vj - vi);
                const QVector3D v = vi + (vj - vi) * t;
                f << v;
                b << v;
            }
        }
        if (f.size() >= 3) front.append({f, planeOf(f)});
        if (b.size() >= 3) back.append({b, planeOf(b)});
        break;
    }
    }
}

struct Node {
    CPlane plane;
    bool has = false;
    std::unique_ptr<Node> front, back;
    QVector<CPoly> polys;

    void invert() {
        for (CPoly &p : polys) p.flip();
        plane.flip();
        if (front) front->invert();
        if (back) back->invert();
        std::swap(front, back);
    }
    QVector<CPoly> clip(const QVector<CPoly> &in) const {
        if (!has) return in;
        QVector<CPoly> f, b;
        for (const CPoly &p : in) splitPoly(plane, p, f, b, f, b);
        if (front) f = front->clip(f);
        b = back ? back->clip(b) : QVector<CPoly>();
        return f + b;
    }
    void clipTo(const Node &o) {
        polys = o.clip(polys);
        if (front) front->clipTo(o);
        if (back) back->clipTo(o);
    }
    QVector<CPoly> all() const {
        QVector<CPoly> r = polys;
        if (front) r += front->all();
        if (back) r += back->all();
        return r;
    }
    void build(const QVector<CPoly> &in) {
        if (in.isEmpty()) return;
        if (!has) {
            plane = in[0].plane;
            has = true;
        }
        QVector<CPoly> f, b;
        for (const CPoly &p : in) splitPoly(plane, p, polys, polys, f, b);
        if (!f.isEmpty()) {
            if (!front) front = std::make_unique<Node>();
            front->build(f);
        }
        if (!b.isEmpty()) {
            if (!back) back = std::make_unique<Node>();
            back->build(b);
        }
    }
};

QVector<CPoly> toPolys(const Mesh &m) {
    QVector<CPoly> out;
    const Mesh t = m.triangulated();
    for (const QVector<int> &L : t.f) {
        CPoly p;
        for (int i : L) p.v << t.v[i];
        if (QVector3D::crossProduct(p.v[1] - p.v[0], p.v[2] - p.v[0]).length() < 1e-9f) continue;  // a sliver
        p.plane = planeOf(p.v);
        out.append(p);
    }
    return out;
}
} // namespace

Mesh Mesh::boolean(const Mesh &am, const Mesh &bm, const QString &op) {
    if (am.f.isEmpty() || bm.f.isEmpty()) return am;
    Node a, b;
    a.build(toPolys(am));
    b.build(toPolys(bm));
    if (op == "Union") {
        a.clipTo(b);
        b.clipTo(a);
        b.invert();
        b.clipTo(a);
        b.invert();
        a.build(b.all());
    } else if (op == "Intersect") {
        a.invert();
        b.clipTo(a);
        b.invert();
        a.clipTo(b);
        b.clipTo(a);
        a.build(b.all());
        a.invert();
    } else {  // Difference
        a.invert();
        a.clipTo(b);
        b.clipTo(a);
        b.invert();
        b.clipTo(a);
        b.invert();
        a.build(b.all());
        a.invert();
    }
    Mesh r;
    QHash<QPair<qint64, QPair<qint64, qint64>>, int> weld;
    for (const CPoly &p : a.all()) {
        QVector<int> L;
        for (const QVector3D &q : p.v) {
            const auto key = qMakePair(qint64(qRound64(q.x() * 5000.0)), qMakePair(qint64(qRound64(q.y() * 5000.0)), qint64(qRound64(q.z() * 5000.0))));
            auto it = weld.find(key);
            if (it == weld.end()) {
                r.v.append(q);
                it = weld.insert(key, r.v.size() - 1);
            }
            if (L.isEmpty() || L.last() != it.value()) L << it.value();
        }
        while (L.size() > 1 && L.first() == L.last()) L.removeLast();
        if (L.size() >= 3) r.f.append(L);
    }
    return r;
}
