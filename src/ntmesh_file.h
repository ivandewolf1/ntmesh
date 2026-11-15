// ASCII .obj and .geo for ntmesh.
// Host only. Include this from a host translation unit, not from a .cu file.
// .geo is Houdini's text JSON geometry. Binary .bgeo and .bgeo.sc are not read here.
// The geometry schema is implemented from the published layout, not from SideFX sources.


#pragma once
#include "ntmesh_strattr.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ntm {

template <typename T, typename Attr>
struct IoTraits {
    static constexpr bool numeric = std::is_base_of<WithAttribs, Attr>::value;
    static constexpr bool strings = std::is_base_of<WithStrAttribs, Attr>::value;
};

namespace io {

enum class JKind { Null, Bool, Num, Str, Arr, Obj };
enum class Domain { Point, Vertex, Prim, Detail };

struct JVal {
    JKind kind = JKind::Null;
    bool b = false;
    bool isInt = false;
    long long ival = 0;
    double num = 0;
    std::string str;
    std::vector<JVal> arr;
    std::vector<std::pair<std::string, JVal>> obj;
};

inline JVal jbool(bool v) { JVal x; x.kind = JKind::Bool; x.b = v; return x; }
inline JVal jint(long long v) {
    JVal x; x.kind = JKind::Num; x.isInt = true; x.ival = v; x.num = (double)v; return x;
}
inline JVal jnum(double v) { JVal x; x.kind = JKind::Num; x.num = v; return x; }
inline JVal jstr(const std::string& s) { JVal x; x.kind = JKind::Str; x.str = s; return x; }
inline JVal jarr() { JVal x; x.kind = JKind::Arr; return x; }
inline JVal jobj() { JVal x; x.kind = JKind::Obj; return x; }

inline void jpair(JVal& a, const char* key, JVal v) {
    a.arr.push_back(jstr(key));
    a.arr.push_back(std::move(v));
}
inline void jkey(JVal& o, const char* key, JVal v) { o.obj.emplace_back(key, std::move(v)); }

inline const JVal* jget(const JVal& v, const char* key) {
    if (v.kind == JKind::Obj) {
        for (const auto& kv : v.obj)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    if (v.kind == JKind::Arr) {
        for (size_t i = 0; i + 1 < v.arr.size(); i += 2)
            if (v.arr[i].kind == JKind::Str && v.arr[i].str == key) return &v.arr[i + 1];
    }
    return nullptr;
}
inline const char* jstrOf(const JVal* v) {
    return (v && v->kind == JKind::Str) ? v->str.c_str() : nullptr;
}
inline int jintOf(const JVal& v, int fallback = 0) {
    if (v.kind != JKind::Num) return fallback;
    return v.isInt ? (int)v.ival : (int)v.num;
}
inline double jnumOf(const JVal& v) {
    if (v.kind != JKind::Num) return 0;
    return v.isInt ? (double)v.ival : v.num;
}

struct Parser {
    const char* s = nullptr;
    size_t i = 0, n = 0;
    int failed = 0;

    void skip() {
        while (i < n && std::isspace((unsigned char)s[i])) ++i;
    }
    int eat(char c) {
        skip();
        if (i < n && s[i] == c) { ++i; return 1; }
        failed = 1;
        return 0;
    }
    int starts(const char* lit) {
        size_t k = std::strlen(lit);
        if (i + k > n || std::memcmp(s + i, lit, k) != 0) return 0;
        i += k;
        return 1;
    }
    int hex4() {
        int v = 0;
        for (int k = 0; k < 4; ++k) {
            if (i >= n) { failed = 1; return 0; }
            unsigned char c = (unsigned char)s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v += c - '0';
            else if (c >= 'a' && c <= 'f') v += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v += c - 'A' + 10;
            else { failed = 1; return 0; }
        }
        return v;
    }
    void utf8(std::string& out, int cp) {
        if (cp < 0x80) out.push_back((char)cp);
        else if (cp < 0x800) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }
    JVal parseString() {
        JVal v = jstr("");
        ++i;
        while (i < n) {
            char c = s[i++];
            if (c == '"') return v;
            if (c != '\\') { v.str.push_back(c); continue; }
            if (i >= n) { failed = 1; return v; }
            char e = s[i++];
            if (e == '"' || e == '\\' || e == '/') v.str.push_back(e);
            else if (e == 'b') v.str.push_back('\b');
            else if (e == 'f') v.str.push_back('\f');
            else if (e == 'n') v.str.push_back('\n');
            else if (e == 'r') v.str.push_back('\r');
            else if (e == 't') v.str.push_back('\t');
            else if (e == 'u') utf8(v.str, hex4());
            else v.str.push_back(e);
        }
        failed = 1;
        return v;
    }
    JVal parseNumber() {
        size_t start = i;
        if (s[i] == '-') ++i;
        while (i < n && std::isdigit((unsigned char)s[i])) ++i;
        int isFloat = 0;
        if (i < n && s[i] == '.') {
            isFloat = 1;
            ++i;
            while (i < n && std::isdigit((unsigned char)s[i])) ++i;
        }
        if (i < n && (s[i] == 'e' || s[i] == 'E')) {
            isFloat = 1;
            ++i;
            if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
            while (i < n && std::isdigit((unsigned char)s[i])) ++i;
        }
        std::string tok(s + start, s + i);
        if (!isFloat) {
            char* end = nullptr;
            long long iv = std::strtoll(tok.c_str(), &end, 10);
            if (end && *end == 0) return jint(iv);
        }
        return jnum(std::strtod(tok.c_str(), nullptr));
    }
    JVal parseValue() {
        skip();
        if (i >= n) { failed = 1; return JVal(); }
        char c = s[i];
        if (c == '"') return parseString();
        if (c == '-' || std::isdigit((unsigned char)c)) return parseNumber();
        if (c == '[') {
            JVal a = jarr();
            ++i;
            skip();
            if (i < n && s[i] == ']') { ++i; return a; }
            while (!failed) {
                a.arr.push_back(parseValue());
                skip();
                if (i < n && s[i] == ',') { ++i; continue; }
                if (i < n && s[i] == ']') { ++i; break; }
                failed = 1;
            }
            return a;
        }
        if (c == '{') {
            JVal o = jobj();
            ++i;
            skip();
            if (i < n && s[i] == '}') { ++i; return o; }
            while (!failed) {
                skip();
                if (i >= n || s[i] != '"') { failed = 1; break; }
                JVal key = parseString();
                if (!eat(':')) break;
                o.obj.emplace_back(std::move(key.str), parseValue());
                skip();
                if (i < n && s[i] == ',') { ++i; continue; }
                if (i < n && s[i] == '}') { ++i; break; }
                failed = 1;
            }
            return o;
        }
        if (starts("true")) return jbool(true);
        if (starts("false")) return jbool(false);
        if (starts("null")) return JVal();
        failed = 1;
        return JVal();
    }
};

inline int writeEscaped(FILE* fp, const std::string& s) {
    if (std::fputc('"', fp) == EOF) return 0;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            if (std::fprintf(fp, "\\%c", c) < 0) return 0;
        } else if (c == '\n') { if (std::fputs("\\n", fp) == EOF) return 0; }
        else if (c == '\r') { if (std::fputs("\\r", fp) == EOF) return 0; }
        else if (c == '\t') { if (std::fputs("\\t", fp) == EOF) return 0; }
        else if (c < 0x20) { if (std::fprintf(fp, "\\u%04x", c) < 0) return 0; }
        else if (std::fputc(c, fp) == EOF) return 0;
    }
    return std::fputc('"', fp) != EOF;
}
inline void indent(FILE* fp, int n) { for (int i = 0; i < n; ++i) std::fputs("  ", fp); }

