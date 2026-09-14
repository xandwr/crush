/**************************************************************************/
/*  character_motor_3d.cpp                                                          */
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

#include "character_motor_3d.h"

#include "core/object/class_db.h"

void CharacterMotor3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_settings", "settings"), &CharacterMotor3D::set_settings);
	ClassDB::bind_method(D_METHOD("get_settings"), &CharacterMotor3D::get_settings);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "settings", PROPERTY_HINT_RESOURCE_TYPE, "MovementSettings"), "set_settings", "get_settings");
	ClassDB::bind_method(D_METHOD("set_clearance_test", "test"), &CharacterMotor3D::set_clearance_test);
	ClassDB::bind_method(D_METHOD("get_clearance_test"), &CharacterMotor3D::get_clearance_test);
	ClassDB::bind_method(D_METHOD("is_crouching"), &CharacterMotor3D::is_crouching);
	ClassDB::bind_method(D_METHOD("get_stance_height"), &CharacterMotor3D::get_stance_height);
	ClassDB::bind_method(D_METHOD("has_jumped"), &CharacterMotor3D::has_jumped);
	ClassDB::bind_method(D_METHOD("has_landed"), &CharacterMotor3D::has_landed);
	ClassDB::bind_method(D_METHOD("get_displacement"), &CharacterMotor3D::get_displacement);
	ClassDB::bind_method(D_METHOD("get_step_height"), &CharacterMotor3D::get_step_height);
	ClassDB::bind_method(D_METHOD("reset_stance", "crouching"), &CharacterMotor3D::reset_stance, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("step", "delta", "input", "view_basis", "jump", "crouch", "sprint", "slow"), &CharacterMotor3D::step, DEFVAL(false), DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("integrate_gravity", "velocity", "up", "delta"), &CharacterMotor3D::integrate_gravity);
	ClassDB::bind_method(D_METHOD("apply_jump", "velocity", "up"), &CharacterMotor3D::apply_jump);
	ClassDB::bind_method(D_METHOD("integrate_lateral_movement", "velocity", "input", "view_basis", "up", "grounded", "delta", "speed_multiplier", "sprinting", "slow_walking", "floor_normal"), &CharacterMotor3D::integrate_lateral_movement, DEFVAL(1.0), DEFVAL(false), DEFVAL(false), DEFVAL(Vector3()));
	ADD_SIGNAL(MethodInfo("stance_changed", PropertyInfo(Variant::FLOAT, "height")));
}

real_t CharacterMotor3D::get_stance_height() const {
	return settings.is_valid() ? (crouching ? settings->get_crouching_height() : settings->get_standing_height()) : 0.0;
}

PackedStringArray CharacterMotor3D::get_configuration_warnings() const {
	PackedStringArray warnings = CharacterBody3D::get_configuration_warnings();
	if (settings.is_null()) {
		warnings.push_back("Assign MovementSettings before stepping the motor.");
	} else {
		warnings.append_array(settings->validation_errors());
	}
	return warnings;
}

bool CharacterMotor3D::can_occupy(real_t p_height, const Transform3D &p_transform) const {
	if (!clearance_test.is_valid()) {
		return false;
	}
	Variant height = p_height;
	Variant transform = p_transform;
	const Variant *args[] = { &height, &transform };
	Variant result;
	Callable::CallError error;
	clearance_test.callp(args, 2, result, error);
	return error.error == Callable::CallError::CALL_OK && result.get_type() == Variant::BOOL && bool(result);
}

void CharacterMotor3D::reset_stance(bool p_crouching) {
	crouching = p_crouching;
	tucked = false;
	jumped = false;
	landed = false;
	displacement = Vector3();
	last_step_height = 0.0;
	emit_signal("stance_changed", get_stance_height());
}

bool CharacterMotor3D::change_stance(bool p_crouching) {
	if (crouching == p_crouching) {
		return true;
	}
	Transform3D target = get_global_transform();
	Vector3 up = get_up_direction();
	real_t height = p_crouching ? settings->get_crouching_height() : settings->get_standing_height();
	if (!p_crouching && tucked && !is_on_floor()) {
		target.origin -= up * (settings->get_standing_height() - settings->get_crouching_height());
	}
	if (!clearance_test.is_valid() || (!p_crouching && !can_occupy(height, target))) {
		return false;
	}
	crouching = p_crouching;
	tucked = false;
	set_global_transform(target);
	emit_signal("stance_changed", height);
	return true;
}

