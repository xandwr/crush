/**************************************************************************/
/*  mirror_3d.cpp                                                         */
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

#include "mirror_3d.h"

#include "core/object/class_db.h"
#include "scene/resources/shader.h"
#include "servers/rendering/rendering_server.h"

Mirror3D::Mirror3D() {
	mesh.instantiate();
	mesh->set_size(size);
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code("shader_type spatial;\n"
					 "render_mode unshaded, cull_back, fog_disabled;\n"
					 "uniform sampler2D reflection_texture : filter_linear, repeat_disable;\n"
					 "uniform bool reflection_valid = false;\n"
					 "void fragment() {\n"
					 "vec3 color = reflection_valid ? texture(reflection_texture, SCREEN_UV).rgb : vec3(0.0);\n"
					 "ALBEDO = color;\n"
					 "}\n");
	material.instantiate();
	material->set_shader(shader);
	mesh->surface_set_material(0, material);
	set_base(mesh->get_rid());
	RS::get_singleton()->instance_geometry_set_cast_shadows_setting(get_instance(), RSE::SHADOW_CASTING_SETTING_OFF);
	_update_mirror();
}

Mirror3D::~Mirror3D() {
	RS::get_singleton()->instance_set_mirror(get_instance(), RID(), size, resolution_scale, cull_mask, false);
}

void Mirror3D::_update_mirror() {
	RS::get_singleton()->instance_set_mirror(get_instance(), material->get_rid(), size, resolution_scale, cull_mask, enabled);
}

void Mirror3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {
		update_configuration_warnings();
	}
}

void Mirror3D::set_enabled(bool p_enabled) {
	enabled = p_enabled;
	_update_mirror();
}

bool Mirror3D::is_enabled() const {
	return enabled;
}

void Mirror3D::set_size(const Vector2 &p_size) {
	ERR_FAIL_COND(!p_size.is_finite() || p_size.x <= 0 || p_size.y <= 0);
	size = p_size;
	mesh->set_size(size);
	_update_collision();
	_update_mirror();
	update_gizmos();
}

Vector2 Mirror3D::get_size() const {
	return size;
}

void Mirror3D::set_resolution_scale(real_t p_scale) {
	ERR_FAIL_COND(!Math::is_finite(p_scale) || p_scale <= 0 || p_scale > 1);
	resolution_scale = p_scale;
	_update_mirror();
}

real_t Mirror3D::get_resolution_scale() const {
	return resolution_scale;
}

void Mirror3D::set_cull_mask(uint32_t p_mask) {
	cull_mask = p_mask;
	_update_mirror();
}

uint32_t Mirror3D::get_cull_mask() const {
	return cull_mask;
}

void Mirror3D::_update_collision() {
	if (!collision_body) {
		if (!collision_enabled) {
			return;
		}
		collision_body = memnew(StaticBody3D);
		collision_body->set_name("CollisionBody3D");
		collision_shape_node = memnew(CollisionShape3D);
		collision_shape_node->set_name("CollisionShape3D");
		collision_body->add_child(collision_shape_node, false, INTERNAL_MODE_BACK);
		default_collision_shape.instantiate();
		add_child(collision_body, false, INTERNAL_MODE_BACK);
	}
	collision_body->set_collision_layer(collision_enabled ? collision_layer : 0);
	collision_body->set_collision_mask(collision_enabled ? collision_mask : 0);
	collision_shape_node->set_disabled(!collision_enabled);
	if (collision_shape.is_valid()) {
		collision_shape_node->set_position(Vector3());
		collision_shape_node->set_shape(collision_shape);
	} else {
		default_collision_shape->set_size(Vector3(size.x, size.y, collision_thickness));
		collision_shape_node->set_position(Vector3(0, 0, -collision_thickness / 2));
		collision_shape_node->set_shape(default_collision_shape);
	}
}

void Mirror3D::set_collision_enabled(bool p_enabled) {
	collision_enabled = p_enabled;
	_update_collision();
}

bool Mirror3D::is_collision_enabled() const {
	return collision_enabled;
}

void Mirror3D::set_collision_thickness(real_t p_thickness) {
	ERR_FAIL_COND(!Math::is_finite(p_thickness) || p_thickness <= 0);
	collision_thickness = p_thickness;
	_update_collision();
}

real_t Mirror3D::get_collision_thickness() const {
	return collision_thickness;
}

void Mirror3D::set_collision_layer(uint32_t p_layer) {
	collision_layer = p_layer;
	_update_collision();
}

uint32_t Mirror3D::get_collision_layer() const {
	return collision_layer;
}

void Mirror3D::set_collision_mask(uint32_t p_mask) {
	collision_mask = p_mask;
	_update_collision();
}

uint32_t Mirror3D::get_collision_mask() const {
	return collision_mask;
}

void Mirror3D::set_collision_shape(const Ref<Shape3D> &p_shape) {
	collision_shape = p_shape;
	_update_collision();
}

Ref<Shape3D> Mirror3D::get_collision_shape() const {
	return collision_shape;
}

StaticBody3D *Mirror3D::get_collision_body() const {
	return collision_body;
}

AABB Mirror3D::get_aabb() const {
	return AABB(Vector3(-size.x / 2, -size.y / 2, 0), Vector3(size.x, size.y, 0));
}

PackedStringArray Mirror3D::get_configuration_warnings() const {
	PackedStringArray warnings = VisualInstance3D::get_configuration_warnings();
	const Transform3D transform = is_inside_tree() ? get_global_transform() : get_transform();
	if (!transform.is_finite() || !transform.basis.is_orthogonal() || transform.basis.determinant() <= 0) {
		warnings.push_back(RTR("Mirror3D requires a finite transform with positive scale and no shear."));
	}
	return warnings;
}

void Mirror3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &Mirror3D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &Mirror3D::is_enabled);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &Mirror3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &Mirror3D::get_size);
	ClassDB::bind_method(D_METHOD("set_resolution_scale", "scale"), &Mirror3D::set_resolution_scale);
	ClassDB::bind_method(D_METHOD("get_resolution_scale"), &Mirror3D::get_resolution_scale);
	ClassDB::bind_method(D_METHOD("set_cull_mask", "mask"), &Mirror3D::set_cull_mask);
	ClassDB::bind_method(D_METHOD("get_cull_mask"), &Mirror3D::get_cull_mask);
	ClassDB::bind_method(D_METHOD("get_collision_body"), &Mirror3D::get_collision_body);
	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &Mirror3D::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("is_collision_enabled"), &Mirror3D::is_collision_enabled);
	ClassDB::bind_method(D_METHOD("set_collision_thickness", "thickness"), &Mirror3D::set_collision_thickness);
	ClassDB::bind_method(D_METHOD("get_collision_thickness"), &Mirror3D::get_collision_thickness);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &Mirror3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &Mirror3D::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &Mirror3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &Mirror3D::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_shape", "shape"), &Mirror3D::set_collision_shape);
	ClassDB::bind_method(D_METHOD("get_collision_shape"), &Mirror3D::get_collision_shape);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "resolution_scale", PROPERTY_HINT_RANGE, "0.01,1,0.01"), "set_resolution_scale", "get_resolution_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cull_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_cull_mask", "get_cull_mask");
	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "is_collision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_thickness", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater,suffix:m"), "set_collision_thickness", "get_collision_thickness");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "collision_shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape3D"), "set_collision_shape", "get_collision_shape");
}
