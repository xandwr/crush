/**************************************************************************/
/*  rope_3d.cpp                                                          */
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

#include "rope_3d.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/3d/world_3d.h"

Rope3D::Rope3D() {
	set_physics_interpolation_mode(PHYSICS_INTERPOLATION_MODE_OFF);
	_resize();
	_schedule();
}

Rope3D::~Rope3D() {
	set_base(RID());
#ifndef PHYSICS_3D_DISABLED
	if (collision_shape.is_valid()) {
		PhysicsServer3D::get_singleton()->free_rid(collision_shape);
	}
#endif
	if (initial_curve.is_valid()) {
		initial_curve->disconnect_changed(callable_mp(this, &Rope3D::_curve_changed));
	}
}

bool Rope3D::_valid_points(const Vector<Vector3> &p_points) const {
	if (p_points.size() != particle_count) {
		return false;
	}
	for (int i = 0; i < p_points.size(); i++) {
		if (!p_points[i].is_finite() || (i > 0 && !Math::is_finite(p_points[i].distance_squared_to(p_points[i - 1])))) {
			return false;
		}
	}
	return true;
}

Vector<Vector3> Rope3D::_initial_points() const {
	Vector<Vector3> points;
	points.resize(particle_count);
	for (int i = 0; i < particle_count; i++) {
		real_t fraction = real_t(i) / (particle_count - 1);
		points.write[i] = initial_curve.is_valid() && initial_curve->get_point_count() >= 2 ? initial_curve->sample_baked(initial_curve->get_baked_length() * fraction) : Vector3(0, -rest_length * fraction, 0);
	}
	return points;
}

void Rope3D::_initialize() {
	Vector<Vector3> points = _initial_points();
	if (is_inside_tree()) {
		Transform3D world = get_global_transform();
		for (int i = 0; i < points.size(); i++) {
			points.write[i] = world.xform(points[i]);
		}
	}
	ERR_FAIL_COND(!_valid_points(points));
	solver.reset(points, true);
	render_points = _initial_points();
	initialized = true;
	_clear_history();
	_render();
}

void Rope3D::_resize() {
	Vector<Vector3> current = get_simulated_points();
	attachments.resize(particle_count);
	pose_targets.clear();
	previous_pose_targets.clear();
	solver.pose.clear();
	geometry_points.resize(particle_count);
	contact_origins.resize(particle_count);
	contact_normals.resize(particle_count);
	contact_positions.resize(particle_count);
	predicted_velocities.resize(particle_count);
	if (initialized && current.size() >= 2) {
		solver.reset(Rope3DSolver::resample(current, particle_count), true);
		if (!render_points.is_empty()) {
			render_points = Rope3DSolver::resample(render_points, particle_count);
		}
		_clear_history();
	} else {
		solver.reset(_initial_points(), true);
		render_points = _initial_points();
	}
	tube.invalidate();
	_render();
}

void Rope3D::_clear_history() {
	dropped_time = 0;
	hard_pin_contact = false;
	pose_sampled = false;
	previous_pose_targets = pose_targets;
	for (int i = 0; i < solver.particles.size(); i++) {
		Rope3DSolver::Particle &particle = solver.particles.write[i];
		particle.previous = particle.position;
		particle.velocity = Vector3();
		particle.attachment_lambda = Vector3();
		particle.pose_lambda = Vector3();
	}
	for (int i = 0; i < attachments.size(); i++) {
		attachments.write[i].previous_target = attachments[i].target;
		attachments.write[i].sampled = false;
	}
	for (int i = 0; i < solver.segments.size(); i++) {
		solver.segments.write[i].lambda = 0;
	}
	published_previous.resize(particle_count);
	for (int i = 0; i < particle_count; i++) {
		published_previous.write[i] = solver.particles[i].position;
	}
	tube.reset_frame();
}

void Rope3D::_schedule() {
	bool automatic = simulation_enabled && simulation_process_mode == SIMULATION_PROCESS_AUTO;
	set_physics_process_internal(automatic);
	set_process_internal(automatic);
}

void Rope3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!initialized) {
				_initialize();
			}
			set_notify_transform(true);
			_schedule();
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				_advance(get_physics_process_delta_time());
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				_render(true);
			}
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			_render();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			for (int i = 0; i < attachments.size(); i++) {
				attachments.write[i].resolved = ObjectID();
				attachments.write[i].sampled = false;
			}
		} break;
	}
}