inline void writeJson(FILE* fp, const JVal& v, int ind) {
    switch (v.kind) {
    case JKind::Null: std::fputs("null", fp); break;
    case JKind::Bool: std::fputs(v.b ? "true" : "false", fp); break;
    case JKind::Num:
        if (v.isInt) std::fprintf(fp, "%lld", v.ival);
        else std::fprintf(fp, "%.17g", v.num);
        break;
    case JKind::Str: writeEscaped(fp, v.str); break;
    case JKind::Arr:
        if (v.arr.empty()) { std::fputs("[]", fp); break; }
        std::fputs("[\n", fp);
        for (size_t i = 0; i < v.arr.size(); ++i) {
            indent(fp, ind + 1);
            writeJson(fp, v.arr[i], ind + 1);
            if (i + 1 != v.arr.size()) std::fputc(',', fp);
            std::fputc('\n', fp);
        }
        indent(fp, ind);
        std::fputc(']', fp);
        break;
    case JKind::Obj:
        if (v.obj.empty()) { std::fputs("{}", fp); break; }
        std::fputs("{\n", fp);
        for (size_t i = 0; i < v.obj.size(); ++i) {
            indent(fp, ind + 1);
            writeEscaped(fp, v.obj[i].first);
            std::fputs(": ", fp);
            writeJson(fp, v.obj[i].second, ind + 1);
            if (i + 1 != v.obj.size()) std::fputc(',', fp);
            std::fputc('\n', fp);
        }
        indent(fp, ind);
        std::fputc('}', fp);
        break;
    }
}

inline int loadText(const char* path, std::string& out) {
    FILE* fp = std::fopen(path, "rb");
    if (!fp) return 0;
    if (std::fseek(fp, 0, SEEK_END) != 0) { std::fclose(fp); return 0; }
    long n = std::ftell(fp);
    if (n < 0) { std::fclose(fp); return 0; }
    std::rewind(fp);
    out.resize((size_t)n);
    size_t got = n ? std::fread(&out[0], 1, (size_t)n, fp) : 0;
    std::fclose(fp);
    if (got != (size_t)n) return 0;
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB &&
        (unsigned char)out[2] == 0xBF)
        out.erase(0, 3);
    return 1;
}

inline int eqIc(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return 0;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}
inline const char* extensionOf(const char* path) {
    const char* dot = nullptr;
    const char* slash = nullptr;
    for (const char* p = path; p && *p; ++p) {
        if (*p == '/' || *p == '\\') slash = p;
        if (*p == '.') dot = p;
    }
    if (!dot || (slash && dot < slash)) return "";
    return dot;
}

inline const char* typeOption(const JVal& def) {
    const JVal* options = jget(def, "options");
    if (!options) return nullptr;
    const JVal* type = jget(*options, "type");
    if (!type) return nullptr;
    return jstrOf(jget(*type, "value"));
}
inline Type kindFromGeo(const char* storage, const char* semantic) {
    int asInt = storage && (std::strncmp(storage, "int", 3) == 0 || std::strncmp(storage, "uint", 4) == 0);
    if (asInt) return Type::Int;
    if (semantic && std::strcmp(semantic, "point") == 0) return Type::Position;
    if (semantic && std::strcmp(semantic, "normal") == 0) return Type::Normal;
    if (semantic && std::strcmp(semantic, "color") == 0) return Type::Color;
    return Type::Float;
}
inline const char* semanticOf(Type kind) {
    if (kind == Type::Position) return "point";
    if (kind == Type::Normal) return "normal";
    if (kind == Type::Color) return "color";
    return nullptr;
}

struct PrimRec {
    int closed = 1;
    int filePrim = 0;
    int fileVert0 = 0;
    std::vector<int> pts;
};

inline int pointAt(const std::vector<int>& indices, int vertex, int pointCount) {
    int pt = vertex;
    if (!indices.empty()) {
        if (vertex < 0 || vertex >= (int)indices.size()) return -1;
        pt = indices[(size_t)vertex];
    }
    if (pt < 0 || pt >= pointCount) return -1;
    return pt;
}
inline int closedFlag(const JVal& data) {
    const JVal* c = jget(data, "closed");
    if (!c) return 1;
    if (c->kind == JKind::Bool) return c->b ? 1 : 0;
    if (c->kind == JKind::Num) return jnumOf(*c) != 0;
    return 1;
}
inline void addRun(std::vector<PrimRec>& prims, const std::vector<int>& indices, int pointCount,
                   int start, const JVal& rle, int closed, int& filePrim, int& fileVert) {
    int cursor = start;
    for (size_t i = 0; i + 1 < rle.arr.size(); i += 2) {
        int nv = jintOf(rle.arr[i]);
        int count = jintOf(rle.arr[i + 1]);
        if (nv < 0) nv = 0;
        if (count < 0) count = 0;
        for (int c = 0; c < count; ++c) {
            PrimRec rec;
            rec.closed = closed;
            rec.filePrim = filePrim++;
            rec.fileVert0 = cursor;
            for (int k = 0; k < nv; ++k) {
                int pt = pointAt(indices, cursor + k, pointCount);
                if (pt >= 0) rec.pts.push_back(pt);
            }
            cursor += nv;
            if (cursor > fileVert) fileVert = cursor;
            prims.push_back(std::move(rec));
        }
    }
}
inline void addExplicit(std::vector<PrimRec>& prims, const std::vector<int>& indices, int pointCount,
                        const JVal& verts, int closed, int& filePrim, int& fileVert) {
    PrimRec rec;
    rec.closed = closed;
    rec.filePrim = filePrim++;
    rec.fileVert0 = fileVert;
    if (verts.kind == JKind::Arr) {
        for (const JVal& v : verts.arr) {
            int id = jintOf(v, -1);
            int pt = pointAt(indices, id, pointCount);
            if (pt >= 0) rec.pts.push_back(pt);
            ++fileVert;
        }
    } else if (verts.kind == JKind::Num) {
        int pt = pointAt(indices, jintOf(verts, -1), pointCount);
        if (pt >= 0) rec.pts.push_back(pt);
        ++fileVert;
    }
    prims.push_back(std::move(rec));
}

