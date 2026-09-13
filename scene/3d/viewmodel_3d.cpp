/**************************************************************************/
/*  viewmodel_3d.cpp                                                      */
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

#include "viewmodel_3d.h"

#include "core/object/class_db.h"
#include "scene/3d/camera_3d.h"

void Viewmodel3D::_update_camera() {
	camera_id = ObjectID();
	for (Node *ancestor = get_parent(); ancestor; ancestor = ancestor->get_parent()) {
		Camera3D *camera = Object::cast_to<Camera3D>(ancestor);
		if (camera) {
			camera_id = camera->get_instance_id();
			break;
		}
	}
}

void Viewmodel3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_PARENTED:
			_update_camera();
			update_configuration_warnings();
			break;
		case NOTIFICATION_EXIT_TREE:
		case NOTIFICATION_UNPARENTED:
			camera_id = ObjectID();
			update_configuration_warnings();
			break;
	}
}

void Viewmodel3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &Viewmodel3D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &Viewmodel3D::is_enabled);
	ClassDB::bind_method(D_METHOD("set_fov", "fov"), &Viewmodel3D::set_fov);
	ClassDB::bind_method(D_METHOD("get_fov"), &Viewmodel3D::get_fov);
	ClassDB::bind_method(D_METHOD("set_near", "near"), &Viewmodel3D::set_near);
	ClassDB::bind_method(D_METHOD("get_near"), &Viewmodel3D::get_near);
	ClassDB::bind_method(D_METHOD("set_far", "far"), &Viewmodel3D::set_far);
	ClassDB::bind_method(D_METHOD("get_far"), &Viewmodel3D::get_far);
	ClassDB::bind_method(D_METHOD("set_cast_world_shadows", "enabled"), &Viewmodel3D::set_cast_world_shadows);
	ClassDB::bind_method(D_METHOD("is_casting_world_shadows"), &Viewmodel3D::is_casting_world_shadows);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fov", PROPERTY_HINT_RANGE, "1,179,0.1,degrees"), "set_fov", "get_fov");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "near", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater,exp,suffix:m"), "set_near", "get_near");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "far", PROPERTY_HINT_RANGE, "0.01,4000,0.01,or_greater,exp,suffix:m"), "set_far", "get_far");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cast_world_shadows"), "set_cast_world_shadows", "is_casting_world_shadows");
}

Camera3D *Viewmodel3D::get_camera_3d() const {
	return ObjectDB::get_instance<Camera3D>(camera_id);
}

void Viewmodel3D::set_enabled(bool p_enabled) {
	enabled = p_enabled;
}

bool Viewmodel3D::is_enabled() const {
	return enabled;
}

void Viewmodel3D::set_fov(real_t p_fov) {
	ERR_FAIL_COND(p_fov < 1.0 || p_fov > 179.0);
	fov = p_fov;
}

real_t Viewmodel3D::get_fov() const {
	return fov;
}

void Viewmodel3D::set_near(real_t p_near) {
	ERR_FAIL_COND(p_near <= 0.0);
	_near = p_near;
}

real_t Viewmodel3D::get_near() const {
	return _near;
}

void Viewmodel3D::set_far(real_t p_far) {
	ERR_FAIL_COND(p_far <= 0.0);
	_far = p_far;
}

real_t Viewmodel3D::get_far() const {
	return _far;
}

void Viewmodel3D::set_cast_world_shadows(bool p_enabled) {
	cast_world_shadows = p_enabled;
}

bool Viewmodel3D::is_casting_world_shadows() const {
	return cast_world_shadows;
}

PackedStringArray Viewmodel3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	bool has_camera_ancestor = false;

	for (const Node *ancestor = get_parent(); ancestor; ancestor = ancestor->get_parent()) {
		if (Object::cast_to<const Viewmodel3D>(ancestor)) {
			warnings.push_back(RTR("Viewmodel3D nodes cannot be nested."));
			return warnings;
		}
		if (Object::cast_to<const Camera3D>(ancestor)) {
			has_camera_ancestor = true;
		}
	}

	if (!has_camera_ancestor) {
		warnings.push_back(RTR("Viewmodel3D must be a descendant of a Camera3D node."));
	}

	return warnings;
}
