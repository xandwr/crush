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

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "scene/3d/camera_3d.h"
#include "servers/rendering/rendering_server.h"

void Viewmodel3D::_set_camera(Camera3D *p_camera) {
	Camera3D *old_camera = get_camera_3d();
	if (old_camera == p_camera) {
		return;
	}
	if (old_camera) {
		if (old_camera->_is_viewmodel_owner(this)) {
			RenderingServer::get_singleton()->camera_set_viewmodel_projection(old_camera->get_camera(), 54.0, 0.01, 100.0);
			RenderingServer::get_singleton()->camera_set_viewmodel_projection_enabled(old_camera->get_camera(), false);
			RenderingServer::get_singleton()->camera_set_viewmodel_cast_world_shadows(old_camera->get_camera(), false);
		}
		old_camera->_unregister_viewmodel(this);
	}
	camera_id = p_camera ? p_camera->get_instance_id() : ObjectID();
	if (p_camera) {
		p_camera->_register_viewmodel(this);
	}
	_camera_ownership_changed();
}

void Viewmodel3D::_update_camera() {
	Camera3D *camera = nullptr;
	for (Node *ancestor = get_parent(); ancestor; ancestor = ancestor->get_parent()) {
		if (Object::cast_to<Viewmodel3D>(ancestor)) {
			break;
		}
		camera = Object::cast_to<Camera3D>(ancestor);
		if (camera) {
			break;
		}
	}
	_set_camera(camera);
}

void Viewmodel3D::_camera_ownership_changed() {
	_update_camera_projection();
	_update_camera_projection_enabled();
	_update_camera_shadow_casting();
	_update_visual_instances();
	update_configuration_warnings();
}

void Viewmodel3D::_update_camera_projection() {
	Camera3D *camera = get_camera_3d();
	if (camera && _owns_camera() && _far > _near) {
		RenderingServer::get_singleton()->camera_set_viewmodel_projection(camera->get_camera(), fov, _near, _far);
	}
}

void Viewmodel3D::_update_camera_projection_enabled() {
	Camera3D *camera = get_camera_3d();
	if (camera && _owns_camera()) {
		RenderingServer::get_singleton()->camera_set_viewmodel_projection_enabled(camera->get_camera(), use_viewmodel_projection);
	}
}

void Viewmodel3D::_update_camera_shadow_casting() {
	Camera3D *camera = get_camera_3d();
	if (camera && _owns_camera()) {
		RenderingServer::get_singleton()->camera_set_viewmodel_cast_world_shadows(camera->get_camera(), cast_world_shadows);
	}
}

void Viewmodel3D::_update_visual_instances() {
	propagate_notification(NOTIFICATION_VIEWMODEL_CHANGED);
}

bool Viewmodel3D::_owns_camera() const {
	Camera3D *camera = get_camera_3d();
	return camera && camera->_is_viewmodel_owner(this);
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
			_set_camera(nullptr);
			break;
	}
}

void Viewmodel3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_camera_3d"), &Viewmodel3D::get_camera_3d);
	ClassDB::bind_method(D_METHOD("set_use_viewmodel_projection", "enabled"), &Viewmodel3D::set_use_viewmodel_projection);
	ClassDB::bind_method(D_METHOD("is_using_viewmodel_projection"), &Viewmodel3D::is_using_viewmodel_projection);
	ClassDB::bind_method(D_METHOD("set_visible_to_other_cameras", "enabled"), &Viewmodel3D::set_visible_to_other_cameras);
	ClassDB::bind_method(D_METHOD("is_visible_to_other_cameras"), &Viewmodel3D::is_visible_to_other_cameras);
	ClassDB::bind_method(D_METHOD("set_fov", "fov"), &Viewmodel3D::set_fov);
	ClassDB::bind_method(D_METHOD("get_fov"), &Viewmodel3D::get_fov);
	ClassDB::bind_method(D_METHOD("set_near", "near"), &Viewmodel3D::set_near);
	ClassDB::bind_method(D_METHOD("get_near"), &Viewmodel3D::get_near);
	ClassDB::bind_method(D_METHOD("set_far", "far"), &Viewmodel3D::set_far);
	ClassDB::bind_method(D_METHOD("get_far"), &Viewmodel3D::get_far);
	ClassDB::bind_method(D_METHOD("set_cast_world_shadows", "enabled"), &Viewmodel3D::set_cast_world_shadows);
	ClassDB::bind_method(D_METHOD("is_casting_world_shadows"), &Viewmodel3D::is_casting_world_shadows);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_viewmodel_projection"), "set_use_viewmodel_projection", "is_using_viewmodel_projection");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visible_to_other_cameras"), "set_visible_to_other_cameras", "is_visible_to_other_cameras");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fov", PROPERTY_HINT_RANGE, "1,179,0.1,degrees"), "set_fov", "get_fov");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "near", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater,exp,suffix:m"), "set_near", "get_near");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "far", PROPERTY_HINT_RANGE, "0.01,4000,0.01,or_greater,exp,suffix:m"), "set_far", "get_far");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cast_world_shadows"), "set_cast_world_shadows", "is_casting_world_shadows");
}

Camera3D *Viewmodel3D::get_camera_3d() const {
	return ObjectDB::get_instance<Camera3D>(camera_id);
}

void Viewmodel3D::set_use_viewmodel_projection(bool p_enabled) {
	use_viewmodel_projection = p_enabled;
	_update_camera_projection_enabled();
	_update_visual_instances();
}

bool Viewmodel3D::is_using_viewmodel_projection() const {
	return use_viewmodel_projection;
}

void Viewmodel3D::set_visible_to_other_cameras(bool p_enabled) {
	visible_to_other_cameras = p_enabled;
	_update_visual_instances();
}

bool Viewmodel3D::is_visible_to_other_cameras() const {
	return visible_to_other_cameras;
}

void Viewmodel3D::set_fov(real_t p_fov) {
	ERR_FAIL_COND(!Math::is_finite(p_fov) || p_fov < 1.0 || p_fov > 179.0);
	fov = p_fov;
	_update_camera_projection();
}

real_t Viewmodel3D::get_fov() const {
	return fov;
}

void Viewmodel3D::set_near(real_t p_near) {
	ERR_FAIL_COND(!Math::is_finite(p_near) || p_near <= 0.0);
	_near = p_near;
	_update_camera_projection();
	update_configuration_warnings();
}

real_t Viewmodel3D::get_near() const {
	return _near;
}

void Viewmodel3D::set_far(real_t p_far) {
	ERR_FAIL_COND(!Math::is_finite(p_far) || p_far <= 0.0);
	_far = p_far;
	_update_camera_projection();
	update_configuration_warnings();
}

real_t Viewmodel3D::get_far() const {
	return _far;
}

void Viewmodel3D::set_cast_world_shadows(bool p_enabled) {
	cast_world_shadows = p_enabled;
	_update_camera_shadow_casting();
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

	Camera3D *camera = get_camera_3d();
	if (camera && camera->_get_viewmodel_count() > 1) {
		if (_owns_camera()) {
			warnings.push_back(RTR("Only one Viewmodel3D can control a Camera3D. This node is active; additional Viewmodel3D nodes targeting the same camera render normally."));
		} else {
			warnings.push_back(RTR("Another Viewmodel3D already controls this Camera3D. This node renders normally until it becomes the camera's owner."));
		}
	}
	if (_far <= _near) {
		warnings.push_back(RTR("The viewmodel far clipping distance must be greater than the near clipping distance."));
	}

	return warnings;
}