void Rope3D::_render(bool p_interpolate) {
	if (!render_enabled) {
		set_base(RID());
		return;
	}
	Transform3D to_local;
	Transform3D world = is_inside_tree() ? get_global_transform() : get_transform();
	bool singular = !world.is_finite() || !Math::is_finite(world.basis.determinant()) || Math::is_zero_approx(world.basis.determinant());
	if (singular != singular_transform) {
		singular_transform = singular;
		update_configuration_warnings();
	}
	if (singular) {
		return;
	}
	if (simulation_enabled && !Engine::get_singleton()->is_editor_hint()) {
		to_local = world.affine_inverse();
		real_t fraction = p_interpolate && get_tree() && get_tree()->is_physics_interpolation_enabled() ? Engine::get_singleton()->get_physics_interpolation_fraction() : 1;
		for (int i = 0; i < particle_count; i++) {
			geometry_points.write[i] = published_previous.size() == particle_count ? published_previous[i].lerp(solver.particles[i].position, fraction) : solver.particles[i].position;
		}
	} else {
		geometry_points = Engine::get_singleton()->is_editor_hint() ? _initial_points() : render_points;
	}
	if (!_valid_points(geometry_points)) {
		return;
	}
	tube.update(geometry_points, to_local, radius, radial_segments, cap_mode, uv_repeat_length);
	if (simulation_enabled && !Engine::get_singleton()->is_editor_hint()) {
		Vector3 extents = Vector3(to_local.basis[0].length(), to_local.basis[1].length(), to_local.basis[2].length()) * radius;
		for (int i = 0; i < particle_count; i++) {
			Vector3 current = to_local.xform(solver.particles[i].position);
			tube.bounds.expand_to(current - extents);
			tube.bounds.expand_to(current + extents);
			if (published_previous.size() == particle_count) {
				Vector3 previous = to_local.xform(published_previous[i]);
				tube.bounds.expand_to(previous - extents);
				tube.bounds.expand_to(previous + extents);
			}
		}
		tube.mesh->set_custom_aabb(tube.bounds);
	}
	set_base(tube.mesh->get_rid());
	set_custom_aabb(tube.bounds);
	update_gizmos();
}

void Rope3D::set_initial_curve(const Ref<Curve3D> &p_curve) {
	if (initial_curve == p_curve) {
		return;
	}
	if (p_curve.is_valid()) {
		for (int i = 0; i < p_curve->get_point_count(); i++) {
			ERR_FAIL_COND(!p_curve->get_point_position(i).is_finite() || !p_curve->get_point_in(i).is_finite() || !p_curve->get_point_out(i).is_finite());
		}
	}
	if (initial_curve.is_valid()) {
		initial_curve->disconnect_changed(callable_mp(this, &Rope3D::_curve_changed));
	}
	initial_curve = p_curve;
	if (initial_curve.is_valid()) {
		initial_curve->connect_changed(callable_mp(this, &Rope3D::_curve_changed));
	}
	_curve_changed();
}

void Rope3D::_curve_changed() {
	ERR_FAIL_COND(!_valid_points(_initial_points()));
	_initialize();
}

void Rope3D::attach_particle(int p_index, const NodePath &p_path, const Vector3 &p_offset, real_t p_compliance) {
	ERR_FAIL_INDEX(p_index, particle_count);
	ERR_FAIL_COND(p_path.is_empty() || !p_offset.is_finite() || !Math::is_finite(p_compliance) || p_compliance < 0);
	Attachment &attachment = attachments.write[p_index];
	attachment = Attachment();
	attachment.path = p_path;
	attachment.offset = p_offset;
	attachment.compliance = p_compliance;
	attachment.enabled = true;
	update_configuration_warnings();
}

void Rope3D::set_particle_target(int p_index, const Vector3 &p_position, real_t p_compliance) {
	ERR_FAIL_INDEX(p_index, particle_count);
	ERR_FAIL_COND(!p_position.is_finite() || !Math::is_finite(p_compliance) || p_compliance < 0);
	Attachment &attachment = attachments.write[p_index];
	if (!attachment.enabled || !attachment.direct_target) {
		attachment = Attachment();
		attachment.previous_target = p_position;
	}
	attachment.path = NodePath();
	attachment.target = p_position;
	attachment.compliance = p_compliance;
	attachment.enabled = true;
	attachment.missing = false;
	attachment.direct_target = true;
}

void Rope3D::clear_attachment(int p_index) {
	ERR_FAIL_INDEX(p_index, particle_count);
	attachments.write[p_index] = Attachment();
	solver.particles.write[p_index].attached = false;
	update_configuration_warnings();
}

void Rope3D::set_pose_targets(const Vector<Vector3> &p_points, real_t p_compliance) {
	ERR_FAIL_COND(!_valid_points(p_points) || !Math::is_finite(p_compliance) || p_compliance < 0);
	pose_targets = p_points;
	solver.pose_compliance = p_compliance;
	if (!pose_sampled) {
		previous_pose_targets = pose_targets;
	}
}

void Rope3D::clear_pose_targets() {
	pose_targets.clear();
	previous_pose_targets.clear();
	solver.pose.clear();
	pose_sampled = false;
}

