/**************************************************************************/
/*  movement_settings.cpp                                                          */
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

#include "movement_settings.h"

#include "core/object/class_db.h"

void MovementSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_gravity_acceleration", "value"), &MovementSettings::set_gravity_acceleration);
	ClassDB::bind_method(D_METHOD("get_gravity_acceleration"), &MovementSettings::get_gravity_acceleration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gravity_acceleration", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_gravity_acceleration", "get_gravity_acceleration");
	ClassDB::bind_method(D_METHOD("set_jump_impulse", "value"), &MovementSettings::set_jump_impulse);
	ClassDB::bind_method(D_METHOD("get_jump_impulse"), &MovementSettings::get_jump_impulse);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_impulse", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_jump_impulse", "get_jump_impulse");
	ClassDB::bind_method(D_METHOD("set_base_wish_speed", "value"), &MovementSettings::set_base_wish_speed);
	ClassDB::bind_method(D_METHOD("get_base_wish_speed"), &MovementSettings::get_base_wish_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "base_wish_speed", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_base_wish_speed", "get_base_wish_speed");
	ClassDB::bind_method(D_METHOD("set_sprint_wish_speed", "value"), &MovementSettings::set_sprint_wish_speed);
	ClassDB::bind_method(D_METHOD("get_sprint_wish_speed"), &MovementSettings::get_sprint_wish_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sprint_wish_speed", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_sprint_wish_speed", "get_sprint_wish_speed");
	ClassDB::bind_method(D_METHOD("set_slow_walk_speed_multiplier", "value"), &MovementSettings::set_slow_walk_speed_multiplier);
	ClassDB::bind_method(D_METHOD("get_slow_walk_speed_multiplier"), &MovementSettings::get_slow_walk_speed_multiplier);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slow_walk_speed_multiplier", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_slow_walk_speed_multiplier", "get_slow_walk_speed_multiplier");
	ClassDB::bind_method(D_METHOD("set_ground_acceleration_rate", "value"), &MovementSettings::set_ground_acceleration_rate);
	ClassDB::bind_method(D_METHOD("get_ground_acceleration_rate"), &MovementSettings::get_ground_acceleration_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ground_acceleration_rate", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_ground_acceleration_rate", "get_ground_acceleration_rate");
	ClassDB::bind_method(D_METHOD("set_ground_friction_rate", "value"), &MovementSettings::set_ground_friction_rate);
	ClassDB::bind_method(D_METHOD("get_ground_friction_rate"), &MovementSettings::get_ground_friction_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ground_friction_rate", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_ground_friction_rate", "get_ground_friction_rate");
	ClassDB::bind_method(D_METHOD("set_ground_stop_speed", "value"), &MovementSettings::set_ground_stop_speed);
	ClassDB::bind_method(D_METHOD("get_ground_stop_speed"), &MovementSettings::get_ground_stop_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ground_stop_speed", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_ground_stop_speed", "get_ground_stop_speed");
	ClassDB::bind_method(D_METHOD("set_air_acceleration_rate", "value"), &MovementSettings::set_air_acceleration_rate);
	ClassDB::bind_method(D_METHOD("get_air_acceleration_rate"), &MovementSettings::get_air_acceleration_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "air_acceleration_rate", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_air_acceleration_rate", "get_air_acceleration_rate");
	ClassDB::bind_method(D_METHOD("set_air_wish_speed_cap", "value"), &MovementSettings::set_air_wish_speed_cap);
	ClassDB::bind_method(D_METHOD("get_air_wish_speed_cap"), &MovementSettings::get_air_wish_speed_cap);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "air_wish_speed_cap", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_air_wish_speed_cap", "get_air_wish_speed_cap");
	ClassDB::bind_method(D_METHOD("set_standing_height", "value"), &MovementSettings::set_standing_height);
	ClassDB::bind_method(D_METHOD("get_standing_height"), &MovementSettings::get_standing_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "standing_height", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_standing_height", "get_standing_height");
	ClassDB::bind_method(D_METHOD("set_crouching_height", "value"), &MovementSettings::set_crouching_height);
	ClassDB::bind_method(D_METHOD("get_crouching_height"), &MovementSettings::get_crouching_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "crouching_height", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_crouching_height", "get_crouching_height");
	ClassDB::bind_method(D_METHOD("set_crouch_speed_multiplier", "value"), &MovementSettings::set_crouch_speed_multiplier);
	ClassDB::bind_method(D_METHOD("get_crouch_speed_multiplier"), &MovementSettings::get_crouch_speed_multiplier);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "crouch_speed_multiplier", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_crouch_speed_multiplier", "get_crouch_speed_multiplier");
	ClassDB::bind_method(D_METHOD("set_step_height", "value"), &MovementSettings::set_step_height);
	ClassDB::bind_method(D_METHOD("get_step_height"), &MovementSettings::get_step_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "step_height", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_step_height", "get_step_height");
	ClassDB::bind_method(D_METHOD("set_ground_snap_distance", "value"), &MovementSettings::set_ground_snap_distance);
	ClassDB::bind_method(D_METHOD("get_ground_snap_distance"), &MovementSettings::get_ground_snap_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ground_snap_distance", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_ground_snap_distance", "get_ground_snap_distance");
	ClassDB::bind_method(D_METHOD("validation_errors"), &MovementSettings::validation_errors);
}

PackedStringArray MovementSettings::validation_errors() const {
	PackedStringArray errors;
	if (!Math::is_finite(gravity_acceleration) || gravity_acceleration < 0.0) {
		errors.push_back("gravity_acceleration must be finite and nonnegative.");
	}
	if (!Math::is_finite(jump_impulse) || jump_impulse < 0.0) {
		errors.push_back("jump_impulse must be finite and nonnegative.");
	}
	if (!Math::is_finite(base_wish_speed) || base_wish_speed < 0.0) {
		errors.push_back("base_wish_speed must be finite and nonnegative.");
	}
	if (!Math::is_finite(sprint_wish_speed) || sprint_wish_speed < 0.0) {
		errors.push_back("sprint_wish_speed must be finite and nonnegative.");
	}
	if (!Math::is_finite(slow_walk_speed_multiplier) || slow_walk_speed_multiplier < 0.0) {
		errors.push_back("slow_walk_speed_multiplier must be finite and nonnegative.");
	}
	if (!Math::is_finite(ground_acceleration_rate) || ground_acceleration_rate < 0.0) {
		errors.push_back("ground_acceleration_rate must be finite and nonnegative.");
	}
	if (!Math::is_finite(ground_friction_rate) || ground_friction_rate < 0.0) {
		errors.push_back("ground_friction_rate must be finite and nonnegative.");
	}
	if (!Math::is_finite(ground_stop_speed) || ground_stop_speed < 0.0) {
		errors.push_back("ground_stop_speed must be finite and nonnegative.");
	}
	if (!Math::is_finite(air_acceleration_rate) || air_acceleration_rate < 0.0) {
		errors.push_back("air_acceleration_rate must be finite and nonnegative.");
	}
	if (!Math::is_finite(air_wish_speed_cap) || air_wish_speed_cap < 0.0) {
		errors.push_back("air_wish_speed_cap must be finite and nonnegative.");
	}
	if (!Math::is_finite(standing_height) || standing_height < 0.0) {
		errors.push_back("standing_height must be finite and nonnegative.");
	}
	if (!Math::is_finite(crouching_height) || crouching_height < 0.0) {
		errors.push_back("crouching_height must be finite and nonnegative.");
	}
	if (!Math::is_finite(crouch_speed_multiplier) || crouch_speed_multiplier < 0.0) {
		errors.push_back("crouch_speed_multiplier must be finite and nonnegative.");
	}
	if (!Math::is_finite(step_height) || step_height < 0.0) {
		errors.push_back("step_height must be finite and nonnegative.");
	}
	if (!Math::is_finite(ground_snap_distance) || ground_snap_distance < 0.0) {
		errors.push_back("ground_snap_distance must be finite and nonnegative.");
	}
	if (standing_height <= 0.0 || crouching_height <= 0.0 || crouching_height > standing_height) {
		errors.push_back("Heights must be positive and crouching_height cannot exceed standing_height.");
	}
	if (slow_walk_speed_multiplier > 1.0 || crouch_speed_multiplier > 1.0) {
		errors.push_back("Speed multipliers cannot exceed one.");
	}
	return errors;
}
