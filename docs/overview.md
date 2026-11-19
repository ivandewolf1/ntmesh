# Overview

**ntmesh** is a header-only polygonal mesh. The position type is a template parameter, so the same calls build an `ntmesh<float>` or an `ntmesh<double>`. An optional `NTL_HD` prefix keeps the topology and numeric-attribute headers usable from CUDA. It is meant for geometry processing: triangles, quads, n-gons, and open polylines, with attributes and file IO kept in extra headers.

## Philosophy

**Utility over checking.** There is little error checking. Feed the mesh good data. A failed allocation leaves the existing block in place and does not pretend the buffer grew. The next write of a slot that was not allocated is the fault.

The mesh has no destructor. Call `clear()` when you are done with it. A plain copy shares the pointers. `copyMesh` is the deep copy.

## Headers

| Header | Mesh type | Role |
|--------|-----------|------|
| [`ntmesh.h`](ntmesh.md) | `ntm::ntmesh<T>` | Points and the four prim lists |
| [`ntmesh_attribs.h`](ntmesh_attribs.md) | `ntm::ntmesh<T, ntm::WithAttribs>` | Numeric columns on points, vertices, prims, and the mesh |
| [`ntmesh_strattr.h`](ntmesh_strattr.md) | `ntm::ntmesh<T, ntm::WithStrAttribs>` | Those columns plus host-only strings |
| [`ntmesh_file.h`](ntmesh_file.md) | any of the three | ASCII `.obj` and ASCII `.geo` |
| [`ntmesh_bw_tets.h`](ntmesh_bw_tets.md) | `ntm::TetMesh<T>` | Bowyer–Watson tets of a point array |

`ntmesh.h` needs `nvec3.h` and `nmat4.h` from ntv on the include path. The attribute headers include the mesh header. A translation unit that only needs topology includes `ntmesh.h` and nothing else. `ntmesh_file.h` is host-only: do not include it from a `.cu`.

**CUDA:** before including, `#define NTL_HD __host__ __device__`. Left undefined, it is empty and the headers stay host-only. Accessors are marked with it. Allocation, `merge`, `trim`, `transform`, triangulation, and file IO stay on the host.

## Layout

Points are `ntv::nvec3<T>`. Corner indices are `int`, 0-based. Each prim kind keeps its own index list:

- A triangle is three indices, `[f*3, f*3+3)`, with no offset table.
- A quad is four indices, `[f*4, f*4+4)`.
- An n-gon is a closed polygon. Face `f` uses `[ngonOffsets[f], ngonOffsets[f+1])`.
- A line is an open polyline. Line `f` uses `[lineOffsets[f], lineOffsets[f+1])`.

`primVertex` walks one combined order: triangles, then quads, then n-gons, then lines. Vertex and prim attributes follow that same order.

## Ownership

Arrays live in `Buf<T>`: a pointer, a count, and a capacity.

| Call | What it does |
|------|----------------|
| plain copy | Shares every pointer |
| `clear()` | Frees the buffers. Call it once for a set of shared copies |
| `copyMesh` | New buffers. Same attribute policy. Positions convert from the source scalar to the destination scalar |
| `copyTopo` | Points and the four prim lists only. The destination is cleared first, so its attributes are dropped |
| `copyNumeric` | Points, prim lists, and numeric columns. The string pool stays behind |

`takeTopology` moves the point buffer and the six index buffers out of another mesh and leaves that mesh's buffers empty. `copyMesh` and `copyTopo` use it so the temporary owns the new allocations until the destination takes them. The attribute policy is copied separately.

## Winding

Clockwise, seen from the outside, is the front face.

`newell` returns the raw front vector. Its length is twice the planar area of a planar polygon. The unit triangle `(0,0,0), (1,0,0), (0,1,0)` has `newell` and `faceNormal` `(0,0,-1)`. `totalVolume` is positive for that winding. A line returns a zero normal and adds nothing to area or volume.

## What stays out

Groups, binary `.bgeo`, `.bgeo.sc`, STL, and USD are not in this library. The tet header builds a Delaunay tetrahedralization of a point set. It does not tetrahedralize the interior of a closed polygon mesh.

See the [README](../README.md) for the build and the sample files.