template <typename Mesh>
void addRecorded(Mesh& mesh, const PrimRec& rec, std::vector<int>& vertMap, std::vector<int>& primMap, bool separate) {
    int n = (int)rec.pts.size();
    int triC = mesh.triangleCount(), quadC = mesh.quadCount(), ngonC = mesh.ngonCount(), lineC = mesh.lineCount();
    int triV = mesh.triangleIndices.count, quadV = mesh.quadIndices.count;
    int ngonV = mesh.ngonIndices.count, lineV = mesh.lineIndices.count;
    const int* pts = rec.pts.empty() ? nullptr : rec.pts.data();
    int meshPrim = 0;
    int base = 0;
    if (!rec.closed || n < 3) {
        mesh.addLine(pts, n);
        meshPrim = triC + quadC + ngonC + lineC;
        base = triV + quadV + ngonV + lineV;
    } else if (separate && n == 3) {
        mesh.addFace(pts, 3, true);
        meshPrim = triC;
        base = triC * 3;
    } else if (separate && n == 4) {
        mesh.addFace(pts, 4, true);
        meshPrim = triC + quadC;
        base = triV + quadC * 4;
    } else {
        mesh.addFace(pts, n, false);
        meshPrim = triC + quadC + ngonC;
        base = triV + quadV + ngonV;
    }
    if (rec.filePrim >= 0 && rec.filePrim < (int)primMap.size()) primMap[(size_t)rec.filePrim] = meshPrim;
    for (int k = 0; k < n; ++k) {
        int src = rec.fileVert0 + k;
        if (src >= 0 && src < (int)vertMap.size()) vertMap[(size_t)src] = base + k;
    }
}

template <typename Mesh>
void addBuckets(Mesh& mesh, const std::vector<PrimRec>& prims, std::vector<int>& vertMap, std::vector<int>& primMap,
                bool separate) {
    if (separate) {
        for (const PrimRec& rec : prims)
            if (rec.closed && rec.pts.size() == 3) addRecorded(mesh, rec, vertMap, primMap, true);
        for (const PrimRec& rec : prims)
            if (rec.closed && rec.pts.size() == 4) addRecorded(mesh, rec, vertMap, primMap, true);
        for (const PrimRec& rec : prims)
            if (rec.closed && rec.pts.size() > 4) addRecorded(mesh, rec, vertMap, primMap, true);
    } else {
        for (const PrimRec& rec : prims)
            if (rec.closed && rec.pts.size() >= 3) addRecorded(mesh, rec, vertMap, primMap, false);
    }
    for (const PrimRec& rec : prims)
        if (!rec.closed || rec.pts.size() < 3) addRecorded(mesh, rec, vertMap, primMap, separate);
}

inline int mapIndex(const std::vector<int>& map, int i) {
    if (map.empty()) return i;
    if (i < 0 || i >= (int)map.size()) return -1;
    return map[(size_t)i];
}

struct PackedRef {
    const JVal* node = nullptr;
    int soa = 0;
};
inline PackedRef findPacked(const JVal& block) {
    PackedRef p;
    const JVal* values = jget(block, "values");
    const JVal* home = values ? values : &block;
    p.node = jget(*home, "tuples");
    if (p.node) return p;
    p.node = jget(*home, "arrays");
    if (p.node) { p.soa = 1; return p; }
    p.node = jget(*home, "rawpagedata");
    if (p.node) return p;
    const JVal* indices = jget(block, "indices");
    if (!indices) return p;
    p.node = jget(*indices, "tuples");
    if (p.node) return p;
    p.node = jget(*indices, "arrays");
    if (p.node) p.soa = 1;
    else p.node = nullptr;
    return p;
}
inline int tupleSizeOf(const JVal& block) {
    if (const JVal* sz = jget(block, "size")) return jintOf(*sz, 1);
    const JVal* values = jget(block, "values");
    if (values) if (const JVal* sz = jget(*values, "size")) return jintOf(*sz, 1);
    const JVal* indices = jget(block, "indices");
    if (indices) if (const JVal* sz = jget(*indices, "size")) return jintOf(*sz, 1);
    return 1;
}
inline std::vector<double> flattenNumbers(const JVal& packed, int tupleSize, int soa) {
    std::vector<double> out;
    if (packed.kind != JKind::Arr || tupleSize < 1) return out;
    if (soa) {
        if (packed.arr.empty()) return out;
        if (tupleSize == 1 && packed.arr.size() == 1 && packed.arr[0].kind == JKind::Arr) {
            for (const JVal& c : packed.arr[0].arr) out.push_back(jnumOf(c));
            return out;
        }
        size_t n = 0;
        for (const JVal& col : packed.arr)
            if (col.kind == JKind::Arr && col.arr.size() > n) n = col.arr.size();
        out.assign(n * (size_t)tupleSize, 0);
        int cols = (int)packed.arr.size();
        if (cols > tupleSize) cols = tupleSize;
        for (int c = 0; c < cols; ++c) {
            if (packed.arr[(size_t)c].kind != JKind::Arr) continue;
            const std::vector<JVal>& col = packed.arr[(size_t)c].arr;
            for (size_t r = 0; r < col.size() && r < n; ++r)
                out[r * (size_t)tupleSize + (size_t)c] = jnumOf(col[r]);
        }
        return out;
    }
    for (const JVal& row : packed.arr) {
        if (row.kind == JKind::Num) out.push_back(jnumOf(row));
        else if (row.kind == JKind::Arr)
            for (const JVal& c : row.arr) out.push_back(jnumOf(c));
    }
    return out;
}

template <typename T, typename Attr>
int meshVertIndex(const ntmesh<T, Attr>& mesh, int f, int k) {
    int nt = mesh.triangleCount();
    if (f < nt) return f * 3 + k;
    f -= nt;
    int base = mesh.triangleIndices.count;
    int nq = mesh.quadCount();
    if (f < nq) return base + f * 4 + k;
    f -= nq;
    base += mesh.quadIndices.count;
    int nn = mesh.ngonCount();
    if (f < nn) return base + mesh.ngonOffsets[f] + k;
    f -= nn;
    base += mesh.ngonIndices.count;
    return base + mesh.lineOffsets[f] + k;
}

inline void pushRun(std::vector<int>& rle, int nverts) {
    if (rle.size() >= 2 && rle[rle.size() - 2] == nverts) rle.back() += 1;
    else { rle.push_back(nverts); rle.push_back(1); }
}

