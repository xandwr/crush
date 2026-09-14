/**************************************************************************/
/*  character_motor_3d.h                                                          */
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

#include "movement_settings.h"

#include "scene/3d/physics/character_body_3d.h"

class CharacterMotor3D : public CharacterBody3D {
	GDCLASS(CharacterMotor3D, CharacterBody3D);
	Ref<MovementSettings> settings;
	Callable clearance_test;
	bool crouching = false;
	bool tucked = false;
	bool jumped = false;
	bool landed = false;
	Vector3 displacement;
	real_t last_step_height = 0.0;
	bool change_stance(bool p_crouching);
	bool can_occupy(real_t p_height, const Transform3D &p_transform) const;
	bool sweep(const Transform3D &p_from, const Vector3 &p_motion, PhysicsServer3D::MotionResult &r_result) const;
	Transform3D slide_trial(const Transform3D &p_from, Vector3 p_motion) const;
	bool try_step(const Transform3D &p_from, const Vector3 &p_motion, Transform3D &r_target) const;

protected:
	static void _bind_methods();

public:
	PackedStringArray get_configuration_warnings() const override;
	void set_settings(const Ref<MovementSettings> &p_settings) {
		settings = p_settings;
		update_configuration_warnings();
	}
	Ref<MovementSettings> get_settings() const { return settings; }
	void set_clearance_test(const Callable &p_test) { clearance_test = p_test; }
	Callable get_clearance_test() const { return clearance_test; }
	bool is_crouching() const { return crouching; }
	real_t get_stance_height() const;
	bool has_jumped() const { return jumped; }
	bool has_landed() const { return landed; }
	Vector3 get_displacement() const { return displacement; }
	real_t get_step_height() const { return last_step_height; }
	void reset_stance(bool p_crouching = false);
	bool step(double p_delta, const Vector2 &p_input, const Basis &p_view_basis, bool p_jump = false, bool p_crouch = false, bool p_sprint = false, bool p_slow = false);
	Vector3 integrate_gravity(const Vector3 &p_velocity, const Vector3 &p_up, double p_delta) const;
	Vector3 apply_jump(const Vector3 &p_velocity, const Vector3 &p_up) const;
	Vector3 integrate_lateral_movement(const Vector3 &p_velocity, const Vector2 &p_input, const Basis &p_basis, const Vector3 &p_up, bool p_grounded, double p_delta, double p_multiplier = 1.0, bool p_sprint = false, bool p_slow = false, const Vector3 &p_floor_normal = Vector3()) const;
};
