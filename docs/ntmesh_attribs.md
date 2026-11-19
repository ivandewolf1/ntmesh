# Numeric attributes

Point, vertex, prim, and mesh columns of `int` or `float`. Header: [`src/ntmesh_attribs.h`](../src/ntmesh_attribs.h).

```cpp
#include "ntmesh_attribs.h"

ntm::ntmesh<float, ntm::WithAttribs> mesh;
ntm::Attribute* N = mesh.addPointAttrib("N", ntm::Type::Normal, 3);
N->floatAt(0)[1] = 1.f;
```

Includes [ntmesh](ntmesh.md). The mesh stays trivially copyable: upload the bytes, then rewrite every pointer in the copy to its device address. That is each `Buf::data`, each attribute's `data`, and each `AttributeSet::data`. String columns are a different header, [strings](ntmesh_strattr.md).

---

## `Type`

| Kind | Storage | `transform` |
|------|---------|-------------|
| `Int` | `int` | left alone |
| `Float` | `float` | left alone |
| `Position` | `float` | full matrix, including translation |
| `Normal` | `float` | inverse transpose of the upper 3×3, then renormalized |
| `Color` | `float` | left alone |

`Position` and `Normal` use the first three components. A tuple shorter than three is skipped. Components past the third stay put. A normal whose squared length is below `1e-12` stays as it is, which keeps a zero normal at zero. The point buffer itself always takes the full matrix.

---

## `Attribute`

| Field | Meaning |
|-------|---------|
| `name[32]` | Fixed name. A longer name is cut to 31 characters plus a null |
| `kind` | `Type` |
| `tupleSize` | Components per element. A request below 1 becomes 1 |
| `count`, `capacity`, `data` | Same roles as `Buf` |

| API | Meaning |
|-----|---------|
| `isInt()` | `kind == Type::Int` |
| `floatAt(i)` / `intAt(i)` | Pointer to element `i`. The caller picks the one that matches `kind` |

`nameEq` compares at most 32 bytes and treats a null pointer as not equal.

---

## `AttributeSet`

One domain: points, vertices, prims, or the mesh. `data` is an array of `Attribute`. `find(name)` returns the column or null. `operator[]` and `size()` index the columns, not the elements inside a column.

A failed `realloc` of the column array or of a column's values leaves the old block and the old capacity. `attribResize` does not raise `count` when the capacity is still short. `attribInsert` returns before it moves bytes when the count did not grow.

---

## `WithAttribs`

The attribute policy. Use `ntmesh<T, WithAttribs>`.

| Member | Domain |
|--------|--------|
| `pointAttribs` | One element per point |
| `vertexAttribs` | One element per corner, in combined prim order |
| `primAttribs` | One element per prim, in that same order |
| `meshAttribs` | A single element |
| `pointN`, `vertN`, `primN` | Live sizes the policy last fitted |

| API | Meaning |
|-----|---------|
| `addPointAttrib(name, kind, tupleSize)` | Create a point column sized to the current point count |
| `addVertexAttrib` / `addFaceAttrib` / `addMeshAttrib` | The other three domains. A mesh column has one element |
| `findPointAttrib` / `findVertexAttrib` / `findFaceAttrib` / `findMeshAttrib` | The column, or null |

Adding a name that already exists returns the existing column and leaves its kind and tuple size alone. The new `kind` argument is ignored in that case.

`addPoint`, `addFace`, `addLine`, `trim`, `merge`, `reverse`, `triangulate`, and `removeUnusedPoints` keep the columns aligned with the topology. On merge, a column missing from the destination is created. A column whose kind or tuple size disagrees is left alone. A mesh column that already exists on the destination keeps the destination value.

```cpp
ntm::Attribute* id = mesh.addFaceAttrib("id", ntm::Type::Int, 1);
id->intAt(0)[0] = 7;
ntm::Attribute* Cd = mesh.addPointAttrib("Cd", ntm::Type::Color, 3);
Cd->floatAt(0)[0] = 1.f;
```

---

## Normals

These write a column named `"N"` of kind `Normal` and tuple size 3. They return that column, or null if it cannot be created or the scratch array is unavailable. An existing `"N"` of a different kind or a tuple shorter than 3 makes the call return null. Values are `float`, normalized.

| API | What is written |
|-----|-----------------|
| `computeFaceNormals(mesh)` | `faceNormal` of every prim, including lines, which are zero |
| `computePointNormals(mesh)` | Area-weighted sum of the raw Newell vector of every closed face that uses the point. A repeated corner on one face is counted once |
| `computeVertexNormals(mesh, cuspVal)` | Area-weighted vertex normal. `cuspVal` is radians |

For a vertex on face `f`, every closed face that shares that point is added, except a face whose unit normal is farther than `cuspVal` from face `f`. The test is `dot < cos(cuspVal) - 1e-5`. Face `f` itself is always included. `cuspVal >= π` accepts every face. A zero face is stored as zero.

```cpp
mesh.computeFaceNormals();
mesh.computePointNormals();
mesh.computeVertexNormals(0.5f);
```

---

## Upload

`WithAttribs` is trivially copyable. After copying the bytes to the device, point each pointer at the device copy of what it used to address:

- `points.data` and the six index buffers
- `pointAttribs.data`, `vertexAttribs.data`, `primAttribs.data`, `meshAttribs.data`
- each `Attribute::data` inside those sets

Do not upload an `ntmesh<T, WithStrAttribs>`.
