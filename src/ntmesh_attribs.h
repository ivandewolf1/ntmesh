// Numeric attributes for ntmesh.
// Point, vertex, prim, and mesh columns of int or float.
// The mesh stays trivially copyable: upload the bytes, then rewrite the pointers.
// String attributes live in ntmesh_strattr.h and are host-only.


#pragma once
#include "ntmesh.h"

namespace ntm {

// The types of attributes that can be added to a mesh.
enum class Type {
    Int,
    Float,
    Position, // float tuple, takes the full matrix including translation
    Normal,  // when we transform a mesh, the normals should only rottate, and maintain unit length.
    Color    // when we transform a mesh, the color should not be affected.
};

// An attribute is a named column of int or float values.
struct Attribute {
    char  name[32];
    Type  kind;
    int   tupleSize;
    int   count;
    int   capacity;
    void* data;

    NTL_HD int isInt() const { return kind == Type::Int; }
    NTL_HD float* floatAt(int i) { return (float*)data + i * tupleSize; }
    NTL_HD const float* floatAt(int i) const { return (const float*)data + i * tupleSize; }
    NTL_HD int* intAt(int i) { return (int*)data + i * tupleSize; }
    NTL_HD const int* intAt(int i) const { return (const int*)data + i * tupleSize; }
};

// Compare two attribute names.
NTL_HD inline int nameEq(const char* a, const char* b) {
    if (!a || !b) return 0;
    for (int i = 0; i < 32; ++i) {
        if (a[i] != b[i]) return 0;
        if (a[i] == 0) return 1;
    }
    return 1;
}

// A set of attributes, on points, prims, verts, or the mesh.
struct AttributeSet {
    Attribute* data = nullptr;
    int        count = 0;
    int        capacity = 0;

