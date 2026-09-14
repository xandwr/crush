/**************************************************************************/
/*  rope_3d_gizmo_plugin.cpp                                             */
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


#include "rope_3d_gizmo_plugin.h"

#include "scene/3d/rope_3d.h"

Rope3DGizmoPlugin::Rope3DGizmoPlugin() {
	create_material("rope_centerline", Color(0.5, 0.8, 1));
	create_material("rope_attachment", Color(1, 0.7, 0.3));
}

bool Rope3DGizmoPlugin::has_gizmo(Node3D *p_node) {
	return Object::cast_to<Rope3D>(p_node) != nullptr;
}

String Rope3DGizmoPlugin::get_gizmo_name() const {
	return "Rope3D";
}

int Rope3DGizmoPlugin::get_priority() const {
	return -1;
}

void Rope3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	Rope3D *rope = Object::cast_to<Rope3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();
	Vector<Vector3> points = rope->get_render_points();
	Vector<Vector3> lines;
	for (int i = 1; i < points.size(); i++) {
		lines.push_back(points[i - 1]);
		lines.push_back(points[i]);
	}
	p_gizmo->add_lines(lines, get_material("rope_centerline", p_gizmo));
	p_gizmo->add_collision_segments(lines);
	if (Math::is_zero_approx(rope->get_global_transform().basis.determinant())) {
		return;
	}
	lines.clear();
	for (int i = 0; i < points.size(); i++) {
		String prefix = "attachments/" + itos(i) + "/";
		if (!bool(rope->get(prefix + "enabled"))) {
			continue;
		}
		NodePath path = rope->get(prefix + "node_path");
		Node3D *target = path.is_empty() ? nullptr : Object::cast_to<Node3D>(rope->get_node_or_null(path));
		if (target && target->is_inside_tree()) {
			Vector3 offset = rope->get(prefix + "local_offset");
			lines.push_back(points[i]);
			lines.push_back(rope->to_local(target->to_global(offset)));
		}
	}
	p_gizmo->add_lines(lines, get_material("rope_attachment", p_gizmo));
}
