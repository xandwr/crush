/**************************************************************************/
/*  arm_3d.cpp                                                            */
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

#include "arm_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"

Arm3D::Arm3D() {
	set_physics_interpolation_mode(PHYSICS_INTERPOLATION_MODE_OFF);
	set_process_internal(true);
	set_physics_process_internal(true);
}

Arm3D::~Arm3D() {
	set_base(RID());
}

bool Arm3D::_sample_targets(Vector<Vector3> &r_points) {
	Node3D *shoulder = shoulder_target.is_empty() ? nullptr : Object::cast_to<Node3D>(get_node_or_null(shoulder_target));
	Node3D *hand = hand_target.is_empty() ? nullptr : Object::cast_to<Node3D>(get_node_or_null(hand_target));
	bool valid = shoulder && hand && shoulder->is_inside_tree() && hand->is_inside_tree();
	Transform3D shoulder_transform;
	Vector3 hand_position;
	if (valid) {
		shoulder_transform = shoulder->get_global_transform();
		hand_position = hand->get_global_position();
		valid = shoulder_transform.is_finite() && hand_position.is_finite() && !Math::is_zero_approx(shoulder_transform.basis.determinant());
	}
	if (valid != targets_valid) {
		targets_valid = valid;
		update_configuration_warnings();
	}
	if (!valid) {
		initialized = false;
		return false;
	}
	if (shoulder_id != shoulder->get_instance_id() || hand_id != hand->get_instance_id()) {
		shoulder_id = shoulder->get_instance_id();
		hand_id = hand->get_instance_id();
		initialized = false;
		previous_bend = Vector3();
	}
	Vector3 start = shoulder_transform.origin;
	Vector3 separation = hand_position - start;
	real_t distance = separation.length();
	if (!Math::is_finite(distance)) {
		initialized = false;
		return false;
	}
	Basis orientation = shoulder_transform.basis.orthonormalized();
	Vector3 axis = distance > CMP_EPSILON ? separation / distance : -orientation.get_column(2);
	real_t minimum_reach = Math::abs(upper_arm_length - forearm_length);
	real_t maximum_reach = upper_arm_length + forearm_length;
	reach_limited = distance <= minimum_reach || distance >= maximum_reach || distance < CMP_EPSILON;
	if (distance < minimum_reach || distance > maximum_reach) {
		distance = CLAMP(distance, minimum_reach, maximum_reach);
		hand_position = start + axis * distance;
	}
	Vector3 bend = orientation.xform(elbow_direction.normalized());
	bend -= axis * bend.dot(axis);
	if (bend.length_squared() < 0.0001) {
		bend = previous_bend - axis * previous_bend.dot(axis);
	}
	if (bend.length_squared() < CMP_EPSILON * CMP_EPSILON) {
		Vector3 fallback = Math::abs(axis.y) < 0.9 ? Vector3(0, -1, 0) : Vector3(1, 0, 0);
		bend = fallback - axis * fallback.dot(axis);
	}
	bend.normalize();
	previous_bend = bend;
	real_t along = 0;
	real_t height = 0;
	if (distance >= maximum_reach) {
		along = upper_arm_length;
	} else if (distance <= minimum_reach || distance < CMP_EPSILON) {
		if (distance < CMP_EPSILON) {
			height = upper_arm_length;
		} else {
			along = upper_arm_length > forearm_length ? upper_arm_length : -upper_arm_length;
		}
	} else {
		along = (upper_arm_length * upper_arm_length - forearm_length * forearm_length + distance * distance) / (2 * distance);
		height = Math::sqrt(MAX(real_t(0), upper_arm_length * upper_arm_length - along * along));
	}
	r_points = { start, start + axis * along + bend * height, hand_position };
	return true;
}

void Arm3D::_configure_solver() {
	solver.rest_length = upper_arm_length + forearm_length;
	solver.mass = total_mass;
	solver.damping = damping;
	solver.gravity = gravity;
	solver.stretch_compliance = stretch_compliance;
	solver.pose_compliance = elbow_stiffness > 0 ? 1 / elbow_stiffness : 0;
	solver.update_lengths();
	if (solver.segments.size() == 2) {
		solver.segments.write[0].length = upper_arm_length;
		solver.segments.write[1].length = forearm_length;
	}
}

