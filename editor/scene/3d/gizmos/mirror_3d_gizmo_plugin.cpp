/**************************************************************************/
/*  mirror_3d_gizmo_plugin.cpp                                            */
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

#include "mirror_3d_gizmo_plugin.h"

#include "core/math/geometry_3d.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mirror_3d.h"

Mirror3DGizmoPlugin::Mirror3DGizmoPlugin() {
	create_material("mirror", Color(0.9, 0.5, 0.5));
	create_handle_material("handles");
}

bool Mirror3DGizmoPlugin::has_gizmo(Node3D *p_node) {
	return Object::cast_to<Mirror3D>(p_node) != nullptr;
}

String Mirror3DGizmoPlugin::get_gizmo_name() const {
	return "Mirror3D";
}

int Mirror3DGizmoPlugin::get_priority() const {
	return -1;
}

void Mirror3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	Mirror3D *mirror = Object::cast_to<Mirror3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();
	const Vector2 half_size = mirror->get_size() / 2;
	Vector<Vector3> lines;
	const Vector3 corners[] = { Vector3(-half_size.x, -half_size.y, 0), Vector3(half_size.x, -half_size.y, 0), Vector3(half_size.x, half_size.y, 0), Vector3(-half_size.x, half_size.y, 0) };
	for (int i = 0; i < 4; i++) {
		lines.push_back(corners[i]);
		lines.push_back(corners[(i + 1) % 4]);
	}
	p_gizmo->add_collision_segments(lines);
	lines.push_back(Vector3());
	lines.push_back(Vector3(0, 0, 0.5));
	lines.push_back(Vector3(0, 0, 0.5));
	lines.push_back(Vector3(0.1, 0, 0.35));
	lines.push_back(Vector3(0, 0, 0.5));
	lines.push_back(Vector3(-0.1, 0, 0.35));
	p_gizmo->add_lines(lines, get_material("mirror", p_gizmo));
	Vector<Vector3> handles;
	handles.push_back(Vector3(half_size.x, 0, 0));
	handles.push_back(Vector3(0, half_size.y, 0));
	p_gizmo->add_handles(handles, get_material("handles"));
}

String Mirror3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return p_id == 0 ? TTR("Width") : TTR("Height");
}

Variant Mirror3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return Object::cast_to<Mirror3D>(p_gizmo->get_node_3d())->get_size();
}

void Mirror3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	Mirror3D *mirror = Object::cast_to<Mirror3D>(p_gizmo->get_node_3d());
	const Transform3D inverse = mirror->get_global_transform().affine_inverse();
	const Vector3 ray_origin = p_camera->project_ray_origin(p_point);
	const Vector3 ray_end = ray_origin + p_camera->project_ray_normal(p_point) * 4096;
	Vector3 axis;
	axis[p_id] = 4096;
	Vector3 closest_axis;
	Vector3 closest_ray;
	Geometry3D::get_closest_points_between_segments(Vector3(), axis, inverse.xform(ray_origin), inverse.xform(ray_end), closest_axis, closest_ray);
	Vector2 size = mirror->get_size();
	size[p_id] = MAX(0.001, closest_axis[p_id] * 2);
	mirror->set_size(size);
}

void Mirror3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	Mirror3D *mirror = Object::cast_to<Mirror3D>(p_gizmo->get_node_3d());
	if (p_cancel) {
		mirror->set_size(p_restore);
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change Mirror Size"));
	undo_redo->add_do_method(mirror, "set_size", mirror->get_size());
	undo_redo->add_undo_method(mirror, "set_size", p_restore);
	undo_redo->commit_action();
}