inline JVal typeOptions(const char* semantic) {
    JVal opt = jobj();
    if (!semantic) return opt;
    JVal inner = jobj();
    jkey(inner, "type", jstr("string"));
    jkey(inner, "value", jstr(semantic));
    jkey(opt, "type", std::move(inner));
    return opt;
}

inline JVal numericColumn(const char* name, Type kind, int tupleSize, const std::vector<double>& values,
                           const char* storageIn = nullptr) {
    JVal entry = jarr();
    JVal def = jarr();
    jpair(def, "scope", jstr("public"));
    jpair(def, "type", jstr("numeric"));
    jpair(def, "name", jstr(name));
    jpair(def, "options", typeOptions(semanticOf(kind)));
    JVal body = jarr();
    const char* storage = storageIn ? storageIn : (kind == Type::Int ? "int32" : "fpreal32");
    jpair(body, "size", jint(tupleSize));
    jpair(body, "storage", jstr(storage));
    JVal valuesNode = jarr();
    jpair(valuesNode, "size", jint(tupleSize));
    jpair(valuesNode, "storage", jstr(storage));
    JVal tuples = jarr();
    int n = tupleSize > 0 ? (int)values.size() / tupleSize : 0;
    for (int i = 0; i < n; ++i) {
        JVal row = jarr();
        for (int c = 0; c < tupleSize; ++c) {
            double x = values[(size_t)i * (size_t)tupleSize + (size_t)c];
            if (kind == Type::Int) row.arr.push_back(jint((long long)x));
            else row.arr.push_back(jnum(x));
        }
        tuples.arr.push_back(std::move(row));
    }
    jpair(valuesNode, "tuples", std::move(tuples));
    jpair(body, "values", std::move(valuesNode));
    entry.arr.push_back(std::move(def));
    entry.arr.push_back(std::move(body));
    return entry;
}

inline void collectColumn(const Attribute& col, std::vector<double>& values) {
    int n = col.count;
    int w = col.tupleSize < 1 ? 1 : col.tupleSize;
    values.assign((size_t)n * (size_t)w, 0);
    for (int i = 0; i < n; ++i) {
        if (col.kind == Type::Int) {
            const int* slot = col.intAt(i);
            for (int c = 0; c < w; ++c) values[(size_t)i * (size_t)w + (size_t)c] = slot[c];
        } else {
            const float* slot = col.floatAt(i);
            for (int c = 0; c < w; ++c) values[(size_t)i * (size_t)w + (size_t)c] = slot[c];
        }
    }
}

inline void appendColumns(JVal& list, const AttributeSet& set, int skipP) {
    for (int i = 0; i < set.count; ++i) {
        const Attribute& col = set.data[i];
        if (skipP && std::strcmp(col.name, "P") == 0) continue;
        std::vector<double> values;
        collectColumn(col, values);
        list.arr.push_back(numericColumn(col.name, col.kind, col.tupleSize < 1 ? 1 : col.tupleSize, values));
    }
}

inline void appendStrings(JVal& list, const StringSet& set, const std::vector<std::string>& pool) {
    for (int i = 0; i < set.count; ++i) {
        const StringAttribute& col = set.data[i];
        JVal entry = jarr();
        JVal def = jarr();
        jpair(def, "scope", jstr("public"));
        jpair(def, "type", jstr("string"));
        jpair(def, "name", jstr(col.name));
        jpair(def, "options", jobj());
        JVal body = jarr();
        jpair(body, "size", jint(1));
        JVal strings = jarr();
        if (pool.empty()) strings.arr.push_back(jstr(""));
        else for (const std::string& s : pool) strings.arr.push_back(jstr(s));
        jpair(body, "strings", std::move(strings));
        JVal indices = jarr();
        jpair(indices, "size", jint(1));
        jpair(indices, "storage", jstr("int32"));
        JVal tuples = jarr();
        for (int e = 0; e < col.indices.count; ++e) {
            JVal row = jarr();
            row.arr.push_back(jint(col.indices[e]));
            tuples.arr.push_back(std::move(row));
        }
        jpair(indices, "tuples", std::move(tuples));
        jpair(body, "indices", std::move(indices));
        entry.arr.push_back(std::move(def));
        entry.arr.push_back(std::move(body));
        list.arr.push_back(std::move(entry));
    }
}

template <typename T, typename Attr>
JVal positionColumn(const ntmesh<T, Attr>& mesh) {
    std::vector<double> values((size_t)mesh.pointCount() * 3);
    for (int i = 0; i < mesh.pointCount(); ++i) {
        values[(size_t)i * 3 + 0] = (double)mesh.points[i][0];
        values[(size_t)i * 3 + 1] = (double)mesh.points[i][1];
        values[(size_t)i * 3 + 2] = (double)mesh.points[i][2];
    }
    const char* storage = sizeof(T) == sizeof(double) ? "fpreal64" : "fpreal32";
    return numericColumn("P", Type::Position, 3, values, storage);
}

template <typename T, typename Attr>
void readPoints(ntmesh<T, Attr>& mesh, const JVal& packed, int tupleSize, int soa) {
    int w = tupleSize < 1 ? 1 : tupleSize;
    std::vector<double> flat = flattenNumbers(packed, w, soa);
    int n = (int)flat.size() / w;
    for (int i = 0; i < n; ++i) {
        T x = (T)flat[(size_t)i * (size_t)w + 0];
        T y = w > 1 ? (T)flat[(size_t)i * (size_t)w + 1] : (T)0;
        T z = w > 2 ? (T)flat[(size_t)i * (size_t)w + 2] : (T)0;
        mesh.addPoint(ntv::nvec3<T>(x, y, z));
    }
}

} // namespace io

