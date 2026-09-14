/**************************************************************************/
/*  test_trenchbroom_brush_compiler.cpp                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tests/test_macros.h"
TEST_FORCE_LINK(test_trenchbroom_brush_compiler)
#ifdef TOOLS_ENABLED
#include "editor/import/3d/trenchbroom_brush_compiler.h"

#include <limits>
namespace TestTrenchBroomBrushCompiler {
using Parser = TrenchBroomMapParser;
using Compiler = TrenchBroomBrushCompiler;
static const char *fixture = R"MAP(// Game: fixture
// Format: Standard
{
"classname" "worldspawn"
"message" "A \"quoted\" room"
{
( 0 0 0 ) ( 0 32 0 ) ( 32 32 0 ) stone/floor 8 -4 90 0.5 -2
( 0 0 32 ) ( 32 32 32 ) ( 0 32 32 ) stone/ceiling 0 0 0 1 1
( 0 0 0 ) ( 32 0 32 ) ( 0 0 32 ) stone/wall 0 0 0 1 1
( 0 32 0 ) ( 0 32 32 ) ( 32 32 32 ) stone/wall 0 0 0 1 1
( 0 0 0 ) ( 0 0 32 ) ( 0 32 32 ) stone/wall 0 0 0 1 1
( 32 0 0 ) ( 32 32 32 ) ( 32 0 32 ) stone/wall 0 0 0 1 1
}
}
{
"classname" "ent_spawnpoint"
"origin" "16 32 48"
"angle" "90"
"team" "2"
"custom" "C:\maps\test"
}
)MAP";
static Parser::Brush cube() {
	Parser::Map map;
	Parser::Diagnostic diagnostic;
	CHECK(Parser::parse(fixture, Parser::STANDARD, map, diagnostic) == OK);
	return map.entities[0].brushes[0];
}
TEST_CASE("[Editor][TrenchBroomBrushCompiler] Cube mesh collision and material") {
	Compiler::Options options;
	Compiler::MaterialInfo material;
	Ref<StandardMaterial3D> resource;
	resource.instantiate();
	material.material = resource;
	material.texture_size = Vector2(128, 32);
	options.materials.insert("stone/floor", material);
	Compiler::Result result;
	Parser::Diagnostic diagnostic;
	REQUIRE(Compiler::compile(cube(), Parser::STANDARD, options, result, diagnostic) == OK);
	CHECK(diagnostic.message.is_empty());
	REQUIRE(result.mesh.is_valid());
	CHECK(result.mesh->get_surface_count() == 6);
	CHECK(result.mesh->surface_get_name(0) == "stone/floor");
	CHECK(result.mesh->surface_get_material(0) == material.material);
	CHECK(result.mesh->get_aabb().position.is_equal_approx(Vector3(0, 0, -1)));
	CHECK(result.mesh->get_aabb().size.distance_to(Vector3(1, 1, 1)) < 0.0001);
	REQUIRE(result.collision.is_valid());
	CHECK(result.collision->get_points().size() == 8);
	CHECK(result.collision->get_margin() == 0);
	for (int i = 0; i < 6; i++) {
		Array arrays = result.mesh->surface_get_arrays(i);
		PackedVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
		PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
		PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
		REQUIRE(positions.size() == 4);
		REQUIRE(indices.size() == 6);
		for (int j = 0; j < indices.size(); j += 3) {
			Vector3 cross = (positions[indices[j + 1]] - positions[indices[j]]).cross(positions[indices[j + 2]] - positions[indices[j]]);
			CHECK(cross.dot(normals[indices[j]]) < 0);
		}
		for (const Vector3 &position : positions) {
			bool found = false;
			for (const Vector3 &point : result.collision->get_points()) {
				found |= point.is_equal_approx(position);
			}
			CHECK(found);
		}
	}
	Array arrays = result.mesh->surface_get_arrays(0);
	PackedVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
	PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
	for (int i = 0; i < positions.size(); i++) {
		Vector3 map_point(positions[i].x * 32, -positions[i].z * 32, positions[i].y * 32);
		CHECK(uvs[i].is_equal_approx(Vector2((map_point.y / 0.5 + 8) / 128, (map_point.x / -2 - 4) / 32)));
	}
}
TEST_CASE("[Editor][TrenchBroomBrushCompiler] Valve axes ignore serialized rotation") {
	Parser::Brush brush = cube();
	for (auto &face : brush.faces) {
		face.u_axis = Vector3(1, 2, 3);
		face.v_axis = Vector3(0, -1, 0);
		face.rotation = 73;
		face.scale = Vector2(-2, 4);
		face.offset = Vector2(8, -4);
	}
	Compiler::Options options;
	options.unit_scale = 1;
	Compiler::Result result;
	Parser::Diagnostic diagnostic;
	REQUIRE(Compiler::compile(brush, Parser::VALVE_220, options, result, diagnostic) == OK);
	for (int surface = 0; surface < 6; surface++) {
		Array arrays = result.mesh->surface_get_arrays(surface);
		PackedVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
		PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
		for (int i = 0; i < positions.size(); i++) {
			Vector3 point(positions[i].x, -positions[i].z, positions[i].y);
			CHECK(uvs[i].is_equal_approx((Vector2(brush.faces[surface].u_axis.dot(point), -point.y) / Vector2(-2, 4) + Vector2(8, -4)) / 64));
		}
	}
}
TEST_CASE("[Editor][TrenchBroomBrushCompiler] Sloped wedge") {
	Parser::Brush brush = cube();
	brush.faces.remove_at(1);
	Parser::Face slope;
	slope.points[0] = Vector3(0, 0, 32);
	slope.points[1] = Vector3(32, 0, 0);
	slope.points[2] = Vector3(0, 32, 32);
	slope.material = "slope";
	slope.scale = Vector2(1, 1);
	// The x=32 face has no area after the slope clips the cube.
	brush.faces.remove_at(4);
	brush.faces.push_back(slope);
	Compiler::Result result;
	Parser::Diagnostic diagnostic;
	REQUIRE(Compiler::compile(brush, Parser::STANDARD, Compiler::Options(), result, diagnostic) == OK);
	CHECK(result.mesh->get_surface_count() == 5);
	CHECK(result.collision->get_points().size() == 6);
	CHECK(result.mesh->surface_get_name(4) == "slope");
	Array arrays = result.mesh->surface_get_arrays(4);
	PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
	CHECK(normals[0].distance_to(Vector3(1, 1, 0).normalized()) < 0.0001);
}
TEST_CASE("[Editor][TrenchBroomBrushCompiler] Invalid brushes clear previous output") {
	Compiler::Options options;
	Compiler::Result result;
	Parser::Diagnostic diagnostic;
	for (int kind = 0; kind < 7; kind++) {
		Parser::Brush brush = cube();
		REQUIRE(Compiler::compile(brush, Parser::STANDARD, options, result, diagnostic) == OK);
		switch (kind) {
			case 0:
				brush.faces.remove_at(1);
				break;
			case 1:
				brush.faces.push_back(brush.faces[0]);
				break;
			case 2:
				brush.faces.write[0].points[1] = brush.faces[0].points[0];
				break;
			case 3:
				SWAP(brush.faces.write[0].points[0], brush.faces.write[0].points[1]);
				break;
			case 4:
				brush.faces.write[0].scale.x = 0;
				break;
			case 5:
				brush.faces.write[0].points[0].x = Math::INF;
				break;
			case 6:
				brush.faces.resize(3);
				break;
		}
		CHECK(Compiler::compile(brush, Parser::STANDARD, options, result, diagnostic) == ERR_INVALID_DATA);
		CHECK(result.mesh.is_null());
		CHECK(result.collision.is_null());
		CHECK_FALSE(diagnostic.message.is_empty());
		CHECK(diagnostic.line > 0);
	}
	options.unit_scale = 0;
	CHECK(Compiler::compile(cube(), Parser::STANDARD, options, result, diagnostic) == ERR_INVALID_PARAMETER);
	options.unit_scale = 1;
	Compiler::MaterialInfo invalid;
	invalid.texture_size.x = 0;
	options.materials.insert("stone/floor", invalid);
	CHECK(Compiler::compile(cube(), Parser::STANDARD, options, result, diagnostic) == ERR_INVALID_PARAMETER);
	CHECK(result.mesh.is_null());
}

TEST_CASE("[Editor][TrenchBroomBrushCompiler] Translated tetrahedron and tolerance") {
	Parser::Brush brush;
	const Vector3 points[] = { Vector3(0, 0, 0), Vector3(32, 0, 0), Vector3(0, 32, 0), Vector3(0, 0, 32) };
	const int triangles[][3] = { { 0, 2, 1 }, { 0, 1, 3 }, { 0, 3, 2 }, { 1, 2, 3 } };
	for (const auto &triangle : triangles) {
		Parser::Face face;
		for (int i = 0; i < 3; i++) {
			face.points[i] = points[triangle[i]] + Vector3(-128, 256, 64);
		}
		face.scale = Vector2(1, 1);
		face.material = "tetra";
		brush.faces.push_back(face);
	}
	Compiler::Result result;
	Parser::Diagnostic diagnostic;
	REQUIRE(Compiler::compile(brush, Parser::STANDARD, Compiler::Options(), result, diagnostic) == OK);
	CHECK(result.collision->get_points().size() == 4);
	for (int i = 0; i < 4; i++) {
		Array arrays = result.mesh->surface_get_arrays(i);
		PackedVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
		PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
		CHECK(positions.size() == 3);
		CHECK(indices.size() == 3);
	}
	Compiler::Options options;
	options.tolerance = std::numeric_limits<real_t>::quiet_NaN();
	CHECK(Compiler::compile(brush, Parser::STANDARD, options, result, diagnostic) == ERR_INVALID_PARAMETER);
	CHECK(result.collision.is_null());
}

} // namespace TestTrenchBroomBrushCompiler
#endif // TOOLS_ENABLED
