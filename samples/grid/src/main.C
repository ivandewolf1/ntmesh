// 4x4 grid of mixed_manifold_bunny.geo on the XZ plane, Y up, centered on the origin.
// plain.geo is topology. attrib.geo is the same placement with point N, vertex uv, and primitive Cd.
// The negative-X columns yaw about Y. The positive-X columns use a random axis and angle.

#include <cmath>
#include <cstdio>
#include <string>
#include "ntmesh_file.h"

namespace {

struct Rng {
    unsigned state;
    float next() {
        state = state * 1664525u + 1013904223u;
        return (state >> 8) * (1.f / 16777216.f);
    }
};

bool columnsPresent(const ntm::ntmesh<float, ntm::WithAttribs>& mesh) {
    const ntm::Attribute* N = mesh.findPointAttrib("N");
    const ntm::Attribute* uv = mesh.findVertexAttrib("uv");
    const ntm::Attribute* Cd = mesh.findFaceAttrib("Cd");
    if (!N || N->kind != ntm::Type::Normal || N->tupleSize < 3) return false;
    if (!uv || uv->isInt() || uv->tupleSize < 2) return false;
    if (!Cd || Cd->kind != ntm::Type::Color || Cd->tupleSize < 3) return false;
    return true;
}

template <typename Mesh>
int writeGrid(const Mesh& src, const char* path) {
    ntv::nvec3<float> mn, mx;
    src.bounds(mn, mx);
    ntv::nvec3<float> d = mx - mn;
    float diag = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    const float kMaxScale = 1.15f;
    float pitch = diag * kMaxScale * 1.35f;
    float jitter = pitch * 0.04f;
    const float pi = 3.14159265358979323846f;

    Mesh out;
    Rng rng{1984u};
    for (int i = 0; i < 4; ++i) {
        for (int k = 0; k < 4; ++k) {
            float s = 0.85f + rng.next() * (kMaxScale - 0.85f);
            float angle = rng.next() * (2.f * pi);
            ntv::nvec3<float> axis(0.f, 1.f, 0.f);
            if (i >= 2) {
                float z = rng.next() * 2.f - 1.f;
                float az = rng.next() * (2.f * pi);
                float r = std::sqrt(std::max(0.f, 1.f - z * z));
                axis = ntv::nvec3<float>(r * std::cos(az), r * std::sin(az), z);
            }
            float jx = (rng.next() * 2.f - 1.f) * jitter;
            float jz = (rng.next() * 2.f - 1.f) * jitter;

            ntv::nmat4<float> m;
            m.setRotation(axis, angle);
            m.scale(ntv::nvec3<float>(s, s, s));
            m.translate(ntv::nvec3<float>((i - 1.5f) * pitch + jx, 0.f, (k - 1.5f) * pitch + jz));

            Mesh copy;
            ntm::copyMesh(copy, src);
            copy.transform(m);
            out.merge(copy);
            copy.clear();
        }
    }

    int ok = ntm::writeFile(out, path);
    std::printf("%s points %d prims %d -> %s\n", ok ? "wrote" : "failed", out.pointCount(), out.primCount(), path);
    out.clear();
    return ok;
}

} // namespace

int main() {
    ntm::ntmesh<float> plain;
    if (!ntm::readFile(plain, BUNNY_GEO)) {
        std::fprintf(stderr, "could not read %s\n", BUNNY_GEO);
        return 1;
    }
    ntm::ntmesh<float, ntm::WithAttribs> attrib;
    if (!ntm::readFile(attrib, BUNNY_GEO)) {
        std::fprintf(stderr, "could not read %s\n", BUNNY_GEO);
        plain.clear();
        return 1;
    }
    if (!columnsPresent(attrib)) {
        std::fprintf(stderr, "missing point N, vertex uv, or primitive Cd\n");
        plain.clear();
        attrib.clear();
        return 1;
    }

    std::string plainPath = std::string(OUT_DIR) + "/plain.geo";
    std::string attribPath = std::string(OUT_DIR) + "/attrib.geo";
    int ok = writeGrid(plain, plainPath.c_str()) && writeGrid(attrib, attribPath.c_str());
    plain.clear();
    attrib.clear();
    return ok ? 0 : 1;
}
