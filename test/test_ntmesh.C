#include <cstdio>
#include <cstring>
#include <iostream>
#include <type_traits>
#include "tests.h"
#include "ntmesh_file.h"
#include "ntmesh_bw_tets.h"
#include <cmath>

using ntm::Type;
using ntm::Attribute;
using ntm::ntmesh;
using ntm::readFile;
using ntm::writeFile;
using ntv::nvec3;

using AMesh = ntm::ntmesh<float, ntm::WithAttribs>;
using SMesh = ntm::ntmesh<float, ntm::WithStrAttribs>;

void
ntmeshTest()
{
  const int fails_before = g_fails;
  std::cout << "*************** test ntmesh *********************\n";
  static_assert(std::is_trivially_copyable<ntmesh<float>>::value, "plain mesh copies as bytes");
  static_assert(std::is_trivially_copyable<AMesh>::value, "numeric mesh copies as bytes");
  static_assert(!std::is_trivially_copyable<SMesh>::value, "string mesh stays on the host");

  // empty mesh, topology only
  {
    ntmesh<float> m;
    expect_true(m.pointCount() == 0, "empty point count");
    expect_true(m.vertexCount() == 0, "empty vertex count");
    expect_true(m.primCount() == 0, "empty face count");
    nvec3<float> mn(1, 2, 3);
    nvec3<float> mx(4, 5, 6);
    m.bounds(mn, mx);
    expect_vec(mn, nvec3<float>(1, 2, 3), 0.f, "empty bounds leaves min");
    expect_vec(mx, nvec3<float>(4, 5, 6), 0.f, "empty bounds leaves max");
    m.addPoint(nvec3<float>(1, -2, 0));
    m.addPoint(nvec3<float>(3, 4, -5));
    m.addPoint(nvec3<float>(-1, 0, 2));
    m.bounds(mn, mx);
    expect_vec(mn, nvec3<float>(-1, -2, -5), 0.f, "bounds min");
    expect_vec(mx, nvec3<float>(3, 4, 2), 0.f, "bounds max");
    m.clear();
  }

  // points, a triangle, a quad, and a 5-gon share one face list
  {
    AMesh m;
    m.reserve(8, 16, 4);
    int p0 = m.addPoint(nvec3<float>(0, 0, 0));
    int p1 = m.addPoint(nvec3<float>(1, 0, 0));
    int p2 = m.addPoint(nvec3<float>(0, 1, 0));
    int p3 = m.addPoint(nvec3<float>(0, 0, 1));
    int p4 = m.addPoint(nvec3<float>(1, 1, 0));
    expect_true(m.pointCount() == 5, "five points");
    expect_vec(m.points[p1], nvec3<float>(1, 0, 0), 1e-6f, "point 1");
    expect_true(m.points.capacity >= 8, "point reserve");

    int tri[] = {p0, p1, p2};
    int quad[] = {p0, p1, p4, p2};
    int ngon[] = {p0, p1, p4, p2, p3};
    m.addFace(tri, 3);
    m.addFace(quad, 4);
    m.addFace(ngon, 5);

    expect_true(m.primCount() == 3, "three faces");
    expect_true(m.vertexCount() == 12, "twelve corners");
    expect_true(m.primVertexCount(0) == 3, "triangle size");
    expect_true(m.primVertexCount(1) == 4, "quad size");
    expect_true(m.primVertexCount(2) == 5, "ngon size");
    expect_true(m.triangleCount() == 1, "one packed triangle");
    expect_true(m.quadCount() == 1, "one packed quad");
    expect_true(m.ngonCount() == 1, "one ngon");
    expect_true(m.triangleVertex(0, 2) == p2, "packed triangle corner");
    expect_true(m.quadVertex(0, 2) == p4, "packed quad corner");
    expect_true(m.ngonVertex(0, 4) == p3, "ngon last corner");
    expect_true(m.primVertex(0, 2) == p2, "triangle last corner");
    expect_true(m.primVertex(1, 2) == p4, "quad third corner");
    expect_true(m.ngonOffsets[0] == 0, "ngon offsets start at 0");
    expect_true(m.ngonOffsets[1] == 5, "ngon offsets end at ngon vertex count");

    Attribute* N = m.addPointAttrib("N", Type::Normal, 3);
    expect_true(N->count == 5, "normal sized to points");
    expect_true(N->kind == Type::Normal, "normal kind");
    N->floatAt(1)[0] = 1.f;
    N->floatAt(1)[1] = 0.f;
    N->floatAt(1)[2] = 0.f;
    expect_true(m.addPointAttrib("N", Type::Float, 1) == N, "second add returns existing");
    expect_true(N->kind == Type::Normal, "existing kind kept");

    int p5 = m.addPoint(nvec3<float>(2, 0, 0));
    expect_true(p5 == 5, "sixth point index");
    expect_true(N->count == 6, "normal grows with points");
    expect_true(N->floatAt(5)[0] == 0.f && N->floatAt(5)[1] == 0.f && N->floatAt(5)[2] == 0.f,
                "new normal is zero");
    expect_true(N->floatAt(1)[0] == 1.f, "old normal kept");

    Attribute* id = m.addVertexAttrib("id", Type::Int, 1);
    expect_true(id->count == 12, "vertex attrib sized to corners");
    id->intAt(4)[0] = 9;
    expect_true(id->intAt(4)[0] == 9, "vertex int write");

    Attribute* Cd = m.addFaceAttrib("Cd", Type::Color, 3);
    expect_true(Cd->count == 3, "face color sized to faces");
    Cd->floatAt(1)[0] = 0.5f;
    Attribute* piece = m.addMeshAttrib("piece", Type::Int, 1);
    expect_true(piece->count == 1, "mesh attrib is one value");
    piece->intAt(0)[0] = 4;
    m.addPoint(nvec3<float>(3, 0, 0));
    expect_true(piece->count == 1 && piece->intAt(0)[0] == 4, "mesh attrib stays one value");
    expect_true(m.findPointAttrib("missing") == nullptr, "missing point attrib");
    expect_true(m.findFaceAttrib("Cd") == Cd, "find face attrib");
    expect_true(m.findVertexAttrib("uv") == nullptr, "missing vertex attrib");

    m.clear();
    expect_true(m.points.data == nullptr, "clear points");
    expect_true(m.triangleIndices.data == nullptr, "clear triangles");
    expect_true(m.quadIndices.data == nullptr, "clear quads");
    expect_true(m.ngonIndices.data == nullptr, "clear ngon indices");
    expect_true(m.ngonOffsets.data == nullptr, "clear ngon offsets");
    expect_true(m.pointAttribs.data == nullptr, "clear point attribs");
    expect_true(m.meshAttribs.data == nullptr, "clear mesh attribs");
  }

  // merge rebases corners and appends matching columns
  {
    AMesh m;
    int p0 = m.addPoint(nvec3<float>(0, 0, 0));
    int p1 = m.addPoint(nvec3<float>(1, 0, 0));
    int p2 = m.addPoint(nvec3<float>(0, 1, 0));
    int tri[] = {p0, p1, p2};
    m.addFace(tri, 3);
    Attribute* N = m.addPointAttrib("N", Type::Normal, 3);
    N->floatAt(1)[0] = 1.f;
    Attribute* Cd = m.addFaceAttrib("Cd", Type::Color, 3);
    Cd->floatAt(0)[0] = 0.5f;
    Attribute* id = m.addVertexAttrib("id", Type::Int, 1);
    id->intAt(1)[0] = 9;
    Attribute* foo = m.addPointAttrib("foo", Type::Float, 1);
    foo->floatAt(0)[0] = 3.f;

    AMesh s;
    s.addPoint(nvec3<float>(2, 2, 2));
    int corner[] = {0};
    s.addFace(corner, 1);
    Attribute* sN = s.addPointAttrib("N", Type::Normal, 3);
    sN->floatAt(0)[2] = 4.f;
    Attribute* sCd = s.addFaceAttrib("Cd", Type::Color, 3);
    sCd->floatAt(0)[1] = 0.25f;
    Attribute* sFoo = s.addPointAttrib("foo", Type::Int, 1);
    sFoo->intAt(0)[0] = 7;
    s.addPointAttrib("extra", Type::Float, 1)->floatAt(0)[0] = 8.f;
    m.addMeshAttrib("piece", Type::Int, 1)->intAt(0)[0] = 3;
    s.addMeshAttrib("piece", Type::Int, 1)->intAt(0)[0] = 9;
    s.addMeshAttrib("note", Type::Float, 1)->floatAt(0)[0] = 0.5f;

    AMesh alias = m;
    expect_true(alias.points.data == m.points.data, "copy shares buffers");

    m.merge(s);
    expect_true(m.pointCount() == 4, "merged point count");
    expect_true(m.vertexCount() == 4, "merged vertex count");
    expect_true(m.primCount() == 2, "merged face count");
    expect_true(m.primVertex(1, 0) == 3, "rebased corner");
    expect_vec(m.points[3], nvec3<float>(2, 2, 2), 1e-6f, "merged point");
    expect_true(m.findPointAttrib("N")->floatAt(1)[0] == 1.f, "kept normal");
    expect_true(m.findPointAttrib("N")->floatAt(3)[2] == 4.f, "appended normal");
    expect_true(m.findFaceAttrib("Cd")->floatAt(0)[0] == 0.5f, "kept color");
    expect_true(m.findFaceAttrib("Cd")->floatAt(1)[1] == 0.25f, "appended color");
    expect_true(m.findVertexAttrib("id")->intAt(1)[0] == 9, "kept vertex id");
    expect_true(m.findVertexAttrib("id")->count == 4, "vertex id grown");
    expect_true(m.findVertexAttrib("id")->intAt(3)[0] == 0, "new vertex id is zero");
    expect_true(m.findPointAttrib("foo")->kind == Type::Float, "kind mismatch skipped");
    expect_true(m.findPointAttrib("foo")->floatAt(0)[0] == 3.f, "mismatched column kept");
    expect_true(m.findPointAttrib("foo")->floatAt(3)[0] == 0.f, "mismatched tail stays zero");
    expect_true(m.findPointAttrib("extra")->floatAt(3)[0] == 8.f, "new column appended");
    expect_true(m.findPointAttrib("extra")->floatAt(0)[0] == 0.f, "new column old range is zero");
    expect_true(m.findMeshAttrib("piece")->count == 1, "mesh attrib stays one");
    expect_true(m.findMeshAttrib("piece")->intAt(0)[0] == 3, "mesh attrib keeps destination");
    expect_true(m.findMeshAttrib("note")->floatAt(0)[0] == 0.5f, "mesh attrib copied from source");

    AMesh empty;
    m.merge(empty);
    expect_true(m.pointCount() == 4 && m.primCount() == 2, "merge empty leaves mesh");

    m.trim(3, 1, 0, 0, 0);
    expect_true(m.pointCount() == 3, "trim points");
    expect_true(m.vertexCount() == 3, "trim vertices");
    expect_true(m.primCount() == 1, "trim faces");
    expect_true(m.primVertexCount(0) == 3, "trimmed face intact");
    expect_true(m.findPointAttrib("N")->count == 3, "trim normal");
    expect_true(m.findPointAttrib("N")->floatAt(1)[0] == 1.f, "trimmed normal value");
    expect_true(m.findFaceAttrib("Cd")->count == 1, "trim color");
    expect_true(m.findMeshAttrib("piece")->count == 1, "trim leaves mesh attrib");
    expect_true(m.findMeshAttrib("piece")->intAt(0)[0] == 3, "trim keeps mesh value");
    expect_true(m.points.capacity >= 4, "trim keeps capacity");

    m.clear();
    empty.clear();
    s.clear();
  }

  // double mesh uses the same calls
  {
    ntmesh<double> m;
    int p0 = m.addPoint(nvec3<double>(0, 0, 0));
    int p1 = m.addPoint(nvec3<double>(1, 0, 0));
    int p2 = m.addPoint(nvec3<double>(0, 1, 0));
    int tri[] = {p0, p1, p2};
    m.addFace(tri, 3);
    expect_true(m.primVertex(0, 1) == p1, "double face corner");
    expect_vec(m.points[p2], nvec3<double>(0, 1, 0), 1e-12, "double point");
    ntmesh<float> narrowed;
    ntm::copyMesh(narrowed, m);
    expect_true(narrowed.pointCount() == 3, "plain copy point count");
    expect_true(narrowed.primVertex(0, 1) == p1, "plain copy corner");
    expect_vec(narrowed.points[p2], nvec3<float>(0, 1, 0), 1e-6f, "plain narrowed point");
    narrowed.clear();
    m.clear();
  }

  // double CPU mesh copied into a float mesh
  {
    ntmesh<double, ntm::WithAttribs> cpu;
    int p0 = cpu.addPoint(nvec3<double>(0.5, 1.5, 2.5));
    int p1 = cpu.addPoint(nvec3<double>(1, 0, 0));
    int p2 = cpu.addPoint(nvec3<double>(0, 1, 0));
    int tri[] = {p0, p1, p2};
    cpu.addFace(tri, 3);
    cpu.addPointAttrib("N", Type::Normal, 3)->floatAt(1)[0] = 1.f;
    cpu.addVertexAttrib("id", Type::Int, 1)->intAt(2)[0] = 9;
    cpu.addMeshAttrib("piece", Type::Int, 1)->intAt(0)[0] = 6;

    AMesh gpu;
    gpu.addPoint(nvec3<float>(9, 9, 9));
    ntm::copyMesh(gpu, cpu);
    expect_true(gpu.pointCount() == 3, "copied point count");
    expect_true(gpu.primCount() == 1, "copied face count");
    expect_true(gpu.primVertex(0, 2) == p2, "copied corner");
    expect_vec(gpu.points[0], nvec3<float>(0.5f, 1.5f, 2.5f), 1e-6f, "narrowed point");
    expect_true(gpu.points.data != nullptr, "float points allocated");
    expect_true(gpu.findPointAttrib("N")->floatAt(1)[0] == 1.f, "copied normal");
    expect_true(gpu.findVertexAttrib("id")->intAt(2)[0] == 9, "copied vertex id");
    expect_true(gpu.findMeshAttrib("piece")->intAt(0)[0] == 6, "copied mesh attrib");

    gpu.points[0][0] = 0.f;
    gpu.findPointAttrib("N")->floatAt(1)[0] = 0.f;
    expect_true(cpu.points[0][0] == 0.5, "source point unchanged");
    expect_true(cpu.findPointAttrib("N")->floatAt(1)[0] == 1.f, "source normal unchanged");

    AMesh again;
    ntm::copyMesh(again, gpu);
    expect_true(again.points.data != gpu.points.data, "same-type copy allocates");
    expect_vec(again.points[0], nvec3<float>(0.f, 1.5f, 2.5f), 1e-6f, "same-type point");

    ntm::copyMesh(gpu, gpu);
    expect_true(gpu.pointCount() == 3, "self copy keeps points");
    expect_true(gpu.findPointAttrib("N")->floatAt(1)[0] == 0.f, "self copy keeps normal");

    gpu.clear();
    again.clear();
    cpu.clear();
  }

  // triangles and quads can be packed, or stored with the n-gons
  {
    AMesh m;
    int p0 = m.addPoint(nvec3<float>(0, 0, 0));
    int p1 = m.addPoint(nvec3<float>(1, 0, 0));
    int p2 = m.addPoint(nvec3<float>(0, 1, 0));
    int p3 = m.addPoint(nvec3<float>(1, 1, 0));
    int asNgon[] = {p0, p1, p2};
    int packed[] = {p0, p2, p3};
    int quad[] = {p0, p1, p3, p2};
    m.addFace(asNgon, 3, false);
    m.addFace(packed, 3);
    m.addFace(quad, 4);
    expect_true(m.triangleCount() == 1, "optional packed triangle");
    expect_true(m.quadCount() == 1, "optional packed quad");
    expect_true(m.ngonCount() == 1, "triangle kept as ngon");
    expect_true(m.primCount() == 3, "packed and ngon faces");
    expect_true(m.vertexCount() == 10, "3 + 3 + 4 corners");
    expect_true(m.ngonVertex(0, 2) == p2, "ngon triangle corner");
    expect_true(m.triangleVertex(0, 2) == p3, "separate triangle corner");
    expect_true(m.quadVertex(0, 3) == p2, "separate quad corner");
    expect_true(m.primVertex(0, 2) == p3, "face order is tris, quads, ngons");
    expect_true(m.primVertex(2, 2) == p2, "ngon is the last face");

    Attribute* id = m.addVertexAttrib("id", Type::Int, 1);
    expect_true(id->count == 10, "vertex attrib covers every list");
    id->intAt(2)[0] = 7;
    expect_true(id->intAt(2)[0] == 7, "triangle corner id");
    Attribute* Cd = m.addFaceAttrib("Cd", Type::Color, 3);
    expect_true(Cd->count == 3, "face attrib covers every list");
    Cd->floatAt(1)[0] = 0.25f;

    AMesh extra;
    int e0 = extra.addPoint(nvec3<float>(2, 0, 0));
    int e1 = extra.addPoint(nvec3<float>(3, 0, 0));
    int e2 = extra.addPoint(nvec3<float>(2, 1, 0));
    int eq[] = {e0, e1, e2, e0};
    extra.addFace(eq, 4);
    extra.addFaceAttrib("Cd", Type::Color, 3)->floatAt(0)[1] = 0.5f;
    m.merge(extra);
    expect_true(m.quadCount() == 2, "merged quad count");
    expect_true(m.quadVertex(1, 0) == 4, "rebased quad index");
    expect_true(m.primVertex(2, 0) == 4, "merged quad follows the first quad");
    expect_true(m.findFaceAttrib("Cd")->floatAt(1)[0] == 0.25f, "first quad color kept");
    expect_true(m.findFaceAttrib("Cd")->floatAt(2)[1] == 0.5f, "merged quad color");
    expect_true(m.findVertexAttrib("id")->intAt(2)[0] == 7, "triangle id survives quad merge");

    m.trim(4, 1, 0, 1, 0);
    expect_true(m.triangleCount() == 1, "trim keeps the triangle");
    expect_true(m.quadCount() == 0, "trim drops quads");
    expect_true(m.ngonCount() == 1, "trim keeps the ngon");
    expect_true(m.pointCount() == 4, "trim points");
    expect_true(m.triangleVertex(0, 2) == p3, "trimmed triangle intact");
    expect_true(m.ngonVertex(0, 2) == p2, "trimmed ngon intact");
    expect_true(m.findVertexAttrib("id")->count == 6, "trim drops quad corners");
    expect_true(m.findVertexAttrib("id")->intAt(2)[0] == 7, "trim keeps triangle id");
    expect_true(m.findFaceAttrib("Cd")->count == 2, "trim face attribs");

    AMesh copied;
    ntm::copyMesh(copied, m);
    expect_true(copied.triangleCount() == 1 && copied.ngonCount() == 1, "copy keeps both lists");
    expect_true(copied.triangleIndices.data != m.triangleIndices.data, "copy allocates triangle indices");
    expect_true(copied.triangleVertex(0, 2) == p3, "copied packed corner");
    expect_true(copied.ngonVertex(0, 1) == p1, "copied ngon corner");

    m.clear();
    extra.clear();
    copied.clear();
  }

  // open polylines keep their own index and offset lists
  {
    AMesh m;
    int p0 = m.addPoint(nvec3<float>(0, 0, 0));
    int p1 = m.addPoint(nvec3<float>(1, 0, 0));
    int p2 = m.addPoint(nvec3<float>(1, 1, 0));
    int p3 = m.addPoint(nvec3<float>(0, 1, 0));
    int tri[] = {p0, p1, p2};
    int span[] = {p0, p1};
    int bend[] = {p0, p1, p2, p3};
    m.addFace(tri, 3);
    m.addLine(span, 2);
    m.addLine(bend, 4);
    expect_true(m.lineCount() == 2, "two lines");
    expect_true(m.triangleCount() == 1, "line does not become a face");
    expect_true(m.primCount() == 3, "lines count as prims");
    expect_true(m.vertexCount() == 9, "line corners follow polygon corners");
    expect_true(m.lineVertexCount(0) == 2, "first line size");
    expect_true(m.lineVertexCount(1) == 4, "second line size");
    expect_true(m.lineVertex(0, 1) == p1, "first line end");
    expect_true(m.lineVertex(1, 3) == p3, "second line end");
    expect_true(m.lineOffsets[0] == 0 && m.lineOffsets[2] == 6, "line offsets");
    expect_true(m.primVertex(1, 1) == p1, "lines follow polygon prims");
    expect_true(m.primVertex(2, 3) == p3, "second line is the last prim");

    Attribute* id = m.addVertexAttrib("id", Type::Int, 1);
    id->intAt(4)[0] = 8;
    expect_true(id->count == 9, "vertex attrib includes line corners");
    Attribute* w = m.addFaceAttrib("width", Type::Float, 1);
    expect_true(w->count == 3, "prim attrib includes lines");
    w->floatAt(1)[0] = 0.5f;
    w->floatAt(2)[0] = 1.5f;

    AMesh more;
    int a = more.addPoint(nvec3<float>(2, 0, 0));
    int b = more.addPoint(nvec3<float>(3, 0, 0));
    int seg[] = {a, b};
    more.addLine(seg, 2);
    more.addFaceAttrib("width", Type::Float, 1)->floatAt(0)[0] = 2.f;
    m.merge(more);
    expect_true(m.lineCount() == 3, "merged line count");
    expect_true(m.lineVertex(2, 0) == 4, "rebased line index");
    expect_true(m.pointCount() == 6, "merged line points");
    expect_true(m.findFaceAttrib("width")->floatAt(1)[0] == 0.5f, "first line width kept");
    expect_true(m.findFaceAttrib("width")->floatAt(3)[0] == 2.f, "merged line width");
    expect_true(m.findVertexAttrib("id")->intAt(4)[0] == 8, "line corner id survives merge");

    m.trim(4, 1, 0, 0, 1);
    expect_true(m.lineCount() == 1, "trim keeps the first line");
    expect_true(m.triangleCount() == 1, "trim keeps the triangle");
    expect_true(m.lineVertex(0, 1) == p1, "trimmed line intact");
    expect_true(m.findVertexAttrib("id")->count == 5, "trim drops later line corners");
    expect_true(m.findVertexAttrib("id")->intAt(4)[0] == 8, "trim keeps first line id");
    expect_true(m.findFaceAttrib("width")->count == 2, "trim line prim attrib");
    expect_true(m.findFaceAttrib("width")->floatAt(1)[0] == 0.5f, "trim keeps first line width");

    AMesh copied;
    ntm::copyMesh(copied, m);
    expect_true(copied.lineCount() == 1, "copy keeps lines");
    expect_true(copied.lineIndices.data != m.lineIndices.data, "copy allocates line indices");
    expect_true(copied.lineVertex(0, 0) == p0 && copied.lineVertex(0, 1) == p1, "copied line corners");

    m.clear();
    more.clear();
    copied.clear();
    expect_true(m.lineIndices.data == nullptr && m.lineOffsets.data == nullptr, "clear lines");
  }

  // a two-corner face is a line, including when separate is off
  {
    ntmesh<float> m;
    int p0 = m.addPoint(nvec3<float>(0, 0, 0));
    int p1 = m.addPoint(nvec3<float>(1, 0, 0));
    int p2 = m.addPoint(nvec3<float>(1, 1, 0));
    int edge[] = {p0, p1};
    int other[] = {p1, p2};
    int tri[] = {p0, p1, p2};
    m.addFace(tri, 3);
    m.addFace(edge, 2);
    m.addFace(other, 2, false);
    expect_true(m.lineCount() == 2, "two-corner faces are lines");
    expect_true(m.triangleCount() == 1, "two-corner face is not a triangle");
    expect_true(m.ngonCount() == 0, "two-corner face is not an n-gon");
    expect_true(m.primCount() == 3, "two-corner face is a prim");
    expect_true(m.lineVertex(0, 0) == p0 && m.lineVertex(0, 1) == p1, "first two-corner face");
    expect_true(m.lineVertex(1, 0) == p1 && m.lineVertex(1, 1) == p2, "separate false still a line");
    expect_true(m.primVertex(1, 1) == p1, "two-corner face follows the triangle");
    m.clear();
  }

  // string attributes stay on the host; topology and numeric columns can be dumped
  {
    SMesh m;
    int p0 = m.addPoint(nvec3<float>(0, 0, 0));
    int p1 = m.addPoint(nvec3<float>(1, 0, 0));
    int p2 = m.addPoint(nvec3<float>(0, 1, 0));
    int tri[] = {p0, p1, p2};
    m.addFace(tri, 3);
    ntm::StringAttribute* mat = m.addPointString("mat");
    m.setString(*mat, 0, "oak");
    m.setString(*mat, 1, "oak");
    m.setString(*mat, 2, "pine");
    expect_true(mat->indices[0] == mat->indices[1], "repeated string shares an index");
    expect_true(mat->indices[0] != 0, "oak is not the empty string");
    expect_true(std::strcmp(m.getString(*mat, 0), "oak") == 0, "point string oak");
    expect_true(std::strcmp(m.getString(*mat, 2), "pine") == 0, "point string pine");
    expect_true(m.pool.size() == 3, "pool holds empty, oak, pine");
    m.addPointAttrib("N", Type::Normal, 3)->floatAt(1)[0] = 1.f;
    m.setString(*m.addMeshString("note"), 0, "keep");
    m.setString(*m.addFaceString("part"), 0, "door");

    SMesh s;
    s.addPoint(nvec3<float>(2, 2, 2));
    int corner[] = {0};
    s.addFace(corner, 1);
    s.setString(*s.addPointString("mat"), 0, "oak");
    s.setString(*s.addFaceString("part"), 0, "window");
    s.setString(*s.addMeshString("note"), 0, "lose");
    s.setString(*s.addMeshString("tag"), 0, "from-src");
    m.merge(s);
    expect_true(m.pointCount() == 4, "string merge point count");
    expect_true(m.primCount() == 2, "string merge prim count");
    expect_true(std::strcmp(m.getString(*m.findPointString("mat"), 0), "oak") == 0, "merged kept oak");
    expect_true(std::strcmp(m.getString(*m.findPointString("mat"), 3), "oak") == 0, "merged interned oak");
    expect_true(m.findPointString("mat")->indices[0] == m.findPointString("mat")->indices[3],
                "merge shares oak");
    expect_true(std::strcmp(m.getString(*m.findFaceString("part"), 0), "door") == 0, "kept prim string");
    expect_true(std::strcmp(m.getString(*m.findFaceString("part"), 1), "window") == 0, "merged prim string");
    expect_true(std::strcmp(m.getString(*m.findMeshString("note"), 0), "keep") == 0, "detail string keeps destination");
    expect_true(std::strcmp(m.getString(*m.findMeshString("tag"), 0), "from-src") == 0, "detail string copied");
    expect_true(m.pool.size() == 7, "pool gained window and from-src");

    ntmesh<float> topo;
    ntm::copyTopo(topo, m);
    expect_true(topo.pointCount() == 4, "topo dump point count");
    expect_true(topo.primCount() == 2, "topo dump prim count");
    expect_true(topo.primVertex(0, 1) == p1, "topo dump corner");
    expect_true(topo.points.data != m.points.data, "topo dump allocates points");

    AMesh numeric;
    numeric.addPointAttrib("N", Type::Normal, 3);
    ntm::copyNumeric(numeric, m);
    expect_true(numeric.pointCount() == 4, "numeric dump points");
    expect_true(numeric.primVertex(1, 0) == 3, "numeric dump rebased corner");
    expect_true(numeric.findPointAttrib("N")->floatAt(1)[0] == 1.f, "numeric dump keeps normal");
    expect_true(numeric.findPointAttrib("N")->data != m.findPointAttrib("N")->data, "numeric dump allocates");
    numeric.findPointAttrib("N")->floatAt(1)[0] = 0.f;
    expect_true(m.findPointAttrib("N")->floatAt(1)[0] == 1.f, "numeric dump leaves the host mesh");

    m.trim(3, 1, 0, 0, 0);
    expect_true(m.findPointString("mat")->indices.count == 3, "trim string column");
    expect_true(std::strcmp(m.getString(*m.findPointString("mat"), 2), "pine") == 0, "trim keeps pine");
    expect_true(m.findFaceString("part")->indices.count == 1, "trim prim string");
    expect_true(std::strcmp(m.getString(*m.findFaceString("part"), 0), "door") == 0, "trim keeps door");
    expect_true(std::strcmp(m.getString(*m.findMeshString("note"), 0), "keep") == 0, "trim keeps detail string");

    SMesh copied;
    ntm::copyMesh(copied, m);
    m.setString(*m.findPointString("mat"), 0, "ash");
    expect_true(std::strcmp(copied.getString(*copied.findPointString("mat"), 0), "oak") == 0,
                "string copy is independent");
    expect_true(copied.pool.data() != m.pool.data(), "string copy allocates the pool");

    m.clear();
    s.clear();
    topo.clear();
    numeric.clear();
    copied.clear();
    expect_true(m.pointStrings.data == nullptr, "clear string columns");
    expect_true(m.pool.empty(), "clear string pool");
    expect_true(m.pointAttribs.data == nullptr, "clear numeric columns on a string mesh");
  }

  // an nmat4 moves positions and Point columns, rotates normals, and skips the rest
  {
    AMesh m;
    int p0 = m.addPoint(nvec3<float>(1, 0, 0));
    int p1 = m.addPoint(nvec3<float>(0, 1, 0));
    int tri[] = {p0, p1, p0};
    m.addFace(tri, 3);
    Attribute* rest = m.addPointAttrib("rest", Type::Position, 3);
    rest->floatAt(0)[0] = 1.f;
    rest->floatAt(0)[1] = 0.f;
    rest->floatAt(0)[2] = 0.f;
    Attribute* uv = m.addPointAttrib("uv", Type::Position, 2);
    uv->floatAt(0)[0] = 0.25f;
    uv->floatAt(0)[1] = 0.75f;
    Attribute* rest4 = m.addPointAttrib("rest4", Type::Position, 4);
    rest4->floatAt(1)[0] = 0.f;
    rest4->floatAt(1)[1] = 1.f;
    rest4->floatAt(1)[2] = 0.f;
    rest4->floatAt(1)[3] = 5.f;
    Attribute* N = m.addVertexAttrib("N", Type::Normal, 3);
    N->floatAt(0)[0] = 0.f;
    N->floatAt(0)[1] = 1.f;
    N->floatAt(0)[2] = 0.f;
    N->floatAt(1)[0] = 0.f;
    N->floatAt(1)[1] = 0.f;
    N->floatAt(1)[2] = 0.f;
    Attribute* Cd = m.addFaceAttrib("Cd", Type::Color, 3);
    Cd->floatAt(0)[0] = 0.2f;
    Cd->floatAt(0)[1] = 0.4f;
    Cd->floatAt(0)[2] = 0.6f;
    Attribute* scale = m.addPointAttrib("scale", Type::Float, 1);
    scale->floatAt(0)[0] = 3.5f;
    Attribute* id = m.addVertexAttrib("id", Type::Int, 1);
    id->intAt(2)[0] = 7;
    Attribute* pivot = m.addMeshAttrib("pivot", Type::Position, 3);
    pivot->floatAt(0)[0] = 1.f;
    pivot->floatAt(0)[1] = 2.f;
    pivot->floatAt(0)[2] = 3.f;

    ntv::nmat4<float> xform;
    xform.identity();
    xform.translate(nvec3<float>(10, 0, 0));
    xform(1, 0) = 1.f;
    m.transform(xform);
    expect_vec(m.points[0], nvec3<float>(11, 1, 0), 1e-5f, "position takes translation and shear");
    expect_vec(m.points[1], nvec3<float>(10, 1, 0), 1e-5f, "second position");
    expect_true(m.triangleVertex(0, 1) == p1, "transform keeps corner indices");
    expect_true(rest->floatAt(0)[0] == 11.f && rest->floatAt(0)[1] == 1.f && rest->floatAt(0)[2] == 0.f,
                "point attribute follows the matrix");
    expect_true(uv->floatAt(0)[0] == 0.25f && uv->floatAt(0)[1] == 0.75f, "short point tuple stays");
    expect_true(rest4->floatAt(1)[0] == 10.f && rest4->floatAt(1)[1] == 1.f && rest4->floatAt(1)[2] == 0.f,
                "point tuple uses the first three");
    expect_true(rest4->floatAt(1)[3] == 5.f, "point tuple keeps the extra component");
    const float h = 0.70710678f;
    expect_true(std::fabs(N->floatAt(0)[0] + h) < 1e-5f && std::fabs(N->floatAt(0)[1] - h) < 1e-5f
                    && std::fabs(N->floatAt(0)[2]) < 1e-5f,
                "normal uses the inverse transpose");
    expect_true(N->floatAt(1)[0] == 0.f && N->floatAt(1)[1] == 0.f && N->floatAt(1)[2] == 0.f,
                "zero normal stays zero");
    expect_true(Cd->floatAt(0)[0] == 0.2f && Cd->floatAt(0)[1] == 0.4f && Cd->floatAt(0)[2] == 0.6f,
                "color stays");
    expect_true(scale->floatAt(0)[0] == 3.5f, "float column stays");
    expect_true(id->intAt(2)[0] == 7, "int column stays");
    expect_true(pivot->floatAt(0)[0] == 11.f && pivot->floatAt(0)[1] == 3.f && pivot->floatAt(0)[2] == 3.f,
                "mesh point attribute moves");
    m.clear();

    ntmesh<float> plain;
    plain.addPoint(nvec3<float>(1, 0, 0));
    ntv::nmat4<float> shift;
    shift.identity();
    shift.translate(nvec3<float>(0, 2, 0));
    plain.transform(shift);
    expect_vec(plain.points[0], nvec3<float>(1, 2, 0), 1e-5f, "plain mesh positions move");
    plain.clear();

    SMesh strings;
    strings.addPoint(nvec3<float>(1, 0, 0));
    strings.setString(*strings.addPointString("mat"), 0, "oak");
    Attribute* sN = strings.addPointAttrib("N", Type::Normal, 3);
    sN->floatAt(0)[1] = 1.f;
    strings.transform(xform);
    expect_true(std::strcmp(strings.getString(*strings.findPointString("mat"), 0), "oak") == 0,
                "transform leaves strings");
    expect_true(std::fabs(sN->floatAt(0)[0] + h) < 1e-5f && std::fabs(sN->floatAt(0)[1] - h) < 1e-5f,
                "string mesh still rotates normals");
    strings.clear();
  }

  {
    const char* objPath = "test/_tmp_io.obj";
    const char* geoPath = "test/_tmp_io.geo";
    ntmesh<float> plain;
    int p0 = plain.addPoint(nvec3<float>(0, 0, 0));
    int p1 = plain.addPoint(nvec3<float>(1, 0, 0));
    int p2 = plain.addPoint(nvec3<float>(0, 1, 0));
    int tri[] = {p0, p1, p2};
    int seg[] = {p0, p2};
    plain.addFace(tri, 3);
    plain.addLine(seg, 2);
    expect_true(writeFile(plain, objPath) == 1, "write obj");
    ntmesh<float> back;
    expect_true(readFile(back, objPath) == 1, "read obj");
    expect_true(back.pointCount() == 3 && back.triangleCount() == 1 && back.lineCount() == 1, "obj topology");
    expect_true(back.triangleVertex(0, 1) == 1 && back.lineVertex(0, 1) == 2, "obj corners");
    expect_vec(back.points[2], nvec3<float>(0, 1, 0), 1e-5f, "obj point");
    back.clear();
    plain.clear();

    AMesh shaded;
    int a = shaded.addPoint(nvec3<float>(0, 0, 0));
    int b = shaded.addPoint(nvec3<float>(1, 0, 0));
    int c = shaded.addPoint(nvec3<float>(0, 1, 0));
    int face[] = {a, b, c};
    shaded.addFace(face, 3);
    Attribute* N = shaded.addPointAttrib("N", Type::Normal, 3);
    Attribute* uv = shaded.addPointAttrib("uv", Type::Float, 2);
    N->floatAt(1)[0] = 0.f; N->floatAt(1)[1] = 1.f; N->floatAt(1)[2] = 0.f;
    uv->floatAt(2)[0] = 0.25f; uv->floatAt(2)[1] = 0.5f;
    expect_true(writeFile(shaded, objPath) == 1, "write point N obj");
    AMesh shadedBack;
    expect_true(readFile(shadedBack, objPath) == 1, "read point N obj");
    const Attribute* Nb = shadedBack.findPointAttrib("N");
    const Attribute* uvb = shadedBack.findPointAttrib("uv");
    expect_true(Nb && !shadedBack.findVertexAttrib("N"), "point normal stays on points");
    expect_true(uvb && uvb->tupleSize == 2 && uvb->floatAt(2)[0] == 0.25f && uvb->floatAt(2)[1] == 0.5f, "point uv");
    expect_true(Nb && Nb->floatAt(1)[1] == 1.f, "point normal value");
    shaded.clear();
    shadedBack.clear();

    AMesh corners;
    int q0 = corners.addPoint(nvec3<float>(0, 0, 0));
    int q1 = corners.addPoint(nvec3<float>(1, 0, 0));
    int q2 = corners.addPoint(nvec3<float>(0, 1, 0));
    int q3 = corners.addPoint(nvec3<float>(0, 0, 1));
    int t0[] = {q0, q1, q2};
    int t1[] = {q0, q2, q3};
    corners.addFace(t0, 3);
    corners.addFace(t1, 3);
    Attribute* vN = corners.addVertexAttrib("N", Type::Normal, 3);
    vN->floatAt(0)[1] = 1.f;
    vN->floatAt(3)[0] = 1.f;
    expect_true(writeFile(corners, objPath) == 1, "write vertex N obj");
    AMesh cornerBack;
    expect_true(readFile(cornerBack, objPath) == 1, "read vertex N obj");
    const Attribute* vNb = cornerBack.findVertexAttrib("N");
    expect_true(vNb && !cornerBack.findPointAttrib("N"), "split normals stay on vertices");
    expect_true(vNb && vNb->floatAt(0)[1] == 1.f && vNb->floatAt(3)[0] == 1.f, "vertex normal values");
    corners.clear();
    cornerBack.clear();

    AMesh rich;
    int pts[14];
    for (int i = 0; i < 14; ++i) pts[i] = rich.addPoint(nvec3<float>((float)i, 0, 0));
    rich.addFace(pts, 3);
    rich.addFace(pts + 3, 4);
    rich.addFace(pts + 7, 5);
    rich.addLine(pts + 12, 2);
    Attribute* pN = rich.addPointAttrib("N", Type::Normal, 3);
    pN->floatAt(4)[2] = 1.f;
    Attribute* vId = rich.addVertexAttrib("id", Type::Int, 1);
    vId->intAt(0)[0] = 11;
    vId->intAt(13)[0] = 22;
    Attribute* Cd = rich.addFaceAttrib("Cd", Type::Color, 3);
    Cd->floatAt(2)[0] = 0.2f; Cd->floatAt(2)[1] = 0.4f; Cd->floatAt(2)[2] = 0.6f;
    Attribute* detail = rich.addMeshAttrib("piece", Type::Int, 1);
    detail->intAt(0)[0] = 9;
    expect_true(writeFile(rich, geoPath) == 1, "write geo");
    AMesh richBack;
    expect_true(readFile(richBack, geoPath) == 1, "read geo");
    expect_true(richBack.triangleCount() == 1 && richBack.quadCount() == 1 && richBack.ngonCount() == 1 &&
                    richBack.lineCount() == 1,
                "geo prim lists");
    expect_true(richBack.primVertex(2, 4) == 11 && richBack.lineVertex(0, 1) == 13, "geo corners");
    const Attribute* pNb = richBack.findPointAttrib("N");
    const Attribute* vIdb = richBack.findVertexAttrib("id");
    const Attribute* Cdb = richBack.findFaceAttrib("Cd");
    const Attribute* detailb = richBack.findMeshAttrib("piece");
    expect_true(pNb && pNb->kind == Type::Normal && pNb->floatAt(4)[2] == 1.f, "geo point normal");
    expect_true(!richBack.findPointAttrib("P"), "P stays the point buffer");
    expect_true(vIdb && vIdb->intAt(0)[0] == 11 && vIdb->intAt(13)[0] == 22, "geo vertex int");
    expect_true(Cdb && Cdb->kind == Type::Color && Cdb->floatAt(2)[0] == 0.2f && Cdb->floatAt(2)[2] == 0.6f,
                "geo prim color");
    expect_true(detailb && detailb->intAt(0)[0] == 9, "geo mesh int");
    ntmesh<float> topoOnly;
    expect_true(readFile(topoOnly, geoPath) == 1, "plain mesh reads geo");
    expect_true(topoOnly.pointCount() == 14 && topoOnly.triangleCount() == 1 && topoOnly.lineCount() == 1,
                "plain geo topology");
    rich.clear();
    richBack.clear();
    topoOnly.clear();

    FILE* fp = std::fopen(geoPath, "wb");
    expect_true(fp != nullptr, "open remap geo");
    if (fp) {
      std::fputs(
          "[\"pointcount\",7,\"vertexcount\",7,\"primitivecount\",2,\n"
          "\"topology\",[\"pointref\",[\"indices\",[0,0,0,0,0,0,0,0,0,0,3,4,5,6,0,0,0,0,0,0,0,1,2]]],\n"
          "\"attributes\",[\n"
          "\"pointattributes\",[[[\"name\",\"P\",\"type\",\"numeric\",\"options\",{\"type\":{\"type\":\"string\",\"value\":\"point\"}}],\n"
          "[\"size\",3,\"storage\",\"fpreal32\",\"values\",[\"size\",3,\"storage\",\"fpreal32\",\"tuples\",\n"
          "[[0,0,0],[1,0,0],[0,1,0],[1,1,0],[2,1,0],[2,0,0],[1,2,0]]]]]],\n"
          "\"vertexattributes\",[[[\"name\",\"id\",\"type\",\"numeric\",\"options\",{}],\n"
          "[\"size\",1,\"storage\",\"int32\",\"values\",[\"size\",1,\"storage\",\"int32\",\"tuples\",\n"
          "[[0],[0],[0],[0],[0],[0],[0],[0],[0],[0],[9],[0],[0],[0],[0],[0],[0],[0],[0],[0],[7],[0],[0]]]]]],\n"
          "\"primitiveattributes\",[[[\"name\",\"piece\",\"type\",\"numeric\",\"options\",{}],\n"
          "[\"size\",1,\"storage\",\"int32\",\"values\",[\"size\",1,\"storage\",\"int32\",\"tuples\",[[100],[200]]]]]]],\n"
          "\"primitives\",[\n"
          "[\"type\",\"Polygon_run\",\"startvertex\",10,\"nvertices_rle\",[4,1],\"closed\",true],\n"
          "[\"type\",\"Polygon_run\",\"startvertex\",20,\"nvertices_rle\",[3,1]]]]\n",
          fp);
      std::fclose(fp);
    }
    AMesh remapped;
    expect_true(readFile(remapped, geoPath) == 1, "read reordered geo");
    expect_true(remapped.triangleCount() == 1 && remapped.quadCount() == 1, "reorder lists");
    expect_true(remapped.triangleVertex(0, 0) == 0 && remapped.triangleVertex(0, 2) == 2, "triangle points");
    expect_true(remapped.quadVertex(0, 0) == 3 && remapped.quadVertex(0, 3) == 6, "quad points");
    const Attribute* piece = remapped.findFaceAttrib("piece");
    const Attribute* vid = remapped.findVertexAttrib("id");
    expect_true(piece && piece->intAt(0)[0] == 200 && piece->intAt(1)[0] == 100, "prim attrib follows the move");
    expect_true(vid && vid->intAt(0)[0] == 7 && vid->intAt(3)[0] == 9, "vertex attrib follows the move");
    remapped.clear();

    AMesh asNgons;
    expect_true(readFile(asNgons, geoPath, false) == 1, "read geo into n-gons");
    expect_true(asNgons.triangleCount() == 0 && asNgons.quadCount() == 0 && asNgons.ngonCount() == 2,
                "closed faces share the n-gon list");
    expect_true(asNgons.ngonVertex(0, 0) == 3 && asNgons.ngonVertex(0, 3) == 6, "first polygon stays a quad");
    expect_true(asNgons.ngonVertex(1, 0) == 0 && asNgons.ngonVertex(1, 2) == 2, "second polygon stays a triangle");
    const Attribute* pieceN = asNgons.findFaceAttrib("piece");
    const Attribute* vidN = asNgons.findVertexAttrib("id");
    expect_true(pieceN && pieceN->intAt(0)[0] == 100 && pieceN->intAt(1)[0] == 200, "prim attrib stays in file order");
    expect_true(vidN && vidN->intAt(0)[0] == 9 && vidN->intAt(4)[0] == 7, "vertex attrib stays in file order");
    asNgons.clear();

    fp = std::fopen(geoPath, "wb");
    if (fp) {
      std::fputs(
          "[\"attributes\",[\"pointattributes\",[[[\"name\",\"P\",\"type\",\"numeric\"],\n"
          "[\"size\",3,\"values\",[\"tuples\",[[0,0,0],[1,0,0],[0,1,0]]]]]]],\n"
          "\"primitives\",[[\"type\",\"Poly\",\"vertex\",[0,1,2]]]]\n",
          fp);
      std::fclose(fp);
    }
    ntmesh<float> poly;
    expect_true(readFile(poly, geoPath) == 1 && poly.triangleCount() == 1 && poly.triangleVertex(0, 2) == 2,
                "poly vertex list");
    poly.clear();

    fp = std::fopen(geoPath, "wb");
    if (fp) {
      std::fputs(
          "[\"attributes\",[\"pointattributes\",[[[\"name\",\"P\",\"type\",\"numeric\"],\n"
          "[\"size\",3,\"values\",[\"tuples\",[[0,0,0],[1,0,0],[1,1,0],[2,0,0],[3,0,0],[3,1,0]]]]]]],\n"
          "\"primitives\",[\n"
          "[[\"type\",\"Polygon_run\"],[\"startvertex\",0,\"nvertices_rle\",[3,1]]],\n"
          "[[\"type\",\"PolygonCurve_run\"],[\"startvertex\",3,\"nvertices\",[3]]]]]\n",
          fp);
      std::fclose(fp);
    }
    ntmesh<float> curve;
    expect_true(readFile(curve, geoPath) == 1, "read polygon curve");
    expect_true(curve.triangleCount() == 1 && curve.ngonCount() == 0 && curve.lineCount() == 1, "curve is a line");
    expect_true(curve.lineVertexCount(0) == 3 && curve.lineVertex(0, 0) == 3 && curve.lineVertex(0, 2) == 5,
                "curve corners");
    curve.clear();

    SMesh named;
    named.addPoint(nvec3<float>(1, 2, 3));
    int sp[] = {0, 0, 0};
    named.addFace(sp, 3);
    named.setString(*named.addPointString("mat"), 0, "oak");
    named.setString(*named.addFaceString("group"), 0, "shell");
    expect_true(writeFile(named, geoPath) == 1, "write string geo");
    SMesh namedBack;
    expect_true(readFile(namedBack, geoPath) == 1, "read string geo");
    expect_true(std::strcmp(namedBack.getString(*namedBack.findPointString("mat"), 0), "oak") == 0, "point string");
    expect_true(std::strcmp(namedBack.getString(*namedBack.findFaceString("group"), 0), "shell") == 0, "prim string");
    named.clear();
    namedBack.clear();

    ntmesh<float> missing;
    missing.addPoint(nvec3<float>(1, 0, 0));
    expect_true(readFile(missing, "test/_tmp_missing.geo") == 0, "missing file");
    expect_true(missing.pointCount() == 0, "failed read clears");
    missing.addPoint(nvec3<float>(1, 0, 0));
    expect_true(readFile(missing, "test/_tmp_io.stl") == 0, "unknown suffix");
    expect_true(missing.pointCount() == 1, "unknown suffix leaves the mesh");
    missing.clear();

    std::remove(objPath);
    std::remove(geoPath);
  }

  {
    AMesh bunny;
    expect_true(readFile(bunny, "samplegeo/light_bunny.geo") == 1, "read light bunny");
    expect_true(bunny.pointCount() == 2552, "bunny points");
    expect_true(bunny.triangleCount() == 761 && bunny.quadCount() == 1931 && bunny.ngonCount() == 68, "bunny faces");
    expect_true(bunny.lineCount() == 1 && bunny.lineVertexCount(0) == 13, "bunny curve");
    const Attribute* bN = bunny.findPointAttrib("N");
    const Attribute* buv = bunny.findVertexAttrib("uv");
    expect_true(bN && bN->kind == Type::Normal && bN->count == bunny.pointCount(), "bunny point normals");
    expect_true(buv && buv->tupleSize == 3 && buv->count == bunny.vertexCount(), "bunny vertex uv");
    int used[2552];
    for (int i = 0; i < 2552; ++i) used[i] = 0;
    for (int f = 0; f < bunny.primCount(); ++f)
      for (int k = 0; k < bunny.primVertexCount(f); ++k) used[bunny.primVertex(f, k)] = 1;
    int loose = 0;
    for (int i = 0; i < 2552; ++i)
      if (!used[i]) ++loose;
    expect_true(loose == 13, "bunny loose points");
    expect_true(bunny.isClosed() == 0, "bunny is not closed");

    ntmesh<float> mixed;
    expect_true(readFile(mixed, "samplegeo/mixed_manifold_bunny.geo") == 1, "read mixed bunny");
    expect_true(mixed.pointCount() == 1176, "mixed bunny points");
    expect_true(mixed.triangleCount() == 189 && mixed.quadCount() == 1072 && mixed.ngonCount() == 5,
                "mixed bunny faces");
    expect_true(mixed.isClosed() == 1, "mixed bunny is closed");
    expect_true(mixed.totalVolume() > 0.f, "mixed bunny volume is positive");
    mixed.clear();

    ntm::TetMesh<float> bunnyTets;
    bunnyTets.build(bunny.points.data, bunny.pointCount());
    expect_true(bunnyTets.pointCount == 2552 && bunnyTets.count > 0, "bunny tets");
    if (bunnyTets.count > 0 && bunnyTets.pointCount == 2552) {
      const int nsite = 2552 + 8;
      double* xyz = new double[(size_t)nsite * 3];
      for (int i = 0; i < nsite; ++i) {
        nvec3<float> p = bunnyTets.position(bunny.points.data, i);
        xyz[3 * i] = p[0];
        xyz[3 * i + 1] = p[1];
        xyz[3 * i + 2] = p[2];
      }
      char used[2560];
      for (int i = 0; i < nsite; ++i) used[i] = 0;
      int hull = 0;
      bool orientOk = true, linkOk = true;
      double vol = 0.0;
      for (int ti = 0; ti < bunnyTets.count; ++ti) {
        const ntm::Tet& tet = bunnyTets.tets[ti];
        for (int k = 0; k < 4; ++k) {
          if (tet.v[k] >= 0 && tet.v[k] < nsite) used[tet.v[k]] = 1;
          int nb = tet.n[k];
          if (nb < 0) {
            ++hull;
            continue;
          }
          int back = 0;
          for (int s = 0; s < 4; ++s)
            if (bunnyTets.tets[nb].n[s] == ti) ++back;
          if (back != 1) linkOk = false;
        }
        if (ntm::detail::orient3d(xyz, tet.v[0], tet.v[1], tet.v[2], tet.v[3]) <= 0) orientOk = false;
        nvec3<double> a(xyz[3 * tet.v[0]], xyz[3 * tet.v[0] + 1], xyz[3 * tet.v[0] + 2]);
        nvec3<double> b(xyz[3 * tet.v[1]], xyz[3 * tet.v[1] + 1], xyz[3 * tet.v[1] + 2]);
        nvec3<double> c(xyz[3 * tet.v[2]], xyz[3 * tet.v[2] + 1], xyz[3 * tet.v[2] + 2]);
        nvec3<double> d(xyz[3 * tet.v[3]], xyz[3 * tet.v[3] + 1], xyz[3 * tet.v[3] + 2]);
        vol += (b - a).cross(c - a).dot(d - a) / 6.0;
      }
      bool allUsed = true;
      for (int i = 0; i < nsite; ++i)
        if (!used[i]) allUsed = false;
      double dx = (double)bunnyTets.corner[6][0] - (double)bunnyTets.corner[0][0];
      double dy = (double)bunnyTets.corner[6][1] - (double)bunnyTets.corner[0][1];
      double dz = (double)bunnyTets.corner[6][2] - (double)bunnyTets.corner[0][2];
      expect_true(orientOk && linkOk && hull == 12 && allUsed, "bunny tet links");
      expect_true(std::fabs(vol - dx * dy * dz) <= 1e-5 * dx * dy * dz, "bunny tet volume");
      delete[] xyz;
    }
    bunnyTets.clear();
    bunny.clear();
  }

  {
    auto areaXY = [](const ntmesh<float>& mesh) {
      float a = 0.f;
      for (int f = 0; f < mesh.triangleCount(); ++f) {
        nvec3<float> p0 = mesh.points[mesh.triangleVertex(f, 0)];
        nvec3<float> p1 = mesh.points[mesh.triangleVertex(f, 1)];
        nvec3<float> p2 = mesh.points[mesh.triangleVertex(f, 2)];
        a += (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p1[1] - p0[1]) * (p2[0] - p0[0]);
      }
      return 0.5f * a;
    };
    auto hasEdge = [](const ntmesh<float>& mesh, int a, int b) {
      for (int f = 0; f < mesh.triangleCount(); ++f) {
        for (int k = 0; k < 3; ++k) {
          if (mesh.triangleVertex(f, k) == a && mesh.triangleVertex(f, (k + 1) % 3) == b) return true;
        }
      }
      return false;
    };

    ntmesh<float> quad;
    int q0 = quad.addPoint(nvec3<float>(0, 0, 0));
    int q1 = quad.addPoint(nvec3<float>(1, 0, 0));
    int q2 = quad.addPoint(nvec3<float>(1, 1, 0));
    int q3 = quad.addPoint(nvec3<float>(0, 1, 0));
    int square[] = {q0, q1, q2, q3};
    quad.addFace(square, 4);
    expect_true(quad.triangulate(), "quad triangulates");
    expect_true(quad.triangleCount() == 2 && quad.quadCount() == 0 && quad.ngonCount() == 0, "quad becomes two triangles");
    expect_true(std::fabs(areaXY(quad) - 1.f) < 1e-5f, "square area");
    expect_true(hasEdge(quad, q0, q1) && hasEdge(quad, q1, q2) && hasEdge(quad, q2, q3) && hasEdge(quad, q3, q0),
                "square boundary");
    quad.clear();

    ntmesh<float> dent;
    int d0 = dent.addPoint(nvec3<float>(0, 0, 0));
    int d1 = dent.addPoint(nvec3<float>(3, 0, 0));
    int d2 = dent.addPoint(nvec3<float>(1, 0.25f, 0));
    int d3 = dent.addPoint(nvec3<float>(3, 2, 0));
    int d4 = dent.addPoint(nvec3<float>(0, 2, 0));
    int dentFace[] = {d0, d1, d2, d3, d4};
    dent.addFace(dentFace, 5);
    float dentArea = 0.f;
    for (int k = 0; k < 5; ++k) {
      nvec3<float> a = dent.points[dentFace[k]];
      nvec3<float> b = dent.points[dentFace[(k + 1) % 5]];
      dentArea += a[0] * b[1] - b[0] * a[1];
    }
    dentArea *= 0.5f;
    expect_true(dent.triangulate(), "concave ngon triangulates");
    expect_true(dent.triangleCount() == 3 && dent.ngonCount() == 0, "concave ngon becomes three triangles");
    expect_true(std::fabs(areaXY(dent) - dentArea) < 1e-4f, "concave area is preserved");
    expect_true(hasEdge(dent, d0, d1) && hasEdge(dent, d1, d2) && hasEdge(dent, d2, d3) && hasEdge(dent, d3, d4) &&
                    hasEdge(dent, d4, d0),
                "concave boundary");
    dent.clear();

    SMesh mixed;
    int a = mixed.addPoint(nvec3<float>(0, 0, 0));
    int b = mixed.addPoint(nvec3<float>(1, 0, 0));
    int c = mixed.addPoint(nvec3<float>(0, 1, 0));
    int d = mixed.addPoint(nvec3<float>(1, 1, 0));
    int e = mixed.addPoint(nvec3<float>(2, 0, 0));
    int tri[] = {a, b, e};
    int face[] = {a, b, d, c};
    int line[] = {b, e};
    mixed.addFace(tri, 3);
    mixed.addFace(face, 4);
    mixed.addLine(line, 2);
    Attribute* vid = mixed.addVertexAttrib("id", Type::Int, 1);
    for (int k = 0; k < 3; ++k) vid->intAt(k)[0] = 1 + k;
    vid->intAt(3)[0] = 10;
    vid->intAt(4)[0] = 20;
    vid->intAt(5)[0] = 30;
    vid->intAt(6)[0] = 40;
    vid->intAt(7)[0] = 80;
    vid->intAt(8)[0] = 90;
    Attribute* pid = mixed.addFaceAttrib("pid", Type::Int, 1);
    pid->intAt(0)[0] = 5;
    pid->intAt(1)[0] = 7;
    pid->intAt(2)[0] = 9;
    mixed.setString(*mixed.addFaceString("name"), 1, "quad");
    mixed.setString(*mixed.addFaceString("name"), 2, "wire");
    expect_true(mixed.triangulate(), "mixed mesh triangulates");
    expect_true(mixed.triangleCount() == 3 && mixed.quadCount() == 0 && mixed.lineCount() == 1, "mixed lists");
    expect_true(mixed.lineVertex(0, 0) == b && mixed.lineVertex(0, 1) == e, "line stays");
    expect_true(vid->intAt(0)[0] == 1 && vid->intAt(1)[0] == 2 && vid->intAt(2)[0] == 3, "triangle corners stay");
    expect_true(vid->intAt(9)[0] == 80 && vid->intAt(10)[0] == 90, "line corners stay at the end");
    expect_true(pid->intAt(0)[0] == 5 && pid->intAt(1)[0] == 7 && pid->intAt(2)[0] == 7 && pid->intAt(3)[0] == 9,
                "prim values follow the split");
    expect_true(std::strcmp(mixed.getString(*mixed.findFaceString("name"), 1), "quad") == 0, "split prim string");
    expect_true(std::strcmp(mixed.getString(*mixed.findFaceString("name"), 3), "wire") == 0, "line string stays");
    int cornerValue[4] = {10, 20, 30, 40};
    int cornerPoint[4] = {a, b, d, c};
    for (int f = 1; f < 3; ++f) {
      for (int k = 0; k < 3; ++k) {
        int pt = mixed.triangleVertex(f, k);
        int expect = -1;
        for (int s = 0; s < 4; ++s)
          if (cornerPoint[s] == pt) expect = cornerValue[s];
        expect_true(vid->intAt(3 + (f - 1) * 3 + k)[0] == expect, "quad corner value follows the point");
      }
    }
    mixed.clear();

    ntmesh<float> asNgon;
    int n0 = asNgon.addPoint(nvec3<float>(0, 0, 0));
    int n1 = asNgon.addPoint(nvec3<float>(1, 0, 0));
    int n2 = asNgon.addPoint(nvec3<float>(0, 1, 0));
    int packed[] = {n0, n1, n2};
    asNgon.addFace(packed, 3, false);
    expect_true(asNgon.triangulate(), "triangle stored as an ngon triangulates");
    expect_true(asNgon.triangleCount() == 1 && asNgon.ngonCount() == 0 && asNgon.triangleVertex(0, 2) == n2,
                "stored triangle ngon becomes a triangle");
    asNgon.clear();

    ntmesh<float> quadsStay;
    int s0 = quadsStay.addPoint(nvec3<float>(0, 0, 0));
    int s1 = quadsStay.addPoint(nvec3<float>(1, 0, 0));
    int s2 = quadsStay.addPoint(nvec3<float>(1, 1, 0));
    int s3 = quadsStay.addPoint(nvec3<float>(0, 1, 0));
    int s4 = quadsStay.addPoint(nvec3<float>(3, 0, 0));
    int s5 = quadsStay.addPoint(nvec3<float>(1, 0.25f, 0));
    int s6 = quadsStay.addPoint(nvec3<float>(3, 2, 0));
    int s7 = quadsStay.addPoint(nvec3<float>(0, 2, 0));
    int stayQuad[] = {s0, s1, s2, s3};
    int stayNgon[] = {s0, s4, s5, s6, s7};
    quadsStay.addFace(stayQuad, 4);
    quadsStay.addFace(stayNgon, 5);
    float ngonArea = 0.f;
    for (int k = 0; k < 5; ++k) {
      nvec3<float> pa = quadsStay.points[stayNgon[k]];
      nvec3<float> pb = quadsStay.points[stayNgon[(k + 1) % 5]];
      ngonArea += pa[0] * pb[1] - pb[0] * pa[1];
    }
    ngonArea *= 0.5f;
    expect_true(quadsStay.triangulateNgons(), "ngon triangulates beside a quad");
    expect_true(quadsStay.quadCount() == 1 && quadsStay.ngonCount() == 0 && quadsStay.triangleCount() == 3,
                "ngons split and the quad list stays");
    expect_true(quadsStay.quadVertex(0, 0) == s0 && quadsStay.quadVertex(0, 1) == s1 &&
                    quadsStay.quadVertex(0, 2) == s2 && quadsStay.quadVertex(0, 3) == s3,
                "quad corners stay in order");
    expect_true(std::fabs(areaXY(quadsStay) - ngonArea) < 1e-4f, "ngon area moves onto the triangles");
    expect_true(hasEdge(quadsStay, s0, s4) && hasEdge(quadsStay, s4, s5) && hasEdge(quadsStay, s5, s6) &&
                    hasEdge(quadsStay, s6, s7) && hasEdge(quadsStay, s7, s0),
                "ngon boundary is on the new triangles");
    quadsStay.clear();

    SMesh ngonSplit;
    int g0 = ngonSplit.addPoint(nvec3<float>(0, 0, 0));
    int g1 = ngonSplit.addPoint(nvec3<float>(1, 0, 0));
    int g2 = ngonSplit.addPoint(nvec3<float>(0, 1, 0));
    int g3 = ngonSplit.addPoint(nvec3<float>(1, 1, 0));
    int g4 = ngonSplit.addPoint(nvec3<float>(2, 0, 0));
    int g5 = ngonSplit.addPoint(nvec3<float>(2, 1, 0));
    int g6 = ngonSplit.addPoint(nvec3<float>(2, 2, 0));
    int keepTri[] = {g0, g1, g2};
    int keepQuad[] = {g0, g1, g3, g2};
    int splitNgon[] = {g0, g4, g5, g6, g2};
    int keepLine[] = {g1, g4};
    ngonSplit.addFace(keepTri, 3);
    ngonSplit.addFace(keepQuad, 4);
    ngonSplit.addFace(splitNgon, 5);
    ngonSplit.addLine(keepLine, 2);
    Attribute* splitV = ngonSplit.addVertexAttrib("id", Type::Int, 1);
    for (int i = 0; i < ngonSplit.vertexCount(); ++i) splitV->intAt(i)[0] = 100 + i;
    Attribute* splitP = ngonSplit.addFaceAttrib("pid", Type::Int, 1);
    for (int f = 0; f < ngonSplit.primCount(); ++f) splitP->intAt(f)[0] = 50 + f;
    ngonSplit.setString(*ngonSplit.addFaceString("name"), 1, "quad");
    ngonSplit.setString(*ngonSplit.addFaceString("name"), 2, "ngon");
    ngonSplit.setString(*ngonSplit.addFaceString("name"), 3, "wire");
    expect_true(ngonSplit.triangulateNgons(), "attributed ngon triangulates");
    expect_true(ngonSplit.triangleCount() == 4 && ngonSplit.quadCount() == 1 && ngonSplit.ngonCount() == 0 &&
                    ngonSplit.lineCount() == 1,
                "ngon split keeps the quad and the line");
    expect_true(ngonSplit.triangleVertex(0, 0) == g0 && ngonSplit.triangleVertex(0, 2) == g2, "first triangle stays");
    expect_true(ngonSplit.quadVertex(0, 0) == g0 && ngonSplit.quadVertex(0, 1) == g1 && ngonSplit.quadVertex(0, 2) == g3 &&
                    ngonSplit.quadVertex(0, 3) == g2,
                "quad corners stay after the ngon split");
    expect_true(ngonSplit.lineVertex(0, 0) == g1 && ngonSplit.lineVertex(0, 1) == g4, "line stays after the ngon split");
    expect_true(splitV->intAt(0)[0] == 100 && splitV->intAt(1)[0] == 101 && splitV->intAt(2)[0] == 102,
                "triangle corner values stay");
    expect_true(splitV->intAt(12)[0] == 103 && splitV->intAt(13)[0] == 104 && splitV->intAt(14)[0] == 105 &&
                    splitV->intAt(15)[0] == 106,
                "quad corner values stay behind the new triangles");
    expect_true(splitV->intAt(16)[0] == 112 && splitV->intAt(17)[0] == 113, "line corner values stay at the end");
    int ngonValue[5] = {107, 108, 109, 110, 111};
    int ngonPoint[5] = {g0, g4, g5, g6, g2};
    for (int f = 1; f < 4; ++f) {
      for (int k = 0; k < 3; ++k) {
        int pt = ngonSplit.triangleVertex(f, k);
        int expect = -1;
        for (int s = 0; s < 5; ++s)
          if (ngonPoint[s] == pt) expect = ngonValue[s];
        expect_true(splitV->intAt(3 + (f - 1) * 3 + k)[0] == expect, "ngon corner value follows the point");
      }
    }
    expect_true(splitP->intAt(0)[0] == 50 && splitP->intAt(1)[0] == 52 && splitP->intAt(2)[0] == 52 &&
                    splitP->intAt(3)[0] == 52 && splitP->intAt(4)[0] == 51 && splitP->intAt(5)[0] == 53,
                "prim values keep quads behind the split ngons");
    expect_true(std::strcmp(ngonSplit.getString(*ngonSplit.findFaceString("name"), 1), "ngon") == 0, "split ngon string");
    expect_true(std::strcmp(ngonSplit.getString(*ngonSplit.findFaceString("name"), 4), "quad") == 0, "quad string stays");
    expect_true(std::strcmp(ngonSplit.getString(*ngonSplit.findFaceString("name"), 5), "wire") == 0, "line string stays after ngons");
    ngonSplit.clear();

    SMesh turned;
    int t0 = turned.addPoint(nvec3<float>(0, 0, 0));
    int t1 = turned.addPoint(nvec3<float>(1, 0, 0));
    int t2 = turned.addPoint(nvec3<float>(1, 1, 0));
    int t3 = turned.addPoint(nvec3<float>(0, 1, 0));
    int t4 = turned.addPoint(nvec3<float>(0, 2, 0));
    int t5 = turned.addPoint(nvec3<float>(2, 0, 0));
    int triR[] = {t0, t1, t2};
    int quadR[] = {t0, t1, t2, t3};
    int ngonR[] = {t0, t1, t2, t3, t4};
    int lineR[] = {t1, t3, t5};
    turned.addFace(triR, 3);
    turned.addFace(quadR, 4);
    turned.addFace(ngonR, 5);
    turned.addLine(lineR, 3);
    Attribute* corner = turned.addVertexAttrib("id", Type::Int, 1);
    for (int i = 0; i < turned.vertexCount(); ++i) corner->intAt(i)[0] = 100 + i;
    Attribute* faceId = turned.addFaceAttrib("pid", Type::Int, 1);
    for (int f = 0; f < turned.primCount(); ++f) faceId->intAt(f)[0] = 50 + f;
    turned.setString(*turned.addVertexString("tag"), 0, "first");
    turned.setString(*turned.addVertexString("tag"), 2, "last");
    turned.reverse();
    expect_true(turned.triangleVertex(0, 0) == t2 && turned.triangleVertex(0, 1) == t1 && turned.triangleVertex(0, 2) == t0,
                "triangle winding flips");
    expect_true(turned.quadVertex(0, 0) == t3 && turned.quadVertex(0, 3) == t0, "quad winding flips");
    expect_true(turned.ngonVertex(0, 0) == t4 && turned.ngonVertex(0, 4) == t0, "ngon winding flips");
    expect_true(turned.lineVertex(0, 0) == t5 && turned.lineVertex(0, 2) == t1, "line direction flips");
    expect_true(corner->intAt(0)[0] == 102 && corner->intAt(2)[0] == 100, "triangle corner values flip");
    expect_true(corner->intAt(3)[0] == 106 && corner->intAt(6)[0] == 103, "quad corner values flip");
    expect_true(faceId->intAt(0)[0] == 50 && faceId->intAt(1)[0] == 51 && faceId->intAt(3)[0] == 53,
                "prim values stay");
    expect_true(std::strcmp(turned.getString(*turned.findVertexString("tag"), 0), "last") == 0, "vertex string flips");
    expect_true(std::strcmp(turned.getString(*turned.findVertexString("tag"), 2), "first") == 0, "vertex string lands on the last corner");
    turned.reverse();
    expect_true(turned.triangleVertex(0, 0) == t0 && turned.triangleVertex(0, 2) == t2, "second reverse restores the triangle");
    expect_true(turned.lineVertex(0, 0) == t1 && corner->intAt(0)[0] == 100, "second reverse restores corners");
    turned.clear();

    SMesh loose;
    int u0 = loose.addPoint(nvec3<float>(0, 0, 0));
    int u1 = loose.addPoint(nvec3<float>(9, 0, 0));
    int u2 = loose.addPoint(nvec3<float>(0, 1, 0));
    int u3 = loose.addPoint(nvec3<float>(8, 0, 0));
    int u4 = loose.addPoint(nvec3<float>(2, 0, 0));
    int triU[] = {u0, u2, u0};
    int lineU[] = {u4, u2};
    loose.addFace(triU, 3);
    loose.addLine(lineU, 2);
    Attribute* pointId = loose.addPointAttrib("id", Type::Int, 1);
    pointId->intAt(u0)[0] = 10;
    pointId->intAt(u1)[0] = 11;
    pointId->intAt(u2)[0] = 12;
    pointId->intAt(u3)[0] = 13;
    pointId->intAt(u4)[0] = 14;
    loose.setString(*loose.addPointString("mat"), u1, "gone");
    loose.setString(*loose.addPointString("mat"), u2, "oak");
    loose.removeUnusedPoints();
    expect_true(loose.pointCount() == 3, "unused points drop");
    expect_vec(loose.points[0], nvec3<float>(0, 0, 0), 0.f, "first used point stays");
    expect_vec(loose.points[1], nvec3<float>(0, 1, 0), 0.f, "second used point slides down");
    expect_vec(loose.points[2], nvec3<float>(2, 0, 0), 0.f, "line point follows");
    expect_true(loose.triangleVertex(0, 0) == 0 && loose.triangleVertex(0, 1) == 1 && loose.triangleVertex(0, 2) == 0,
                "triangle indices remap");
    expect_true(loose.lineVertex(0, 0) == 2 && loose.lineVertex(0, 1) == 1, "line indices remap");
    expect_true(pointId->count == 3 && pointId->intAt(0)[0] == 10 && pointId->intAt(1)[0] == 12 && pointId->intAt(2)[0] == 14,
                "point attribs follow survivors");
    expect_true(std::strcmp(loose.getString(*loose.findPointString("mat"), 1), "oak") == 0, "point string follows");
    loose.removeUnusedPoints();
    expect_true(loose.pointCount() == 3 && loose.triangleVertex(0, 1) == 1, "second pass finds nothing to drop");
    loose.clear();

    SMesh onlyPoints;
    onlyPoints.addPoint(nvec3<float>(1, 0, 0));
    onlyPoints.addPoint(nvec3<float>(2, 0, 0));
    onlyPoints.removeUnusedPoints();
    expect_true(onlyPoints.pointCount() == 0, "unreferenced points all drop");
    onlyPoints.clear();
  }

  {
    ntmesh<float> sheet;
    int s0 = sheet.addPoint(nvec3<float>(0, 0, 0));
    int s1 = sheet.addPoint(nvec3<float>(2, 1, 0));
    int s2 = sheet.addPoint(nvec3<float>(0, 2, 0));
    int s3 = sheet.addPoint(nvec3<float>(1, 1, 0));
    int dent[] = {s0, s1, s2, s3};
    sheet.addFace(dent, 4);
    expect_true(std::fabs(sheet.totalSurfaceArea() - 1.f) < 1e-4f, "concave quad area");
    expect_true(std::fabs(sheet.totalVolume()) < 1e-5f, "flat face has no volume");
    expect_true(sheet.isClosed() == 0, "open sheet is not closed");
    sheet.clear();

    ntmesh<float> cube;
    nvec3<float> cpts[8] = {
        nvec3<float>(0, 0, 0), nvec3<float>(1, 0, 0), nvec3<float>(1, 1, 0), nvec3<float>(0, 1, 0),
        nvec3<float>(0, 0, 1), nvec3<float>(1, 0, 1), nvec3<float>(1, 1, 1), nvec3<float>(0, 1, 1)};
    int cid[8];
    for (int i = 0; i < 8; ++i) cid[i] = cube.addPoint(cpts[i]);
    int faces[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5}};
    for (int f = 0; f < 6; ++f) {
      int q[4] = {cid[faces[f][0]], cid[faces[f][1]], cid[faces[f][2]], cid[faces[f][3]]};
      cube.addFace(q, 4);
    }
    expect_true(cube.isClosed() == 1, "cube is closed");
    expect_true(std::fabs(cube.totalSurfaceArea() - 6.f) < 1e-4f, "cube area");
    expect_true(std::fabs(cube.totalVolume() + 1.f) < 1e-4f, "cube volume");
    int lineC[] = {cid[0], cid[6]};
    cube.addLine(lineC, 2);
    expect_true(cube.isClosed() == 1 && std::fabs(cube.totalSurfaceArea() - 6.f) < 1e-4f, "a line does not open the cube");
    cube.reverse();
    expect_true(cube.isClosed() == 1 && std::fabs(cube.totalVolume() - 1.f) < 1e-4f, "reversed cube has positive volume");
    cube.clear();

    ntmesh<float> pair;
    int a = pair.addPoint(nvec3<float>(0, 0, 0));
    int b = pair.addPoint(nvec3<float>(1, 0, 0));
    int c = pair.addPoint(nvec3<float>(0, 1, 0));
    int front[] = {a, b, c};
    int back[] = {a, c, b};
    pair.addFace(front, 3);
    pair.addFace(back, 3);
    expect_true(pair.isClosed() == 1, "opposite triangles close");
    expect_true(std::fabs(pair.totalSurfaceArea() - 1.f) < 1e-4f, "both sides count as area");
    expect_true(std::fabs(pair.totalVolume()) < 1e-5f, "opposite faces cancel");
    pair.clear();
  }

  {
    ntmesh<float> tri;
    int t0 = tri.addPoint(nvec3<float>(0, 0, 0));
    int t1 = tri.addPoint(nvec3<float>(1, 0, 0));
    int t2 = tri.addPoint(nvec3<float>(0, 1, 0));
    int corners[] = {t0, t1, t2};
    tri.addFace(corners, 3);
    int two[] = {t0, t1};
    expect_vec(tri.newell(corners, 3), nvec3<float>(0, 0, -1), 1e-5f, "triangle newell length is twice the area");
    expect_vec(tri.faceNormal(0), nvec3<float>(0, 0, -1), 1e-5f, "triangle face normal");
    expect_vec(tri.newell(two, 2), nvec3<float>(0, 0, 0), 0.f, "short span has no newell");
    tri.addLine(two, 2);
    expect_vec(tri.faceNormal(1), nvec3<float>(0, 0, 0), 0.f, "a line has no face normal");
    expect_vec(tri.faceNormal(-1), nvec3<float>(0, 0, 0), 0.f, "out of range face normal");
    tri.reverse();
    expect_vec(tri.faceNormal(0), nvec3<float>(0, 0, 1), 1e-5f, "reverse flips the face normal");
    tri.clear();

    AMesh weighted;
    int w0 = weighted.addPoint(nvec3<float>(0, 0, 0));
    int w1 = weighted.addPoint(nvec3<float>(2, 0, 0));
    int w2 = weighted.addPoint(nvec3<float>(0, 2, 0));
    int w3 = weighted.addPoint(nvec3<float>(0, 0, 1));
    int w4 = weighted.addPoint(nvec3<float>(0, 1, 0));
    weighted.addPoint(nvec3<float>(5, 5, 5));
    int big[] = {w0, w1, w2};
    int small[] = {w0, w3, w4};
    weighted.addFace(big, 3);
    weighted.addFace(small, 3);
    Attribute* pointN = ntm::computePointNormals(weighted);
    nvec3<float> lean(1.f, 0.f, -4.f);
    lean.normalize();
    expect_true(pointN && pointN->kind == Type::Normal && pointN->tupleSize == 3, "point N column");
    expect_vec(nvec3<float>(pointN->floatAt(0)[0], pointN->floatAt(0)[1], pointN->floatAt(0)[2]), lean, 1e-5f,
               "large face weighs more");
    expect_vec(nvec3<float>(pointN->floatAt(1)[0], pointN->floatAt(1)[1], pointN->floatAt(1)[2]), nvec3<float>(0, 0, -1), 1e-5f,
               "point on the large face only");
    expect_vec(nvec3<float>(pointN->floatAt(3)[0], pointN->floatAt(3)[1], pointN->floatAt(3)[2]), nvec3<float>(1, 0, 0), 1e-5f,
               "point on the small face only");
    expect_vec(nvec3<float>(pointN->floatAt(5)[0], pointN->floatAt(5)[1], pointN->floatAt(5)[2]), nvec3<float>(0, 0, 0), 0.f,
               "unused point stays zero");
    weighted.clear();

    AMesh cube;
    nvec3<float> cpts[8] = {
        nvec3<float>(0, 0, 0), nvec3<float>(1, 0, 0), nvec3<float>(1, 1, 0), nvec3<float>(0, 1, 0),
        nvec3<float>(0, 0, 1), nvec3<float>(1, 0, 1), nvec3<float>(1, 1, 1), nvec3<float>(0, 1, 1)};
    int cid[8];
    for (int i = 0; i < 8; ++i) cid[i] = cube.addPoint(cpts[i]);
    int faces[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5}};
    for (int f = 0; f < 6; ++f) {
      int q[4] = {cid[faces[f][0]], cid[faces[f][1]], cid[faces[f][2]], cid[faces[f][3]]};
      cube.addFace(q, 4);
    }
    nvec3<float> corner(-1.f, -1.f, -1.f);
    corner.normalize();
    Attribute* flat = ntm::computeVertexNormals(cube, 0.f);
    expect_vec(nvec3<float>(flat->floatAt(0)[0], flat->floatAt(0)[1], flat->floatAt(0)[2]), nvec3<float>(0, 0, 1), 1e-5f,
               "cusp zero keeps the face normal");
    Attribute* smooth = ntm::computeVertexNormals(cube, 3.14159265f);
    expect_vec(nvec3<float>(smooth->floatAt(22)[0], smooth->floatAt(22)[1], smooth->floatAt(22)[2]), corner, 1e-5f,
               "cusp of pi gathers every face at the point");
    Attribute* cubeN = ntm::computePointNormals(cube);
    expect_vec(nvec3<float>(cubeN->floatAt(6)[0], cubeN->floatAt(6)[1], cubeN->floatAt(6)[2]), corner, 1e-5f,
               "cube corner point normal");
    int edge[] = {cid[0], cid[6]};
    cube.addLine(edge, 2);
    Attribute* faceN = ntm::computeFaceNormals(cube);
    expect_true(faceN && faceN->count == 7, "face N covers the line too");
    expect_vec(nvec3<float>(faceN->floatAt(0)[0], faceN->floatAt(0)[1], faceN->floatAt(0)[2]), nvec3<float>(0, 0, 1), 1e-5f,
               "first cube face normal");
    expect_vec(nvec3<float>(faceN->floatAt(5)[0], faceN->floatAt(5)[1], faceN->floatAt(5)[2]), nvec3<float>(-1, 0, 0), 1e-5f,
               "last cube face normal");
    expect_vec(nvec3<float>(faceN->floatAt(6)[0], faceN->floatAt(6)[1], faceN->floatAt(6)[2]), nvec3<float>(0, 0, 0), 0.f,
               "line prim normal stays zero");
    cube.clear();
  }

  {
    using ntm::detail::inSphere;
    using ntm::detail::orient3d;
    double unit[15] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0.2, 0.2, 0.2};
    expect_true(orient3d(unit, 0, 1, 2, 3) == 1, "right-handed tet is positive");
    expect_true(orient3d(unit, 0, 1, 3, 2) == -1, "swapping two corners flips orient");
    expect_true(inSphere(unit, 0, 1, 2, 3, 4) == 1, "a point inside the corner tet is inside its sphere");
    unit[12] = 0.5;
    unit[13] = 0.5;
    unit[14] = 0.5;
    expect_true(inSphere(unit, 0, 1, 2, 3, 4) == 1, "the circumcenter is inside the sphere");
    unit[12] = 2;
    unit[13] = 2;
    unit[14] = 2;
    expect_true(inSphere(unit, 0, 1, 2, 3, 4) == -1, "a far point is outside the sphere");
    unit[12] = 1;
    unit[13] = 1;
    unit[14] = 0;
    int onSphere = inSphere(unit, 0, 1, 2, 3, 4);
    int onSwap = inSphere(unit, 1, 0, 2, 3, 4);
    expect_true(onSphere != 0 && onSwap == -onSphere, "a cospherical tie breaks and flips when swapped");

    double flat[12] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    expect_true(orient3d(flat, 0, 1, 2, 3) == -orient3d(flat, 1, 0, 2, 3), "coplanar orient flips");
    expect_true(orient3d(flat, 0, 1, 2, 3) != 0, "coplanar orient still picks a side");
    double same[12] = {1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3};
    expect_true(orient3d(same, 0, 1, 2, 3) == -orient3d(same, 0, 1, 3, 2), "duplicate points still orient");
    expect_true(orient3d(same, 0, 1, 2, 3) == orient3d(same, 1, 0, 3, 2), "an even swap of duplicates keeps the sign");

    auto checkMesh = [](const char* name, const nvec3<float>* pts, int n, ntm::TetMesh<float>& mesh) {
      expect_true(mesh.pointCount == n && mesh.count > 0, name);
      if (mesh.pointCount != n || mesh.count <= 0) return;
      float minx = pts[0][0], maxx = minx, miny = pts[0][1], maxy = miny, minz = pts[0][2], maxz = minz;
      for (int i = 1; i < n; ++i) {
        if (pts[i][0] < minx) minx = pts[i][0];
        if (pts[i][0] > maxx) maxx = pts[i][0];
        if (pts[i][1] < miny) miny = pts[i][1];
        if (pts[i][1] > maxy) maxy = pts[i][1];
        if (pts[i][2] < minz) minz = pts[i][2];
        if (pts[i][2] > maxz) maxz = pts[i][2];
      }
      expect_true(mesh.corner[0][0] < minx && mesh.corner[0][1] < miny && mesh.corner[0][2] < minz, name);
      expect_true(mesh.corner[6][0] > maxx && mesh.corner[6][1] > maxy && mesh.corner[6][2] > maxz, name);

      int nsite = n + 8;
      double* xyz = new double[(size_t)nsite * 3];
      for (int i = 0; i < nsite; ++i) {
        nvec3<float> p = mesh.position(pts, i);
        xyz[3 * i] = p[0];
        xyz[3 * i + 1] = p[1];
        xyz[3 * i + 2] = p[2];
      }
      char* used = new char[(size_t)nsite];
      for (int i = 0; i < nsite; ++i) used[i] = 0;
      int hull = 0;
      double vol = 0.0;
      bool orientOk = true, linkOk = true, indexOk = true, insideOk = true, sphereOk = true;
      for (int ti = 0; ti < mesh.count; ++ti) {
        const ntm::Tet& tet = mesh.tets[ti];
        for (int k = 0; k < 4; ++k) {
          if (tet.v[k] < 0 || tet.v[k] >= nsite) indexOk = false;
          else used[tet.v[k]] = 1;
          int nb = tet.n[k];
          if (nb < 0) {
            ++hull;
            continue;
          }
          if (nb >= mesh.count) {
            linkOk = false;
            continue;
          }
          int back = 0;
          for (int s = 0; s < 4; ++s)
            if (mesh.tets[nb].n[s] == ti) ++back;
          if (back != 1) linkOk = false;
        }
        if (orient3d(xyz, tet.v[0], tet.v[1], tet.v[2], tet.v[3]) <= 0) orientOk = false;
        nvec3<double> a(xyz[3 * tet.v[0]], xyz[3 * tet.v[0] + 1], xyz[3 * tet.v[0] + 2]);
        nvec3<double> b(xyz[3 * tet.v[1]], xyz[3 * tet.v[1] + 1], xyz[3 * tet.v[1] + 2]);
        nvec3<double> c(xyz[3 * tet.v[2]], xyz[3 * tet.v[2] + 1], xyz[3 * tet.v[2] + 2]);
        nvec3<double> d(xyz[3 * tet.v[3]], xyz[3 * tet.v[3] + 1], xyz[3 * tet.v[3] + 2]);
        vol += (b - a).cross(c - a).dot(d - a) / 6.0;
        for (int s = 0; s < nsite; ++s) {
          bool vert = s == tet.v[0] || s == tet.v[1] || s == tet.v[2] || s == tet.v[3];
          if (vert) continue;
          int pos = 0;
          int slots[4][3] = {{1, 3, 2}, {0, 2, 3}, {0, 3, 1}, {0, 1, 2}};
          for (int k = 0; k < 4; ++k) {
            int side = orient3d(xyz, tet.v[slots[k][0]], tet.v[slots[k][1]], tet.v[slots[k][2]], s);
            if (side > 0) ++pos;
          }
          if (pos == 4) insideOk = false;
          if (ntm::detail::inSphereGeom(xyz, tet.v[0], tet.v[1], tet.v[2], tet.v[3], s) > 0) sphereOk = false;
        }
      }
      bool allUsed = true;
      for (int i = 0; i < nsite; ++i)
        if (!used[i]) allUsed = false;
      double dx = (double)mesh.corner[6][0] - (double)mesh.corner[0][0];
      double dy = (double)mesh.corner[6][1] - (double)mesh.corner[0][1];
      double dz = (double)mesh.corner[6][2] - (double)mesh.corner[0][2];
      double cube = dx * dy * dz;
      bool volOk = std::fabs(vol - cube) <= 1e-5 * cube;
      expect_true(indexOk, name);
      expect_true(orientOk, name);
      expect_true(linkOk, name);
      expect_true(hull == 12, name);
      expect_true(allUsed, name);
      expect_true(insideOk, name);
      expect_true(sphereOk, name);
      expect_true(volOk, name);
      delete[] xyz;
      delete[] used;
    };

    ntm::TetMesh<float> tets;
    expect_true(tets.count == 0, "a new tet mesh is empty");
    tets.build(nullptr, 0);
    expect_true(tets.count == 0 && tets.pointCount == 0, "an empty build stays empty");
    nvec3<float> one(0.25f, -0.5f, 0.125f);
    tets.build(&one, 1);
    checkMesh("one point", &one, 1, tets);
    ntm::TetMesh<float> share = tets;
    expect_true(share.tets == tets.tets && share.count == tets.count, "a copy shares the tet array");
    tets.clear();
    expect_true(tets.tets == nullptr && tets.count == 0, "clear drops the tet array");

    nvec3<float> dup[5];
    for (int i = 0; i < 5; ++i) dup[i] = nvec3<float>(0.2f, 0.2f, 0.2f);
    tets.build(dup, 5);
    checkMesh("five duplicates", dup, 5, tets);
    tets.clear();

    nvec3<float> grid[27];
    int ng = 0;
    for (int z = 0; z < 3; ++z)
      for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) grid[ng++] = nvec3<float>(x * 0.5f, y * 0.5f, z * 0.5f);
    tets.build(grid, 27);
    checkMesh("3x3x3 grid", grid, 27, tets);
    tets.clear();

    nvec3<float> rnd[25];
    unsigned seed = 1;
    for (int i = 0; i < 25; ++i) {
      float p[3];
      for (int a = 0; a < 3; ++a) {
        seed = seed * 1664525u + 1013904223u;
        p[a] = (seed >> 8) * (1.f / 16777216.f);
      }
      rnd[i] = nvec3<float>(p[0], p[1], p[2]);
    }
    rnd[24] = rnd[3];
    tets.build(rnd, 25);
    checkMesh("random points with a duplicate", rnd, 25, tets);
    tets.clear();
  }

  const int nfail = g_fails - fails_before;
  if (nfail == 0)
    std::cout << "ntmesh: ok\n";
  else
    std::cout << "ntmesh: " << nfail << " failed\n";
}