    NTL_HD Attribute* find(const char* name) {
        for (int i = 0; i < count; ++i)
            if (nameEq(data[i].name, name)) return &data[i];
        return nullptr;
    }
    NTL_HD const Attribute* find(const char* name) const {
        for (int i = 0; i < count; ++i)
            if (nameEq(data[i].name, name)) return &data[i];
        return nullptr;
    }
    NTL_HD Attribute& operator[](int i) { return data[i]; }
    NTL_HD const Attribute& operator[](int i) const { return data[i]; }
    NTL_HD int size() const { return count; }
};

inline int elemStride(const Attribute& a) {
    int w = (a.kind == Type::Int) ? (int)sizeof(int) : (int)sizeof(float);
    return w * a.tupleSize;
}

inline void setName(char* dst, const char* name) {
    int i = 0;
    if (name) {
        for (; i < 31 && name[i]; ++i) dst[i] = name[i];
    }
    dst[i] = 0;
}

inline void attribReserve(Attribute& a, int cap) {
    if (cap <= a.capacity) return;
    a.data = std::realloc(a.data, (size_t)elemStride(a) * (size_t)cap);
    a.capacity = cap;
}

inline void attribResize(Attribute& a, int n) {
    if (n < 0) n = 0;
    if (n > a.capacity) attribReserve(a, n);
    if (n > a.count && a.data)
        std::memset((char*)a.data + (size_t)elemStride(a) * (size_t)a.count, 0,
                    (size_t)elemStride(a) * (size_t)(n - a.count));
    a.count = n;
}

inline void setReserve(AttributeSet& s, int cap) {
    if (cap <= s.capacity) return;
    s.data = static_cast<Attribute*>(std::realloc(s.data, sizeof(Attribute) * (size_t)cap));
    s.capacity = cap;
}

inline void fitSet(AttributeSet& s, int n) {
    for (int i = 0; i < s.count; ++i) attribResize(s.data[i], n);
}

// Insert n elements at index `at`, shifting the tail up. New elements are zero.
inline void attribInsert(Attribute& a, int at, int n) {
    if (n <= 0) return;
    if (at < 0) at = 0;
    if (at > a.count) at = a.count;
    int old = a.count;
    attribResize(a, old + n);
    int stride = elemStride(a);
    if (!a.data || stride <= 0) return;
    if (at < old)
        std::memmove((char*)a.data + (size_t)stride * (size_t)(at + n),
                     (char*)a.data + (size_t)stride * (size_t)at,
                     (size_t)stride * (size_t)(old - at));
    std::memset((char*)a.data + (size_t)stride * (size_t)at, 0, (size_t)stride * (size_t)n);
}

inline void insertSet(AttributeSet& s, int at, int n) {
    for (int i = 0; i < s.count; ++i) attribInsert(s.data[i], at, n);
}

// Drop n elements starting at `at`.
inline void attribErase(Attribute& a, int at, int n) {
    if (n <= 0 || at >= a.count) return;
    if (at < 0) at = 0;
    if (at + n > a.count) n = a.count - at;
    int stride = elemStride(a);
    int tail = a.count - (at + n);
    if (a.data && stride > 0 && tail > 0)
        std::memmove((char*)a.data + (size_t)stride * (size_t)at,
                     (char*)a.data + (size_t)stride * (size_t)(at + n),
                     (size_t)stride * (size_t)tail);
    a.count -= n;
}

inline void eraseSet(AttributeSet& s, int at, int n) {
    for (int i = 0; i < s.count; ++i) attribErase(s.data[i], at, n);
}

inline void swapAttribElem(AttributeSet& s, int a, int b) {
    if (a == b) return;
    for (int i = 0; i < s.count; ++i) {
        Attribute& col = s.data[i];
        if (!col.data || a < 0 || b < 0 || a >= col.count || b >= col.count) continue;
        int stride = elemStride(col);
        if (stride <= 0) continue;
        unsigned char* pa = (unsigned char*)col.data + (size_t)stride * (size_t)a;
        unsigned char* pb = (unsigned char*)col.data + (size_t)stride * (size_t)b;
        for (int k = 0; k < stride; ++k) {
            unsigned char tmp = pa[k];
            pa[k] = pb[k];
            pb[k] = tmp;
        }
    }
}

inline void copyAttribElem(AttributeSet& s, int dst, int src) {
    if (dst == src) return;
    for (int i = 0; i < s.count; ++i) {
        Attribute& a = s.data[i];
        if (!a.data || src < 0 || dst < 0 || src >= a.count || dst >= a.count) continue;
        int stride = elemStride(a);
        if (stride <= 0) continue;
        std::memcpy((char*)a.data + (size_t)stride * (size_t)dst,
                    (char*)a.data + (size_t)stride * (size_t)src,
                    (size_t)stride);
    }
}

inline void reserveSet(AttributeSet& s, int n) {
    for (int i = 0; i < s.count; ++i) attribReserve(s.data[i], n);
}

inline Attribute* addAttrib(AttributeSet& s, const char* name, Type kind, int tupleSize, int domainCount) {
    if (tupleSize < 1) tupleSize = 1;
    if (Attribute* existing = s.find(name)) return existing;
    if (s.count == s.capacity) setReserve(s, s.capacity ? s.capacity * 2 : 4);
    Attribute& a = s.data[s.count++];
    std::memset(&a, 0, sizeof(a));
    setName(a.name, name);
    a.kind = kind;
    a.tupleSize = tupleSize;
    attribResize(a, domainCount);
    return &a;
}

// Insert n elements at dstAt and copy n from srcAt into that hole.
inline void mergeSetInsert(AttributeSet& dst, const AttributeSet& src, int oldN, int dstAt, int srcAt, int n) {
    if (n <= 0) return;
    for (int i = 0; i < dst.count; ++i) attribInsert(dst.data[i], dstAt, n);
    int newN = oldN + n;
    for (int i = 0; i < src.count; ++i) {
        const Attribute& s = src.data[i];
        Attribute* d = dst.find(s.name);
        if (!d) d = addAttrib(dst, s.name, s.kind, s.tupleSize, newN);
        else if (d->kind != s.kind || d->tupleSize != s.tupleSize) continue;
        int ncopy = s.count - srcAt;
        if (ncopy > n) ncopy = n;
        if (ncopy <= 0 || !s.data || !d->data) continue;
        int stride = elemStride(*d);
        std::memcpy((char*)d->data + (size_t)stride * (size_t)dstAt,
                    (char*)s.data + (size_t)stride * (size_t)srcAt,
                    (size_t)stride * (size_t)ncopy);
    }
}

inline void mergeSet(AttributeSet& dst, const AttributeSet& src, int oldN, int newN) {
    fitSet(dst, newN);
    for (int i = 0; i < src.count; ++i) {
        const Attribute& s = src.data[i];
        Attribute* d = dst.find(s.name);
        if (!d) d = addAttrib(dst, s.name, s.kind, s.tupleSize, newN);
        else if (d->kind != s.kind || d->tupleSize != s.tupleSize) continue;
        else if (d->count != newN) attribResize(*d, newN);
        int ncopy = s.count;
        int room = newN - oldN;
        if (ncopy > room) ncopy = room;
        if (ncopy <= 0 || !s.data || !d->data) continue;
        int stride = elemStride(*d);
        std::memcpy((char*)d->data + (size_t)stride * (size_t)oldN, s.data, (size_t)stride * (size_t)ncopy);
    }
}

// One value for the whole mesh. The destination keeps its value when both have the name.
inline void mergeDetail(AttributeSet& dst, const AttributeSet& src) {
    for (int i = 0; i < src.count; ++i) {
        const Attribute& s = src.data[i];
        Attribute* d = dst.find(s.name);
        if (d) continue;
        d = addAttrib(dst, s.name, s.kind, s.tupleSize, 1);
        if (s.count > 0 && s.data && d->data)
            std::memcpy(d->data, s.data, (size_t)elemStride(*d));
    }
}

inline void freeSet(AttributeSet& s) {
    for (int i = 0; i < s.count; ++i) std::free(s.data[i].data);
    std::free(s.data);
    s.data = nullptr;
    s.count = 0;
    s.capacity = 0;
}

inline void setClone(AttributeSet& dst, const AttributeSet& src) {
    freeSet(dst);
    if (src.count <= 0 || !src.data) return;
    setReserve(dst, src.count);
    dst.count = src.count;
    for (int i = 0; i < src.count; ++i) {
        const Attribute& s = src.data[i];
        Attribute& d = dst.data[i];
        std::memset(&d, 0, sizeof(d));
        setName(d.name, s.name);
        d.kind = s.kind;
        d.tupleSize = s.tupleSize;
        if (s.count > 0 && s.data) {
            attribResize(d, s.count);
            std::memcpy(d.data, s.data, (size_t)elemStride(d) * (size_t)s.count);
        }
    }
}

inline const char* kindName(Type kind) {
    switch (kind) {
    case Type::Int:    return "int";
    case Type::Float:  return "float";
    case Type::Position: return "position";
    case Type::Normal: return "normal";
    case Type::Color:  return "color";
    }
    return "?";
}

inline void printSet(const char* domain, const AttributeSet& s) {
    for (int i = 0; i < s.count; ++i) {
        const Attribute& a = s.data[i];
        std::printf("  %s %s %s tuple %d count %d\n", domain, a.name, kindName(a.kind), a.tupleSize, a.count);
    }
}

// Position columns take the affine matrix. Normals take the inverse-transpose 3x3
// and are renormalized. Other kinds are left alone. A tuple shorter than 3 is skipped.
template <typename T>
void transformAttrib(Attribute& a, const ntv::nmat4<T>& m, const ntv::nmat3<T>& nmat) {
    if (a.tupleSize < 3 || !a.data || a.count <= 0) return;
    if (a.kind != Type::Position && a.kind != Type::Normal) return;
    for (int i = 0; i < a.count; ++i) {
        float* f = a.floatAt(i);
        ntv::nvec3<T> v((T)f[0], (T)f[1], (T)f[2]);
        if (a.kind == Type::Position) v = m * v;
        else {
            v = nmat * v;
            v.normalize();
        }
        f[0] = (float)v[0];
        f[1] = (float)v[1];
        f[2] = (float)v[2];
    }
}

template <typename T>
void transformSet(AttributeSet& s, const ntv::nmat4<T>& m, const ntv::nmat3<T>& nmat) {
    for (int i = 0; i < s.count; ++i) transformAttrib(s.data[i], m, nmat);
}

// Numeric attribute policy. Use ntmesh<T, WithAttribs>.
// pointN, vertN, and primN follow the mesh so a new column can be sized
// without the policy seeing the point and index buffers.
struct WithAttribs {
    AttributeSet pointAttribs;
    AttributeSet vertexAttribs;
    AttributeSet primAttribs;
    AttributeSet meshAttribs;
    int          pointN = 0;
    int          vertN = 0;
    int          primN = 0;