void Rope3D::reset_simulation() {
	_initialize();
}

void Rope3D::reset_to_points(const Vector<Vector3> &p_points) {
	ERR_FAIL_COND(!_valid_points(p_points));
	solver.reset(p_points, false);
	initialized = true;
	_clear_history();
	_render();
}

Vector<Vector3> Rope3D::get_simulated_points() const {
	Vector<Vector3> points;
	points.resize(solver.particles.size());
	for (int i = 0; i < points.size(); i++) {
		points.write[i] = solver.particles[i].position;
	}
	return points;
}

Vector3 Rope3D::get_point_position(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, solver.particles.size(), Vector3());
	return solver.particles[p_index].position;
}

void Rope3D::apply_impulse(int p_index, const Vector3 &p_impulse) {
	ERR_FAIL_INDEX(p_index, particle_count);
	ERR_FAIL_COND(!p_impulse.is_finite());
	const Attachment &attachment = attachments[p_index];
	if (attachment.enabled && attachment.compliance == 0 && (attachment.direct_target || (!attachment.path.is_empty() && is_inside_tree() && Object::cast_to<Node3D>(get_node_or_null(attachment.path))))) {
		return;
	}
	Vector3 velocity = solver.particles[p_index].velocity + p_impulse * solver.particles[p_index].inverse_mass;
	ERR_FAIL_COND(!velocity.is_finite());
	solver.particles.write[p_index].velocity = velocity;
}

void Rope3D::set_render_points(const Vector<Vector3> &p_points) {
	ERR_FAIL_COND(simulation_enabled || !_valid_points(p_points));
	render_points = p_points;
	_render();
}

void Rope3D::advance_simulation(real_t p_delta) {
	ERR_FAIL_COND(simulation_process_mode != SIMULATION_PROCESS_MANUAL);
	_advance(p_delta);
}

