// Host-only string attributes for ntmesh.
// Each value is an index into a pool of std::string, so repeated strings are stored once.
// ntmesh<T, WithStrAttribs<T>> is not trivially copyable and is not uploaded.
// copyTopo dumps points and topology. copyNumeric dumps those plus the numeric columns.


#pragma once
#include "ntmesh_attribs.h"
#include <string>
#include <vector>

namespace ntm {

// One string column. indices are pool ids. 0 is the empty string.
struct StringAttribute {
    char    name[32];
    Buf<int> indices;
};

struct StringSet {
    StringAttribute* data = nullptr;
    int              count = 0;
    int              capacity = 0;

    StringAttribute* find(const char* name) {
        for (int i = 0; i < count; ++i)
            if (nameEq(data[i].name, name)) return &data[i];
        return nullptr;
    }
    const StringAttribute* find(const char* name) const {
        for (int i = 0; i < count; ++i)
            if (nameEq(data[i].name, name)) return &data[i];
        return nullptr;
    }
};

inline void stringSetReserve(StringSet& s, int cap) {
    if (cap <= s.capacity) return;
    StringAttribute* grown =
        static_cast<StringAttribute*>(std::realloc(s.data, sizeof(StringAttribute) * (size_t)cap));
    if (!grown) return;
    s.data = grown;
    s.capacity = cap;
}

inline void freeStringSet(StringSet& s) {
    for (int i = 0; i < s.count; ++i) bufFree(s.data[i].indices);
    std::free(s.data);
    s.data = nullptr;
    s.count = 0;
    s.capacity = 0;
}

inline StringAttribute* addString(StringSet& s, const char* name, int domainCount) {
    if (StringAttribute* existing = s.find(name)) return existing;
    if (s.count == s.capacity) stringSetReserve(s, s.capacity ? s.capacity * 2 : 4);
    StringAttribute& a = s.data[s.count++];
    a = StringAttribute{};
    setName(a.name, name);
    bufFit(a.indices, domainCount);
    return &a;
}

inline void fitStringSet(StringSet& s, int n) {
    for (int i = 0; i < s.count; ++i) bufFit(s.data[i].indices, n);
}

inline void stringInsert(StringAttribute& a, int at, int n) {
    if (n <= 0) return;
    if (at < 0) at = 0;
    if (at > a.indices.count) at = a.indices.count;
    int old = a.indices.count;
    bufReserve(a.indices, old + n);
    if (a.indices.capacity < old + n) return;
    if (at < old && a.indices.data)
        std::memmove(a.indices.data + at + n, a.indices.data + at, sizeof(int) * (size_t)(old - at));
    for (int i = 0; i < n; ++i) a.indices.data[at + i] = 0;
    a.indices.count = old + n;
}

inline void insertStringSet(StringSet& s, int at, int n) {
    for (int i = 0; i < s.count; ++i) stringInsert(s.data[i], at, n);
}

inline void swapStringElem(StringSet& s, int a, int b) {
    if (a == b) return;
    for (int i = 0; i < s.count; ++i) {
        StringAttribute& col = s.data[i];
        if (!col.indices.data || a < 0 || b < 0 || a >= col.indices.count || b >= col.indices.count) continue;
        int tmp = col.indices[a];
        col.indices[a] = col.indices[b];
        col.indices[b] = tmp;
    }
}

inline void copyStringElem(StringSet& s, int dst, int src) {
    if (dst == src) return;
    for (int i = 0; i < s.count; ++i) {
        StringAttribute& a = s.data[i];
        if (!a.indices.data || src < 0 || dst < 0 || src >= a.indices.count || dst >= a.indices.count) continue;
        a.indices[dst] = a.indices[src];
    }
}

inline void stringErase(StringAttribute& a, int at, int n) {
    if (n <= 0 || at >= a.indices.count) return;
    if (at < 0) at = 0;
    if (at + n > a.indices.count) n = a.indices.count - at;
    int tail = a.indices.count - (at + n);
    if (a.indices.data && tail > 0)
        std::memmove(a.indices.data + at, a.indices.data + at + n, sizeof(int) * (size_t)tail);
    a.indices.count -= n;
}

inline void eraseStringSet(StringSet& s, int at, int n) {
    for (int i = 0; i < s.count; ++i) stringErase(s.data[i], at, n);
}

inline void reserveStringSet(StringSet& s, int n) {
    for (int i = 0; i < s.count; ++i) bufReserve(s.data[i].indices, n);
}

inline void cloneStringSet(StringSet& dst, const StringSet& src) {
    freeStringSet(dst);
    if (src.count <= 0 || !src.data) return;
    stringSetReserve(dst, src.count);
    dst.count = src.count;
    for (int i = 0; i < src.count; ++i) {
        StringAttribute& d = dst.data[i];
        d = StringAttribute{};
        setName(d.name, src.data[i].name);
        bufClone(d.indices, src.data[i].indices);
    }
}

inline void printStringSet(const char* domain, const StringSet& s) {
    for (int i = 0; i < s.count; ++i)
        std::printf("  %s %s string count %d\n", domain, s.data[i].name, s.data[i].indices.count);
}

// Numeric attributes plus a string pool. Use ntmesh<T, WithStrAttribs<T>>.
template <typename T>
struct WithStrAttribs : WithAttribs<T> {
    StringSet                pointStrings;
    StringSet                vertexStrings;
    StringSet                primStrings;
    StringSet                meshStrings;
    std::vector<std::string> pool;