bool CharacterMotor3D::sweep(const Transform3D &p_from, const Vector3 &p_motion, PhysicsServer3D::MotionResult &r_result) const {
	PhysicsServer3D::MotionParameters parameters(p_from, p_motion, get_safe_margin());
	parameters.max_collisions = 6;
	return PhysicsServer3D::get_singleton()->body_test_motion(get_rid(), parameters, &r_result);
}

Transform3D CharacterMotor3D::slide_trial(const Transform3D &p_from, Vector3 p_motion) const {
	Transform3D target = p_from;
	Vector3 original = p_motion;
	for (int i = 0; i < get_max_slides() && !p_motion.is_zero_approx(); i++) {
		PhysicsServer3D::MotionResult result;
		bool collided = sweep(target, p_motion, result);
		target.origin += result.travel;
		if (!collided) {
			break;
		}
		p_motion = result.remainder;
		for (int j = 0; j < result.collision_count; j++) {
			if (p_motion.dot(result.collisions[j].normal) < 0) {
				p_motion = p_motion.slide(result.collisions[j].normal);
			}
		}
		if (p_motion.dot(original) <= 0) {
			break;
		}
	}
	return target;
}

bool CharacterMotor3D::try_step(const Transform3D &p_from, const Vector3 &p_motion, Transform3D &r_target) const {
	Vector3 up = get_up_direction();
	Vector3 lateral = p_motion.slide(up);
	if (settings->get_step_height() <= 0.0 || lateral.is_zero_approx()) {
		return false;
	}
	PhysicsServer3D::MotionResult obstacle;
	if (!sweep(p_from, lateral, obstacle)) {
		return false;
	}
	bool wall = false;
	for (int i = 0; i < obstacle.collision_count; i++) {
		if (obstacle.collisions[i].normal.dot(up) < Math::cos(get_floor_max_angle())) {
			wall = true;
		}
	}
	if (!wall) {
		return false;
	}
	Transform3D raised = p_from;
	PhysicsServer3D::MotionResult lift;
	sweep(raised, up * settings->get_step_height(), lift);
	if (lift.travel.dot(up) <= get_safe_margin()) {
		return false;
	}
	raised.origin += lift.travel;
	Transform3D elevated = slide_trial(raised, lateral);
	PhysicsServer3D::MotionResult down;
	if (!sweep(elevated, -up * (settings->get_step_height() + get_safe_margin() * 2), down)) {
		return false;
	}
	bool floor = false;
	for (int i = 0; i < down.collision_count; i++) {
		if (down.collisions[i].normal.dot(up) >= Math::cos(get_floor_max_angle())) {
			floor = true;
		}
	}
	if (!floor) {
		return false;
	}
	elevated.origin += down.travel;
	real_t rise = (elevated.origin - p_from.origin).dot(up);
	if (rise <= get_safe_margin() || rise > settings->get_step_height() + get_safe_margin()) {
		return false;
	}
	Transform3D ordinary = slide_trial(p_from, p_motion);
	if ((elevated.origin - p_from.origin).slide(up).length_squared() <= (ordinary.origin - p_from.origin).slide(up).length_squared() + CMP_EPSILON) {
		return false;
	}
	r_target = elevated;
	return true;
}

Vector3 CharacterMotor3D::integrate_gravity(const Vector3 &p_velocity, const Vector3 &p_up, double p_delta) const {
	ERR_FAIL_COND_V(settings.is_null() || !p_up.is_finite() || p_up.is_zero_approx() || !Math::is_finite(p_delta) || p_delta < 0, p_velocity);
	return p_velocity - p_up.normalized() * settings->get_gravity_acceleration() * p_delta;
}

Vector3 CharacterMotor3D::apply_jump(const Vector3 &p_velocity, const Vector3 &p_up) const {
	ERR_FAIL_COND_V(settings.is_null() || !p_up.is_finite() || p_up.is_zero_approx(), p_velocity);
	return p_velocity.slide(p_up.normalized()) + p_up.normalized() * settings->get_jump_impulse();
}