template <typename T, typename Attr>
void loadAttribList(ntmesh<T, Attr>& mesh, const io::JVal* list, io::Domain domain,
                    const std::vector<int>& vertMap, const std::vector<int>& primMap) {
    if (!list || list->kind != io::JKind::Arr) return;
    for (const io::JVal& item : list->arr) {
        if (item.kind != io::JKind::Arr || item.arr.size() < 2) continue;
        const io::JVal& def = item.arr[0];
        const io::JVal& body = item.arr[1];
        const char* name = io::jstrOf(io::jget(def, "name"));
        const char* type = io::jstrOf(io::jget(def, "type"));
        if (!name || !type) continue;
        if (std::strcmp(type, "string") == 0) {
            if constexpr (IoTraits<T, Attr>::strings) {
                const io::JVal* table = io::jget(body, "strings");
                io::PackedRef packed = io::findPacked(body);
                if (!table || table->kind != io::JKind::Arr || !packed.node) continue;
                std::vector<std::string> words;
                for (const io::JVal& s : table->arr)
                    words.push_back(s.kind == io::JKind::Str ? s.str : std::string());
                int tuple = io::tupleSizeOf(body);
                if (tuple < 1) tuple = 1;
                std::vector<double> ids = io::flattenNumbers(*packed.node, tuple, packed.soa);
                int n = (int)ids.size() / tuple;
                StringAttribute* col = nullptr;
                if (domain == io::Domain::Point) col = mesh.addPointString(name);
                else if (domain == io::Domain::Vertex) col = mesh.addVertexString(name);
                else if (domain == io::Domain::Prim) col = mesh.addFaceString(name);
                else col = mesh.addMeshString(name);
                if (!col) continue;
                for (int i = 0; i < n; ++i) {
                    int dst = i;
                    if (domain == io::Domain::Vertex) dst = io::mapIndex(vertMap, i);
                    else if (domain == io::Domain::Prim) dst = io::mapIndex(primMap, i);
                    if (dst < 0 || dst >= col->indices.count) continue;
                    int id = (int)ids[(size_t)i * (size_t)tuple];
                    const char* text = (id >= 0 && id < (int)words.size()) ? words[(size_t)id].c_str() : "";
                    mesh.setString(*col, dst, text);
                }
            }
            continue;
        }
        if (std::strcmp(type, "numeric") != 0) continue;
        if constexpr (IoTraits<T, Attr>::numeric) {
            if (domain == io::Domain::Point && std::strcmp(name, "P") == 0) continue;
            const char* storage = io::jstrOf(io::jget(body, "storage"));
            Type kind = io::kindFromGeo(storage, io::typeOption(def));
            int tuple = io::tupleSizeOf(body);
            if (tuple < 1) tuple = 1;
            io::PackedRef packed = io::findPacked(body);
            if (!packed.node) continue;
            std::vector<double> values = io::flattenNumbers(*packed.node, tuple, packed.soa);
            Attribute* col = nullptr;
            if (domain == io::Domain::Point) col = mesh.addPointAttrib(name, kind, tuple);
            else if (domain == io::Domain::Vertex) col = mesh.addVertexAttrib(name, kind, tuple);
            else if (domain == io::Domain::Prim) col = mesh.addFaceAttrib(name, kind, tuple);
            else col = mesh.addMeshAttrib(name, kind, tuple);
            if (!col) continue;
            int n = tuple > 0 ? (int)values.size() / tuple : 0;
            int width = col->tupleSize < tuple ? col->tupleSize : tuple;
            if (width < 1) continue;
            for (int i = 0; i < n; ++i) {
                int dst = i;
                if (domain == io::Domain::Vertex) dst = io::mapIndex(vertMap, i);
                else if (domain == io::Domain::Prim) dst = io::mapIndex(primMap, i);
                if (dst < 0 || dst >= col->count) continue;
                if (col->kind == Type::Int) {
                    int* slot = col->intAt(dst);
                    for (int c = 0; c < width; ++c)
                        slot[c] = (int)values[(size_t)i * (size_t)tuple + (size_t)c];
                } else {
                    float* slot = col->floatAt(dst);
                    for (int c = 0; c < width; ++c)
                        slot[c] = (float)values[(size_t)i * (size_t)tuple + (size_t)c];
                }
            }
        }
    }
}

inline void skipCounts(const io::JVal& rle, int& filePrim, int& fileVert, int start) {
    int cursor = start;
    for (size_t i = 0; i + 1 < rle.arr.size(); i += 2) {
        int nv = io::jintOf(rle.arr[i]);
        int count = io::jintOf(rle.arr[i + 1]);
        if (nv < 0) nv = 0;
        if (count < 0) count = 0;
        filePrim += count;
        cursor += nv * count;
    }
    if (cursor > fileVert) fileVert = cursor;
}

template <typename T, typename Attr>
int readGeo(ntmesh<T, Attr>& mesh, const char* path, bool separate = true) {
    mesh.clear();
    std::string text;
    if (!io::loadText(path, text)) return 0;
    io::Parser parser;
    parser.s = text.data();
    parser.n = text.size();
    io::JVal root = parser.parseValue();
    if (parser.failed) { mesh.clear(); return 0; }

    const io::JVal* attributes = io::jget(root, "attributes");
    const io::JVal* pointAttrs = attributes ? io::jget(*attributes, "pointattributes") : nullptr;
    int foundP = 0;
    if (pointAttrs && pointAttrs->kind == io::JKind::Arr) {
        for (const io::JVal& item : pointAttrs->arr) {
            if (item.kind != io::JKind::Arr || item.arr.size() < 2) continue;
            const char* name = io::jstrOf(io::jget(item.arr[0], "name"));
            if (!name || std::strcmp(name, "P") != 0) continue;
            io::PackedRef packed = io::findPacked(item.arr[1]);
            if (!packed.node) break;
            io::readPoints(mesh, *packed.node, io::tupleSizeOf(item.arr[1]), packed.soa);
            foundP = 1;
            break;
        }
    }
    if (!foundP) { mesh.clear(); return 0; }

    std::vector<int> indices;
    if (const io::JVal* topo = io::jget(root, "topology")) {
        if (const io::JVal* pref = io::jget(*topo, "pointref")) {
            if (const io::JVal* ind = io::jget(*pref, "indices")) {
                if (ind->kind == io::JKind::Arr) {
                    for (const io::JVal& v : ind->arr) {
                        if (v.kind == io::JKind::Num) indices.push_back(io::jintOf(v, -1));
                        else if (v.kind == io::JKind::Arr)
                            for (const io::JVal& e : v.arr)
                                if (e.kind == io::JKind::Num) indices.push_back(io::jintOf(e, -1));
                    }
                }
            }
        }
    }

    std::vector<io::PrimRec> prims;
    int filePrim = 0;
    int fileVert = 0;
    if (const io::JVal* primList = io::jget(root, "primitives")) {
        if (primList->kind == io::JKind::Arr) {
            for (const io::JVal& prim : primList->arr) {
                const io::JVal* data = &prim;
                const io::JVal* typeNode = io::jget(prim, "type");
                if (!typeNode && prim.kind == io::JKind::Arr && prim.arr.size() >= 2 &&
                    prim.arr[0].kind != io::JKind::Str) {
                    typeNode = io::jget(prim.arr[0], "type");
                    data = &prim.arr[1];
                }
                const char* type = io::jstrOf(typeNode);
                if (!type) continue;
                int curve = std::strcmp(type, "PolygonCurve_run") == 0;
                int known = curve || std::strcmp(type, "Polygon_run") == 0 || std::strcmp(type, "Polygon") == 0 ||
                            std::strcmp(type, "Poly") == 0;
                int closed = curve ? 0 : io::closedFlag(*data);
                const io::JVal* rle = io::jget(*data, "nvertices_rle");
                const io::JVal* nverts = io::jget(*data, "nvertices");
                const io::JVal* verts = io::jget(*data, "vertex");
                if (!verts) verts = io::jget(*data, "vertices");
                const io::JVal* startNode = io::jget(*data, "startvertex");
                int start = startNode ? io::jintOf(*startNode, fileVert) : fileVert;
                if (rle && rle->kind == io::JKind::Arr) {
                    if (known) io::addRun(prims, indices, mesh.pointCount(), start, *rle, closed, filePrim, fileVert);
                    else skipCounts(*rle, filePrim, fileVert, start);
                } else if (nverts && (nverts->kind == io::JKind::Num || nverts->kind == io::JKind::Arr)) {
                    io::JVal fake = io::jarr();
                    if (nverts->kind == io::JKind::Num) {
                        fake.arr.push_back(io::jint(io::jintOf(*nverts)));
                        fake.arr.push_back(io::jint(1));
                    } else {
                        for (const io::JVal& c : nverts->arr) {
                            fake.arr.push_back(io::jint(io::jintOf(c)));
                            fake.arr.push_back(io::jint(1));
                        }
                    }
                    if (known) io::addRun(prims, indices, mesh.pointCount(), start, fake, closed, filePrim, fileVert);
                    else skipCounts(fake, filePrim, fileVert, start);
                } else if (verts) {
                    if (known) io::addExplicit(prims, indices, mesh.pointCount(), *verts, closed, filePrim, fileVert);
                    else {
                        ++filePrim;
                        int n = verts->kind == io::JKind::Arr ? (int)verts->arr.size() : 1;
                        fileVert += n;
                    }
                }
            }
        }
    }

    std::vector<int> vertMap((size_t)fileVert, -1);
    std::vector<int> primMap((size_t)filePrim, -1);
    io::addBuckets(mesh, prims, vertMap, primMap, separate);
    if (attributes) {
        loadAttribList(mesh, io::jget(*attributes, "pointattributes"), io::Domain::Point, vertMap, primMap);
        loadAttribList(mesh, io::jget(*attributes, "vertexattributes"), io::Domain::Vertex, vertMap, primMap);
        loadAttribList(mesh, io::jget(*attributes, "primitiveattributes"), io::Domain::Prim, vertMap, primMap);
        loadAttribList(mesh, io::jget(*attributes, "globalattributes"), io::Domain::Detail, vertMap, primMap);
    }
    return 1;
}

