# Delaunay tets

Bowyer–Watson tetrahedralization of a point array. Header: [`src/ntmesh_bw_tets.h`](../src/ntmesh_bw_tets.h).

```cpp
#include "ntmesh_bw_tets.h"

ntm::TetMesh<float> tets;
tets.build(points, n);
ntv::nvec3<float> p = tets.position(points, tets.tets[0].v[0]);
tets.clear();
```

Needs `nvec3.h`. It does not include `ntmesh.h` and it does not read a mesh. Pass the point array you already have. The build is host-only. `position` is marked `NTL_HD`.

This fills a `TetMesh`. A later mesher can fill the same result. It is not a tetrahedralization of the interior of a polygon mesh.

---

## `Tet`

| Field | Meaning |
|-------|---------|
| `v[0..3]` | The four corner indices |
| `n[0..3]` | The neighbor across the face opposite that corner, or `-1` when the face is on the hull |

Indices below `pointCount` refer to the caller's array, in the same order. The next eight indices are the bounding-cube corners stored on the result, so every cell stays finite. `v[i] < pointCount` is one of the input points. `v[i] - pointCount` selects `corner[]`.

---

## `TetMesh<T>`

| Field | Meaning |
|-------|---------|
| `tets` | The live array. Null when `count` is 0 |
| `count` | Number of tets |
| `pointCount` | How many of the indices belong to the caller. The eight cube corners follow |
| `corner[8]` | Cube corners in the result's scalar type |

| API | Meaning |
|-----|---------|
| `build(pts, n)` | Replace the result with the tets of `pts[0..n)`. A null `pts` or `n <= 0` clears and returns |
| `clear()` | Frees `tets` and zeroes `count` and `pointCount` |
| `position(pts, i)` | `pts[i]` when `i < pointCount`, otherwise `corner[i - pointCount]` |

`build` copies the input into a temporary `double` array, translates that copy near the origin, and frees it before returning. Predicates run in `double`. The seed cube is slightly larger than the bounding box. Exact duplicate coordinates are inserted. A point that lands exactly on a sphere does not by itself delete that tet. If the cavity boundary is not a ball, face-adjacent tets with an insphere result of zero are added, and a vertex dropped by that step is inserted again.

A failed allocation calls `clear()` and returns an empty mesh. Call `clear()` when you are finished either way. A plain copy shares `tets`.

```cpp
for (int i = 0; i < tets.count; ++i) {
    const ntm::Tet& tet = tets.tets[i];
    for (int k = 0; k < 4; ++k) {
        int nb = tet.n[k];
        ntv::nvec3<float> q = tets.position(pts, tet.v[k]);
        (void)nb;
        (void)q;
    }
}
```
