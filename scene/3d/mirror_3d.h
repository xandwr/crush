/**************************************************************************/
/*  mirror_3d.h                                                           */
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

#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/material.h"

class Mirror3D : public VisualInstance3D {
	GDCLASS(Mirror3D, VisualInstance3D);

	Ref<QuadMesh> mesh;
	Ref<ShaderMaterial> material;
	Vector2 size = Vector2(2, 2);
	bool enabled = true;
	real_t resolution_scale = 0.5;
	uint32_t cull_mask = (1 << 20) - 1;

	bool collision_enabled = false;
	real_t collision_thickness = 0.1;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	Ref<Shape3D> collision_shape;
	Ref<BoxShape3D> default_collision_shape;
	StaticBody3D *collision_body = nullptr;
	CollisionShape3D *collision_shape_node = nullptr;

	void _update_collision();
	void _update_mirror();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_enabled(bool p_enabled);
	bool is_enabled() const;
	void set_size(const Vector2 &p_size);
	Vector2 get_size() const;
	void set_resolution_scale(real_t p_scale);
	real_t get_resolution_scale() const;
	void set_cull_mask(uint32_t p_mask);
	uint32_t get_cull_mask() const;
	void set_collision_enabled(bool p_enabled);
	bool is_collision_enabled() const;
	void set_collision_thickness(real_t p_thickness);
	real_t get_collision_thickness() const;
	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const;
	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const;
	void set_collision_shape(const Ref<Shape3D> &p_shape);
	Ref<Shape3D> get_collision_shape() const;
	StaticBody3D *get_collision_body() const;
	AABB get_aabb() const override;
	PackedStringArray get_configuration_warnings() const override;
	Mirror3D();
	~Mirror3D();
};