Vector3 CharacterMotor3D::integrate_lateral_movement(const Vector3 &p_velocity, const Vector2 &p_input, const Basis &p_basis, const Vector3 &p_up, bool p_grounded, double p_delta, double p_multiplier, bool p_sprint, bool p_slow, const Vector3 &p_floor_normal) const {
	ERR_FAIL_COND_V(settings.is_null() || !p_up.is_finite() || p_up.is_zero_approx() || !Math::is_finite(p_delta) || p_delta < 0, p_velocity);
	Vector3 up = p_up.normalized();
	Vector3 lateral = p_velocity.slide(up);
	Vector3 vertical = up * p_velocity.dot(up);
	Vector3 normal = p_grounded ? p_floor_normal.normalized() : Vector3();
	if (!normal.is_zero_approx()) {
		vertical = Vector3();
	}
	if (p_grounded) {
		real_t speed = lateral.length();
		if (speed > 0) {
			real_t drop = MAX(speed, settings->get_ground_stop_speed()) * settings->get_ground_friction_rate() * p_delta;
			lateral *= MAX(speed - drop, real_t(0)) / speed;
		}
	}
	Vector2 input = p_input.limit_length();
	Vector3 wish = p_basis.get_column(0).slide(up).normalized() * input.x + p_basis.get_column(2).slide(up).normalized() * input.y;
	if (!wish.is_zero_approx()) {
		wish.normalize();
		real_t speed = p_slow ? settings->get_base_wish_speed() * settings->get_slow_walk_speed_multiplier() : (p_sprint ? settings->get_sprint_wish_speed() : settings->get_base_wish_speed());
		speed *= p_multiplier * input.length();
		real_t target = p_grounded ? speed : MIN(speed, settings->get_air_wish_speed_cap());
		real_t add = target - lateral.dot(wish);
		if (add > 0) {
			real_t rate = p_grounded ? settings->get_ground_acceleration_rate() : settings->get_air_acceleration_rate();
			lateral += wish * MIN(add, real_t(rate * speed * p_delta));
		}
	}
	if (!normal.is_zero_approx() && normal.dot(up) > CMP_EPSILON) {
		lateral -= up * lateral.dot(normal) / normal.dot(up);
	}
	return lateral + vertical;
}

bool CharacterMotor3D::step(double p_delta, const Vector2 &p_input, const Basis &p_view_basis, bool p_jump, bool p_crouch, bool p_sprint, bool p_slow) {
	if (!is_inside_tree() || settings.is_null() || !settings->validation_errors().is_empty() || !Math::is_finite(p_delta) || p_delta <= 0 || !p_input.is_finite() || !p_view_basis.is_finite() || !get_velocity().is_finite() || !get_global_transform().is_finite()) {
		return false;
	}
	Vector3 up = get_up_direction();
	bool grounded = is_on_floor();
	if (grounded) {
		tucked = false;
	}
	Transform3D initial = get_global_transform();
	change_stance(p_crouch);
	Transform3D start = get_global_transform();
	jumped = p_jump && grounded;
	landed = false;
	last_step_height = 0;
	Vector3 motion_velocity = integrate_lateral_movement(get_velocity(), p_input, p_view_basis, up, grounded, p_delta, crouching && grounded ? settings->get_crouch_speed_multiplier() : 1.0, p_sprint && !crouching, p_slow && !crouching, grounded ? get_floor_normal() : Vector3());
	if (!grounded) {
		motion_velocity = integrate_gravity(motion_velocity, up, p_delta);
	}
	if (jumped) {
		motion_velocity = apply_jump(motion_velocity, up);
	}
	set_floor_snap_length(settings->get_ground_snap_distance());
	Transform3D step_target;
	if (grounded && !jumped && try_step(start, motion_velocity * p_delta, step_target)) {
		last_step_height = (step_target.origin - start.origin).dot(up);
		set_global_transform(step_target);
		set_velocity(Vector3());
		move_and_slide_with_delta(p_delta);
		apply_floor_snap();
		set_velocity(motion_velocity.slide(up));
	} else {
		set_velocity(motion_velocity);
		move_and_slide_with_delta(p_delta);
		if (grounded && !jumped) {
			apply_floor_snap();
		}
	}
	landed = !grounded && is_on_floor();
	if (crouching && !is_on_floor() && !tucked) {
		Transform3D target = get_global_transform();
		Vector3 tuck = up * (settings->get_standing_height() - settings->get_crouching_height());
		PhysicsServer3D::MotionResult result;
		bool blocked = sweep(target, tuck, result);
		target.origin += tuck;
		if (!blocked && can_occupy(get_stance_height(), target)) {
			set_global_transform(target);
			tucked = true;
		}
	}
	change_stance(p_crouch);
	displacement = get_global_position() - initial.origin;
	update_motion_origin(initial.origin, p_delta);
	return true;
}