    StringAttribute* findPointString(const char* name) { return pointStrings.find(name); }
    StringAttribute* findVertexString(const char* name) { return vertexStrings.find(name); }
    StringAttribute* findFaceString(const char* name) { return primStrings.find(name); }
    StringAttribute* findMeshString(const char* name) { return meshStrings.find(name); }
    const StringAttribute* findPointString(const char* name) const { return pointStrings.find(name); }
    const StringAttribute* findVertexString(const char* name) const { return vertexStrings.find(name); }
    const StringAttribute* findFaceString(const char* name) const { return primStrings.find(name); }
    const StringAttribute* findMeshString(const char* name) const { return meshStrings.find(name); }

    StringAttribute* addPointString(const char* name) { return addString(pointStrings, name, this->pointN); }
    StringAttribute* addVertexString(const char* name) { return addString(vertexStrings, name, this->vertN); }
    StringAttribute* addFaceString(const char* name) { return addString(primStrings, name, this->primN); }
    StringAttribute* addMeshString(const char* name) { return addString(meshStrings, name, 1); }

    int intern(const char* s) {
        if (pool.empty()) pool.push_back("");
        if (!s || !s[0]) return 0;
        for (int i = 1; i < (int)pool.size(); ++i)
            if (pool[(size_t)i] == s) return i;
        pool.push_back(s);
        return (int)pool.size() - 1;
    }
    int remap(const WithStrAttribs& from, int id) {
        if (id <= 0 || id >= (int)from.pool.size()) return 0;
        return intern(from.pool[(size_t)id].c_str());
    }
    void setString(StringAttribute& a, int i, const char* value) { a.indices[i] = intern(value); }
    const char* getString(const StringAttribute& a, int i) const {
        int id = a.indices[i];
        if (id <= 0 || id >= (int)pool.size()) return "";
        return pool[(size_t)id].c_str();
    }