void Arm3D::_reset(const Vector<Vector3> &p_points) {
	solver.reset(p_points, true);
	_configure_solver();
	for (int i : { 0, 2 }) {
		solver.particles.write[i].attached = true;
		solver.particles.write[i].target = p_points[i];
	}
	previous_points = p_points;
	target_points = p_points;
	initialized = true;
	tube.reset_frame();
}

void Arm3D::_advance(real_t p_delta) {
	if (!Math::is_finite(p_delta) || p_delta <= 0) {
		return;
	}
	Vector<Vector3> points;
	if (!_sample_targets(points)) {
		set_base(RID());
		return;
	}
	if (!initialized) {
		_reset(points);
	}
	previous_points = get_joint_positions();
	if (!simulation_enabled || reach_limited) {
		_reset(points);
		return;
	}
	real_t duration = MIN(p_delta, max_substep_duration * max_substeps);
	dropped_time += p_delta - duration;
	int steps = MAX(1, int(Math::ceil(duration / max_substep_duration)));
	real_t dt = duration / steps;
	for (int step = 0; step < steps; step++) {
		real_t fraction = real_t(step + 1) / steps;
		for (int i : { 0, 2 }) {
			solver.particles.write[i].target = target_points[i].lerp(points[i], fraction);
		}
		if (elbow_stiffness > 0) {
			solver.pose = points;
			solver.pose.write[1] = target_points[1].lerp(points[1], fraction);
		} else {
			solver.pose.clear();
		}
		solver.predict(dt);
		for (int iteration = 0; iteration < solver_iterations; iteration++) {
			solver.project(dt, iteration % 2 != 0);
		}
		solver.finish(dt);
	}
	target_points = points;
}

void Arm3D::_render(bool p_interpolate) {
	if (!initialized || !is_inside_tree()) {
		set_base(RID());
		if (!render_points.is_empty()) {
			render_points.clear();
			update_gizmos();
		}
		return;
	}
	Transform3D world = get_global_transform();
	if (!world.is_finite() || Math::is_zero_approx(world.basis.determinant())) {
		set_base(RID());
		return;
	}
	Vector<Vector3> points = get_joint_positions();
	if (p_interpolate && simulation_enabled && get_tree()->is_physics_interpolation_enabled()) {
		real_t fraction = Engine::get_singleton()->get_physics_interpolation_fraction();
		for (int i = 0; i < 3; i++) {
			points.write[i] = previous_points[i].lerp(points[i], fraction);
		}
	}
	Vector<Vector3> curve;
	Vector<real_t> radii;
	curve.push_back(points[0]);
	radii.push_back(upper_arm_radius);
	if (elbow_roundness > 0) {
		Vector3 entry = points[1].lerp(points[0], elbow_roundness * 0.5);
		Vector3 exit = points[1].lerp(points[2], elbow_roundness * 0.5);
		real_t entry_radius = Math::lerp(upper_arm_radius, upper_arm_end_radius, real_t(1) - elbow_roundness * real_t(0.5));
		real_t exit_radius = Math::lerp(forearm_radius, forearm_end_radius, elbow_roundness * real_t(0.5));
		for (int i = 0; i <= 8; i++) {
			real_t t = real_t(i) / 8;
			curve.push_back(entry.lerp(points[1], t).lerp(points[1].lerp(exit, t), t));
			radii.push_back(Math::lerp(entry_radius, exit_radius, t * t * (3 - 2 * t)));
		}
	} else {
		if (upper_arm_end_radius != forearm_radius) {
			curve.push_back(points[1].lerp(points[0], 0.1));
			radii.push_back(Math::lerp(upper_arm_radius, upper_arm_end_radius, real_t(0.9)));
		}
		curve.push_back(points[1]);
		radii.push_back((upper_arm_end_radius + forearm_radius) * 0.5);
		if (upper_arm_end_radius != forearm_radius) {
			curve.push_back(points[1].lerp(points[2], 0.1));
			radii.push_back(Math::lerp(forearm_radius, forearm_end_radius, real_t(0.1)));
		}
	}
	curve.push_back(points[2]);
	radii.push_back(forearm_end_radius);
	Transform3D to_local = world.affine_inverse();
	Vector<Vector3> local_points;
	for (const Vector3 &point : curve) {
		local_points.push_back(to_local.xform(point));
	}
	if (local_points == render_points && world == rendered_transform && get_base().is_valid()) {
		return;
	}
	render_points = local_points;
	rendered_transform = world;
	tube.update(curve, to_local, upper_arm_radius, radial_segments, 2, 1, radii);
	set_base(tube.mesh->get_rid());
	set_custom_aabb(tube.bounds);
	update_gizmos();
}

