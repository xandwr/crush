/**************************************************************************/
/*  rope_3d_tube.cpp                                                     */
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

#include "rope_3d_tube.h"

#include "servers/rendering/rendering_server.h"

Rope3DTube::Rope3DTube() {
	mesh.instantiate();
}

Vector3 Rope3DTube::transport(const Vector3 &p_frame, const Vector3 &p_from, const Vector3 &p_to) {
	Vector3 axis = p_from.cross(p_to);
	real_t sine = axis.length();
	Vector3 frame = p_frame;
	if (sine > CMP_EPSILON) {
		frame = frame.rotated(axis / sine, Math::atan2(sine, real_t(CLAMP(p_from.dot(p_to), -1.0, 1.0))));
	}
	frame -= p_to * frame.dot(p_to);
	if (frame.length_squared() < CMP_EPSILON * CMP_EPSILON) {
		frame = p_to.cross(Math::abs(p_to.y) < 0.9 ? Vector3(0, 1, 0) : Vector3(1, 0, 0));
	}
	return frame.normalized();
}

void Rope3DTube::topology(int p_count, int p_sides, int p_caps, bool p_mirrored) {
	count = p_count;
	sides = p_sides;
	caps = p_caps;
	mirrored = p_mirrored;
	layout.clear();
	indices.clear();
	for (int i = 0; i < count; i++) {
		for (int j = 0; j <= sides; j++) {
			layout.push_back({ i, real_t(Math::TAU * j / sides), 0, 0 });
		}
	}
	auto connect_rings = [&](int a, int b, bool reverse) {
		for (int j = 0; j < sides; j++) {
			int triangle[] = { a + j, b + j, a + j + 1, a + j + 1, b + j, b + j + 1 };
			for (int k = 0; k < 6; k += 3) {
				indices.push_back(triangle[k]);
				indices.push_back(triangle[k + (reverse ? 2 : 1)]);
				indices.push_back(triangle[k + (reverse ? 1 : 2)]);
			}
		}
	};
	for (int i = 0; i < count - 1; i++) {
		connect_rings(i * (sides + 1), (i + 1) * (sides + 1), mirrored);
	}
	if (caps != 0) {
		for (int end = 0; end < 2; end++) {
			int point = end == 0 ? 0 : count - 1;
			int sign = end == 0 ? -1 : 1;
			int previous_ring = point * (sides + 1);
			if (caps == 1) {
				previous_ring = layout.size();
				for (int j = 0; j <= sides; j++) {
					layout.push_back({ point, real_t(Math::TAU * j / sides), 0, sign });
				}
			} else {
				for (int latitude = 1; latitude <= 3; latitude++) {
					int ring = layout.size();
					for (int j = 0; j <= sides; j++) {
						layout.push_back({ point, real_t(Math::TAU * j / sides), real_t(Math::PI * latitude / 8), sign });
					}
					connect_rings(previous_ring, ring, mirrored != (end == 0));
					previous_ring = ring;
				}
			}
			int pole = layout.size();
			layout.push_back({ point, 0, real_t(Math::PI / 2), sign });
			for (int j = 0; j < sides; j++) {
				indices.push_back(previous_ring + j);
				indices.push_back(mirrored != (end == 0) ? previous_ring + j + 1 : pole);
				indices.push_back(mirrored != (end == 0) ? pole : previous_ring + j + 1);
			}
		}
	}
	vertices.resize(layout.size());
	normals.resize(layout.size());
	uvs.resize(layout.size());
	tangents.resize(count);
	frames.resize(count);
	distances.resize(count);
	dirty = true;
}