template <typename T, typename Attr>
int writeGeo(const ntmesh<T, Attr>& mesh, const char* path) {
    FILE* fp = std::fopen(path, "wb");
    if (!fp) return 0;
    io::JVal root = io::jarr();
    io::jpair(root, "fileversion", io::jstr("20.0.0"));
    io::jpair(root, "hasindex", io::jbool(false));
    io::jpair(root, "pointcount", io::jint(mesh.pointCount()));
    io::jpair(root, "vertexcount", io::jint(mesh.vertexCount()));
    io::jpair(root, "primitivecount", io::jint(mesh.primCount()));

    io::JVal info = io::jobj();
    io::jkey(info, "software", io::jstr("ntmesh"));
    if (mesh.pointCount() > 0) {
        T xmin = mesh.points[0][0], xmax = xmin;
        T ymin = mesh.points[0][1], ymax = ymin;
        T zmin = mesh.points[0][2], zmax = zmin;
        for (int i = 1; i < mesh.pointCount(); ++i) {
            if (mesh.points[i][0] < xmin) xmin = mesh.points[i][0];
            if (mesh.points[i][0] > xmax) xmax = mesh.points[i][0];
            if (mesh.points[i][1] < ymin) ymin = mesh.points[i][1];
            if (mesh.points[i][1] > ymax) ymax = mesh.points[i][1];
            if (mesh.points[i][2] < zmin) zmin = mesh.points[i][2];
            if (mesh.points[i][2] > zmax) zmax = mesh.points[i][2];
        }
        io::JVal bounds = io::jarr();
        bounds.arr.push_back(io::jnum((double)xmin));
        bounds.arr.push_back(io::jnum((double)ymin));
        bounds.arr.push_back(io::jnum((double)zmin));
        bounds.arr.push_back(io::jnum((double)xmax));
        bounds.arr.push_back(io::jnum((double)ymax));
        bounds.arr.push_back(io::jnum((double)zmax));
        io::jkey(info, "bounds", std::move(bounds));
    }
    io::jpair(root, "info", std::move(info));

    io::JVal indices = io::jarr();
    for (int f = 0; f < mesh.primCount(); ++f) {
        int n = mesh.primVertexCount(f);
        for (int k = 0; k < n; ++k) indices.arr.push_back(io::jint(mesh.primVertex(f, k)));
    }
    io::JVal pref = io::jarr();
    io::jpair(pref, "indices", std::move(indices));
    io::JVal topo = io::jarr();
    io::jpair(topo, "pointref", std::move(pref));
    io::jpair(root, "topology", std::move(topo));

    io::JVal pointList = io::jarr();
    pointList.arr.push_back(io::positionColumn(mesh));
    io::JVal vertList = io::jarr();
    io::JVal faceList = io::jarr();
    io::JVal detailList = io::jarr();
    if constexpr (IoTraits<T, Attr>::numeric) {
        io::appendColumns(pointList, mesh.pointAttribs, 1);
        io::appendColumns(vertList, mesh.vertexAttribs, 0);
        io::appendColumns(faceList, mesh.primAttribs, 0);
        io::appendColumns(detailList, mesh.meshAttribs, 0);
    }
    if constexpr (IoTraits<T, Attr>::strings) {
        io::appendStrings(pointList, mesh.pointStrings, mesh.pool);
        io::appendStrings(vertList, mesh.vertexStrings, mesh.pool);
        io::appendStrings(faceList, mesh.primStrings, mesh.pool);
        io::appendStrings(detailList, mesh.meshStrings, mesh.pool);
    }
    io::JVal attrs = io::jarr();
    io::jpair(attrs, "pointattributes", std::move(pointList));
    if (!vertList.arr.empty()) io::jpair(attrs, "vertexattributes", std::move(vertList));
    if (!faceList.arr.empty()) io::jpair(attrs, "primitiveattributes", std::move(faceList));
    if (!detailList.arr.empty()) io::jpair(attrs, "globalattributes", std::move(detailList));
    io::jpair(root, "attributes", std::move(attrs));

    int nclosed = mesh.triangleCount() + mesh.quadCount() + mesh.ngonCount();
    std::vector<int> closedRle;
    std::vector<int> openRle;
    for (int f = 0; f < mesh.primCount(); ++f) {
        int nv = mesh.primVertexCount(f);
        if (f < nclosed) io::pushRun(closedRle, nv);
        else io::pushRun(openRle, nv);
    }
    auto makeRun = [](int start, const std::vector<int>& rle, int closed) {
        io::JVal prim = io::jarr();
        io::jpair(prim, "type", io::jstr("Polygon_run"));
        io::jpair(prim, "startvertex", io::jint(start));
        io::JVal run = io::jarr();
        for (int v : rle) run.arr.push_back(io::jint(v));
        io::jpair(prim, "nvertices_rle", std::move(run));
        if (!closed) io::jpair(prim, "closed", io::jbool(false));
        return prim;
    };
    int closedVerts = mesh.triangleIndices.count + mesh.quadIndices.count + mesh.ngonIndices.count;
    io::JVal primitives = io::jarr();
    if (nclosed > 0) primitives.arr.push_back(makeRun(0, closedRle, 1));
    if (mesh.lineCount() > 0) primitives.arr.push_back(makeRun(closedVerts, openRle, 0));
    io::jpair(root, "primitives", std::move(primitives));

    io::writeJson(fp, root, 0);
    std::fputc('\n', fp);
    int ok = std::ferror(fp) == 0;
    std::fclose(fp);
    return ok ? 1 : 0;
}