void Rope3D::_advance(real_t p_delta) {
	ERR_FAIL_COND(!Math::is_finite(p_delta) || p_delta < 0 || advancing);
	if (!simulation_enabled || p_delta == 0 || Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (collision_enabled) {
#ifndef PHYSICS_3D_DISABLED
		ERR_FAIL_COND(!is_inside_tree() || !Engine::get_singleton()->is_in_physics_frame());
		space_state = get_world_3d()->get_direct_space_state();
		ERR_FAIL_NULL(space_state);
		if (!collision_shape.is_valid()) {
			collision_shape = PhysicsServer3D::get_singleton()->sphere_shape_create();
			PhysicsServer3D::get_singleton()->shape_set_data(collision_shape, collision_radius);
		}
		query.shape_rid = collision_shape;
		query.exclude = collision_exceptions;
		query.collision_mask = collision_mask;
		query.margin = collision_margin;
#endif
	}
	advancing = true;
	if (!initialized) {
		_initialize();
	}
	published_previous.resize(particle_count);
	for (int i = 0; i < particle_count; i++) {
		published_previous.write[i] = solver.particles[i].position;
	}
	bool warnings_changed = false;
	for (int i = 0; i < particle_count; i++) {
		Attachment &attachment = attachments.write[i];
		Rope3DSolver::Particle &particle = solver.particles.write[i];
		bool available = attachment.enabled;
		if (available && !attachment.direct_target) {
			Node3D *node = is_inside_tree() && !attachment.path.is_empty() ? Object::cast_to<Node3D>(get_node_or_null(attachment.path)) : nullptr;
			bool missing = !node || !node->is_inside_tree();
			warnings_changed |= attachment.missing != missing;
			attachment.missing = missing;
			available = !missing;
			if (available) {
				if (attachment.resolved != node->get_instance_id()) {
					attachment.sampled = false;
				}
				attachment.resolved = node->get_instance_id();
				attachment.target = node->get_global_transform().xform(attachment.offset);
			} else {
				attachment.resolved = ObjectID();
				attachment.sampled = false;
			}
		}
		available &= attachment.target.is_finite();
		particle.attached = available;
		particle.compliance = attachment.compliance;
		if (!attachment.sampled) {
			attachment.previous_target = attachment.target;
		}
	}
	real_t accepted = MIN(p_delta, max_substep_duration * max_substeps);
	dropped_time += p_delta - accepted;
	int steps = CLAMP(int(Math::ceil(accepted / max_substep_duration)), 1, max_substeps);
	real_t dt = accepted / steps;
	hard_pin_contact = false;
	for (int step = 0; step < steps; step++) {
		real_t fraction = real_t(step + 1) / steps;
		for (int i = 0; i < particle_count; i++) {
			solver.particles.write[i].target = attachments[i].previous_target.lerp(attachments[i].target, fraction);
			contact_normals.write[i] = Vector3();
		}
		if (!pose_targets.is_empty()) {
			solver.pose.resize(particle_count);
			for (int i = 0; i < particle_count; i++) {
				solver.pose.write[i] = previous_pose_targets[i].lerp(pose_targets[i], fraction);
			}
		}
#ifndef PHYSICS_3D_DISABLED
		queries_remaining = particle_count * (solver_iterations + 1) * 5;
#endif
		for (int i = 0; i < particle_count; i++) {
			contact_origins.write[i] = solver.particles[i].position;
		}
		solver.predict(dt);
		for (int i = 0; i < particle_count; i++) {
			predicted_velocities.write[i] = solver.particles[i].velocity;
		}
#ifndef PHYSICS_3D_DISABLED
		if (collision_enabled) {
			_contacts(true);
		}
#endif
		for (int iteration = 0; iteration < solver_iterations; iteration++) {
			for (int i = 0; i < particle_count; i++) {
				contact_origins.write[i] = solver.particles[i].position;
			}
			solver.project(dt, (iteration + step) % 2 != 0);
#ifndef PHYSICS_3D_DISABLED
			if (collision_enabled) {
				_contacts(true);
			}
#endif
			solver.restore_pins();
		}
		solver.finish(dt);
		for (int i = 0; i < particle_count; i++) {
			Rope3DSolver::Particle &particle = solver.particles.write[i];
			if (!particle.position.is_finite() || !particle.velocity.is_finite()) {
				particle.position = published_previous[i];
				particle.velocity = Vector3();
			}
			Vector3 normal = contact_normals[i];
			if (!normal.is_zero_approx() && solver.weight(i) > 0) {
				real_t inward = particle.velocity.dot(normal);
				particle.velocity -= normal * MIN(inward, real_t(0));
				Vector3 tangent = particle.velocity - normal * particle.velocity.dot(normal);
				real_t normal_impulse_speed = MAX(MAX(-inward, -predicted_velocities[i].dot(normal)), real_t(0));
				real_t reduction = MIN(tangent.length(), friction * normal_impulse_speed);
				if (tangent.length() > CMP_EPSILON) {
					particle.velocity -= tangent.normalized() * reduction;
				}
			}
		}
	}
	for (int i = 0; i < particle_count; i++) {
		attachments.write[i].previous_target = attachments[i].target;
		attachments.write[i].sampled = solver.particles[i].attached;
	}
	previous_pose_targets = pose_targets;
	pose_sampled = !pose_targets.is_empty();
	if (warnings_changed) {
		update_configuration_warnings();
	}
	if (simulation_process_mode == SIMULATION_PROCESS_MANUAL) {
		_render();
	}
	advancing = false;
}

#ifndef PHYSICS_3D_DISABLED
void Rope3D::_contacts(bool p_sweep) {
	for (int i = 0; i < particle_count && queries_remaining > 0; i++) {
		Rope3DSolver::Particle &particle = solver.particles.write[i];
		bool pinned = solver.weight(i) == 0;
		if (!pinned && !contact_normals[i].is_zero_approx()) {
			real_t separation = contact_normals[i].dot(particle.position - contact_positions[i]);
			if (separation < 0) {
				particle.position -= contact_normals[i] * separation;
			}
		}
		query.transform = Transform3D(Basis(), contact_origins[i]);
		query.motion = particle.position - contact_origins[i];
		PhysicsDirectSpaceState3D::ShapeRestInfo info;
		if (!pinned && p_sweep && query.motion.length_squared() > CMP_EPSILON * CMP_EPSILON) {
			real_t safe = 1, unsafe = 1;
			queries_remaining--;
			if (space_state->cast_motion(query, safe, unsafe) && safe < 1) {
				Vector3 motion = query.motion;
				particle.position = contact_origins[i] + motion * safe;
				query.transform.origin = contact_origins[i] + motion * unsafe + motion.normalized() * MAX(collision_margin, real_t(0.00001));
				query.motion = Vector3();
				queries_remaining--;
				contact_normals.write[i] = space_state->rest_info(query, &info) && info.normal.is_finite() && !info.normal.is_zero_approx() ? info.normal : -motion.normalized();
				contact_positions.write[i] = particle.position;
			}
		}
		query.motion = Vector3();
		// Paired contact points provide bounded initial/moving-overlap recovery on both backends.
		for (int recovery = 0; recovery < 3 && queries_remaining > 0; recovery++) {
			query.transform.origin = particle.position;
			Vector3 contacts[8];
			int count = 0;
			queries_remaining--;
			if (!space_state->collide_shape(query, contacts, 4, count) || count == 0) {
				break;
			}
			if (pinned) {
				hard_pin_contact = true;
				break;
			}
			Vector3 separation;
			for (int contact = 0; contact < count; contact++) {
				Vector3 depth = contacts[contact * 2 + 1] - contacts[contact * 2];
				if (depth.is_finite() && depth.length_squared() > separation.length_squared()) {
					separation = depth;
				}
			}
			if (separation.length_squared() < CMP_EPSILON * CMP_EPSILON) {
				break;
			}
			Vector3 normal = separation.normalized();
			particle.position += separation + normal * collision_margin;
			contact_normals.write[i] = normal;
			contact_positions.write[i] = particle.position;
		}
	}
}
#endif

void Rope3D::add_collision_exception(RID p_rid) {
	ERR_FAIL_COND(!p_rid.is_valid());
	collision_exceptions.insert(p_rid);
}

void Rope3D::remove_collision_exception(RID p_rid) {
	collision_exceptions.erase(p_rid);
}

void Rope3D::clear_collision_exceptions() {
	collision_exceptions.clear();
}

PackedStringArray Rope3D::get_configuration_warnings() const {
	PackedStringArray warnings = GeometryInstance3D::get_configuration_warnings();
	if (singular_transform || (is_inside_tree() && Math::is_zero_approx(get_global_transform().basis.determinant()))) {
		warnings.push_back(RTR("A singular transform suspends Rope3D geometry updates."));
	}
	for (int i = 0; i < attachments.size(); i++) {
		const Attachment &attachment = attachments[i];
		if (attachment.enabled && !attachment.direct_target && (attachment.path.is_empty() || !is_inside_tree() || !Object::cast_to<Node3D>(get_node_or_null(attachment.path)))) {
			warnings.push_back(vformat(RTR("Particle %d attachment is unresolved. Its constraint is released."), i));
		}
	}
#ifdef PHYSICS_3D_DISABLED
	if (collision_enabled) {
		warnings.push_back(RTR("Rope3D collision is unavailable in a build without 3D physics."));
	}
#endif
	return warnings;
}

void Rope3D::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < particle_count; i++) {
		String prefix = "attachments/" + itos(i) + "/";
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "enabled"));
		p_list->push_back(PropertyInfo(Variant::NODE_PATH, prefix + "node_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"));
		p_list->push_back(PropertyInfo(Variant::VECTOR3, prefix + "local_offset"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "compliance", PROPERTY_HINT_RANGE, "0,1,0.00001,or_greater"));
	}
}