void Arm3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			reset_simulation();
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				_advance(get_physics_process_delta_time());
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (Engine::get_singleton()->is_editor_hint() || !simulation_enabled) {
				Vector<Vector3> points;
				if (_sample_targets(points)) {
					_reset(points);
				}
				_render();
			} else {
				_render(true);
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			initialized = false;
			shoulder_id = ObjectID();
			hand_id = ObjectID();
		} break;
	}
}

void Arm3D::reset_simulation() {
	initialized = false;
	previous_bend = Vector3();
	dropped_time = 0;
	if (is_inside_tree()) {
		Vector<Vector3> points;
		if (_sample_targets(points)) {
			_reset(points);
		}
		_render();
	}
}

void Arm3D::apply_elbow_impulse(Vector3 p_impulse) {
	ERR_FAIL_COND(!p_impulse.is_finite());
	if (!initialized || !simulation_enabled) {
		return;
	}
	Vector3 velocity = solver.particles[1].velocity + p_impulse * solver.particles[1].inverse_mass;
	ERR_FAIL_COND(!velocity.is_finite());
	solver.particles.write[1].velocity = velocity;
}

Vector<Vector3> Arm3D::get_joint_positions() const {
	Vector<Vector3> points;
	if (initialized) {
		for (const Rope3DSolver::Particle &particle : solver.particles) {
			points.push_back(particle.position);
		}
	}
	return points;
}

void Arm3D::_settings_changed(bool p_reset) {
	_configure_solver();
	render_points.clear();
	if (p_reset) {
		reset_simulation();
	} else {
		_render();
	}
	update_configuration_warnings();
}

PackedStringArray Arm3D::get_configuration_warnings() const {
	PackedStringArray warnings = GeometryInstance3D::get_configuration_warnings();
	if (!is_inside_tree()) {
		return warnings;
	}
	for (const NodePath &path : { shoulder_target, hand_target }) {
		Node3D *target = path.is_empty() ? nullptr : Object::cast_to<Node3D>(get_node_or_null(path));
		if (!target || !target->is_inside_tree()) {
			warnings.push_back(RTR("Assign both a shoulder target and a hand target to Node3D nodes. Arm3D is hidden until both targets are available."));
			break;
		}
	}
	Node3D *shoulder = shoulder_target.is_empty() ? nullptr : Object::cast_to<Node3D>(get_node_or_null(shoulder_target));
	if (Math::is_zero_approx(get_global_transform().basis.determinant()) || (shoulder && Math::is_zero_approx(shoulder->get_global_transform().basis.determinant()))) {
		warnings.push_back(RTR("Arm3D and its shoulder target require invertible transforms (nonzero scale)."));
	}
	return warnings;
}

void Arm3D::set_shoulder_target(NodePath p_value) {
	if (shoulder_target == p_value) {
		return;
	}
	shoulder_target = p_value;
	_settings_changed(true);
}

void Arm3D::set_hand_target(NodePath p_value) {
	if (hand_target == p_value) {
		return;
	}
	hand_target = p_value;
	_settings_changed(true);
}

void Arm3D::set_upper_arm_length(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.001 || p_value > 1000000);
	if (upper_arm_length == p_value) {
		return;
	}
	upper_arm_length = p_value;
	_settings_changed(true);
}