inline int objIndex(int idx, int count) {
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx;
    return -1;
}

inline int parseObjCorner(const char*& p, int nV, int nVt, int nVn, int& v, int& vt, int& vn) {
    v = vt = vn = -1;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == 0 || *p == '\n' || *p == '\r' || *p == '#') return 0;
    char* end = nullptr;
    long iv = std::strtol(p, &end, 10);
    if (end == p) return 0;
    p = end;
    v = objIndex((int)iv, nV);
    if (*p == '/') {
        ++p;
        if (*p != '/') {
            long it = std::strtol(p, &end, 10);
            if (end != p) {
                vt = objIndex((int)it, nVt);
                p = end;
            }
        }
        if (*p == '/') {
            ++p;
            long in = std::strtol(p, &end, 10);
            if (end != p) {
                vn = objIndex((int)in, nVn);
                p = end;
            }
        }
    }
    return v >= 0;
}

template <typename T, typename Attr>
int readObj(ntmesh<T, Attr>& mesh, const char* path, bool separate = true) {
    mesh.clear();
    std::string text;
    if (!io::loadText(path, text)) return 0;
    std::vector<ntv::nvec3<double>> pos;
    std::vector<ntv::nvec3<double>> nrm;
    std::vector<double> uv;
    int uvWidth = 2;
    struct Face {
        int closed = 1;
        std::vector<int> p, vt, vn;
    };
    std::vector<Face> faces;
    const char* s = text.c_str();
    while (*s) {
        while (*s == ' ' || *s == '\t') ++s;
        const char* line = s;
        while (*s && *s != '\n' && *s != '\r') ++s;
        std::string row(line, s);
        if (*s == '\r') ++s;
        if (*s == '\n') ++s;
        if (row.empty() || row[0] == '#') continue;
        if (row.size() >= 2 && row[0] == 'v' && row[1] == ' ') {
            double x = 0, y = 0, z = 0;
            std::sscanf(row.c_str() + 2, "%lf %lf %lf", &x, &y, &z);
            pos.push_back(ntv::nvec3<double>(x, y, z));
        } else if (row.size() >= 3 && row[0] == 'v' && row[1] == 'n' && std::isspace((unsigned char)row[2])) {
            double x = 0, y = 0, z = 0;
            std::sscanf(row.c_str() + 3, "%lf %lf %lf", &x, &y, &z);
            nrm.push_back(ntv::nvec3<double>(x, y, z));
        } else if (row.size() >= 3 && row[0] == 'v' && row[1] == 't' && std::isspace((unsigned char)row[2])) {
            double u = 0, v = 0, w = 0;
            int got = std::sscanf(row.c_str() + 3, "%lf %lf %lf", &u, &v, &w);
            if (got >= 3) uvWidth = 3;
            uv.push_back(u);
            uv.push_back(v);
            uv.push_back(w);
        } else if (row.size() >= 2 && (row[0] == 'f' || row[0] == 'l') && std::isspace((unsigned char)row[1])) {
            Face face;
            face.closed = row[0] == 'f';
            const char* p = row.c_str() + 2;
            int v = -1, vt = -1, vn = -1;
            while (parseObjCorner(p, (int)pos.size(), (int)(uv.size() / 3), (int)nrm.size(), v, vt, vn)) {
                face.p.push_back(v);
                face.vt.push_back(vt);
                face.vn.push_back(vn);
            }
            if (!face.p.empty()) faces.push_back(std::move(face));
        }
    }
    for (const ntv::nvec3<double>& p : pos) mesh.addPoint(ntv::nvec3<T>((T)p[0], (T)p[1], (T)p[2]));

    std::vector<io::PrimRec> recs;
    int filePrim = 0;
    int fileVert = 0;
    for (const Face& face : faces) {
        io::PrimRec rec;
        rec.closed = face.closed;
        rec.filePrim = filePrim++;
        rec.fileVert0 = fileVert;
        rec.pts = face.p;
        fileVert += (int)face.p.size();
        recs.push_back(std::move(rec));
    }
    std::vector<int> vertMap((size_t)fileVert, -1);
    std::vector<int> primMap((size_t)filePrim, -1);
    io::addBuckets(mesh, recs, vertMap, primMap, separate);

    if constexpr (IoTraits<T, Attr>::numeric) {
        int anyN = 0;
        int pointN = 1;
        int anyUv = 0;
        int pointUv = 1;
        for (const Face& face : faces) {
            for (size_t k = 0; k < face.p.size(); ++k) {
                if (face.vn[k] >= 0) {
                    anyN = 1;
                    if (face.vn[k] != face.p[k]) pointN = 0;
                } else pointN = 0;
                if (face.vt[k] >= 0) {
                    anyUv = 1;
                    if (face.vt[k] != face.p[k]) pointUv = 0;
                } else pointUv = 0;
            }
        }
        if ((int)nrm.size() != mesh.pointCount()) pointN = 0;
        if ((int)(uv.size() / 3) != mesh.pointCount()) pointUv = 0;
        if (anyN && pointN) {
            Attribute* N = mesh.addPointAttrib("N", Type::Normal, 3);
            for (int i = 0; i < mesh.pointCount() && i < (int)nrm.size(); ++i) {
                N->floatAt(i)[0] = (float)nrm[(size_t)i][0];
                N->floatAt(i)[1] = (float)nrm[(size_t)i][1];
                N->floatAt(i)[2] = (float)nrm[(size_t)i][2];
            }
        } else if (anyN) {
            Attribute* N = mesh.addVertexAttrib("N", Type::Normal, 3);
            for (size_t fi = 0; fi < faces.size(); ++fi) {
                const Face& face = faces[fi];
                for (size_t k = 0; k < face.vn.size(); ++k) {
                    int dst = io::mapIndex(vertMap, recs[fi].fileVert0 + (int)k);
                    int src = face.vn[k];
                    if (dst < 0 || src < 0 || src >= (int)nrm.size()) continue;
                    N->floatAt(dst)[0] = (float)nrm[(size_t)src][0];
                    N->floatAt(dst)[1] = (float)nrm[(size_t)src][1];
                    N->floatAt(dst)[2] = (float)nrm[(size_t)src][2];
                }
            }
        }
        if (anyUv && pointUv) {
            Attribute* tex = mesh.addPointAttrib("uv", Type::Float, uvWidth);
            for (int i = 0; i < mesh.pointCount() && i < (int)(uv.size() / 3); ++i) {
                tex->floatAt(i)[0] = (float)uv[(size_t)i * 3 + 0];
                tex->floatAt(i)[1] = (float)uv[(size_t)i * 3 + 1];
                if (uvWidth > 2) tex->floatAt(i)[2] = (float)uv[(size_t)i * 3 + 2];
            }
        } else if (anyUv) {
            Attribute* tex = mesh.addVertexAttrib("uv", Type::Float, uvWidth);
            for (size_t fi = 0; fi < faces.size(); ++fi) {
                const Face& face = faces[fi];
                for (size_t k = 0; k < face.vt.size(); ++k) {
                    int dst = io::mapIndex(vertMap, recs[fi].fileVert0 + (int)k);
                    int src = face.vt[k];
                    if (dst < 0 || src < 0 || src >= (int)(uv.size() / 3)) continue;
                    tex->floatAt(dst)[0] = (float)uv[(size_t)src * 3 + 0];
                    tex->floatAt(dst)[1] = (float)uv[(size_t)src * 3 + 1];
                    if (uvWidth > 2) tex->floatAt(dst)[2] = (float)uv[(size_t)src * 3 + 2];
                }
            }
        }
    }
    return 1;
}