bool Rope3D::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;
	if (!name.begins_with("attachments/") || name.get_slice_count("/") != 3) {
		return false;
	}
	int index = name.get_slice("/", 1).to_int();
	ERR_FAIL_INDEX_V(index, particle_count, false);
	String field = name.get_slice("/", 2);
	Attachment &attachment = attachments.write[index];
	if (field == "enabled") {
		attachment.enabled = p_value;
		attachment.direct_target = false;
	} else if (field == "node_path") {
		attachment.path = p_value;
		attachment.direct_target = false;
	} else if (field == "local_offset") {
		Vector3 offset = p_value;
		ERR_FAIL_COND_V(!offset.is_finite(), false);
		attachment.offset = offset;
	} else if (field == "compliance") {
		real_t compliance = p_value;
		ERR_FAIL_COND_V(!Math::is_finite(compliance) || compliance < 0, false);
		attachment.compliance = compliance;
	} else {
		return false;
	}
	attachment.sampled = false;
	attachment.resolved = ObjectID();
	update_configuration_warnings();
	return true;
}

bool Rope3D::_get(const StringName &p_name, Variant &r_value) const {
	String name = p_name;
	if (!name.begins_with("attachments/") || name.get_slice_count("/") != 3) {
		return false;
	}
	int index = name.get_slice("/", 1).to_int();
	ERR_FAIL_INDEX_V(index, particle_count, false);
	const Attachment &attachment = attachments[index];
	String field = name.get_slice("/", 2);
	if (field == "enabled") {
		r_value = attachment.enabled;
	} else if (field == "node_path") {
		r_value = attachment.path;
	} else if (field == "local_offset") {
		r_value = attachment.offset;
	} else if (field == "compliance") {
		r_value = attachment.compliance;
	} else {
		return false;
	}
	return true;
}

void Rope3D::set_particle_count(int p_value) {
	ERR_FAIL_COND(p_value < 2 || p_value > 256);
	if (particle_count == p_value) {
		return;
	}
	particle_count = p_value;
	_resize();
	notify_property_list_changed();
}

void Rope3D::set_rest_length(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0);
	if (rest_length == p_value) {
		return;
	}
	rest_length = p_value;
	solver.rest_length = rest_length;
	solver.update_lengths();
}

void Rope3D::set_total_mass(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.000001 || p_value > 1000000);
	if (total_mass == p_value) {
		return;
	}
	total_mass = p_value;
	solver.mass = total_mass;
	solver.update_lengths();
}

void Rope3D::set_stretch_compliance(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0);
	if (stretch_compliance == p_value) {
		return;
	}
	stretch_compliance = p_value;
	solver.stretch_compliance = p_value;
}

