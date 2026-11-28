# ntmesh

Polygonal mesh of points, triangles, quads, n-gons, and open polylines. Header: [`src/ntmesh.h`](../src/ntmesh.h).

```cpp
#include "ntmesh.h"

ntm::ntmesh<float> mesh;
int a = mesh.addPoint(ntv::nvec3<float>(0, 0, 0));
int b = mesh.addPoint(ntv::nvec3<float>(1, 0, 0));
int c = mesh.addPoint(ntv::nvec3<float>(0, 1, 0));
int tri[] = {a, b, c};
mesh.addFace(tri, 3);
mesh.clear();
```

Needs `nvec3.h` and `nmat4.h` from ntv. No attribute or file code is pulled in. For columns see [attributes](ntmesh_attribs.md). For strings see [strings](ntmesh_strattr.md). Conventions are in the [overview](overview.md).

The second template argument defaults to `None`, an empty attribute policy. `ntmesh<T>` and `ntmesh<T, None>` are the same type.

---

## Storage

### `Buf<T>`

| Field | Meaning |
|-------|---------|
| `data` | Pointer. Null until the first allocation |
| `count` | Live elements |
| `capacity` | Allocated elements |
| `operator[]` | Element `i`. No range check |
| `size()` | `count` |

`bufReserve(b, cap)` grows `capacity` to at least `cap`. A failed `realloc` leaves `data` and `capacity` as they were. `bufFit(b, n)` sets `count` to `n` and default-constructs the new slots. If the capacity is still short of `n`, `count` stays put. `bufFree` frees the block and zeroes the three fields. `bufClone` frees `dst` and copies `src` into a new block.

The mesh members are:

| Member | Contents |
|--------|----------|
| `points` | `nvec3<T>` |
| `triangleIndices` | 3 indices per triangle |
| `quadIndices` | 4 indices per quad |
| `ngonIndices`, `ngonOffsets` | Closed polygons |
| `lineIndices`, `lineOffsets` | Open polylines |

A plain copy shares those pointers. There is no destructor.

---

## Counts and corners

All of these are `NTL_HD`.

| API | Meaning |
|-----|---------|
| `pointCount()` | `points.count` |
| `triangleCount()` / `quadCount()` | Packed faces |
| `ngonCount()` / `lineCount()` | `offsets.count - 1`, or 0 when the offset buffer is empty |
| `primCount()` | Sum of the four lists |
| `vertexCount()` | Sum of the corner counts |
| `triangleVertex(f, k)` / `quadVertex(f, k)` | Corner `k` of that packed face |
| `ngonVertex(f, k)` / `ngonVertexCount(f)` | One n-gon |
| `lineVertex(f, k)` / `lineVertexCount(f)` | One polyline |
| `primVertex(f, k)` / `primVertexCount(f)` | Corner `k` of prim `f` in combined order |
| `bounds(mn, mx)` | Axis-aligned box of the point buffer. An empty mesh leaves `mn` and `mx` unchanged |

```cpp
mesh.triangleVertex(0, 2);   // third corner of the first triangle
mesh.ngonVertex(0, 4);       // fifth corner of the first n-gon
mesh.primVertex(f, k);       // corner k of prim f, across all four lists
```

Combined prim order is triangles, then quads, then n-gons, then lines. `primVertex` on a line walks the polyline.

---

## Filling

### `addPoint(p)`

Appends `p` and returns its index. Point attributes grow with the point count.

### `addFace(corners, n, separate = true)`

Files one prim by corner count.

| `n` | `separate == true` | `separate == false` |
|-----|--------------------|---------------------|
| 2 | line | line |
| 3 | triangle | n-gon |
| 4 | quad | n-gon |
| any other | n-gon | n-gon |

A negative `n` is treated as 0, which is an empty n-gon. Vertex and prim attributes are inserted at the new prim's place in the combined order.

### `addLine(points, n)`

Appends an open polyline of any length, including a length other than 2. Corners land after the polygon corners.

### `reserve(npoints, nverts, nprims)`

Grows capacity for points, every index buffer, and the attribute policy. Live counts stay as they are. A failed growth leaves the old capacity.