template <typename T, typename Attr>
int writeObj(const ntmesh<T, Attr>& mesh, const char* path) {
    FILE* fp = std::fopen(path, "wb");
    if (!fp) return 0;
    for (int i = 0; i < mesh.pointCount(); ++i) {
        std::fprintf(fp, "v %.9g %.9g %.9g\n",
                     (double)mesh.points[i][0], (double)mesh.points[i][1], (double)mesh.points[i][2]);
    }
    const Attribute* pointN = nullptr;
    const Attribute* vertN = nullptr;
    const Attribute* pointUv = nullptr;
    const Attribute* vertUv = nullptr;
    if constexpr (IoTraits<T, Attr>::numeric) {
        const Attribute* vn = mesh.findVertexAttrib("N");
        if (vn && vn->kind == Type::Normal && vn->tupleSize >= 3 && vn->count >= mesh.vertexCount() && vn->data)
            vertN = vn;
        const Attribute* pn = mesh.findPointAttrib("N");
        if (!vertN && pn && pn->kind == Type::Normal && pn->tupleSize >= 3 && pn->count >= mesh.pointCount() && pn->data)
            pointN = pn;
        const Attribute* vu = mesh.findVertexAttrib("uv");
        if (vu && vu->kind != Type::Int && vu->tupleSize >= 2 && vu->count >= mesh.vertexCount() && vu->data)
            vertUv = vu;
        const Attribute* pu = mesh.findPointAttrib("uv");
        if (!vertUv && pu && pu->kind != Type::Int && pu->tupleSize >= 2 && pu->count >= mesh.pointCount() && pu->data)
            pointUv = pu;
    }
    if (vertN) {
        for (int i = 0; i < mesh.vertexCount(); ++i) {
            const float* s = vertN->floatAt(i);
            std::fprintf(fp, "vn %.9g %.9g %.9g\n", (double)s[0], (double)s[1], (double)s[2]);
        }
    } else if (pointN) {
        for (int i = 0; i < mesh.pointCount(); ++i) {
            const float* s = pointN->floatAt(i);
            std::fprintf(fp, "vn %.9g %.9g %.9g\n", (double)s[0], (double)s[1], (double)s[2]);
        }
    }
    const Attribute* uvCol = vertUv ? vertUv : pointUv;
    int uvN = vertUv ? mesh.vertexCount() : mesh.pointCount();
    if (uvCol) {
        for (int i = 0; i < uvN; ++i) {
            const float* s = uvCol->floatAt(i);
            if (uvCol->tupleSize >= 3)
                std::fprintf(fp, "vt %.9g %.9g %.9g\n", (double)s[0], (double)s[1], (double)s[2]);
            else
                std::fprintf(fp, "vt %.9g %.9g\n", (double)s[0], (double)s[1]);
        }
    }
    int nclosed = mesh.triangleCount() + mesh.quadCount() + mesh.ngonCount();
    for (int f = 0; f < mesh.primCount(); ++f) {
        std::fprintf(fp, "%c", f < nclosed ? 'f' : 'l');
        int n = mesh.primVertexCount(f);
        for (int k = 0; k < n; ++k) {
            int pi = mesh.primVertex(f, k) + 1;
            int vi = io::meshVertIndex(mesh, f, k) + 1;
            if (uvCol || vertN || pointN) {
                std::fprintf(fp, " %d", pi);
                std::fputc('/', fp);
                if (vertUv) std::fprintf(fp, "%d", vi);
                else if (pointUv) std::fprintf(fp, "%d", pi);
                if (vertN) std::fprintf(fp, "/%d", vi);
                else if (pointN) std::fprintf(fp, "/%d", pi);
            } else {
                std::fprintf(fp, " %d", pi);
            }
        }
        std::fputc('\n', fp);
    }
    int ok = std::ferror(fp) == 0;
    std::fclose(fp);
    return ok ? 1 : 0;
}

template <typename T, typename Attr>
int readFile(ntmesh<T, Attr>& mesh, const char* path, bool separate = true) {
    const char* ext = io::extensionOf(path);
    if (io::eqIc(ext, ".obj")) return readObj(mesh, path, separate);
    if (io::eqIc(ext, ".geo")) return readGeo(mesh, path, separate);
    return 0;
}

template <typename T, typename Attr>
int writeFile(const ntmesh<T, Attr>& mesh, const char* path) {
    const char* ext = io::extensionOf(path);
    if (io::eqIc(ext, ".obj")) return writeObj(mesh, path);
    if (io::eqIc(ext, ".geo")) return writeGeo(mesh, path);
    return 0;
}

} // namespace ntm