    NTL_HD Attribute* findPointAttrib(const char* name) { return pointAttribs.find(name); }
    NTL_HD Attribute* findVertexAttrib(const char* name) { return vertexAttribs.find(name); }
    NTL_HD Attribute* findFaceAttrib(const char* name) { return primAttribs.find(name); }
    NTL_HD Attribute* findMeshAttrib(const char* name) { return meshAttribs.find(name); }
    NTL_HD const Attribute* findPointAttrib(const char* name) const { return pointAttribs.find(name); }
    NTL_HD const Attribute* findVertexAttrib(const char* name) const { return vertexAttribs.find(name); }
    NTL_HD const Attribute* findFaceAttrib(const char* name) const { return primAttribs.find(name); }
    NTL_HD const Attribute* findMeshAttrib(const char* name) const { return meshAttribs.find(name); }

    Attribute* addPointAttrib(const char* name, Type kind, int tupleSize) {
        return addAttrib(pointAttribs, name, kind, tupleSize, pointN);
    }
    Attribute* addVertexAttrib(const char* name, Type kind, int tupleSize) {
        return addAttrib(vertexAttribs, name, kind, tupleSize, vertN);
    }
    Attribute* addFaceAttrib(const char* name, Type kind, int tupleSize) {
        return addAttrib(primAttribs, name, kind, tupleSize, primN);
    }
    Attribute* addMeshAttrib(const char* name, Type kind, int tupleSize) {
        return addAttrib(meshAttribs, name, kind, tupleSize, 1);
    }

