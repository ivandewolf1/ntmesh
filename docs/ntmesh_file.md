# Files

ASCII Wavefront `.obj` and ASCII Houdini `.geo`. Header: [`src/ntmesh_file.h`](../src/ntmesh_file.h).

```cpp
#include "ntmesh_file.h"

ntm::readFile(mesh, "box.obj");
ntm::writeFile(mesh, "box.geo");
```

Host only. Include it from a host translation unit, not from a `.cu`. It works on a plain mesh, a `WithAttribs<T>` mesh, and a `WithStrAttribs<T>` mesh. A plain `ntmesh<T>` still reads the points and the faces. Numeric columns are read and written for `WithAttribs<T>`. String columns are read and written only for `WithStrAttribs<T>`.

`readFile` and `writeFile` pick the format from the suffix, compared without case. Anything else returns 0. Both return 1 on success and 0 on failure.

Binary `.bgeo` and `.bgeo.sc` are not read here. The `.geo` schema is the published text JSON layout.

---

## `readFile(mesh, path, separate = true)`

The `separate` flag is the same filing rule as `addFace`. Left on, a 3-corner face becomes a triangle and a 4-corner face becomes a quad. Pass `false` to put every closed face on the n-gon list, in file order. A 2-corner face is a line either way.

The mesh is replaced. A failed read leaves it cleared.

### `.obj`

`v` lines become points. `f` lines become closed faces. `l` lines become open polylines. Corner indices may be negative, counting back from the latest point, texture vertex, or normal. `vn` and `vt` are loaded only onto an attributed mesh:

| File record | Column |
|-------------|--------|
| `vn` | Point attribute `N`, kind `Normal`, tuple size 3, when a corner carries a normal index |
| `vt` | Vertex attribute `uv`, tuple size 2 or 3 to match the file |

A corner with no texture or normal index stores zero in that slot. Other `.obj` statements are skipped.

### `.geo`

The point buffer is column `P` on the point domain. Closed faces are a `Polygon_run`. An open polyline is a `PolygonCurve_run`, and is read back as a line. Numeric columns on the point, vertex, primitive, and detail domains are loaded with their storage and semantic:

| Semantic | `Type` |
|----------|--------|
| `point` | `Position` |
| `normal` | `Normal` |
| `color` | `Color` |
| `vector`, `texturecoord`, or anything else | `Float` |
| integer storage | `Int` |

`P` is the point buffer, so it is not also stored as a point attribute. String columns become string attributes when the mesh has a pool. Dictionary columns are skipped.

---

## `writeFile(mesh, path)`

### `.obj`

Points are `v` lines, printed with `%.9g`. Closed faces are `f`. Lines are `l`. Indices are 1-based.

On an attributed mesh, a vertex `N` of kind `Normal` and tuple size at least 3 is written as `vn`, one per corner. If that column is absent, a point `N` of the same shape is written, one per point. A vertex `uv` that is not an `int` and has tuple size at least 2 is written as `vt`. If that column is absent, a point `uv` of the same shape is written. Corners then use the `v/vt/vn` form that matches which of those columns were written. Other attributes are not written.

### `.geo`

One text JSON document. Topology is polygon runs: a closed face is `Polygon_run`, an open polyline is `PolygonCurve_run`. The point buffer is written as `P`, `fpreal64` when `T` is `double` and `fpreal32` otherwise. Numeric columns use that same storage. A file of either storage is cast into `T` on read. String columns are written only for `WithStrAttribs<T>`. A point column named `P` is not written a second time.

```cpp
ntm::ntmesh<float, ntm::WithAttribs<float>> mesh;
if (!ntm::readFile(mesh, "samplegeo/light_bunny.geo")) { /* failed */ }
ntm::writeFile(mesh, "out.obj");
mesh.clear();
```