    void attrReserve(int npoints, int nverts, int nprims) {
        WithAttribs<T>::attrReserve(npoints, nverts, nprims);
        reserveStringSet(pointStrings, npoints);
        reserveStringSet(vertexStrings, nverts);
        reserveStringSet(primStrings, nprims);
        reserveStringSet(meshStrings, 1);
    }
    void attrFitPoints(int n) { WithAttribs<T>::attrFitPoints(n); fitStringSet(pointStrings, n); }
    void attrFitVerts(int n) { WithAttribs<T>::attrFitVerts(n); fitStringSet(vertexStrings, n); }
    void attrFitPrims(int n) { WithAttribs<T>::attrFitPrims(n); fitStringSet(primStrings, n); }
    void attrFitDetail() { WithAttribs<T>::attrFitDetail(); fitStringSet(meshStrings, 1); }
    void attrInsertVerts(int at, int n) { WithAttribs<T>::attrInsertVerts(at, n); insertStringSet(vertexStrings, at, n); }
    void attrInsertPrims(int at, int n) { WithAttribs<T>::attrInsertPrims(at, n); insertStringSet(primStrings, at, n); }
    void attrEraseVerts(int at, int n) { WithAttribs<T>::attrEraseVerts(at, n); eraseStringSet(vertexStrings, at, n); }
    void attrErasePrims(int at, int n) { WithAttribs<T>::attrErasePrims(at, n); eraseStringSet(primStrings, at, n); }
    void attrCopyPoint(int dst, int src) { WithAttribs<T>::attrCopyPoint(dst, src); copyStringElem(pointStrings, dst, src); }
    void attrCopyVert(int dst, int src) { WithAttribs<T>::attrCopyVert(dst, src); copyStringElem(vertexStrings, dst, src); }
    void attrCopyPrim(int dst, int src) { WithAttribs<T>::attrCopyPrim(dst, src); copyStringElem(primStrings, dst, src); }
    void attrSwapVert(int a, int b) { WithAttribs<T>::attrSwapVert(a, b); swapStringElem(vertexStrings, a, b); }