void Arm3D::set_forearm_length(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.001 || p_value > 1000000);
	if (forearm_length == p_value) {
		return;
	}
	forearm_length = p_value;
	_settings_changed(true);
}

void Arm3D::set_elbow_direction(Vector3 p_value) {
	ERR_FAIL_COND(!p_value.is_finite() || p_value.length_squared() < CMP_EPSILON * CMP_EPSILON || !Math::is_finite(p_value.length_squared()));
	if (elbow_direction == p_value) {
		return;
	}
	elbow_direction = p_value;
	_settings_changed(true);
}

void Arm3D::set_elbow_stiffness(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0);
	if (elbow_stiffness == p_value) {
		return;
	}
	elbow_stiffness = p_value;
	_settings_changed(false);
}

void Arm3D::set_elbow_roundness(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	if (elbow_roundness == p_value) {
		return;
	}
	elbow_roundness = p_value;
	_settings_changed(false);
}

void Arm3D::set_simulation_enabled(bool p_value) {
	if (simulation_enabled == p_value) {
		return;
	}
	simulation_enabled = p_value;
	_settings_changed(true);
}

void Arm3D::set_total_mass(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.000001 || p_value > 1000000);
	if (total_mass == p_value) {
		return;
	}
	total_mass = p_value;
	_settings_changed(false);
}

void Arm3D::set_damping(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0);
	if (damping == p_value) {
		return;
	}
	damping = p_value;
	_settings_changed(false);
}

void Arm3D::set_gravity(Vector3 p_value) {
	ERR_FAIL_COND(!p_value.is_finite());
	if (gravity == p_value) {
		return;
	}
	gravity = p_value;
	_settings_changed(false);
}

void Arm3D::set_stretch_compliance(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0);
	if (stretch_compliance == p_value) {
		return;
	}
	stretch_compliance = p_value;
	_settings_changed(false);
}

void Arm3D::set_radius(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 1000000);
	if (upper_arm_radius == p_value && upper_arm_end_radius == p_value && forearm_radius == p_value && forearm_end_radius == p_value) {
		return;
	}
	upper_arm_radius = p_value;
	upper_arm_end_radius = p_value;
	forearm_radius = p_value;
	forearm_end_radius = p_value;
	_settings_changed(false);
}

void Arm3D::set_upper_arm_radius(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 1000000);
	if (upper_arm_radius == p_value) {
		return;
	}
	upper_arm_radius = p_value;
	_settings_changed(false);
}

void Arm3D::set_forearm_radius(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 1000000);
	if (forearm_radius == p_value) {
		return;
	}
	forearm_radius = p_value;
	_settings_changed(false);
}

void Arm3D::set_upper_arm_end_radius(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 1000000);
	if (upper_arm_end_radius == p_value) {
		return;
	}
	upper_arm_end_radius = p_value;
	_settings_changed(false);
}

void Arm3D::set_forearm_end_radius(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 1000000);
	if (forearm_end_radius == p_value) {
		return;
	}
	forearm_end_radius = p_value;
	_settings_changed(false);
}

void Arm3D::set_radial_segments(int p_value) {
	ERR_FAIL_COND(p_value < 3 || p_value > 64);
	if (radial_segments == p_value) {
		return;
	}
	radial_segments = p_value;
	_settings_changed(false);
}

void Arm3D::set_solver_iterations(int p_value) {
	ERR_FAIL_COND(p_value < 1 || p_value > 64);
	if (solver_iterations == p_value) {
		return;
	}
	solver_iterations = p_value;
	_settings_changed(false);
}

void Arm3D::set_max_substep_duration(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.00001 || p_value > 0.1);
	if (max_substep_duration == p_value) {
		return;
	}
	max_substep_duration = p_value;
	_settings_changed(false);
}

void Arm3D::set_max_substeps(int p_value) {
	ERR_FAIL_COND(p_value < 1 || p_value > 64);
	if (max_substeps == p_value) {
		return;
	}
	max_substeps = p_value;
	_settings_changed(false);
}

