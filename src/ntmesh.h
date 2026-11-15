// Lightweight templated mesh library
// 2026
// Ivan DeWolf
//
// Topology lives in this header, and the mesh is cuda-ready.
// ntmesh_attribs.h adds numeric point, vertex, prim, and mesh attributes.
// ntmesh_strattr.h adds host-only string attributes.
// Include this header alone for a mesh of points, triangles, quads, n-gons, and lines.


#pragma once
#include "nvec3.h"
#include "nmat4.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef NTL_HD
#  define NTL_HD
#endif

namespace ntm {

// A buffer of POD values. Copies share the pointer. clear() frees it once.
template <typename T>
struct Buf {
    T*  data = nullptr;
    int count = 0;
    int capacity = 0;

    NTL_HD T& operator[](int i) { return data[i]; }
    NTL_HD const T& operator[](int i) const { return data[i]; }
    NTL_HD int size() const { return count; }
};

// reserve space for a buffer by a given capacity
template <typename T>
void bufReserve(Buf<T>& b, int cap) {
    if (cap <= b.capacity) return;
    b.data = static_cast<T*>(std::realloc(b.data, sizeof(T) * (size_t)cap));
    b.capacity = cap;
}

// fit a buffer to a given number of elements
template <typename T>
void bufFit(Buf<T>& b, int n) {
    if (n < 0) n = 0;
    if (n > b.capacity) bufReserve(b, n);
    for (int i = b.count; i < n; ++i) b.data[i] = T();
    b.count = n;
}

// free a buffer
template <typename T>
void bufFree(Buf<T>& b) {
    std::free(b.data);
    b.data = nullptr;
    b.count = 0;
    b.capacity = 0;
}

// clone a buffer
template <typename T>
void bufClone(Buf<T>& dst, const Buf<T>& src) {
    bufFree(dst);
    if (src.count <= 0 || !src.data) return;
    bufFit(dst, src.count);
    std::memcpy(dst.data, src.data, sizeof(T) * (size_t)src.count);
}

// No attribute storage. Topology methods still call these hooks.
struct None {
    void attrReserve(int, int, int) {}
    void attrFitPoints(int) {}
    void attrFitVerts(int) {}
    void attrFitPrims(int) {}
    void attrFitDetail() {}
    void attrInsertVerts(int, int) {}
    void attrInsertPrims(int, int) {}
    void attrEraseVerts(int, int) {}
    void attrErasePrims(int, int) {}
    void attrCopyPoint(int, int) {}
    void attrCopyVert(int, int) {}
    void attrCopyPrim(int, int) {}
    void attrSwapVert(int, int) {}
    void attrMergePoints(const None&, int, int) {}
    void attrMergeVerts(const None&, int, int, int, int) {}
    void attrMergePrims(const None&, int, int, int, int) {}
    void attrMergeDetail(const None&) {}
    void attrClear() {}
    void attrClone(const None&) {}
    void attrPrint() const {}
    template <typename U>
    void attrTransform(const ntv::nmat4<U>&) {}
};

// -------------------------------------------------------------------------------------------------
// The ntmesh class - this is the primary reason for this library.
// -------------------------------------------------------------------------------------------------
// Points, corner indices, n-gon faces, triangle indices, quad indices.
// A triangle or quad can live in its own packed list, or in the n-gon list.
// Packed lists have no offsets: triangle f is indices [f*3, f*3+3), quad f is [f*4, f*4+4).
// N-gon f uses corners [ngonOffsets[f], ngonOffsets[f + 1]).
// Line f is an open polyline using corners [lineOffsets[f], lineOffsets[f + 1]).
// Attr is None, or a policy from ntmesh_attribs.h / ntmesh_strattr.h.
// Vertex and prim attributes, when present, are ordered triangles, quads, n-gons, then lines.
template <typename T, typename Attr = None>
struct ntmesh : Attr {
    Buf<ntv::nvec3<T>> points;
    Buf<int>           ngonIndices;
    Buf<int>           ngonOffsets;
    Buf<int>           lineIndices;
    Buf<int>           lineOffsets;
    Buf<int>           triangleIndices;
    Buf<int>           quadIndices;

    NTL_HD int pointCount() const { return points.count; }
    NTL_HD int triangleCount() const { return triangleIndices.count / 3; }
    NTL_HD int quadCount() const { return quadIndices.count / 4; }
    NTL_HD int ngonCount() const { return ngonOffsets.count == 0 ? 0 : ngonOffsets.count - 1; }
    NTL_HD int lineCount() const { return lineOffsets.count == 0 ? 0 : lineOffsets.count - 1; }
    NTL_HD int primCount() const { return triangleCount() + quadCount() + ngonCount() + lineCount(); }
    NTL_HD int vertexCount() const {
        return triangleIndices.count + quadIndices.count + ngonIndices.count + lineIndices.count;
    }
    NTL_HD int ngonVertexCount(int f) const { return ngonOffsets[f + 1] - ngonOffsets[f]; }
    NTL_HD int ngonVertex(int f, int k) const { return ngonIndices[ngonOffsets[f] + k]; }
    NTL_HD int lineVertexCount(int f) const { return lineOffsets[f + 1] - lineOffsets[f]; }
    NTL_HD int lineVertex(int f, int k) const { return lineIndices[lineOffsets[f] + k]; }
    NTL_HD int triangleVertex(int f, int k) const { return triangleIndices[(f * 3) + k]; }
    NTL_HD int quadVertex(int f, int k) const { return quadIndices[(f * 4) + k]; }
    NTL_HD int primVertexCount(int f) const {
        int nt = triangleCount();
        if (f < nt) return 3;
        f -= nt;
        int nq = quadCount();
        if (f < nq) return 4;
        f -= nq;
        int nn = ngonCount();
        if (f < nn) return ngonVertexCount(f);
        f -= nn;
        return lineVertexCount(f);
    }
    NTL_HD int primVertex(int f, int k) const {
        int nt = triangleCount();
        if (f < nt) return triangleVertex(f, k);
        f -= nt;
        int nq = quadCount();
        if (f < nq) return quadVertex(f, k);
        f -= nq;
        int nn = ngonCount();
        if (f < nn) return ngonVertex(f, k);
        f -= nn;
        return lineVertex(f, k);
    }
    // Axis-aligned box of the point buffer. With no points, mn and mx stay as they were.
    NTL_HD void bounds(ntv::nvec3<T>& mn, ntv::nvec3<T>& mx) const;