void Rope3D::set_bend_compliance(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0);
	if (bend_compliance == p_value) {
		return;
	}
	bend_compliance = p_value;
	solver.bend_compliance = p_value;
}

void Rope3D::set_bend_enabled(bool p_value) {
	if (bend_enabled == p_value) {
		return;
	}
	bend_enabled = p_value;
	solver.bend_enabled = p_value;
}

void Rope3D::set_damping(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0);
	if (damping == p_value) {
		return;
	}
	damping = p_value;
	solver.damping = p_value;
}

void Rope3D::set_gravity(Vector3 p_value) {
	ERR_FAIL_COND(!p_value.is_finite());
	if (gravity == p_value) {
		return;
	}
	gravity = p_value;
	solver.gravity = p_value;
}

void Rope3D::set_simulation_enabled(bool p_value) {
	if (simulation_enabled == p_value) {
		return;
	}
	simulation_enabled = p_value;
	_clear_history();
	_schedule();
	_render();
}

void Rope3D::set_simulation_process_mode(SimulationProcessMode p_value) {
	ERR_FAIL_COND(p_value < SIMULATION_PROCESS_AUTO || p_value > SIMULATION_PROCESS_MANUAL);
	if (simulation_process_mode == p_value) {
		return;
	}
	simulation_process_mode = p_value;
	_clear_history();
	_schedule();
	_render();
}

void Rope3D::set_max_substep_duration(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.00001 || p_value > 0.1);
	if (max_substep_duration == p_value) {
		return;
	}
	max_substep_duration = p_value;
}

void Rope3D::set_max_substeps(int p_value) {
	ERR_FAIL_COND(p_value < 1 || p_value > 64);
	if (max_substeps == p_value) {
		return;
	}
	max_substeps = p_value;
}

void Rope3D::set_solver_iterations(int p_value) {
	ERR_FAIL_COND(p_value < 1 || p_value > 64);
	if (solver_iterations == p_value) {
		return;
	}
	solver_iterations = p_value;
}

void Rope3D::set_render_enabled(bool p_value) {
	if (render_enabled == p_value) {
		return;
	}
	render_enabled = p_value;
	_render();
}

void Rope3D::set_radius(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0);
	if (radius == p_value) {
		return;
	}
	radius = p_value;
	_render();
}

void Rope3D::set_radial_segments(int p_value) {
	ERR_FAIL_COND(p_value < 3 || p_value > 64);
	if (radial_segments == p_value) {
		return;
	}
	radial_segments = p_value;
	_render();
}

void Rope3D::set_cap_mode(CapMode p_value) {
	ERR_FAIL_COND(p_value < CAP_NONE || p_value > CAP_ROUND);
	if (cap_mode == p_value) {
		return;
	}
	cap_mode = p_value;
	_render();
}

void Rope3D::set_uv_repeat_length(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0);
	if (uv_repeat_length == p_value) {
		return;
	}
	uv_repeat_length = p_value;
	_render();
}

void Rope3D::set_collision_enabled(bool p_value) {
	if (collision_enabled == p_value) {
		return;
	}
	collision_enabled = p_value;
#ifndef PHYSICS_3D_DISABLED
	if (collision_shape.is_valid()) {
		PhysicsServer3D::get_singleton()->shape_set_data(collision_shape, collision_radius);
	}
#endif
	update_configuration_warnings();
}

void Rope3D::set_collision_radius(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0);
	if (collision_radius == p_value) {
		return;
	}
	collision_radius = p_value;
#ifndef PHYSICS_3D_DISABLED
	if (collision_shape.is_valid()) {
		PhysicsServer3D::get_singleton()->shape_set_data(collision_shape, collision_radius);
	}
#endif
	update_configuration_warnings();
}

void Rope3D::set_collision_mask(uint32_t p_value) {
	if (collision_mask == p_value) {
		return;
	}
	collision_mask = p_value;
}

void Rope3D::set_collision_margin(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0);
	if (collision_margin == p_value) {
		return;
	}
	collision_margin = p_value;
}

void Rope3D::set_friction(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	if (friction == p_value) {
		return;
	}
	friction = p_value;
}