void Arm3D::_bind_methods() {
	ADD_GROUP("Targets", "");
	ClassDB::bind_method(D_METHOD("set_shoulder_target", "value"), &Arm3D::set_shoulder_target);
	ClassDB::bind_method(D_METHOD("get_shoulder_target"), &Arm3D::get_shoulder_target);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "shoulder_target", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_shoulder_target", "get_shoulder_target");
	ClassDB::bind_method(D_METHOD("set_hand_target", "value"), &Arm3D::set_hand_target);
	ClassDB::bind_method(D_METHOD("get_hand_target"), &Arm3D::get_hand_target);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "hand_target", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_hand_target", "get_hand_target");
	ADD_GROUP("Dimensions", "");
	ClassDB::bind_method(D_METHOD("set_upper_arm_length", "value"), &Arm3D::set_upper_arm_length);
	ClassDB::bind_method(D_METHOD("get_upper_arm_length"), &Arm3D::get_upper_arm_length);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "upper_arm_length", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater"), "set_upper_arm_length", "get_upper_arm_length");
	ClassDB::bind_method(D_METHOD("set_forearm_length", "value"), &Arm3D::set_forearm_length);
	ClassDB::bind_method(D_METHOD("get_forearm_length"), &Arm3D::get_forearm_length);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "forearm_length", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater"), "set_forearm_length", "get_forearm_length");
	ADD_GROUP("Elbow", "");
	ClassDB::bind_method(D_METHOD("set_elbow_direction", "value"), &Arm3D::set_elbow_direction);
	ClassDB::bind_method(D_METHOD("get_elbow_direction"), &Arm3D::get_elbow_direction);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "elbow_direction", PROPERTY_HINT_NONE, ""), "set_elbow_direction", "get_elbow_direction");
	ClassDB::bind_method(D_METHOD("set_elbow_stiffness", "value"), &Arm3D::set_elbow_stiffness);
	ClassDB::bind_method(D_METHOD("get_elbow_stiffness"), &Arm3D::get_elbow_stiffness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "elbow_stiffness", PROPERTY_HINT_RANGE, "0,10000,1,or_greater"), "set_elbow_stiffness", "get_elbow_stiffness");
	ClassDB::bind_method(D_METHOD("set_elbow_roundness", "value"), &Arm3D::set_elbow_roundness);
	ClassDB::bind_method(D_METHOD("get_elbow_roundness"), &Arm3D::get_elbow_roundness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "elbow_roundness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_elbow_roundness", "get_elbow_roundness");
	ADD_GROUP("Physics", "");
	ClassDB::bind_method(D_METHOD("set_simulation_enabled", "value"), &Arm3D::set_simulation_enabled);
	ClassDB::bind_method(D_METHOD("get_simulation_enabled"), &Arm3D::get_simulation_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simulation_enabled", PROPERTY_HINT_NONE, ""), "set_simulation_enabled", "get_simulation_enabled");
	ClassDB::bind_method(D_METHOD("set_total_mass", "value"), &Arm3D::set_total_mass);
	ClassDB::bind_method(D_METHOD("get_total_mass"), &Arm3D::get_total_mass);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "total_mass", PROPERTY_HINT_RANGE, "0.000001,1000,0.01,or_greater"), "set_total_mass", "get_total_mass");
	ClassDB::bind_method(D_METHOD("set_damping", "value"), &Arm3D::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &Arm3D::get_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping", PROPERTY_HINT_RANGE, "0,100,0.1,or_greater"), "set_damping", "get_damping");
	ClassDB::bind_method(D_METHOD("set_gravity", "value"), &Arm3D::set_gravity);
	ClassDB::bind_method(D_METHOD("get_gravity"), &Arm3D::get_gravity);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity", PROPERTY_HINT_NONE, ""), "set_gravity", "get_gravity");
	ClassDB::bind_method(D_METHOD("set_stretch_compliance", "value"), &Arm3D::set_stretch_compliance);
	ClassDB::bind_method(D_METHOD("get_stretch_compliance"), &Arm3D::get_stretch_compliance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stretch_compliance", PROPERTY_HINT_RANGE, "0,1,0.00001,or_greater"), "set_stretch_compliance", "get_stretch_compliance");
	ADD_GROUP("Appearance", "");
	ClassDB::bind_method(D_METHOD("set_radius", "value"), &Arm3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &Arm3D::get_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_radius", "get_radius");
	ClassDB::bind_method(D_METHOD("set_upper_arm_radius", "value"), &Arm3D::set_upper_arm_radius);
	ClassDB::bind_method(D_METHOD("get_upper_arm_radius"), &Arm3D::get_upper_arm_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "upper_arm_radius", PROPERTY_HINT_RANGE, "0.00001,10,0.001,or_greater"), "set_upper_arm_radius", "get_upper_arm_radius");
	ClassDB::bind_method(D_METHOD("set_upper_arm_end_radius", "value"), &Arm3D::set_upper_arm_end_radius);
	ClassDB::bind_method(D_METHOD("get_upper_arm_end_radius"), &Arm3D::get_upper_arm_end_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "upper_arm_end_radius", PROPERTY_HINT_RANGE, "0.00001,10,0.001,or_greater"), "set_upper_arm_end_radius", "get_upper_arm_end_radius");
	ClassDB::bind_method(D_METHOD("set_forearm_radius", "value"), &Arm3D::set_forearm_radius);
	ClassDB::bind_method(D_METHOD("get_forearm_radius"), &Arm3D::get_forearm_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "forearm_radius", PROPERTY_HINT_RANGE, "0.00001,10,0.001,or_greater"), "set_forearm_radius", "get_forearm_radius");
	ClassDB::bind_method(D_METHOD("set_forearm_end_radius", "value"), &Arm3D::set_forearm_end_radius);
	ClassDB::bind_method(D_METHOD("get_forearm_end_radius"), &Arm3D::get_forearm_end_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "forearm_end_radius", PROPERTY_HINT_RANGE, "0.00001,10,0.001,or_greater"), "set_forearm_end_radius", "get_forearm_end_radius");
	ClassDB::bind_method(D_METHOD("set_radial_segments", "value"), &Arm3D::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &Arm3D::get_radial_segments);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "3,64,1"), "set_radial_segments", "get_radial_segments");
	ADD_GROUP("Solver", "");
	ClassDB::bind_method(D_METHOD("set_solver_iterations", "value"), &Arm3D::set_solver_iterations);
	ClassDB::bind_method(D_METHOD("get_solver_iterations"), &Arm3D::get_solver_iterations);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "solver_iterations", PROPERTY_HINT_RANGE, "1,64,1"), "set_solver_iterations", "get_solver_iterations");
	ClassDB::bind_method(D_METHOD("set_max_substep_duration", "value"), &Arm3D::set_max_substep_duration);
	ClassDB::bind_method(D_METHOD("get_max_substep_duration"), &Arm3D::get_max_substep_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_substep_duration", PROPERTY_HINT_RANGE, "0.00001,0.1,0.00001"), "set_max_substep_duration", "get_max_substep_duration");
	ClassDB::bind_method(D_METHOD("set_max_substeps", "value"), &Arm3D::set_max_substeps);
	ClassDB::bind_method(D_METHOD("get_max_substeps"), &Arm3D::get_max_substeps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_substeps", PROPERTY_HINT_RANGE, "1,64,1"), "set_max_substeps", "get_max_substeps");
	ClassDB::bind_method(D_METHOD("reset_simulation"), &Arm3D::reset_simulation);
	ClassDB::bind_method(D_METHOD("get_joint_positions"), &Arm3D::get_joint_positions);
	ClassDB::bind_method(D_METHOD("get_mesh"), &Arm3D::get_mesh);
	ClassDB::bind_method(D_METHOD("get_dropped_simulation_time"), &Arm3D::get_dropped_simulation_time);
	ClassDB::bind_method(D_METHOD("apply_elbow_impulse", "impulse"), &Arm3D::apply_elbow_impulse);
}