void Rope3DTube::update(const Vector<Vector3> &p_points, const Transform3D &p_to_local, real_t p_radius, int p_sides, int p_caps, real_t p_uv_repeat, const Vector<real_t> &p_radii) {
	ERR_FAIL_COND(!p_radii.is_empty() && p_radii.size() != p_points.size());
	bool mirror = p_to_local.basis.determinant() < 0;
	if (count != p_points.size() || sides != p_sides || caps != p_caps || mirrored != mirror) {
		topology(p_points.size(), p_sides, p_caps, mirror);
	}
	for (int i = 0; i < count; i++) {
		Vector3 tangent = i == count - 1 ? p_points[i] - p_points[i - 1] : p_points[i + 1] - p_points[i];
		if (i > 0 && i < count - 1) {
			Vector3 incoming = p_points[i] - p_points[i - 1];
			if (incoming.length_squared() > CMP_EPSILON * CMP_EPSILON && tangent.length_squared() > CMP_EPSILON * CMP_EPSILON) {
				Vector3 average = incoming.normalized() + tangent.normalized();
				if (average.length_squared() > CMP_EPSILON * CMP_EPSILON) {
					tangent = average;
				}
			}
		}
		tangents.write[i] = tangent.length_squared() > CMP_EPSILON * CMP_EPSILON ? tangent.normalized() : (i > 0 ? tangents[i - 1] : first_tangent);
		frames.write[i] = i == 0 ? transport(first_frame, first_tangent, tangents[i]) : transport(frames[i - 1], tangents[i - 1], tangents[i]);
		distances.write[i] = i == 0 ? 0 : distances[i - 1] + p_points[i].distance_to(p_points[i - 1]);
	}
	first_frame = frames[0];
	first_tangent = tangents[0];
	Basis normal_to_local = p_to_local.basis.inverse().transposed();
	AABB current;
	for (int i = 0; i < layout.size(); i++) {
		const Vertex &v = layout[i];
		real_t radius = p_radii.is_empty() ? p_radius : p_radii[v.point];
		Vector3 radial = frames[v.point] * Math::cos(v.angle) + tangents[v.point].cross(frames[v.point]) * Math::sin(v.angle);
		Vector3 normal = radial;
		Vector3 offset = radial * radius;
		if (!p_radii.is_empty()) {
			int previous = MAX(0, v.point - 1);
			int next = MIN(count - 1, v.point + 1);
			real_t span = distances[next] - distances[previous];
			if (span > CMP_EPSILON) {
				normal = (radial - tangents[v.point] * ((p_radii[next] - p_radii[previous]) / span)).normalized();
			}
		}
		if (v.cap != 0) {
			if (caps == 1) {
				normal = tangents[v.point] * v.cap;
				offset = v.latitude == 0 ? offset : Vector3();
			} else {
				normal = radial * Math::cos(v.latitude) + tangents[v.point] * (v.cap * Math::sin(v.latitude));
				offset = normal * radius;
			}
		}
		vertices.write[i] = p_to_local.xform(p_points[v.point] + offset);
		normals.write[i] = normal_to_local.xform(normal).normalized();
		uvs.write[i] = Vector2(v.angle / Math::TAU, (distances[v.point] + (caps == 2 ? v.cap * radius * Math::sin(v.latitude) : 0)) / p_uv_repeat);
		if (i == 0) {
			current.position = vertices[i];
		} else {
			current.expand_to(vertices[i]);
		}
	}
	bounds = have_bounds ? current.merge(previous_bounds) : current;
	previous_bounds = current;
	have_bounds = true;
	mesh->set_custom_aabb(bounds);
	RenderingServer *server = RenderingServer::get_singleton();
	if (dirty) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_INDEX] = indices;
		mesh->clear_surfaces();
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), Mesh::ARRAY_FLAG_USE_DYNAMIC_UPDATE);
		RenderingServerTypes::SurfaceData data = server->mesh_get_surface(mesh->get_rid(), 0);
		vertex_buffer = data.vertex_data;
		attribute_buffer = data.attribute_data;
		normal_offset = server->mesh_surface_get_format_offset(data.format, layout.size(), Mesh::ARRAY_NORMAL);
		normal_stride = server->mesh_surface_get_format_normal_tangent_stride(data.format, layout.size());
		vertex_stride = server->mesh_surface_get_format_vertex_stride(data.format, layout.size());
		uv_offset = server->mesh_surface_get_format_offset(data.format, layout.size(), Mesh::ARRAY_TEX_UV);
		attribute_stride = server->mesh_surface_get_format_attribute_stride(data.format, layout.size());
		dirty = false;
	} else {
		uint8_t *vb = vertex_buffer.ptrw();
		uint8_t *ab = attribute_buffer.ptrw();
		for (int i = 0; i < layout.size(); i++) {
			float position[] = { float(vertices[i].x), float(vertices[i].y), float(vertices[i].z) };
			memcpy(vb + i * vertex_stride, position, sizeof(position));
			Vector2 oct = normals[i].octahedron_encode();
			uint16_t normal[] = { uint16_t(CLAMP(oct.x * 65535, 0, 65535)), uint16_t(CLAMP(oct.y * 65535, 0, 65535)) };
			memcpy(vb + normal_offset + i * normal_stride, normal, sizeof(normal));
			float uv[] = { float(uvs[i].x), float(uvs[i].y) };
			memcpy(ab + uv_offset + i * attribute_stride, uv, sizeof(uv));
		}
		mesh->surface_update_vertex_region(0, 0, vertex_buffer);
		mesh->surface_update_attribute_region(0, 0, attribute_buffer);
	}
}
