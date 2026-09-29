/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/vsg/ecs/FeatureBuilder.h>
#include <rocky/weemesh.h>
#include <algorithm>
#include <cmath>

using namespace ROCKY_NAMESPACE;

// Detailed boundaries must keep accepting cuts after their vertex indices exceed 16 bits.
TEST_CASE("tessellation supports more than 65535 vertices", "[tessellation]")
{
    weemesh::mesh_t mesh;
    int count = 0;
    for (; count < 65536; ++count)
        if (mesh.get_or_create_vertex({ double(count), -10.0, 0.0 }, 0) != count)
            break;
    REQUIRE(count == 65536);

    const int a = mesh.get_or_create_vertex({ 0.0, 0.0, 0.0 }, 0);
    const int b = mesh.get_or_create_vertex({ 10.0, 0.0, 0.0 }, 0);
    const int c = mesh.get_or_create_vertex({ 0.0, 10.0, 0.0 }, 0);
    REQUIRE(a == 65536);
    REQUIRE(b == 65537);
    REQUIRE(c == 65538);
    REQUIRE(mesh.add_triangle(a, b, c) >= 0);
    mesh.insert(weemesh::vert_t{ 2.0, 2.0, 0.0 }, 0);
    CHECK(mesh.triangles.size() == 3u);
    CHECK(mesh.verts.size() == 65540u);
}

// Barycentric weights must remain accurate for long, nearly parallel edges in either winding.
TEST_CASE("tessellation computes stable weights for narrow triangles", "[tessellation]")
{
    weemesh::triangle_t triangle;
    triangle.p0 = { 10000.0, 0.0, 0.0 };
    triangle.p1 = { 0.0, 0.001, 0.0 };
    triangle.p2 = { 0.0, 0.0, 0.0 };
    double expectedY = 0.3, expectedZ = 0.5;
    SECTION("clockwise")
    {
        std::swap(triangle.p1, triangle.p2);
        std::swap(expectedY, expectedZ);
    }
    SECTION("counterclockwise") { }
    weemesh::vert_t weights;
    REQUIRE(triangle.get_barycentric({ 2000.0, 0.0003, 0.0 }, weights, weemesh::DEFAULT_EPSILON));
    CHECK(weights.x == Approx(0.2));
    CHECK(weights.y == Approx(expectedY));
    CHECK(weights.z == Approx(expectedZ));
}

// Inserting near an edge must conserve area instead of discarding a thin child triangle.
TEST_CASE("tessellation preserves area for points close to edges", "[tessellation]")
{
    weemesh::mesh_t mesh;
    const int a = mesh.get_or_create_vertex({ 0.0, 0.0, 0.0 }, 0);
    const int b = mesh.get_or_create_vertex({ 100.0, 0.0, 0.0 }, 0);
    const int c = mesh.get_or_create_vertex({ 0.0, 100.0, 0.0 }, 0);
    mesh.add_triangle(a, b, c);
    double y = 0.00005;
    std::size_t expectedTriangles = 3u;
    SECTION("close to edge") { }
    SECTION("exactly on edge")
    {
        y = 0.0;
        expectedTriangles = 2u;
    }
    mesh.insert(weemesh::vert_t{ 50.0, y, 0.0 }, 0);
    REQUIRE(mesh.triangles.size() == expectedTriangles);
    double area = 0.0;
    for (const auto& entry : mesh.triangles)
    {
        const auto& tri = entry.second;
        area += std::abs((tri.p1 - tri.p0).cross2d(tri.p2 - tri.p0)) * 0.5;
    }
    CHECK(area == Approx(5000.0).epsilon(1e-12));
}

// Exercise boundary clipping through FeatureBuilder using a thin concave polygon.
TEST_CASE("polygon tessellation clips narrow concavities", "[featurebuilder][tessellation]")
{
    PolygonGeometry polygon;
    polygon.polygons.push_back(PolygonPart{
        { { 0.0, 0.0, 0.0 }, { 10000.0, 0.0, 0.0 }, { 10000.0, 1.0, 0.0 }, { 100.0, 1.0, 0.0 },
          { 100.0, 2.0, 0.0 }, { 10000.0, 2.0, 0.0 }, { 10000.0, 3.0, 0.0 }, { 0.0, 3.0, 0.0 } }, {} });
    MeshGeometry mesh;
    FeatureBuilder builder;
    builder.buildMeshGeometry(polygon, PolygonStyle{}, mesh);
    REQUIRE_FALSE(mesh.indices.empty());
    double area = 0.0;
    for (std::size_t i = 0u; i < mesh.indices.size(); i += 3u)
    {
        const auto a = mesh.vertices[mesh.indices[i]];
        const auto b = mesh.vertices[mesh.indices[i + 1u]];
        const auto c = mesh.vertices[mesh.indices[i + 2u]];
        area += std::abs(glm::cross(b - a, c - a).z) * 0.5;
    }
    CHECK(area == Approx(20100.0).epsilon(1e-9));
}

// A narrow triangle can still cover a substantial area and must accept boundary cuts.
TEST_CASE("tessellation cuts narrow triangles", "[tessellation]")
{
    weemesh::mesh_t mesh;
    const int a = mesh.get_or_create_vertex({ 0.0, 0.0, 0.0 }, 0);
    const int b = mesh.get_or_create_vertex({ 10000.0, 0.0, 0.0 }, 0);
    const int c = mesh.get_or_create_vertex({ 0.0, 1.0, 0.0 }, 0);
    SECTION("counterclockwise") { mesh.add_triangle(a, b, c); }
    SECTION("clockwise") { mesh.add_triangle(a, c, b); }
    mesh.insert(weemesh::segment_t({ 5000.0, -1.0, 0.0 }, { 5000.0, 2.0, 0.0 }), 0);

    REQUIRE(mesh.triangles.size() > 1u);
    double area = 0.0;
    for (const auto& entry : mesh.triangles)
    {
        const auto& tri = entry.second;
        CHECK_FALSE((tri.a_min[0] < 5000.0 && tri.a_max[0] > 5000.0));
        area += std::abs((tri.p1 - tri.p0).cross2d(tri.p2 - tri.p0)) * 0.5;
    }
    CHECK(area == Approx(5000.0));
}

// Collinear XY triangles may represent vertical surfaces and must survive insertion unchanged.
TEST_CASE("tessellation preserves vertical triangles", "[tessellation]")
{
    weemesh::mesh_t mesh;
    const int a = mesh.get_or_create_vertex({ 0.0, 0.0, 0.0 }, 0);
    const int b = mesh.get_or_create_vertex({ 100.0, 0.0, 0.0 }, 0);
    const int c = mesh.get_or_create_vertex({ 50.0, 0.0, 100.0 }, 0);
    const auto id = mesh.add_triangle(a, b, c);
    REQUIRE(id >= 0);
    REQUIRE(mesh.triangles.at(id).is_2d_degenerate);
    weemesh::vert_t weights;
    CHECK_FALSE(mesh.triangles.at(id).get_barycentric({ 50.0, 0.0, 0.0 }, weights, mesh.epsilon));
    mesh.insert(weemesh::segment_t({ 25.0, -1.0, 0.0 }, { 25.0, 1.0, 0.0 }), 0);
    REQUIRE(mesh.triangles.size() == 1u);
    CHECK(mesh.triangles.at(id).i2 == c);
    CHECK(mesh.verts[c].z == 100.0);
}
