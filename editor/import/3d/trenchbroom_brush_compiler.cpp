/**************************************************************************/
/*  trenchbroom_brush_compiler.cpp                                        */
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

#include "trenchbroom_brush_compiler.h"

#include "core/math/plane.h"

#include <algorithm>

namespace {
Vector3 convert_point(const Vector3 &p_point, real_t p_scale) {
	return Vector3(p_point.y, p_point.z, p_point.x) * p_scale;
}
struct AngularVertex {
	int index;
	real_t angle;
};
void standard_axes(const Vector3 &p_normal, Vector3 &r_u, Vector3 &r_v) {
	const Vector3 normals[] = { Vector3(0, 0, 1), Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, -1, 0) };
	const Vector3 us[] = { Vector3(1, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 1, 0), Vector3(1, 0, 0), Vector3(1, 0, 0) };
	const Vector3 vs[] = { Vector3(0, -1, 0), Vector3(0, -1, 0), Vector3(0, 0, -1), Vector3(0, 0, -1), Vector3(0, 0, -1), Vector3(0, 0, -1) };
	int best = 0;
	for (int i = 1; i < 6; i++) {
		if (p_normal.dot(normals[i]) > p_normal.dot(normals[best])) {
			best = i;
		}
	}
	r_u = us[best];
	r_v = vs[best];
}
} // namespace

