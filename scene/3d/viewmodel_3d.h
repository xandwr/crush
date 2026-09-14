/**************************************************************************/
/*  viewmodel_3d.h                                                        */
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

#include "scene/3d/node_3d.h"

class Camera3D;

class Viewmodel3D : public Node3D {
	GDCLASS(Viewmodel3D, Node3D);
	friend class Camera3D;
	friend class VisualInstance3D;

	static constexpr int NOTIFICATION_VIEWMODEL_CHANGED = 1001;

	ObjectID camera_id;
	bool use_viewmodel_projection = true;
	bool visible_to_other_cameras = false;
	real_t fov = 54.0;
	real_t _near = 0.01;
	real_t _far = 100.0;
	bool cast_world_shadows = false;

	void _set_camera(Camera3D *p_camera);
	void _update_camera();
	void _camera_ownership_changed();
	void _update_camera_projection();
	void _update_camera_projection_enabled();
	void _update_camera_shadow_casting();
	void _update_visual_instances();
	bool _owns_camera() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	Camera3D *get_camera_3d() const;

	void set_use_viewmodel_projection(bool p_enabled);
	bool is_using_viewmodel_projection() const;

	void set_visible_to_other_cameras(bool p_enabled);
	bool is_visible_to_other_cameras() const;

	void set_fov(real_t p_fov);
	real_t get_fov() const;

	void set_near(real_t p_near);
	real_t get_near() const;

	void set_far(real_t p_far);
	real_t get_far() const;

	void set_cast_world_shadows(bool p_enabled);
	bool is_casting_world_shadows() const;

	PackedStringArray get_configuration_warnings() const override;
};