    void takeTopology(ntmesh& src) {
        points = src.points; src.points = {};
        triangleIndices = src.triangleIndices; src.triangleIndices = {};
        quadIndices = src.quadIndices; src.quadIndices = {};
        ngonIndices = src.ngonIndices; src.ngonIndices = {};
        ngonOffsets = src.ngonOffsets; src.ngonOffsets = {};
        lineIndices = src.lineIndices; src.lineIndices = {};
        lineOffsets = src.lineOffsets; src.lineOffsets = {};
    }

    void reserve(int npoints, int nverts, int nprims);
    void trim(int npoints, int ntris, int nquads, int nngons, int nlines);
    void merge(const ntmesh& src);
    void clear();
    void print() const;
    int  addPoint(ntv::nvec3<T> p);
    // A 2-corner prim goes in the line lists, a 3-corner prim goes in
    // triangleIndices, and a 4-corner prim in quadIndices. 
    // More than 4 corners go in the n-gon list, 
    // unless separate is true, in which case it goes in the n-gon list.
    void addFace(const int* cornerPoints, int n, bool separate = true);
    void addLine(const int* points, int n);
    // Quads and n-gons become triangles. Lines stay. Each new triangle keeps the
    // source winding, and vertex and prim attributes are copied onto it.
    void triangulate();
    // Flip the corner order of every closed face and every line. Vertex attributes move with their corners.
    void reverse();
    // Drop points that no corner references and remap the index lists. Point attributes move with the survivors.
    void removeUnusedPoints();
    // Sum of the face areas. Lines add nothing. Each face is the fan from its first corner.
    NTL_HD T totalSurfaceArea() const; // nice-to-have
    // Signed volume of the face fans. Positive when faces wind outward. Meaningful when isClosed() is true.
    NTL_HD T totalVolume() const; // nice-to-have
    // Every face edge has one opposite partner. Lines are ignored. No faces means not closed.
    int isClosed() const; // nice-to-have
    // Raw Newell vector of a corner span. Its length is twice the planar area. Fewer than three corners returns zero.
    NTL_HD ntv::nvec3<T> newell(const int* corners, int n) const;
    // Unit Newell normal of prim f. A line, a degenerate face, and an out-of-range prim return zero.
    NTL_HD ntv::nvec3<T> faceNormal(int f) const;
    // The point buffer and Position attributes take the full matrix. Normals take the
    // inverse transpose of the linear 3x3 and are renormalized. Color, float,
    // and int columns are left alone.
    void transform(const ntv::nmat4<T>& m);
};

// reserve space for a mesh
template <typename T, typename Attr>
void ntmesh<T, Attr>::reserve(int npoints, int nverts, int nprims) {
    bufReserve(points, npoints);
    bufReserve(triangleIndices, nverts);
    bufReserve(quadIndices, nverts);
    bufReserve(ngonIndices, nverts);
    bufReserve(ngonOffsets, nprims > 0 ? nprims + 1 : 0);
    bufReserve(lineIndices, nverts);
    bufReserve(lineOffsets, nprims > 0 ? nprims + 1 : 0);
    this->attrReserve(npoints, nverts, nprims);
}

// trim a mesh to a given number of points, vertices, and prims
template <typename T, typename Attr>
void ntmesh<T, Attr>::trim(int npoints, int ntris, int nquads, int nngons, int nlines) {
    if (npoints < 0) npoints = 0;
    if (ntris < 0) ntris = 0;
    if (nquads < 0) nquads = 0;
    if (nngons < 0) nngons = 0;
    if (nlines < 0) nlines = 0;

    int oldTri = triangleCount();
    int oldQuad = quadCount();
    int oldNgon = ngonCount();
    int oldLine = lineCount();
    int oldTriV = triangleIndices.count;
    int oldQuadV = quadIndices.count;
    int oldNgonV = ngonIndices.count;
    int oldLineV = lineIndices.count;
    int keepNgonV = nngons <= 0 ? 0 : (ngonOffsets.count > nngons ? ngonOffsets[nngons] : oldNgonV);
    int keepLineV = nlines <= 0 ? 0 : (lineOffsets.count > nlines ? lineOffsets[nlines] : oldLineV);

    if (oldLineV > keepLineV)
        this->attrEraseVerts(oldTriV + oldQuadV + oldNgonV + keepLineV, oldLineV - keepLineV);
    if (oldNgonV > keepNgonV)
        this->attrEraseVerts(oldTriV + oldQuadV + keepNgonV, oldNgonV - keepNgonV);
    if (oldQuad > nquads)
        this->attrEraseVerts(oldTriV + nquads * 4, (oldQuad - nquads) * 4);
    if (oldTri > ntris)
        this->attrEraseVerts(ntris * 3, (oldTri - ntris) * 3);

    if (oldLine > nlines)
        this->attrErasePrims(oldTri + oldQuad + oldNgon + nlines, oldLine - nlines);
    if (oldNgon > nngons)
        this->attrErasePrims(oldTri + oldQuad + nngons, oldNgon - nngons);
    if (oldQuad > nquads)
        this->attrErasePrims(oldTri + nquads, oldQuad - nquads);
    if (oldTri > ntris)
        this->attrErasePrims(ntris, oldTri - ntris);

    bufFit(points, npoints);
    bufFit(triangleIndices, ntris * 3);
    bufFit(quadIndices, nquads * 4);
    if (nngons == 0) {
        bufFit(ngonOffsets, 0);
        bufFit(ngonIndices, 0);
    } else {
        bufFit(ngonOffsets, nngons + 1);
        if (nngons > oldNgon)
            for (int i = oldNgon; i <= nngons; ++i) ngonOffsets[i] = keepNgonV;
        bufFit(ngonIndices, keepNgonV);
    }
    if (nlines == 0) {
        bufFit(lineOffsets, 0);
        bufFit(lineIndices, 0);
    } else {
        bufFit(lineOffsets, nlines + 1);
        if (nlines > oldLine)
            for (int i = oldLine; i <= nlines; ++i) lineOffsets[i] = keepLineV;
        bufFit(lineIndices, keepLineV);
    }
    this->attrFitPoints(npoints);
    this->attrFitVerts(vertexCount());
    this->attrFitPrims(primCount());
    this->attrFitDetail();
}

// merge another mesh into this one
template <typename T, typename Attr>
void ntmesh<T, Attr>::merge(const ntmesh<T, Attr>& src) {
    int p0 = points.count;
    int triV = triangleIndices.count;
    int quadV = quadIndices.count;
    int ngonV = ngonIndices.count;
    int lineV = lineIndices.count;
    int triF = triangleCount();
    int quadF = quadCount();
    int ngonF = ngonCount();
    int lineF = lineCount();
    int p1 = p0 + src.points.count;

    bufFit(points, p1);
    if (src.points.count && src.points.data)
        std::memcpy(points.data + p0, src.points.data, sizeof(ntv::nvec3<T>) * (size_t)src.points.count);
    this->attrMergePoints(src, p0, p1);

    int vCount = triV + quadV + ngonV + lineV;
    int fCount = triF + quadF + ngonF + lineF;
    this->attrMergeVerts(src, vCount, triV, 0, src.triangleIndices.count);
    vCount += src.triangleIndices.count;
    this->attrMergeVerts(src, vCount, triV + src.triangleIndices.count + quadV,
                         src.triangleIndices.count, src.quadIndices.count);
    vCount += src.quadIndices.count;
    int ngonVAt = triV + src.triangleIndices.count + quadV + src.quadIndices.count + ngonV;
    this->attrMergeVerts(src, vCount, ngonVAt,
                         src.triangleIndices.count + src.quadIndices.count, src.ngonIndices.count);
    vCount += src.ngonIndices.count;
    this->attrMergeVerts(src, vCount, vCount,
                         src.triangleIndices.count + src.quadIndices.count + src.ngonIndices.count,
                         src.lineIndices.count);

    this->attrMergePrims(src, fCount, triF, 0, src.triangleCount());
    fCount += src.triangleCount();
    this->attrMergePrims(src, fCount, triF + src.triangleCount() + quadF,
                         src.triangleCount(), src.quadCount());
    fCount += src.quadCount();
    int ngonFAt = triF + src.triangleCount() + quadF + src.quadCount() + ngonF;
    this->attrMergePrims(src, fCount, ngonFAt,
                         src.triangleCount() + src.quadCount(), src.ngonCount());
    fCount += src.ngonCount();
    this->attrMergePrims(src, fCount, fCount,
                         src.triangleCount() + src.quadCount() + src.ngonCount(), src.lineCount());
    this->attrMergeDetail(src);

    int st = src.triangleIndices.count;
    bufFit(triangleIndices, triV + st);
    for (int i = 0; i < st; ++i) triangleIndices[triV + i] = src.triangleIndices[i] + p0;
    int sq = src.quadIndices.count;
    bufFit(quadIndices, quadV + sq);
    for (int i = 0; i < sq; ++i) quadIndices[quadV + i] = src.quadIndices[i] + p0;

    int sn = src.ngonIndices.count;
    bufFit(ngonIndices, ngonV + sn);
    for (int i = 0; i < sn; ++i) ngonIndices[ngonV + i] = src.ngonIndices[i] + p0;
    int g1 = ngonF + src.ngonCount();
    if (g1 == 0) {
        ngonOffsets.count = 0;
    } else {
        bufFit(ngonOffsets, g1 + 1);
        if (ngonF == 0) ngonOffsets[0] = 0;
        for (int j = 0; j < src.ngonCount(); ++j)
            ngonOffsets[ngonF + 1 + j] = ngonV + src.ngonOffsets[j + 1] - src.ngonOffsets[0];
    }

    int sl = src.lineIndices.count;
    bufFit(lineIndices, lineV + sl);
    for (int i = 0; i < sl; ++i) lineIndices[lineV + i] = src.lineIndices[i] + p0;
    int l1 = lineF + src.lineCount();
    if (l1 == 0) {
        lineOffsets.count = 0;
    } else {
        bufFit(lineOffsets, l1 + 1);
        if (lineF == 0) lineOffsets[0] = 0;
        for (int j = 0; j < src.lineCount(); ++j)
            lineOffsets[lineF + 1 + j] = lineV + src.lineOffsets[j + 1] - src.lineOffsets[0];
    }
}

// clear a mesh
template <typename T, typename Attr>
void ntmesh<T, Attr>::clear() {
    bufFree(points);
    bufFree(triangleIndices);
    bufFree(quadIndices);
    bufFree(ngonIndices);
    bufFree(ngonOffsets);
    bufFree(lineIndices);
    bufFree(lineOffsets);
    this->attrClear();
}

// add a point to a mesh
template <typename T, typename Attr>
int ntmesh<T, Attr>::addPoint(ntv::nvec3<T> p) {
    int i = points.count;
    bufFit(points, i + 1);
    points[i] = p;
    this->attrFitPoints(points.count);
    return i;
}

// add a prim to a mesh
template <typename T, typename Attr>
void ntmesh<T, Attr>::addFace(const int* cornerPoints, int n, bool separate) {
    if (n < 0) n = 0;
    if (n == 2) {
        addLine(cornerPoints, n);
        return;
    }
    if (separate && n == 3) {
        this->attrInsertVerts(triangleIndices.count, 3);
        this->attrInsertPrims(triangleCount(), 1);
        int base = triangleIndices.count;
        bufFit(triangleIndices, base + 3);
        for (int i = 0; i < 3; ++i) triangleIndices[base + i] = cornerPoints[i];
        return;
    }
    if (separate && n == 4) {
        this->attrInsertVerts(triangleIndices.count + quadIndices.count, 4);
        this->attrInsertPrims(triangleCount() + quadCount(), 1);
        int base = quadIndices.count;
        bufFit(quadIndices, base + 4);
        for (int i = 0; i < 4; ++i) quadIndices[base + i] = cornerPoints[i];
        return;
    }
    this->attrInsertVerts(triangleIndices.count + quadIndices.count + ngonIndices.count, n);
    this->attrInsertPrims(triangleCount() + quadCount() + ngonCount(), 1);
    int v0 = ngonIndices.count;
    bufFit(ngonIndices, v0 + n);
    for (int i = 0; i < n; ++i) ngonIndices[v0 + i] = cornerPoints[i];
    int f0 = ngonCount();
    if (f0 == 0) {
        bufFit(ngonOffsets, 2);
        ngonOffsets[0] = 0;
        ngonOffsets[1] = n;
    } else {
        bufFit(ngonOffsets, f0 + 2);
        ngonOffsets[f0 + 1] = ngonOffsets[f0] + n;
    }
}

// Axis-aligned box of the points. An empty buffer leaves mn and mx unchanged.
template <typename T, typename Attr>
NTL_HD void ntmesh<T, Attr>::bounds(ntv::nvec3<T>& mn, ntv::nvec3<T>& mx) const {
    if (points.count <= 0 || !points.data) return;
    mn = points[0];
    mx = points[0];
    for (int i = 1; i < points.count; ++i) {
        for (int c = 0; c < 3; ++c) {
            if (points[i][c] < mn[c]) mn[c] = points[i][c];
            if (points[i][c] > mx[c]) mx[c] = points[i][c];
        }
    }
}

// transform positions in place, then let the attribute policy update its columns
template <typename T, typename Attr>
void ntmesh<T, Attr>::transform(const ntv::nmat4<T>& m) {
    for (int i = 0; i < points.count; ++i) points[i] = m * points[i];
    this->attrTransform(m);
}

// add an open polyline. Corners are appended after polygon corners.
template <typename T, typename Attr>
void ntmesh<T, Attr>::addLine(const int* points, int n) {
    if (n < 0) n = 0;
    this->attrInsertVerts(vertexCount(), n);
    this->attrInsertPrims(primCount(), 1);
    int v0 = lineIndices.count;
    bufFit(lineIndices, v0 + n);
    for (int i = 0; i < n; ++i) lineIndices[v0 + i] = points[i];
    int f0 = lineCount();
    if (f0 == 0) {
        bufFit(lineOffsets, 2);
        lineOffsets[0] = 0;
        lineOffsets[1] = n;
    } else {
        bufFit(lineOffsets, f0 + 2);
        lineOffsets[f0 + 1] = lineOffsets[f0] + n;
    }
}

template <typename T, typename Attr>
NTL_HD ntv::nvec3<T> ntmesh<T, Attr>::newell(const int* corners, int n) const {
    ntv::nvec3<T> sum(T(0), T(0), T(0));
    if (!points.data || !corners || n < 3 || points.count <= 0) return sum;
    for (int i = 0; i < n; ++i) {
        int j = i + 1 == n ? 0 : i + 1;
        int ia = corners[i];
        int ib = corners[j];
        ntv::nvec3<T> a = (ia >= 0 && ia < points.count) ? points[ia] : ntv::nvec3<T>(T(0), T(0), T(0));
        ntv::nvec3<T> b = (ib >= 0 && ib < points.count) ? points[ib] : ntv::nvec3<T>(T(0), T(0), T(0));
        sum[0] += (a[1] - b[1]) * (a[2] + b[2]);
        sum[1] += (a[2] - b[2]) * (a[0] + b[0]);
        sum[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    return sum;
}

template <typename T, typename Attr>
NTL_HD ntv::nvec3<T> ntmesh<T, Attr>::faceNormal(int f) const {
    ntv::nvec3<T> zero(T(0), T(0), T(0));
    if (f < 0) return zero;
    const int* idx = nullptr;
    int n = 0;
    int nt = triangleCount();
    if (f < nt) {
        idx = triangleIndices.data + f * 3;
        n = 3;
    } else {
        f -= nt;
        int nq = quadCount();
        if (f < nq) {
            idx = quadIndices.data + f * 4;
            n = 4;
        } else {
            f -= nq;
            int nn = ngonCount();
            if (f >= nn) return zero;
            idx = ngonIndices.data + ngonOffsets[f];
            n = ngonVertexCount(f);
        }
    }
    ntv::nvec3<T> nrm = newell(idx, n);
    nrm.normalize();
    return nrm;
}

namespace earclip {

inline double absd(double x) { return x < 0.0 ? -x : x; }

inline double cross2(double ax, double ay, double bx, double by, double cx, double cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

inline int insideTri(double px, double py, double ax, double ay, double bx, double by, double cx, double cy, double eps) {
    double ab = cross2(ax, ay, bx, by, px, py);
    double bc = cross2(bx, by, cx, cy, px, py);
    double ca = cross2(cx, cy, ax, ay, px, py);
    double tri = cross2(ax, ay, bx, by, cx, cy);
    if (tri > eps) return ab > eps && bc > eps && ca > eps;
    if (tri < -eps) return ab < -eps && bc < -eps && ca < -eps;
    return 0;
}

template <typename T>
void emitTri(const int* poly, const int* srcV, int a, int b, int c, int srcPrim,
             int* dstPts, int* dstSrcV, int* dstSrcP, int& written) {
    int base = written * 3;
    dstPts[base + 0] = poly[a];
    dstPts[base + 1] = poly[b];
    dstPts[base + 2] = poly[c];
    dstSrcV[base + 0] = srcV[a];
    dstSrcV[base + 1] = srcV[b];
    dstSrcV[base + 2] = srcV[c];
    dstSrcP[written] = srcPrim;
    ++written;
}

template <typename T>
void fanRemaining(const int* poly, const int* srcV, const int* ring, int m, int srcPrim,
                  int* dstPts, int* dstSrcV, int* dstSrcP, int& written) {
    for (int k = 1; k + 1 < m; ++k)
        emitTri<T>(poly, srcV, ring[0], ring[k], ring[k + 1], srcPrim, dstPts, dstSrcV, dstSrcP, written);
}

// Ear-clip one simple polygon into n-2 triangles. A degenerate or self-crossing
// face falls back to a fan. dst arrays must hold n-2 triangles.
template <typename T, typename Attr>
void clipFace(const ntmesh<T, Attr>& mesh, const int* poly, const int* srcV, int n, int srcPrim,
              int* dstPts, int* dstSrcV, int* dstSrcP, int& written) {
    if (n < 3) return;
    double* u = static_cast<double*>(std::malloc(sizeof(double) * (size_t)n));
    double* v = static_cast<double*>(std::malloc(sizeof(double) * (size_t)n));
    int* ring = static_cast<int*>(std::malloc(sizeof(int) * (size_t)n));
    if (!u || !v || !ring) {
        std::free(u);
        std::free(v);
        std::free(ring);
        return;
    }
    double xyz[3][64];
    double* xs = n <= 64 ? xyz[0] : static_cast<double*>(std::malloc(sizeof(double) * (size_t)n * 3));
    double* ys = xs ? xs + n : nullptr;
    double* zs = ys ? ys + n : nullptr;
    if (!xs) {
        std::free(u);
        std::free(v);
        std::free(ring);
        return;
    }
    const ntv::nvec3<T>* points = mesh.points.data;
    int pointCount = mesh.points.count;
    for (int i = 0; i < n; ++i) {
        double x = 0, y = 0, z = 0;
        int id = poly[i];
        if (points && id >= 0 && id < pointCount) {
            x = (double)points[id][0];
            y = (double)points[id][1];
            z = (double)points[id][2];
        }
        xs[i] = x;
        ys[i] = y;
        zs[i] = z;
        ring[i] = i;
    }
    ntv::nvec3<T> nrm = mesh.newell(poly, n);
    double nx = (double)nrm[0];
    double ny = (double)nrm[1];
    double nz = (double)nrm[2];
    int axis = 0;
    double ax = absd(nx), ay = absd(ny), az = absd(nz);
    if (ay > ax) axis = 1;
    if (az > (axis == 0 ? ax : ay)) axis = 2;
    int uu = (axis + 1) % 3;
    int vv = (axis + 2) % 3;
    double nav = axis == 0 ? nx : (axis == 1 ? ny : nz);
    if (nav < 0) {
        int swap = uu;
        uu = vv;
        vv = swap;
    }
    const double* comp[3] = {xs, ys, zs};
    double scale = 0;
    for (int i = 0; i < n; ++i) {
        u[i] = comp[uu][i];
        v[i] = comp[vv][i];
        if (absd(u[i]) > scale) scale = absd(u[i]);
        if (absd(v[i]) > scale) scale = absd(v[i]);
    }
    double area2 = 0;
    for (int i = 0; i < n; ++i) {
        int j = i + 1 < n ? i + 1 : 0;
        area2 += u[i] * v[j] - u[j] * v[i];
    }
    double eps = 1e-12 * (scale * scale + 1.0);
    int m = n;
    if (absd(area2) <= eps) {
        fanRemaining<T>(poly, srcV, ring, m, srcPrim, dstPts, dstSrcV, dstSrcP, written);
    } else {
        double sign = area2 > 0 ? 1.0 : -1.0;
        while (m > 3) {
            int cut = -1;
            for (int i = 0; i < m; ++i) {
                int ip = ring[(i + m - 1) % m];
                int ic = ring[i];
                int in = ring[(i + 1) % m];
                double cr = cross2(u[ip], v[ip], u[ic], v[ic], u[in], v[in]);
                if (cr * sign < -eps) continue;
                int ear = 1;
                for (int k = 0; k < m; ++k) {
                    int ik = ring[k];
                    if (ik == ip || ik == ic || ik == in) continue;
                    if (insideTri(u[ik], v[ik], u[ip], v[ip], u[ic], v[ic], u[in], v[in], eps)) {
                        ear = 0;
                        break;
                    }
                }
                if (!ear) continue;
                cut = i;
                break;
            }
            if (cut < 0) {
                fanRemaining<T>(poly, srcV, ring, m, srcPrim, dstPts, dstSrcV, dstSrcP, written);
                m = 0;
                break;
            }
            int ip = ring[(cut + m - 1) % m];
            int in = ring[(cut + 1) % m];
            emitTri<T>(poly, srcV, ip, ring[cut], in, srcPrim, dstPts, dstSrcV, dstSrcP, written);
            for (int j = cut; j + 1 < m; ++j) ring[j] = ring[j + 1];
            --m;
        }
        if (m == 3)
            emitTri<T>(poly, srcV, ring[0], ring[1], ring[2], srcPrim, dstPts, dstSrcV, dstSrcP, written);
    }
    if (xs != xyz[0]) std::free(xs);
    std::free(u);
    std::free(v);
    std::free(ring);
}

} // namespace earclip

// Replace quads and n-gons with triangles. Existing triangles stay first.
template <typename T, typename Attr>
void ntmesh<T, Attr>::triangulate() {
    int nq = quadCount();
    int nn = ngonCount();
    if (nq == 0 && nn == 0) return;

    int newTris = nq * 2;
    for (int f = 0; f < nn; ++f) {
        int nv = ngonVertexCount(f);
        if (nv >= 3) newTris += nv - 2;
    }
    int triV = triangleIndices.count;
    int triF = triangleCount();
    int oldPolyV = quadIndices.count + ngonIndices.count;
    int oldPolyF = nq + nn;

    int* dstPts = newTris > 0 ? static_cast<int*>(std::malloc(sizeof(int) * (size_t)newTris * 3)) : nullptr;
    int* dstSrcV = newTris > 0 ? static_cast<int*>(std::malloc(sizeof(int) * (size_t)newTris * 3)) : nullptr;
    int* dstSrcP = newTris > 0 ? static_cast<int*>(std::malloc(sizeof(int) * (size_t)newTris)) : nullptr;
    int written = 0;
    if (newTris > 0 && dstPts && dstSrcV && dstSrcP) {
        for (int q = 0; q < nq; ++q) {
            int poly[4];
            int sv[4];
            for (int k = 0; k < 4; ++k) {
                poly[k] = quadIndices[q * 4 + k];
                sv[k] = triV + q * 4 + k;
            }
            earclip::clipFace(*this, poly, sv, 4, triF + q, dstPts, dstSrcV, dstSrcP, written);
        }
        int vbase = triV + quadIndices.count;
        int vrun = 0;
        for (int f = 0; f < nn; ++f) {
            int nv = ngonVertexCount(f);
            if (nv >= 3) {
                int* poly = static_cast<int*>(std::malloc(sizeof(int) * (size_t)nv * 2));
                if (poly) {
                    int* sv = poly + nv;
                    for (int k = 0; k < nv; ++k) {
                        poly[k] = ngonIndices[ngonOffsets[f] + k];
                        sv[k] = vbase + vrun + k;
                    }
                    earclip::clipFace(*this, poly, sv, nv, triF + nq + f,
                                         dstPts, dstSrcV, dstSrcP, written);
                    std::free(poly);
                }
            }
            vrun += nv;
        }
    }

    int newV = written * 3;
    int newF = written;
    if (newV > 0) this->attrInsertVerts(triV, newV);
    for (int i = 0; i < newV; ++i) {
        int src = dstSrcV[i];
        if (src >= triV) src += newV;
        this->attrCopyVert(triV + i, src);
    }
    if (oldPolyV > 0) this->attrEraseVerts(triV + newV, oldPolyV);

    if (newF > 0) this->attrInsertPrims(triF, newF);
    for (int i = 0; i < newF; ++i) {
        int src = dstSrcP[i];
        if (src >= triF) src += newF;
        this->attrCopyPrim(triF + i, src);
    }
    if (oldPolyF > 0) this->attrErasePrims(triF + newF, oldPolyF);

    if (newV > 0) {
        bufFit(triangleIndices, triV + newV);
        for (int i = 0; i < newV; ++i) triangleIndices[triV + i] = dstPts[i];
    }
    bufFit(quadIndices, 0);
    bufFit(ngonIndices, 0);
    bufFit(ngonOffsets, 0);
    std::free(dstPts);
    std::free(dstSrcV);
    std::free(dstSrcP);
}

// Flip corner order. Vertex attributes for those corners swap with them.
template <typename T, typename Attr>
void ntmesh<T, Attr>::reverse() {
    auto span = [this](int* idx, int vertAt, int n) {
        if (!idx || n < 2) return;
        for (int i = 0; i < n / 2; ++i) {
            int j = n - 1 - i;
            int tmp = idx[i];
            idx[i] = idx[j];
            idx[j] = tmp;
            this->attrSwapVert(vertAt + i, vertAt + j);
        }
    };
    int triV = triangleIndices.count;
    for (int f = 0; f < triangleCount(); ++f)
        span(triangleIndices.data + f * 3, f * 3, 3);
    int quadAt = triV;
    for (int f = 0; f < quadCount(); ++f)
        span(quadIndices.data + f * 4, quadAt + f * 4, 4);
    int ngonAt = triV + quadIndices.count;
    for (int f = 0; f < ngonCount(); ++f) {
        int at = ngonOffsets[f];
        span(ngonIndices.data + at, ngonAt + at, ngonVertexCount(f));
    }
    int lineAt = ngonAt + ngonIndices.count;
    for (int f = 0; f < lineCount(); ++f) {
        int at = lineOffsets[f];
        span(lineIndices.data + at, lineAt + at, lineVertexCount(f));
    }
}

// Drop points that no triangle, quad, n-gon, or line corner references.
template <typename T, typename Attr>
void ntmesh<T, Attr>::removeUnusedPoints() {
    int n = points.count;
    if (n <= 0) return;
    int* map = static_cast<int*>(std::malloc(sizeof(int) * (size_t)n));
    if (!map) return;
    for (int i = 0; i < n; ++i) map[i] = 0;
    auto mark = [&](const Buf<int>& idx) {
        for (int i = 0; i < idx.count; ++i) {
            int p = idx[i];
            if (p >= 0 && p < n) map[p] = 1;
        }
    };
    mark(triangleIndices);
    mark(quadIndices);
    mark(ngonIndices);
    mark(lineIndices);
    int kept = 0;
    for (int i = 0; i < n; ++i) {
        if (!map[i]) {
            map[i] = -1;
            continue;
        }
        if (kept != i) {
            points[kept] = points[i];
            this->attrCopyPoint(kept, i);
        }
        map[i] = kept++;
    }
    if (kept == n) {
        std::free(map);
        return;
    }
    bufFit(points, kept);
    this->attrFitPoints(kept);
    auto remap = [&](Buf<int>& idx) {
        for (int i = 0; i < idx.count; ++i) {
            int p = idx[i];
            if (p >= 0 && p < n) idx[i] = map[p];
        }
    };
    remap(triangleIndices);
    remap(quadIndices);
    remap(ngonIndices);
    remap(lineIndices);
    std::free(map);
}

template <typename T>
NTL_HD void addPolygonFan(const ntv::nvec3<T>* pts, int npts, const int* idx, int n, T& area, T& volume) {
    if (!pts || !idx || n < 3) return;
    for (int i = 0; i < n; ++i) {
        if (idx[i] < 0 || idx[i] >= npts) return;
    }
    ntv::nvec3<T> p0 = pts[idx[0]];
    ntv::nvec3<T> acc(T(0), T(0), T(0));
    T vol = T(0);
    for (int i = 1; i + 1 < n; ++i) {
        ntv::nvec3<T> a = pts[idx[i]];
        ntv::nvec3<T> b = pts[idx[i + 1]];
        acc += (a - p0).cross(b - p0);
        vol += p0.dot(a.cross(b));
    }
    area += acc.length() * T(0.5);
    volume += vol / T(6);
}

template <typename T, typename Attr>
NTL_HD void fanMeasures(const ntmesh<T, Attr>& mesh, T& area, T& volume) {
    area = T(0);
    volume = T(0);
    if (!mesh.points.data) return;
    for (int f = 0; f < mesh.triangleCount(); ++f)
        addPolygonFan(mesh.points.data, mesh.points.count, mesh.triangleIndices.data + f * 3, 3, area, volume);
    for (int f = 0; f < mesh.quadCount(); ++f)
        addPolygonFan(mesh.points.data, mesh.points.count, mesh.quadIndices.data + f * 4, 4, area, volume);
    for (int f = 0; f < mesh.ngonCount(); ++f) {
        int at = mesh.ngonOffsets[f];
        addPolygonFan(mesh.points.data, mesh.points.count, mesh.ngonIndices.data + at, mesh.ngonVertexCount(f), area, volume);
    }
}

template <typename T, typename Attr>
NTL_HD T ntmesh<T, Attr>::totalSurfaceArea() const {
    T area = T(0);
    T volume = T(0);
    fanMeasures(*this, area, volume);
    return area;
}

template <typename T, typename Attr>
NTL_HD T ntmesh<T, Attr>::totalVolume() const {
    T area = T(0);
    T volume = T(0);
    fanMeasures(*this, area, volume);
    return volume;
}

template <typename T, typename Attr>
int ntmesh<T, Attr>::isClosed() const {
    int nedges = triangleIndices.count + quadIndices.count + ngonIndices.count;
    if (nedges <= 0) return 0;
    struct Edge { int a, b; };
    Edge* edges = static_cast<Edge*>(std::malloc(sizeof(Edge) * (size_t)nedges));
    if (!edges) return 0;
    int n = 0;
    auto pushFace = [&](const int* idx, int nv) {
        if (!idx || nv < 2) return;
        for (int k = 0; k < nv; ++k) {
            edges[n].a = idx[k];
            edges[n].b = idx[(k + 1) % nv];
            ++n;
        }
    };
    for (int f = 0; f < triangleCount(); ++f) pushFace(triangleIndices.data + f * 3, 3);
    for (int f = 0; f < quadCount(); ++f) pushFace(quadIndices.data + f * 4, 4);
    for (int f = 0; f < ngonCount(); ++f) pushFace(ngonIndices.data + ngonOffsets[f], ngonVertexCount(f));
    auto lessEdge = [](const void* pa, const void* pb) -> int {
        const Edge* a = static_cast<const Edge*>(pa);
        const Edge* b = static_cast<const Edge*>(pb);
        if (a->a < b->a) return -1;
        if (a->a > b->a) return 1;
        if (a->b < b->b) return -1;
        if (a->b > b->b) return 1;
        return 0;
    };
    std::qsort(edges, (size_t)n, sizeof(Edge), lessEdge);
    int closed = 1;
    for (int i = 0; i < n; ) {
        int j = i + 1;
        while (j < n && edges[j].a == edges[i].a && edges[j].b == edges[i].b) ++j;
        if (j - i != 1 || edges[i].a == edges[i].b) {
            closed = 0;
            break;
        }
        Edge opp;
        opp.a = edges[i].b;
        opp.b = edges[i].a;
        if (!std::bsearch(&opp, edges, (size_t)n, sizeof(Edge), lessEdge)) {
            closed = 0;
            break;
        }
        i = j;
    }
    std::free(edges);
    return closed;
}

// print a mesh. Uses printf, safe for cuda
template <typename T, typename Attr>
void ntmesh<T, Attr>::print() const {
    std::printf("ntmesh points %d vertices %d prims %d tris %d quads %d ngons %d lines %d\n",
                pointCount(), vertexCount(), primCount(), triangleCount(), quadCount(), ngonCount(), lineCount());
    this->attrPrint();
    for (int f = 0; f < primCount(); ++f) {
        std::printf("  prim %d:", f);
        int n = primVertexCount(f);
        for (int k = 0; k < n; ++k) std::printf(" %d", primVertex(f, k));
        std::printf("\n");
    }
}

// Deep copy of one policy. Points convert from Src to Dst.
// Indices are copied into new buffers. The policy clones its own attributes.
template <typename Dst, typename Src, typename Attr>
void copyMesh(ntmesh<Dst, Attr>& dst, const ntmesh<Src, Attr>& src) {
    ntmesh<Dst, Attr> out;
    bufFit(out.points, src.pointCount());
    for (int i = 0; i < src.pointCount(); ++i) {
        const ntv::nvec3<Src>& p = src.points[i];
        out.points[i] = ntv::nvec3<Dst>((Dst)p[0], (Dst)p[1], (Dst)p[2]);
    }
    bufClone(out.triangleIndices, src.triangleIndices);
    bufClone(out.quadIndices, src.quadIndices);
    bufClone(out.ngonIndices, src.ngonIndices);
    bufClone(out.ngonOffsets, src.ngonOffsets);
    bufClone(out.lineIndices, src.lineIndices);
    bufClone(out.lineOffsets, src.lineOffsets);
    out.attrClone(src);

    dst.clear();
    dst.takeTopology(out);
    static_cast<Attr&>(dst) = static_cast<Attr&>(out);
    static_cast<Attr&>(out) = Attr{};
}

// Copy points and topology into dst. dst is cleared first, so its attributes are dropped.
template <typename Dst, typename Src, typename AD, typename AS>
void copyTopo(ntmesh<Dst, AD>& dst, const ntmesh<Src, AS>& src) {
    ntmesh<Dst, AD> out;
    bufFit(out.points, src.pointCount());
    for (int i = 0; i < src.pointCount(); ++i) {
        const ntv::nvec3<Src>& p = src.points[i];
        out.points[i] = ntv::nvec3<Dst>((Dst)p[0], (Dst)p[1], (Dst)p[2]);
    }
    bufClone(out.triangleIndices, src.triangleIndices);
    bufClone(out.quadIndices, src.quadIndices);
    bufClone(out.ngonIndices, src.ngonIndices);
    bufClone(out.ngonOffsets, src.ngonOffsets);
    bufClone(out.lineIndices, src.lineIndices);
    bufClone(out.lineOffsets, src.lineOffsets);

    dst.clear();
    dst.takeTopology(out);
}

} // namespace ntm
