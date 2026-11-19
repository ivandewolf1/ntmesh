# String attributes

Host-only string columns on top of the numeric ones. Header: [`src/ntmesh_strattr.h`](../src/ntmesh_strattr.h).

```cpp
#include "ntmesh_strattr.h"

ntm::ntmesh<float, ntm::WithStrAttribs> mesh;
mesh.addPoint(ntv::nvec3<float>(0, 0, 0));
mesh.setString(*mesh.addPointString("mat"), 0, "oak");
const char* s = mesh.getString(*mesh.findPointString("mat"), 0);
```

Includes [attributes](ntmesh_attribs.md). This mesh is not trivially copyable, and it is not the object you upload. Dump a device mesh out of it with `copyTopo` or `copyNumeric`.

---

## Pool

Every string value is an `int` index into one `std::vector<std::string> pool`. A repeated string is stored once. Index 0 is the empty string. `intern` inserts `""` at 0 on the first use, returns 0 for a null or empty argument, and returns the existing index when the text is already in the pool.

`getString` returns `""` when the index is out of range. `setString(column, i, text)` stores `intern(text)` at element `i`. The index buffer must already cover `i`.

---

## `StringAttribute` and `StringSet`

| Field | Meaning |
|-------|---------|
| `name[32]` | Same 31-character limit as a numeric column |
| `indices` | `Buf<int>` of pool indices, one per element of the domain |

`StringSet` is the column array for one domain: `data`, `count`, `capacity`, and `find(name)`. A failed `realloc` of the column array leaves the old block. `stringInsert` returns before it moves indices when the index buffer did not grow.

---

## `WithStrAttribs`

Extends `WithAttribs`, so every numeric call is still there.

| Member | Domain |
|--------|--------|
| `pointStrings` / `vertexStrings` / `primStrings` / `meshStrings` | String columns |
| `pool` | The shared text |

| API | Meaning |
|-----|---------|
| `addPointString(name)` / `addVertexString` / `addFaceString` / `addMeshString` | Create the column, sized to the domain. An existing name returns the existing column |
| `findPointString` / `findVertexString` / `findFaceString` / `findMeshString` | The column, or null |
| `setString(column, i, text)` | Intern and store |
| `getString(column, i)` | Pool text, or `""` |

Topology edits keep the string columns aligned the same way they keep numeric columns. On merge, indices are remapped through the destination pool, so the destination does not alias the source's strings. A mesh string that already exists on the destination is left alone. A numeric column follows the numeric merge rules.

`clear()` frees the numeric columns, the string columns, and the pool.

```cpp
ntm::StringAttribute* name = mesh.addFaceString("name");
mesh.setString(*name, 0, "wing");
mesh.setString(*name, 1, "wing");   // second element, same pool entry
```

---

## Leaving the string mesh

Both helpers allocate new buffers. Freeing the string mesh does not free the dump. `dst` is cleared first.

| API | Destination | What is copied |
|-----|-------------|----------------|
| `copyTopo(dst, src)` | `ntmesh<Dst, None>` or `ntmesh<Dst, WithAttribs>` | Points and the four prim lists. Attributes on `dst` are dropped |
| `copyNumeric(dst, src)` | `ntmesh<Dst, WithAttribs>` | Those, plus the numeric columns. The pool stays behind |
| `copyMesh(dst, src)` | same policy on both | Full deep copy, including the pool when both sides are `WithStrAttribs` |

Point components convert from `Src` to `Dst`.

```cpp
ntm::ntmesh<float, ntm::WithAttribs> gpu;
ntm::copyNumeric(gpu, mesh);
mesh.clear();
```