### `trim(npoints, ntris, nquads, nngons, nlines)`

Sets the live size of each list and drops the attribute elements that belonged to the tail. Spare capacity stays.

### `merge(src)`

Appends `src`. Corner indices are rebased by the destination point count. Attributes of the same name and kind are copied onto the new elements. A mesh-level column that already exists on the destination keeps the destination value.

### `clear()`

Frees every buffer and the attribute policy.

### `print()`

Host `printf` summary: counts, then the attribute policy, then each prim's corners. The header can be included from a `.cu`. Do not call `print` from the device.

```cpp
mesh.reserve(1000, 3000, 1000);
mesh.merge(other);
mesh.trim(mesh.pointCount(), mesh.triangleCount(), 0, 0, 0);
mesh.clear();
```

---

## Topology edits

### `triangulate()`

Replaces every quad and n-gon with triangles. Existing triangles stay first. Lines stay. Each new triangle keeps the source winding. Vertex and prim attributes are copied from the source corner and the source prim.

Returns `true` when the commit ran. That includes a mesh that was already triangles, a face that was fanned because ear clipping did not find an ear, and an n-gon with fewer than three corners, which is dropped because it cannot become a triangle. Returns `false` when the scratch arrays are missing. In that case the quads, n-gons, and attributes are unchanged.

### `triangulateNgons()`

Same rules for n-gons only. Quads, existing triangles, and lines stay.

```cpp
if (!mesh.triangulate()) { /* quads and n-gons are still there */ }
```

### `reverse()`

Reverses the corner order of every closed face and every line. Vertex attributes move with their corners.

### `removeUnusedPoints()`

Deletes points that no corner references, compacts the point buffer, and remaps every index list. Point attributes move with the survivors. If the remap table cannot be allocated, the mesh is unchanged.

---

## Measures

`totalSurfaceArea` and `totalVolume` are `NTL_HD`. Each closed face is the fan from its first corner. Lines add nothing.

| API | Meaning |
|-----|---------|
| `totalSurfaceArea()` | Sum of the fan areas |
| `totalVolume()` | Signed volume of those fans. Positive when faces wind clockwise from the outside. Meaningful when `isClosed()` is true |
| `isClosed()` | `1` when every directed edge of a triangle, quad, or n-gon appears exactly once, the two ends differ, and the opposite directed edge exists. Lines are ignored. An empty mesh, or a failed edge allocation, returns `0` |
| `newell(corners, n)` | Raw front vector of a corner span. Length is twice the planar area. Fewer than three corners, or a missing point buffer, returns zero |
| `faceNormal(f)` | `newell` of prim `f`, made unit length. A line, a degenerate face, and an out-of-range prim return zero |

```cpp
ntv::nvec3<float> n = mesh.faceNormal(0);
float vol = mesh.totalVolume();
```

---

## Transform and copies

### `transform(m)`

Host only. Each point becomes `m * point`, the ntv affine product, so translation is applied. The attribute policy then updates its columns. On a plain mesh that is the whole call.

### `copyMesh(dst, src)`

Deep copy. Point components convert from `Src` to `Dst`. `dst` is cleared first. The same policy copies its columns as bytes. `WithAttribs<Src>` onto `WithAttribs<Dst>`, and `WithStrAttribs<Src>` onto `WithStrAttribs<Dst>`, converts each numeric component. `Int` columns are copied as `int`.

### `copyTopo(dst, src)`

Points and the four prim lists only. The policies may differ. `dst` is cleared first, so any attributes it held are dropped.

### `takeTopology(src)`

Moves the point buffer and the six index buffers out of `src`, and leaves those fields empty on `src`. The attribute policy is not moved. The copy helpers use this so the temporary owns the new allocations until the destination takes them.

```cpp
ntm::ntmesh<double> cpu;
ntm::ntmesh<float>  gpu;
ntm::copyMesh(gpu, cpu);

ntv::nmat4<float> m;
m.setTranslation(ntv::nvec3<float>(1, 0, 0));
gpu.transform(m);
```