    void attrMergePoints(const WithStrAttribs& src, int oldN, int newN) {
        WithAttribs<T>::attrMergePoints(src, oldN, newN);
        fitStringSet(pointStrings, newN);
        for (int i = 0; i < src.pointStrings.count; ++i) {
            const StringAttribute& s = src.pointStrings.data[i];
            StringAttribute* d = pointStrings.find(s.name);
            if (!d) d = addString(pointStrings, s.name, newN);
            int ncopy = s.indices.count;
            int room = newN - oldN;
            if (ncopy > room) ncopy = room;
            if (ncopy <= 0 || !s.indices.data || !d->indices.data) continue;
            for (int k = 0; k < ncopy; ++k)
                d->indices[oldN + k] = remap(src, s.indices[k]);
        }
    }
    void attrMergeVerts(const WithStrAttribs& src, int oldN, int dstAt, int srcAt, int n) {
        WithAttribs<T>::attrMergeVerts(src, oldN, dstAt, srcAt, n);
        if (n <= 0) return;
        insertStringSet(vertexStrings, dstAt, n);
        int newN = oldN + n;
        for (int i = 0; i < src.vertexStrings.count; ++i) {
            const StringAttribute& s = src.vertexStrings.data[i];
            StringAttribute* d = vertexStrings.find(s.name);
            if (!d) d = addString(vertexStrings, s.name, newN);
            int ncopy = s.indices.count - srcAt;
            if (ncopy > n) ncopy = n;
            if (ncopy <= 0 || !s.indices.data || !d->indices.data) continue;
            for (int k = 0; k < ncopy; ++k)
                d->indices[dstAt + k] = remap(src, s.indices[srcAt + k]);
        }
    }
    void attrMergePrims(const WithStrAttribs& src, int oldN, int dstAt, int srcAt, int n) {
        WithAttribs<T>::attrMergePrims(src, oldN, dstAt, srcAt, n);
        if (n <= 0) return;
        insertStringSet(primStrings, dstAt, n);
        int newN = oldN + n;
        for (int i = 0; i < src.primStrings.count; ++i) {
            const StringAttribute& s = src.primStrings.data[i];
            StringAttribute* d = primStrings.find(s.name);
            if (!d) d = addString(primStrings, s.name, newN);
            int ncopy = s.indices.count - srcAt;
            if (ncopy > n) ncopy = n;
            if (ncopy <= 0 || !s.indices.data || !d->indices.data) continue;
            for (int k = 0; k < ncopy; ++k)
                d->indices[dstAt + k] = remap(src, s.indices[srcAt + k]);
        }
    }
    void attrMergeDetail(const WithStrAttribs& src) {
        WithAttribs<T>::attrMergeDetail(src);
        for (int i = 0; i < src.meshStrings.count; ++i) {
            const StringAttribute& s = src.meshStrings.data[i];
            if (meshStrings.find(s.name)) continue;
            StringAttribute* d = addString(meshStrings, s.name, 1);
            if (s.indices.count > 0 && s.indices.data && d->indices.data)
                d->indices[0] = remap(src, s.indices[0]);
        }
    }
    void attrClear() {
        WithAttribs<T>::attrClear();
        freeStringSet(pointStrings);
        freeStringSet(vertexStrings);
        freeStringSet(primStrings);
        freeStringSet(meshStrings);
        pool.clear();
    }
    void attrClone(const WithStrAttribs& src) {
        WithAttribs<T>::attrClone(src);
        pool = src.pool;
        cloneStringSet(pointStrings, src.pointStrings);
        cloneStringSet(vertexStrings, src.vertexStrings);
        cloneStringSet(primStrings, src.primStrings);
        cloneStringSet(meshStrings, src.meshStrings);
    }
    void attrPrint() const {
        WithAttribs<T>::attrPrint();
        printStringSet("point", pointStrings);
        printStringSet("vertex", vertexStrings);
        printStringSet("prim", primStrings);
        printStringSet("mesh", meshStrings);
    }
};

// Copy topology and numeric columns from a string mesh. The string pool stays behind.
// Numeric columns convert when the scalars differ. Int columns are copied as int.
template <typename Dst, typename Src>
void copyNumeric(ntmesh<Dst, WithAttribs<Dst>>& dst, const ntmesh<Src, WithStrAttribs<Src>>& src) {
    ntmesh<Dst, WithAttribs<Dst>> out;
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
    if constexpr (std::is_same<Dst, Src>::value) {
        static_cast<WithAttribs<Dst>&>(out).attrClone(src);
    } else {
        detail::setConvert(out.pointAttribs, src.pointAttribs);
        detail::setConvert(out.vertexAttribs, src.vertexAttribs);
        detail::setConvert(out.primAttribs, src.primAttribs);
        detail::setConvert(out.meshAttribs, src.meshAttribs);
        out.pointN = src.pointN;
        out.vertN = src.vertN;
        out.primN = src.primN;
    }

    dst.clear();
    dst.takeTopology(out);
    static_cast<WithAttribs<Dst>&>(dst) = static_cast<WithAttribs<Dst>&>(out);
    static_cast<WithAttribs<Dst>&>(out) = WithAttribs<Dst>{};
}

// Deep copy across scalars, including the string pool. Numeric columns convert from Src to Dst.
template <typename Dst, typename Src, typename = std::enable_if_t<!std::is_same<Dst, Src>::value>>
void copyMesh(ntmesh<Dst, WithStrAttribs<Dst>>& dst, const ntmesh<Src, WithStrAttribs<Src>>& src) {
    ntmesh<Dst, WithStrAttribs<Dst>> out;
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
    detail::setConvert(out.pointAttribs, src.pointAttribs);
    detail::setConvert(out.vertexAttribs, src.vertexAttribs);
    detail::setConvert(out.primAttribs, src.primAttribs);
    detail::setConvert(out.meshAttribs, src.meshAttribs);
    out.pointN = src.pointN;
    out.vertN = src.vertN;
    out.primN = src.primN;
    out.pool = src.pool;
    cloneStringSet(out.pointStrings, src.pointStrings);
    cloneStringSet(out.vertexStrings, src.vertexStrings);
    cloneStringSet(out.primStrings, src.primStrings);
    cloneStringSet(out.meshStrings, src.meshStrings);

    dst.clear();
    dst.takeTopology(out);
    static_cast<WithStrAttribs<Dst>&>(dst) = static_cast<WithStrAttribs<Dst>&>(out);
    static_cast<WithStrAttribs<Dst>&>(out) = WithStrAttribs<Dst>{};
}

} // namespace ntm
