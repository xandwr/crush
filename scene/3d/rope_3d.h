/**************************************************************************/
/*  rope_3d.h                                                            */
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

#include "core/templates/hash_set.h"
#include "scene/3d/rope_3d_solver.h"
#include "scene/3d/rope_3d_tube.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/curve.h"

#ifndef PHYSICS_3D_DISABLED
#include "servers/physics_3d/physics_server_3d.h"
#endif

class Rope3D : public GeometryInstance3D {
	GDCLASS(Rope3D, GeometryInstance3D);

public:
	enum SimulationProcessMode { SIMULATION_PROCESS_AUTO,
		SIMULATION_PROCESS_MANUAL };
	enum CapMode { CAP_NONE,
		CAP_FLAT,
		CAP_ROUND };

private:
	struct Attachment {
		NodePath path;
		ObjectID resolved;
		Vector3 offset;
		Vector3 target;
		Vector3 previous_target;
		real_t compliance = 0;
		bool enabled = false;
		bool direct_target = false;
		bool sampled = false;
		bool missing = false;
	};
	Rope3DSolver solver;
	Rope3DTube tube;
	Ref<Curve3D> initial_curve;
	Vector<Attachment> attachments;
	Vector<Vector3> pose_targets;
	Vector<Vector3> previous_pose_targets;
	Vector<Vector3> render_points;
	Vector<Vector3> published_previous;
	Vector<Vector3> geometry_points;
	Vector<Vector3> contact_origins;
	Vector<Vector3> contact_normals;
	Vector<Vector3> contact_positions;
	Vector<Vector3> predicted_velocities;
	HashSet<RID> collision_exceptions;
	double dropped_time = 0;
	bool hard_pin_contact = false;
	bool singular_transform = false;
	bool initialized = false;
	bool advancing = false;
	bool pose_sampled = false;
#ifndef PHYSICS_3D_DISABLED
	RID collision_shape;
	PhysicsDirectSpaceState3D::ShapeParameters query;
	PhysicsDirectSpaceState3D *space_state = nullptr;
	int queries_remaining = 0;
	void _contacts(bool p_sweep);
#endif
	void _curve_changed();
	void _resize();
	void _schedule();
	void _initialize();
	void _render(bool p_interpolate = false);
	void _advance(real_t p_delta);
	Vector<Vector3> _initial_points() const;
	bool _valid_points(const Vector<Vector3> &p_points) const;
	void _clear_history();
	int particle_count = 9;
	real_t rest_length = 1;
	real_t total_mass = 1;
	real_t stretch_compliance = 0;
	real_t bend_compliance = 0.01;
	bool bend_enabled = false;
	real_t damping = 2;
	Vector3 gravity = Vector3(0, -9.8, 0);
	bool simulation_enabled = true;
	SimulationProcessMode simulation_process_mode = SIMULATION_PROCESS_AUTO;
	real_t max_substep_duration = 1.0 / 120;
	int max_substeps = 8;
	int solver_iterations = 6;
	bool render_enabled = true;
	real_t radius = 0.05;
	int radial_segments = 8;
	CapMode cap_mode = CAP_FLAT;
	real_t uv_repeat_length = 1;
	bool collision_enabled = false;
	real_t collision_radius = 0.05;
	uint32_t collision_mask = 1;
	real_t collision_margin = 0.001;
	real_t friction = 0.2;

protected:
	static void _bind_methods();
	void _notification(int p_what);
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_value) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	void set_initial_curve(const Ref<Curve3D> &p_curve);
	Ref<Curve3D> get_initial_curve() const { return initial_curve; }
	void set_particle_count(int p_value);
	int get_particle_count() const { return particle_count; }
	void set_rest_length(real_t p_value);
	real_t get_rest_length() const { return rest_length; }
	void set_total_mass(real_t p_value);
	real_t get_total_mass() const { return total_mass; }
	void set_stretch_compliance(real_t p_value);
	real_t get_stretch_compliance() const { return stretch_compliance; }
	void set_bend_compliance(real_t p_value);
	real_t get_bend_compliance() const { return bend_compliance; }
	void set_bend_enabled(bool p_value);
	bool get_bend_enabled() const { return bend_enabled; }
	void set_damping(real_t p_value);
	real_t get_damping() const { return damping; }
	void set_gravity(Vector3 p_value);
	Vector3 get_gravity() const { return gravity; }
	void set_simulation_enabled(bool p_value);
	bool get_simulation_enabled() const { return simulation_enabled; }
	void set_simulation_process_mode(SimulationProcessMode p_value);
	SimulationProcessMode get_simulation_process_mode() const { return simulation_process_mode; }
	void set_max_substep_duration(real_t p_value);
	real_t get_max_substep_duration() const { return max_substep_duration; }
	void set_max_substeps(int p_value);
	int get_max_substeps() const { return max_substeps; }
	void set_solver_iterations(int p_value);
	int get_solver_iterations() const { return solver_iterations; }
	void set_render_enabled(bool p_value);
	bool get_render_enabled() const { return render_enabled; }
	void set_radius(real_t p_value);
	real_t get_radius() const { return radius; }
	void set_radial_segments(int p_value);
	int get_radial_segments() const { return radial_segments; }
	void set_cap_mode(CapMode p_value);
	CapMode get_cap_mode() const { return cap_mode; }
	void set_uv_repeat_length(real_t p_value);
	real_t get_uv_repeat_length() const { return uv_repeat_length; }
	void set_collision_enabled(bool p_value);
	bool get_collision_enabled() const { return collision_enabled; }
	void set_collision_radius(real_t p_value);
	real_t get_collision_radius() const { return collision_radius; }
	void set_collision_mask(uint32_t p_value);
	uint32_t get_collision_mask() const { return collision_mask; }
	void set_collision_margin(real_t p_value);
	real_t get_collision_margin() const { return collision_margin; }
	void set_friction(real_t p_value);
	real_t get_friction() const { return friction; }

	void attach_particle(int p_index, const NodePath &p_path, const Vector3 &p_offset = Vector3(), real_t p_compliance = 0);
	void set_particle_target(int p_index, const Vector3 &p_position, real_t p_compliance = 0);
	void clear_attachment(int p_index);
	void set_pose_targets(const Vector<Vector3> &p_points, real_t p_compliance);
	void clear_pose_targets();
	void reset_simulation();
	void reset_to_points(const Vector<Vector3> &p_points);
	Vector<Vector3> get_simulated_points() const;
	Vector3 get_point_position(int p_index) const;
	void apply_impulse(int p_index, const Vector3 &p_impulse);
	void set_render_points(const Vector<Vector3> &p_points);
	Vector<Vector3> get_render_points() const { return render_points; }
	void advance_simulation(real_t p_delta);
	void add_collision_exception(RID p_rid);
	void remove_collision_exception(RID p_rid);
	void clear_collision_exceptions();
	real_t get_max_segment_error() const { return solver.max_error(); }
	bool is_overstretched() const { return solver.overstretched(); }
	bool has_hard_pin_contact() const { return hard_pin_contact; }
	double get_dropped_simulation_time() const { return dropped_time; }
	AABB get_aabb() const override { return tube.bounds; }
	Ref<ArrayMesh> get_mesh() const { return tube.mesh; }
	PackedStringArray get_configuration_warnings() const override;
	Rope3D();
	~Rope3D();
};

VARIANT_ENUM_CAST(Rope3D::SimulationProcessMode);
VARIANT_ENUM_CAST(Rope3D::CapMode);
