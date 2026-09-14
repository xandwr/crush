/**************************************************************************/
/*  movement_settings.h                                                          */
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

#include "core/io/resource.h"

class MovementSettings : public Resource {
	GDCLASS(MovementSettings, Resource);
	double gravity_acceleration = 24.0;
	double jump_impulse = 6.5;
	double base_wish_speed = 3.6;
	double sprint_wish_speed = 6.0;
	double slow_walk_speed_multiplier = 0.5;
	double ground_acceleration_rate = 16.0;
	double ground_friction_rate = 7.5;
	double ground_stop_speed = 1.1;
	double air_acceleration_rate = 12.0;
	double air_wish_speed_cap = 0.6;
	double standing_height = 1.8;
	double crouching_height = 1.15;
	double crouch_speed_multiplier = 0.5;
	double step_height = 0.3;
	double ground_snap_distance = 0.3;

protected:
	static void _bind_methods();

public:
	void set_gravity_acceleration(double p_value) {
		gravity_acceleration = p_value;
		emit_changed();
	}
	double get_gravity_acceleration() const { return gravity_acceleration; }
	void set_jump_impulse(double p_value) {
		jump_impulse = p_value;
		emit_changed();
	}
	double get_jump_impulse() const { return jump_impulse; }
	void set_base_wish_speed(double p_value) {
		base_wish_speed = p_value;
		emit_changed();
	}
	double get_base_wish_speed() const { return base_wish_speed; }
	void set_sprint_wish_speed(double p_value) {
		sprint_wish_speed = p_value;
		emit_changed();
	}
	double get_sprint_wish_speed() const { return sprint_wish_speed; }
	void set_slow_walk_speed_multiplier(double p_value) {
		slow_walk_speed_multiplier = p_value;
		emit_changed();
	}
	double get_slow_walk_speed_multiplier() const { return slow_walk_speed_multiplier; }
	void set_ground_acceleration_rate(double p_value) {
		ground_acceleration_rate = p_value;
		emit_changed();
	}
	double get_ground_acceleration_rate() const { return ground_acceleration_rate; }
	void set_ground_friction_rate(double p_value) {
		ground_friction_rate = p_value;
		emit_changed();
	}
	double get_ground_friction_rate() const { return ground_friction_rate; }
	void set_ground_stop_speed(double p_value) {
		ground_stop_speed = p_value;
		emit_changed();
	}
	double get_ground_stop_speed() const { return ground_stop_speed; }
	void set_air_acceleration_rate(double p_value) {
		air_acceleration_rate = p_value;
		emit_changed();
	}
	double get_air_acceleration_rate() const { return air_acceleration_rate; }
	void set_air_wish_speed_cap(double p_value) {
		air_wish_speed_cap = p_value;
		emit_changed();
	}
	double get_air_wish_speed_cap() const { return air_wish_speed_cap; }
	void set_standing_height(double p_value) {
		standing_height = p_value;
		emit_changed();
	}
	double get_standing_height() const { return standing_height; }
	void set_crouching_height(double p_value) {
		crouching_height = p_value;
		emit_changed();
	}
	double get_crouching_height() const { return crouching_height; }
	void set_crouch_speed_multiplier(double p_value) {
		crouch_speed_multiplier = p_value;
		emit_changed();
	}
	double get_crouch_speed_multiplier() const { return crouch_speed_multiplier; }
	void set_step_height(double p_value) {
		step_height = p_value;
		emit_changed();
	}
	double get_step_height() const { return step_height; }
	void set_ground_snap_distance(double p_value) {
		ground_snap_distance = p_value;
		emit_changed();
	}
	double get_ground_snap_distance() const { return ground_snap_distance; }
	PackedStringArray validation_errors() const;
};
