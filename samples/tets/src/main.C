// Tet edges of light_bunny.geo, including the eight cube corners.
// Each edge is written once.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "ntmesh_bw_tets.h"
#include "ntmesh_file.h"

namespace {

struct Edge {
    int a;
    int b;
    bool operator<(const Edge& o) const { return a < o.a || (a == o.a && b < o.b); }
    bool operator==(const Edge& o) const { return a == o.a && b == o.b; }
};

} // namespace

int main() {
    ntm::ntmesh<float> bunny;
    if (!ntm::readFile(bunny, LIGHT_BUNNY)) {
        std::fprintf(stderr, "could not read %s\n", LIGHT_BUNNY);
        return 1;
    }

    ntm::TetMesh<float> tets;
    tets.build(bunny.points.data, bunny.pointCount());
    if (tets.count <= 0) {
        std::fprintf(stderr, "tet build produced no tets\n");
        bunny.clear();
        tets.clear();
        return 1;
    }

    ntm::ntmesh<float> lines;
    for (int i = 0; i < bunny.pointCount(); ++i) lines.addPoint(bunny.points[i]);
    for (int i = 0; i < 8; ++i) lines.addPoint(tets.corner[i]);

    int npts = lines.pointCount();
    std::vector<Edge> edges;
    edges.reserve((size_t)tets.count * 6);
    const int pair[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
    for (int t = 0; t < tets.count; ++t) {
        const ntm::Tet& tet = tets.tets[t];
        for (int e = 0; e < 6; ++e) {
            int a = tet.v[pair[e][0]];
            int b = tet.v[pair[e][1]];
            if (a < 0 || b < 0 || a >= npts || b >= npts || a == b) continue;
            if (a > b) {
                int tmp = a;
                a = b;
                b = tmp;
            }
            edges.push_back(Edge{a, b});
        }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    for (const Edge& e : edges) {
        int corner[2] = {e.a, e.b};
        lines.addLine(corner, 2);
    }

    std::string path = std::string(OUT_DIR) + "/edges.geo";
    int ok = ntm::writeFile(lines, path.c_str());
    std::printf("%s points %d lines %d tets %d -> %s\n", ok ? "wrote" : "failed", lines.pointCount(),
                lines.lineCount(), tets.count, path.c_str());
    lines.clear();
    bunny.clear();
    tets.clear();
    return ok ? 0 : 1;
}