void Rope3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_particle_count", "value"), &Rope3D::set_particle_count);
	ClassDB::bind_method(D_METHOD("get_particle_count"), &Rope3D::get_particle_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "particle_count", PROPERTY_HINT_RANGE, "2,256,1"), "set_particle_count", "get_particle_count");
	ClassDB::bind_method(D_METHOD("set_rest_length", "value"), &Rope3D::set_rest_length);
	ClassDB::bind_method(D_METHOD("get_rest_length"), &Rope3D::get_rest_length);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rest_length", PROPERTY_HINT_RANGE, "0.0001,1000,0.01,or_greater"), "set_rest_length", "get_rest_length");
	ClassDB::bind_method(D_METHOD("set_total_mass", "value"), &Rope3D::set_total_mass);
	ClassDB::bind_method(D_METHOD("get_total_mass"), &Rope3D::get_total_mass);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "total_mass", PROPERTY_HINT_RANGE, "0.000001,1000,0.01,or_greater"), "set_total_mass", "get_total_mass");
	ClassDB::bind_method(D_METHOD("set_stretch_compliance", "value"), &Rope3D::set_stretch_compliance);
	ClassDB::bind_method(D_METHOD("get_stretch_compliance"), &Rope3D::get_stretch_compliance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stretch_compliance", PROPERTY_HINT_RANGE, "0,1,0.00001,or_greater"), "set_stretch_compliance", "get_stretch_compliance");
	ClassDB::bind_method(D_METHOD("set_bend_compliance", "value"), &Rope3D::set_bend_compliance);
	ClassDB::bind_method(D_METHOD("get_bend_compliance"), &Rope3D::get_bend_compliance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bend_compliance", PROPERTY_HINT_RANGE, "0,1,0.00001,or_greater"), "set_bend_compliance", "get_bend_compliance");
	ClassDB::bind_method(D_METHOD("set_bend_enabled", "value"), &Rope3D::set_bend_enabled);
	ClassDB::bind_method(D_METHOD("get_bend_enabled"), &Rope3D::get_bend_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bend_enabled"), "set_bend_enabled", "get_bend_enabled");
	ClassDB::bind_method(D_METHOD("set_damping", "value"), &Rope3D::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &Rope3D::get_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping", PROPERTY_HINT_RANGE, "0,100,0.1,or_greater"), "set_damping", "get_damping");
	ClassDB::bind_method(D_METHOD("set_gravity", "value"), &Rope3D::set_gravity);
	ClassDB::bind_method(D_METHOD("get_gravity"), &Rope3D::get_gravity);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity"), "set_gravity", "get_gravity");
	ClassDB::bind_method(D_METHOD("set_simulation_enabled", "value"), &Rope3D::set_simulation_enabled);
	ClassDB::bind_method(D_METHOD("get_simulation_enabled"), &Rope3D::get_simulation_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simulation_enabled"), "set_simulation_enabled", "get_simulation_enabled");
	ClassDB::bind_method(D_METHOD("set_simulation_process_mode", "value"), &Rope3D::set_simulation_process_mode);
	ClassDB::bind_method(D_METHOD("get_simulation_process_mode"), &Rope3D::get_simulation_process_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "simulation_process_mode", PROPERTY_HINT_ENUM, "Auto,Manual"), "set_simulation_process_mode", "get_simulation_process_mode");
	ClassDB::bind_method(D_METHOD("set_max_substep_duration", "value"), &Rope3D::set_max_substep_duration);
	ClassDB::bind_method(D_METHOD("get_max_substep_duration"), &Rope3D::get_max_substep_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_substep_duration", PROPERTY_HINT_RANGE, "0.00001,0.1,0.00001"), "set_max_substep_duration", "get_max_substep_duration");
	ClassDB::bind_method(D_METHOD("set_max_substeps", "value"), &Rope3D::set_max_substeps);
	ClassDB::bind_method(D_METHOD("get_max_substeps"), &Rope3D::get_max_substeps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_substeps", PROPERTY_HINT_RANGE, "1,64,1"), "set_max_substeps", "get_max_substeps");
	ClassDB::bind_method(D_METHOD("set_solver_iterations", "value"), &Rope3D::set_solver_iterations);
	ClassDB::bind_method(D_METHOD("get_solver_iterations"), &Rope3D::get_solver_iterations);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "solver_iterations", PROPERTY_HINT_RANGE, "1,64,1"), "set_solver_iterations", "get_solver_iterations");
	ClassDB::bind_method(D_METHOD("set_render_enabled", "value"), &Rope3D::set_render_enabled);
	ClassDB::bind_method(D_METHOD("get_render_enabled"), &Rope3D::get_render_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "render_enabled"), "set_render_enabled", "get_render_enabled");
	ClassDB::bind_method(D_METHOD("set_radius", "value"), &Rope3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &Rope3D::get_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.00001,10,0.001,or_greater"), "set_radius", "get_radius");
	ClassDB::bind_method(D_METHOD("set_radial_segments", "value"), &Rope3D::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &Rope3D::get_radial_segments);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "3,64,1"), "set_radial_segments", "get_radial_segments");
	ClassDB::bind_method(D_METHOD("set_cap_mode", "value"), &Rope3D::set_cap_mode);
	ClassDB::bind_method(D_METHOD("get_cap_mode"), &Rope3D::get_cap_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cap_mode", PROPERTY_HINT_ENUM, "None,Flat,Round"), "set_cap_mode", "get_cap_mode");
	ClassDB::bind_method(D_METHOD("set_uv_repeat_length", "value"), &Rope3D::set_uv_repeat_length);
	ClassDB::bind_method(D_METHOD("get_uv_repeat_length"), &Rope3D::get_uv_repeat_length);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "uv_repeat_length", PROPERTY_HINT_RANGE, "0.00001,100,0.01,or_greater"), "set_uv_repeat_length", "get_uv_repeat_length");
	ClassDB::bind_method(D_METHOD("set_collision_enabled", "value"), &Rope3D::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("get_collision_enabled"), &Rope3D::get_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "get_collision_enabled");
	ClassDB::bind_method(D_METHOD("set_collision_radius", "value"), &Rope3D::set_collision_radius);
	ClassDB::bind_method(D_METHOD("get_collision_radius"), &Rope3D::get_collision_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_radius", PROPERTY_HINT_RANGE, "0.00001,10,0.001,or_greater"), "set_collision_radius", "get_collision_radius");
	ClassDB::bind_method(D_METHOD("set_collision_mask", "value"), &Rope3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &Rope3D::get_collision_mask);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ClassDB::bind_method(D_METHOD("set_collision_margin", "value"), &Rope3D::set_collision_margin);
	ClassDB::bind_method(D_METHOD("get_collision_margin"), &Rope3D::get_collision_margin);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_margin", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), "set_collision_margin", "get_collision_margin");
	ClassDB::bind_method(D_METHOD("set_friction", "value"), &Rope3D::set_friction);
	ClassDB::bind_method(D_METHOD("get_friction"), &Rope3D::get_friction);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_friction", "get_friction");
	ClassDB::bind_method(D_METHOD("set_initial_curve", "curve"), &Rope3D::set_initial_curve);
	ClassDB::bind_method(D_METHOD("get_initial_curve"), &Rope3D::get_initial_curve);
	ClassDB::bind_method(D_METHOD("attach_particle", "index", "node_path", "local_offset", "compliance"), &Rope3D::attach_particle, DEFVAL(Vector3()), DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("set_particle_target", "index", "world_position", "compliance"), &Rope3D::set_particle_target, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("clear_attachment", "index"), &Rope3D::clear_attachment);
	ClassDB::bind_method(D_METHOD("set_pose_targets", "world_points", "compliance"), &Rope3D::set_pose_targets);
	ClassDB::bind_method(D_METHOD("clear_pose_targets"), &Rope3D::clear_pose_targets);
	ClassDB::bind_method(D_METHOD("reset_simulation"), &Rope3D::reset_simulation);
	ClassDB::bind_method(D_METHOD("reset_to_points", "world_points"), &Rope3D::reset_to_points);
	ClassDB::bind_method(D_METHOD("get_simulated_points"), &Rope3D::get_simulated_points);
	ClassDB::bind_method(D_METHOD("get_point_position", "index"), &Rope3D::get_point_position);
	ClassDB::bind_method(D_METHOD("apply_impulse", "index", "world_impulse"), &Rope3D::apply_impulse);
	ClassDB::bind_method(D_METHOD("set_render_points", "local_points"), &Rope3D::set_render_points);
	ClassDB::bind_method(D_METHOD("get_render_points"), &Rope3D::get_render_points);
	ClassDB::bind_method(D_METHOD("advance_simulation", "delta"), &Rope3D::advance_simulation);
	ClassDB::bind_method(D_METHOD("add_collision_exception", "rid"), &Rope3D::add_collision_exception);
	ClassDB::bind_method(D_METHOD("remove_collision_exception", "rid"), &Rope3D::remove_collision_exception);
	ClassDB::bind_method(D_METHOD("clear_collision_exceptions"), &Rope3D::clear_collision_exceptions);
	ClassDB::bind_method(D_METHOD("get_max_segment_error"), &Rope3D::get_max_segment_error);
	ClassDB::bind_method(D_METHOD("is_overstretched"), &Rope3D::is_overstretched);
	ClassDB::bind_method(D_METHOD("has_hard_pin_contact"), &Rope3D::has_hard_pin_contact);
	ClassDB::bind_method(D_METHOD("get_dropped_simulation_time"), &Rope3D::get_dropped_simulation_time);
	ClassDB::bind_method(D_METHOD("get_mesh"), &Rope3D::get_mesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "initial_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve3D"), "set_initial_curve", "get_initial_curve");
	BIND_ENUM_CONSTANT(SIMULATION_PROCESS_AUTO);
	BIND_ENUM_CONSTANT(SIMULATION_PROCESS_MANUAL);
	BIND_ENUM_CONSTANT(CAP_NONE);
	BIND_ENUM_CONSTANT(CAP_FLAT);
	BIND_ENUM_CONSTANT(CAP_ROUND);
}