Error TrenchBroomBrushCompiler::compile(const TrenchBroomMapParser::Brush &p_brush, TrenchBroomMapParser::Format p_format, const Options &p_options, Result &r_result, TrenchBroomMapParser::Diagnostic &r_diagnostic) {
	r_result = Result();
	r_diagnostic = TrenchBroomMapParser::Diagnostic();
	auto fail = [&](const String &p_message, int p_line, Error p_error = ERR_INVALID_DATA) {
		r_diagnostic.message = p_message;
		r_diagnostic.line = p_line;
		return p_error;
	};
	if ((p_format != TrenchBroomMapParser::STANDARD && p_format != TrenchBroomMapParser::VALVE_220) || !Math::is_finite(p_options.unit_scale) || p_options.unit_scale <= 0 || !Math::is_finite(p_options.tolerance) || p_options.tolerance <= 0) {
		return fail("Invalid compiler format, scale, or tolerance.", p_brush.line, ERR_INVALID_PARAMETER);
	}
	if (p_brush.faces.size() < 4) {
		return fail("A closed brush requires at least four planes.", p_brush.line);
	}
	Vector<Plane> planes;
	for (const auto &face : p_brush.faces) {
		for (const Vector3 &point : face.points) {
			if (!point.is_finite()) {
				return fail("Nonfinite plane point.", face.line);
			}
		}
		Vector3 normal = (face.points[1] - face.points[0]).cross(face.points[2] - face.points[0]);
		if (!normal.is_finite() || normal.length() <= p_options.tolerance * p_options.tolerance) {
			return fail("Degenerate brush plane.", face.line);
		}
		normal.normalize();
		Plane plane(normal, face.points[0]);
		if (!plane.is_finite()) {
			return fail("Nonfinite brush plane.", face.line);
		}
		for (const Plane &other : planes) {
			if (normal.distance_to(other.normal) < CMP_EPSILON && Math::abs(plane.d - other.d) <= p_options.tolerance) {
				return fail("Duplicate brush plane.", face.line);
			}
		}
		if (!face.scale.is_finite() || face.scale.x == 0 || face.scale.y == 0 || !face.offset.is_finite() || !Math::is_finite(face.rotation) || !face.u_axis.is_finite() || !face.v_axis.is_finite()) {
			return fail("Invalid face UV parameters.", face.line);
		}
		planes.push_back(plane);
	}
	Vector<Vector3> vertices;
	Vector<Vector3> inverted_vertices;
	auto add_unique = [&](Vector<Vector3> &r_vertices, const Vector3 &p_point) {
		for (const Vector3 &vertex : r_vertices) {
			if (vertex.distance_to(p_point) <= p_options.tolerance) {
				return;
			}
		}
		r_vertices.push_back(p_point);
	};
	for (int i = 0; i < planes.size(); i++) {
		for (int j = i + 1; j < planes.size(); j++) {
			for (int k = j + 1; k < planes.size(); k++) {
				Vector3 point;
				if (!planes[i].intersect_3(planes[j], planes[k], &point) || !point.is_finite()) {
					continue;
				}
				bool inside = true;
				bool inverted_inside = true;
				for (const Plane &plane : planes) {
					real_t distance = plane.distance_to(point);
					inside &= distance <= p_options.tolerance;
					inverted_inside &= distance >= -p_options.tolerance;
				}
				if (inside) {
					add_unique(vertices, point);
				}
				if (inverted_inside) {
					add_unique(inverted_vertices, point);
				}
			}
		}
	}
	if (vertices.size() < 4 && inverted_vertices.size() >= 4) {
		vertices = inverted_vertices;
		for (Plane &plane : planes) {
			plane = -plane;
		}
	}
	if (vertices.size() < 4) {
		return fail("Brush has no enclosed volume.", p_brush.line);
	}
	Vector<Vector<int>> polygons;
	HashMap<uint64_t, int> edges;
	for (int i = 0; i < planes.size(); i++) {
		Vector<int> polygon;
		Vector3 center;
		for (int j = 0; j < vertices.size(); j++) {
			if (planes[i].has_point(vertices[j], p_options.tolerance)) {
				polygon.push_back(j);
				center += vertices[j];
			}
		}
		if (polygon.size() < 3) {
			return fail("Brush is open or contains a redundant plane.", p_brush.faces[i].line);
		}
		center /= polygon.size();
		Vector3 u = (vertices[polygon[0]] - center).normalized();
		Vector3 v = planes[i].normal.cross(u);
		Vector<AngularVertex> ordered;
		for (int index : polygon) {
			Vector3 delta = vertices[index] - center;
			ordered.push_back({ index, Math::atan2(delta.dot(v), delta.dot(u)) });
		}
		std::sort(ordered.ptrw(), ordered.ptrw() + ordered.size(), [](const AngularVertex &a, const AngularVertex &b) { return a.angle < b.angle; });
		for (int j = 0; j < polygon.size(); j++) {
			polygon.write[j] = ordered[j].index;
		}
		for (int j = 0; j < polygon.size(); j++) {
			int a = polygon[j];
			int b = polygon[(j + 1) % polygon.size()];
			uint64_t key = (uint64_t(MIN(a, b)) << 32) | uint32_t(MAX(a, b));
			edges[key]++;
		}
		polygons.push_back(polygon);
	}
	for (const auto &edge : edges) {
		if (edge.value != 2) {
			return fail("Brush planes do not form a closed convex boundary.", p_brush.line);
		}
	}
	double volume = 0;
	Vector3 interior;
	for (const Vector3 &vertex : vertices) {
		interior += vertex;
	}
	interior /= vertices.size();
	for (const Vector<int> &polygon : polygons) {
		for (int j = 1; j + 1 < polygon.size(); j++) {
			volume += (vertices[polygon[0]] - interior).dot((vertices[polygon[j]] - interior).cross(vertices[polygon[j + 1]] - interior)) / 6.0;
		}
	}
	if (!Math::is_finite(volume) || volume <= Math::pow(p_options.tolerance, 3)) {
		return fail("Brush has degenerate volume.", p_brush.line);
	}
	bool clip = false;
	bool origin = false;
	for (const auto &face : p_brush.faces) {
		String name = face.material.to_lower();
		clip |= name == "clip";
		origin |= name == "origin";
	}
	if (origin) {
		for (const auto &face : p_brush.faces) {
			if (face.material.to_lower() != "origin") {
				return fail("Origin brushes must use origin on every face.", face.line);
			}
		}
		AABB bounds(vertices[0], Vector3());
		for (const Vector3 &vertex : vertices) {
			bounds.expand_to(vertex);
		}
		r_result.origin = true;
		r_result.origin_center = bounds.get_center();
		return OK;
	}
	Result result;
	result.mesh.instantiate();
	for (int i = 0; i < polygons.size(); i++) {
		const auto &face = p_brush.faces[i];
		if (clip || face.material.to_lower() == "skip") {
			continue;
		}
		MaterialInfo material;
		if (const MaterialInfo *found = p_options.materials.getptr(face.material)) {
			material = *found;
		}
		if (!material.texture_size.is_finite() || material.texture_size.x <= 0 || material.texture_size.y <= 0) {
			return fail("Invalid material texture size.", face.line, ERR_INVALID_PARAMETER);
		}
		Vector3 u = face.u_axis;
		Vector3 v = face.v_axis;
		if (p_format == TrenchBroomMapParser::STANDARD) {
			standard_axes(planes[i].normal, u, v);
			real_t angle = Math::deg_to_rad(Math::fmod(face.rotation, 360.0));
			Vector3 rotated_u = u * Math::cos(angle) - v * Math::sin(angle);
			v = u * Math::sin(angle) + v * Math::cos(angle);
			u = rotated_u;
		}
		PackedVector3Array positions;
		PackedVector3Array normals;
		PackedVector2Array uvs;
		PackedInt32Array indices;
		for (int index : polygons[i]) {
			const Vector3 &point = vertices[index];
			Vector3 position = convert_point(point, p_options.unit_scale);
			Vector2 uv = (Vector2(u.dot(point), v.dot(point)) / face.scale + face.offset) / material.texture_size;
			if (!position.is_finite() || !uv.is_finite()) {
				return fail("Compiled vertex or UV exceeds numeric range.", face.line);
			}
			positions.push_back(position);
			normals.push_back(convert_point(planes[i].normal, 1));
			uvs.push_back(uv);
		}
		// Godot front faces use clockwise winding.
		for (int j = 1; j + 1 < positions.size(); j++) {
			indices.push_back(0);
			indices.push_back(j + 1);
			indices.push_back(j);
		}
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = positions;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_INDEX] = indices;
		result.mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		result.mesh->surface_set_name(result.mesh->get_surface_count() - 1, face.material);
		result.mesh->surface_set_material(result.mesh->get_surface_count() - 1, material.material);
	}
	Vector<Vector3> collision_points;
	for (const Vector3 &vertex : vertices) {
		collision_points.push_back(convert_point(vertex, p_options.unit_scale));
	}
	result.collision.instantiate();
	result.collision->set_points(collision_points);
	result.collision->set_margin(0);
	r_result = result;
	return OK;
}
