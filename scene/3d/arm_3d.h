/**************************************************************************/
/*  arm_3d.h                                                             */
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

#include "scene/3d/rope_3d_solver.h"
#include "scene/3d/rope_3d_tube.h"
#include "scene/3d/visual_instance_3d.h"

class Arm3D : public GeometryInstance3D {
	GDCLASS(Arm3D, GeometryInstance3D);

	Rope3DSolver solver;
	Rope3DTube tube;
	Vector<Vector3> previous_points;
	Vector<Vector3> target_points;
	Vector<Vector3> render_points;
	Vector3 previous_bend;
	Transform3D rendered_transform;
	ObjectID shoulder_id;
	ObjectID hand_id;
	bool initialized = false;
	bool targets_valid = false;
	double dropped_time = 0;
	NodePath shoulder_target = NodePath();
	NodePath hand_target = NodePath();
	real_t upper_arm_length = 0.46;
	real_t forearm_length = 0.45;
	Vector3 elbow_direction = Vector3(0, -1, 0);
	real_t elbow_stiffness = 1000;
	real_t elbow_roundness = 0;
	bool simulation_enabled = true;
	real_t total_mass = 1;
	real_t damping = 12;
	Vector3 gravity = Vector3(0, -9.8, 0);
	real_t stretch_compliance = 0;
	real_t radius = 0.065;
	int radial_segments = 12;
	int solver_iterations = 12;
	real_t max_substep_duration = 1.0 / 120;
	int max_substeps = 8;

	bool _sample_targets(Vector<Vector3> &r_points);
	void _configure_solver();
	void _reset(const Vector<Vector3> &p_points);
	void _advance(real_t p_delta);
	void _render(bool p_interpolate = false);
	void _settings_changed(bool p_reset);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_shoulder_target(NodePath p_value);
	NodePath get_shoulder_target() const { return shoulder_target; }
	void set_hand_target(NodePath p_value);
	NodePath get_hand_target() const { return hand_target; }
	void set_upper_arm_length(real_t p_value);
	real_t get_upper_arm_length() const { return upper_arm_length; }
	void set_forearm_length(real_t p_value);
	real_t get_forearm_length() const { return forearm_length; }
	void set_elbow_direction(Vector3 p_value);
	Vector3 get_elbow_direction() const { return elbow_direction; }
	void set_elbow_stiffness(real_t p_value);
	real_t get_elbow_stiffness() const { return elbow_stiffness; }
	void set_elbow_roundness(real_t p_value);
	real_t get_elbow_roundness() const { return elbow_roundness; }
	void set_simulation_enabled(bool p_value);
	bool get_simulation_enabled() const { return simulation_enabled; }
	void set_total_mass(real_t p_value);
	real_t get_total_mass() const { return total_mass; }
	void set_damping(real_t p_value);
	real_t get_damping() const { return damping; }
	void set_gravity(Vector3 p_value);
	Vector3 get_gravity() const { return gravity; }
	void set_stretch_compliance(real_t p_value);
	real_t get_stretch_compliance() const { return stretch_compliance; }
	void set_radius(real_t p_value);
	real_t get_radius() const { return radius; }
	void set_radial_segments(int p_value);
	int get_radial_segments() const { return radial_segments; }
	void set_solver_iterations(int p_value);
	int get_solver_iterations() const { return solver_iterations; }
	void set_max_substep_duration(real_t p_value);
	real_t get_max_substep_duration() const { return max_substep_duration; }
	void set_max_substeps(int p_value);
	int get_max_substeps() const { return max_substeps; }

	void reset_simulation();
	void apply_elbow_impulse(Vector3 p_impulse);
	Vector<Vector3> get_joint_positions() const;
	Vector<Vector3> get_render_points() const { return render_points; }
	Ref<ArrayMesh> get_mesh() const { return tube.mesh; }
	double get_dropped_simulation_time() const { return dropped_time; }
	AABB get_aabb() const override { return tube.bounds; }
	PackedStringArray get_configuration_warnings() const override;
	Arm3D();
	~Arm3D();
};