    void attrReserve(int npoints, int nverts, int nprims) {
        reserveSet(pointAttribs, npoints);
        reserveSet(vertexAttribs, nverts);
        reserveSet(primAttribs, nprims);
        reserveSet(meshAttribs, 1);
    }
    void attrFitPoints(int n) { pointN = n; fitSet(pointAttribs, n); }
    void attrFitVerts(int n) { vertN = n; fitSet(vertexAttribs, n); }
    void attrFitPrims(int n) { primN = n; fitSet(primAttribs, n); }
    void attrFitDetail() { fitSet(meshAttribs, 1); }
    void attrInsertVerts(int at, int n) { if (n > 0) vertN += n; insertSet(vertexAttribs, at, n); }
    void attrInsertPrims(int at, int n) { if (n > 0) primN += n; insertSet(primAttribs, at, n); }
    void attrEraseVerts(int at, int n) { eraseSet(vertexAttribs, at, n); vertN -= n; if (vertN < 0) vertN = 0; }
    void attrErasePrims(int at, int n) { eraseSet(primAttribs, at, n); primN -= n; if (primN < 0) primN = 0; }
    void attrCopyPoint(int dst, int src) { copyAttribElem(pointAttribs, dst, src); }
    void attrCopyVert(int dst, int src) { copyAttribElem(vertexAttribs, dst, src); }
    void attrCopyPrim(int dst, int src) { copyAttribElem(primAttribs, dst, src); }
    void attrSwapVert(int a, int b) { swapAttribElem(vertexAttribs, a, b); }
    void attrMergePoints(const WithAttribs& src, int oldN, int newN) {
        pointN = newN;
        mergeSet(pointAttribs, src.pointAttribs, oldN, newN);
    }
    void attrMergeVerts(const WithAttribs& src, int oldN, int dstAt, int srcAt, int n) {
        vertN = oldN + n;
        mergeSetInsert(vertexAttribs, src.vertexAttribs, oldN, dstAt, srcAt, n);
    }
    void attrMergePrims(const WithAttribs& src, int oldN, int dstAt, int srcAt, int n) {
        primN = oldN + n;
        mergeSetInsert(primAttribs, src.primAttribs, oldN, dstAt, srcAt, n);
    }
    void attrMergeDetail(const WithAttribs& src) { mergeDetail(meshAttribs, src.meshAttribs); }
    void attrClear() {
        freeSet(pointAttribs);
        freeSet(vertexAttribs);
        freeSet(primAttribs);
        freeSet(meshAttribs);
        pointN = 0;
        vertN = 0;
        primN = 0;
    }
    void attrClone(const WithAttribs& src) {
        setClone(pointAttribs, src.pointAttribs);
        setClone(vertexAttribs, src.vertexAttribs);
        setClone(primAttribs, src.primAttribs);
        setClone(meshAttribs, src.meshAttribs);
        pointN = src.pointN;
        vertN = src.vertN;
        primN = src.primN;
    }
    void attrPrint() const {
        printSet("point", pointAttribs);
        printSet("vertex", vertexAttribs);
        printSet("prim", primAttribs);
        printSet("mesh", meshAttribs);
    }
    template <typename T>
    void attrTransform(const ntv::nmat4<T>& m) {
        ntv::nmat3<T> nmat = m.asM3().inverse();
        nmat.transpose();
        transformSet(pointAttribs, m, nmat);
        transformSet(vertexAttribs, m, nmat);
        transformSet(primAttribs, m, nmat);
        transformSet(meshAttribs, m, nmat);
    }
};

namespace detail {

template <typename T, typename Attr>
ntv::nvec3<T> closedNewell(const ntmesh<T, Attr>& mesh, int f) {
    int nt = mesh.triangleCount();
    if (f < nt) return mesh.newell(mesh.triangleIndices.data + f * 3, 3);
    f -= nt;
    int nq = mesh.quadCount();
    if (f < nq) return mesh.newell(mesh.quadIndices.data + f * 4, 4);
    f -= nq;
    if (f < 0 || f >= mesh.ngonCount()) return ntv::nvec3<T>(T(0), T(0), T(0));
    return mesh.newell(mesh.ngonIndices.data + mesh.ngonOffsets[f], mesh.ngonVertexCount(f));
}

template <typename T, typename Attr>
int closedVertIndex(const ntmesh<T, Attr>& mesh, int f, int k) {
    int nt = mesh.triangleCount();
    if (f < nt) return f * 3 + k;
    int triV = mesh.triangleIndices.count;
    int nq = mesh.quadCount();
    if (f < nt + nq) return triV + (f - nt) * 4 + k;
    int g = f - nt - nq;
    return triV + mesh.quadIndices.count + mesh.ngonOffsets[g] + k;
}

template <typename T, typename Attr>
int faceUsesPoint(const ntmesh<T, Attr>& mesh, int f, int p) {
    int nv = mesh.primVertexCount(f);
    for (int k = 0; k < nv; ++k)
        if (mesh.primVertex(f, k) == p) return 1;
    return 0;
}

inline Attribute* normalColumn(AttributeSet& set, const char* name, int n) {
    Attribute* col = addAttrib(set, name, Type::Normal, 3, n);
    if (!col || col->kind != Type::Normal || col->tupleSize < 3 || col->isInt()) return nullptr;
    attribResize(*col, n);
    if (n > 0 && !col->data) return nullptr;
    for (int i = 0; i < n; ++i) {
        float* d = col->floatAt(i);
        d[0] = 0.f;
        d[1] = 0.f;
        d[2] = 0.f;
    }
    return col;
}

template <typename T>
void storeNormal(Attribute& col, int i, ntv::nvec3<T> n) {
    n.normalize();
    float* d = col.floatAt(i);
    d[0] = (float)n[0];
    d[1] = (float)n[1];
    d[2] = (float)n[2];
}

} // namespace detail

// Unit face normal for every prim. A line is zero. The column is named "N".
template <typename T, typename Attr>
Attribute* computeFaceNormals(ntmesh<T, Attr>& mesh) {
    int nf = mesh.primCount();
    Attribute* col = detail::normalColumn(mesh.primAttribs, "N", nf);
    if (!col) return nullptr;
    for (int f = 0; f < nf; ++f) detail::storeNormal(*col, f, mesh.faceNormal(f));
    return col;
}

// Area-weighted point normal. Every face that uses the point is added. The column is named "N".
template <typename T, typename Attr>
Attribute* computePointNormals(ntmesh<T, Attr>& mesh) {
    int np = mesh.pointCount();
    Attribute* col = detail::normalColumn(mesh.pointAttribs, "N", np);
    if (!col) return nullptr;
    if (np <= 0) return col;
    ntv::nvec3<T>* acc = static_cast<ntv::nvec3<T>*>(std::malloc(sizeof(ntv::nvec3<T>) * (size_t)np));
    if (!acc) return nullptr;
    for (int i = 0; i < np; ++i) acc[i] = ntv::nvec3<T>(T(0), T(0), T(0));
    int nf = mesh.triangleCount() + mesh.quadCount() + mesh.ngonCount();
    for (int f = 0; f < nf; ++f) {
        ntv::nvec3<T> raw = detail::closedNewell(mesh, f);
        int nv = mesh.primVertexCount(f);
        for (int k = 0; k < nv; ++k) {
            int p = mesh.primVertex(f, k);
            if (p < 0 || p >= np) continue;
            int seen = 0;
            for (int s = 0; s < k; ++s)
                if (mesh.primVertex(f, s) == p) seen = 1;
            if (seen) continue;
            acc[p] += raw;
        }
    }
    for (int i = 0; i < np; ++i) detail::storeNormal(*col, i, acc[i]);
    std::free(acc);
    return col;
}

// Area-weighted vertex normal. A face that shares the point is skipped when its normal is farther than cuspVal radians.
template <typename T, typename Attr>
Attribute* computeVertexNormals(ntmesh<T, Attr>& mesh, T cuspVal) {
    int nv = mesh.vertexCount();
    Attribute* col = detail::normalColumn(mesh.vertexAttribs, "N", nv);
    if (!col) return nullptr;
    int nf = mesh.triangleCount() + mesh.quadCount() + mesh.ngonCount();
    if (nf <= 0 || nv <= 0) return col;
    struct FaceN {
        ntv::nvec3<T> raw;
        ntv::nvec3<T> unit;
    };
    FaceN* faces = static_cast<FaceN*>(std::malloc(sizeof(FaceN) * (size_t)nf));
    if (!faces) return nullptr;
    for (int f = 0; f < nf; ++f) {
        faces[f].raw = detail::closedNewell(mesh, f);
        faces[f].unit = faces[f].raw;
        faces[f].unit.normalize();
    }
    const double pi = 3.14159265358979323846;
    int acceptAll = (double)cuspVal >= pi;
    T cosLimit = (T)std::cos((double)cuspVal) - (T)1e-5;
    for (int f = 0; f < nf; ++f) {
        int nc = mesh.primVertexCount(f);
        for (int k = 0; k < nc; ++k) {
            int p = mesh.primVertex(f, k);
            ntv::nvec3<T> acc(T(0), T(0), T(0));
            if (p >= 0 && p < mesh.pointCount()) {
                for (int g = 0; g < nf; ++g) {
                    if (!detail::faceUsesPoint(mesh, g, p)) continue;
                    if (g != f) {
                        if (faces[f].unit.length2() == T(0)) continue;
                        if (!acceptAll && faces[f].unit.dot(faces[g].unit) < cosLimit) continue;
                    }
                    acc += faces[g].raw;
                }
            }
            detail::storeNormal(*col, detail::closedVertIndex(mesh, f, k), acc);
        }
    }
    std::free(faces);
    return col;
}

} // namespace ntm
