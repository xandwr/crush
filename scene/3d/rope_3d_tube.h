/**************************************************************************/
/*  rope_3d_tube.h                                                       */
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


#pragma once

#include "scene/resources/mesh.h"

class Rope3DTube {
	struct Vertex {
		int point = 0;
		real_t angle = 0;
		real_t latitude = 0;
		int cap = 0;
	};
	Vector<Vertex> layout;
	Vector<Vector3> tangents;
	Vector<Vector3> frames;
	Vector<real_t> distances;
	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Vector2> uvs;
	Vector<int> indices;
	Vector<uint8_t> vertex_buffer;
	Vector<uint8_t> attribute_buffer;
	Vector3 first_frame = Vector3(1, 0, 0);
	Vector3 first_tangent = Vector3(0, -1, 0);
	AABB previous_bounds;
	bool have_bounds = false;
	uint32_t normal_offset = 0;
	uint32_t normal_stride = 0;
	uint32_t vertex_stride = 0;
	uint32_t uv_offset = 0;
	uint32_t attribute_stride = 0;
	bool dirty = true;
	int count = 0;
	int sides = 0;
	int caps = 0;
	bool mirrored = false;
	void topology(int p_count, int p_sides, int p_caps, bool p_mirrored);
	static Vector3 transport(const Vector3 &p_frame, const Vector3 &p_from, const Vector3 &p_to);

public:
	Ref<ArrayMesh> mesh;
	AABB bounds;
	Rope3DTube();
	void invalidate() { dirty = true; }
	void reset_frame() {
		first_frame = Vector3(1, 0, 0);
		first_tangent = Vector3(0, -1, 0);
		have_bounds = false;
	}
	void update(const Vector<Vector3> &p_points, const Transform3D &p_to_local, real_t p_radius, int p_sides, int p_caps, real_t p_uv_repeat);
};
