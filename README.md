# ntmesh
nano templated polygonal mesh

Header-only polygonal mesh of points, triangles, quads, n-gons, and open polylines. Templated on the position type, so the same calls build an `ntmesh<float>` or an `ntmesh<double>`. The data structures are CUDA-friendly. Written for utility: little error checking, users are expected to feed it good data. This is intended to be terse, but reasonably feature-rich. Unlike the ntvec library, it should be legible but terse.

The purpose of this is to make a polygonal mesh data structure that is useful for geometry processing algortihms. Triangles, quads, ngons, and polylines are kept in separate lists, but optionally ngons can store everything. 

Topology is the whole of `ntmesh.h`. Numeric attributes, string attributes, and file IO are extra headers so a simple mesh never has to carry them if not needed.

## Documentation

- [Overview](docs/overview.md) — layout, ownership, winding, and which header to include
- [ntmesh](docs/ntmesh.md) · [attributes](docs/ntmesh_attribs.md) · [strings](docs/ntmesh_strattr.md) · [files](docs/ntmesh_file.md) · [tets](docs/ntmesh_bw_tets.md)

## Related

- [ntv](https://github.com/ivandewolf1/ntv) is the vector and matrix library this mesh includes.
- [ntmesh_ray](https://github.com/ivandewolf1/ntmesh_ray) is the polygon ray BVH and the CUDA sample.
- [ntmesh_nearest](https://github.com/ivandewolf1/ntmesh_nearest) is the point-cloud nearest query.

## Headers

| Include | Mesh type | What you get |
|---------|-----------|----------------|
| [`ntmesh.h`](docs/ntmesh.md) | `ntm::ntmesh<T>` | simple polygonal mesh topology |
| [`ntmesh_attribs.h`](docs/ntmesh_attribs.md) | `ntm::ntmesh<T, ntm::WithAttribs>` | point/prim/vert/mesh attributes of type float or int and varying dimension |
| [`ntmesh_strattr.h`](docs/ntmesh_strattr.md) | `ntm::ntmesh<T, ntm::WithStrAttribs>` | point/prim/vert/mesh string attribs, unavailable on GPU |
| [`ntmesh_file.h`](docs/ntmesh_file.md) | any of the three | ASCII `.obj` and `.geo` read/write |

`ntmesh.h` needs `nvec3.h` and `nmat4.h` from ntv on the include path. The attribute headers pull in the mesh header. A translation unit that only needs topology includes `ntmesh.h` and nothing else.

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

## Topology

Points are `ntv::nvec3<T>`. Corner indices are `int`, 0-based.

Each prim kind keeps its own index list:

- A triangle is three indices, `[f*3, f*3+3)`, with no offset table.
- A quad is four indices, `[f*4, f*4+4)`.
- An n-gon is a closed polygon. Face `f` uses `[ngonOffsets[f], ngonOffsets[f+1])`.
- A line is an open polyline. Line `f` uses `[lineOffsets[f], lineOffsets[f+1])`.

`addFace(corners, n)` files the prim by its corner count. Two corners go to the line list. With `separate` left on (the default), three corners go to the triangle list and four go to the quad list. Any other count goes to the n-gon list. Pass `separate = false` to store a triangle or a quad as an n-gon. A two-corner face is a line either way. `addLine` appends an open polyline of any length. `triangulate()` replaces every quad and n-gon with ear-clipped triangles and leaves lines alone. `triangulateNgons()` does that for n-gons only. Both return false, and leave the mesh unchanged, when scratch memory is unavailable. A face that falls back to a fan still counts as success. `reverse()` flips the corner order of every closed face and every line. `removeUnusedPoints()` deletes points that no corner references and remaps the index lists. `totalSurfaceArea()` and `totalVolume()` sum the face fans. `isClosed()` is true when every face edge has one opposite partner. `newell(corners, n)` is the raw Newell vector of a corner span, and `faceNormal(f)` is that vector made unit length. A line returns zero. Vertex and prim attributes are copied onto the new triangles.

`primVertex` walks one combined prim order: triangles, then quads, then n-gons, then lines. Vertex and prim attributes, when present, follow that same order. `primCount` and `vertexCount` include every list.

```cpp
mesh.triangleVertex(0, 2);   // third corner of the first packed triangle
mesh.ngonVertex(0, 4);       // fifth corner of the first n-gon
mesh.lineVertex(0, 1);       // second point of the first polyline
mesh.primVertex(f, k);       // corner k of prim f, across all four lists
```

## Filling a mesh

`reserve(npoints, nverts, nprims)` grows capacity and leaves the live counts alone. `addPoint` / `addFace` / `addLine` append. `trim(npoints, ntris, nquads, nngons, nlines)` sets the live size and keeps the spare capacity. `merge` appends another mesh of the same type and rebases its corner indices by the destination point count.

Arrays live in `Buf<T>`: a pointer, a count, and a capacity. The mesh has no destructor, so call `clear()` when you are done with it. A plain copy shares those pointers. `copyMesh` allocates new buffers, and it can narrow positions from `double` to `float` when both meshes use the same attribute policy.

```cpp
ntm::ntmesh<double> cpu;
ntm::ntmesh<float>  gpu;
ntm::copyMesh(gpu, cpu);
```

`print()` writes a short summary with `printf`.

## Attributes

Include `ntmesh_attribs.h` and use `ntmesh<T, WithAttribs>`. Columns can sit on points, vertices, prims, or the mesh. A mesh column is a single value. On merge, the destination keeps its value when both meshes have that name.

```cpp
#include "ntmesh_attribs.h"

ntm::ntmesh<float, ntm::WithAttribs> mesh;
ntm::Attribute* N = mesh.addPointAttrib("N", ntm::Type::Normal, 3);
N->floatAt(0)[1] = 1.f;
```

`Type` picks storage and how `transform` treats the column:

| Kind | Storage | `transform(nmat4)` |
|------|---------|--------------------|
| `Int` | `int` | left alone |
| `Float` | `float` | left alone |
| `Position` | `float` | full matrix, including translation |
| `Normal` | `float` | inverse transpose of the upper 3×3, then renormalized |
| `Color` | `float` | left alone |

`Position` and `Normal` use the first three components. A shorter tuple is skipped, and any components past the third stay put. A zero-length normal stays zero. The point buffer itself always takes the full matrix, on a plain mesh as well as an attributed one.

`findPointAttrib`, `findVertexAttrib`, `findFaceAttrib`, and `findMeshAttrib` return the column or null. Adding a name that already exists returns the existing column and keeps its kind. `computeFaceNormals()` writes a primitive column named `N` from `faceNormal`. `computePointNormals()` writes a point column named `N`, adding every face that uses the point. `computeVertexNormals(cuspVal)` writes a vertex column named `N`. A face farther than `cuspVal` radians from that corner's face is left out of its sum.

## Strings, host only

Include `ntmesh_strattr.h` and use `ntmesh<T, WithStrAttribs>`. That type has the numeric columns plus string columns on points, vertices, prims, and the mesh. Each value is an index into one `std::vector<std::string>` pool, so a repeated string is stored once. Index 0 is the empty string.

```cpp
ntm::ntmesh<float, ntm::WithStrAttribs> mesh;
mesh.addPoint(ntv::nvec3<float>(0, 0, 0));
mesh.setString(*mesh.addPointString("mat"), 0, "oak");
```

This mesh is for the host. It is not trivially copyable, and it is not the object you upload. Dump a device mesh out of it:

- `copyTopo` copies points and the four prim lists into an `ntmesh<T>` or an `ntmesh<T, WithAttribs>`.
- `copyNumeric` copies those plus the numeric columns into an `ntmesh<T, WithAttribs>`, and leaves the string pool behind.

Both allocate new buffers. Freeing the string mesh does not free the dump.

## Files

`ntmesh_file.h` reads and writes ASCII Wavefront `.obj` and ASCII Houdini `.geo`. `readFile` and `writeFile` pick the format from the suffix. This header is host-only. A device file does not include it.

`.obj` stores points, faces, and `l` lines. On an attributed mesh, a point or vertex `N` is written as `vn`, and `uv` as `vt`. `.geo` is the text JSON geometry document: polygon runs, the point buffer as `P`, and numeric columns on every domain. String columns are written only for `WithStrAttribs`. A plain `ntmesh<T>` still reads the points and the faces from either format. An open polyline is written as a `PolygonCurve_run`, and read back as a line. `readFile(mesh, path, separate)` uses the same filing rule as `addFace`. The default keeps 3-corner faces as triangles and 4-corner faces as quads. Pass `false` to put every closed face on the n-gon list, in file order.

```cpp
#include "ntmesh_file.h"

ntm::readFile(mesh, "box.obj");
ntm::writeFile(mesh, "box.geo");
```

## CUDA

The topology mesh and the numeric-attribute mesh are trivially copyable. The bytes you copy are the bytes a kernel sees. Define the macro before including, the same way ntv does:

```cpp
#define NTL_HD __host__ __device__
#include "ntmesh.h"
```

Left undefined, `NTL_HD` is empty and the headers stay host-only. Accessors are marked with it. Allocation, `merge`, `trim`, and `transform` stay on the host.

After uploading, rewrite every pointer in the copy to its device address: the `Buf` data pointers, and, for `WithAttribs`, each attribute's `data` pointer and the attribute-set pointer. Do not upload an `ntmesh<T, WithStrAttribs>`.

## Samples

`bunny_grid` reads `samplegeo/mixed_manifold_bunny.geo` and writes a 4×4 grid on the XZ plane, centered on the origin. `samples/grid/plain.geo` is the topology. `samples/grid/attrib.geo` is the same placement with point `N`, vertex `uv`, and primitive `Cd`. The negative-X columns yaw about Y. The positive-X columns use a random axis and angle.

`tet_edges` reads `samplegeo/light_bunny.geo`, builds a `TetMesh`, and writes each tet edge once to `samples/tets/edges.geo`. The eight cube corners are included.

```
./build/bunny_grid
./build/tet_edges
```

## Tests

`NTV_ROOT` is the ntv repository, the directory that contains `src/nvec3.h`. A `-DNTV_ROOT=` argument wins, then the `NTV_ROOT` environment variable, then `../ntv/ntv` beside this repo. Configuration stops if it is still missing. Run the binary from the repository root so the tests open `samplegeo/`.

```
cmake -S . -B build
cmake --build build
./build/ntmesh_tests
```

## Dev notes

This is not a fully vibecoded project, though a lot of the code was written with Cursor, using Grok 4.7. Started November 14, 2026. I did lean *heavily* on the AI to generate a lot of this, but it is all carefully thought out and planned by the human author.
