/**************************************************************************/
/*  weapon_simulation.cpp                                                 */
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

#include "weapon_simulation.h"

#include "core/object/class_db.h"

void WeaponSimulationSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_fire_mode", "value"), &WeaponSimulationSettings::set_fire_mode);
	ClassDB::bind_method(D_METHOD("get_fire_mode"), &WeaponSimulationSettings::get_fire_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fire_mode", PROPERTY_HINT_ENUM, "Semi Automatic,Automatic"), "set_fire_mode", "get_fire_mode");
	ClassDB::bind_method(D_METHOD("set_magazine_capacity", "value"), &WeaponSimulationSettings::set_magazine_capacity);
	ClassDB::bind_method(D_METHOD("get_magazine_capacity"), &WeaponSimulationSettings::get_magazine_capacity);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "magazine_capacity", PROPERTY_HINT_RANGE, "1,1000000,1"), "set_magazine_capacity", "get_magazine_capacity");
	ClassDB::bind_method(D_METHOD("set_shot_interval", "value"), &WeaponSimulationSettings::set_shot_interval);
	ClassDB::bind_method(D_METHOD("get_shot_interval"), &WeaponSimulationSettings::get_shot_interval);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shot_interval", PROPERTY_HINT_RANGE, "0.001,3600,0.001,suffix:s"), "set_shot_interval", "get_shot_interval");
	ClassDB::bind_method(D_METHOD("set_reload_duration", "value"), &WeaponSimulationSettings::set_reload_duration);
	ClassDB::bind_method(D_METHOD("get_reload_duration"), &WeaponSimulationSettings::get_reload_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reload_duration", PROPERTY_HINT_RANGE, "0.001,3600,0.001,suffix:s"), "set_reload_duration", "get_reload_duration");
	ClassDB::bind_method(D_METHOD("set_equip_delay", "value"), &WeaponSimulationSettings::set_equip_delay);
	ClassDB::bind_method(D_METHOD("get_equip_delay"), &WeaponSimulationSettings::get_equip_delay);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "equip_delay", PROPERTY_HINT_RANGE, "0,3600,0.001,suffix:s"), "set_equip_delay", "get_equip_delay");

	ClassDB::bind_method(D_METHOD("validation_errors"), &WeaponSimulationSettings::validation_errors);
	BIND_ENUM_CONSTANT(SEMI_AUTOMATIC);
	BIND_ENUM_CONSTANT(AUTOMATIC);
}

PackedStringArray WeaponSimulationSettings::validation_errors() const {
	PackedStringArray errors;
	if (fire_mode != SEMI_AUTOMATIC && fire_mode != AUTOMATIC) {
		errors.push_back("Invalid firearm fire mode");
	}
	if (magazine_capacity < 1 || magazine_capacity > 1000000) {
		errors.push_back("Magazine capacity must be between 1 and 1000000");
	}
	if (!Math::is_finite(shot_interval) || shot_interval <= 0 || shot_interval > 3600) {
		errors.push_back("Shot interval must be finite, positive and at most 3600 seconds");
	}
	if (!Math::is_finite(reload_duration) || reload_duration <= 0 || reload_duration > 3600) {
		errors.push_back("Reload duration must be finite, positive and at most 3600 seconds");
	}
	if (!Math::is_finite(equip_delay) || equip_delay < 0 || equip_delay > 3600) {
		errors.push_back("Equip delay must be finite, nonnegative and at most 3600 seconds");
	}
	return errors;
}

void WeaponSimulation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "settings", "tick_rate", "magazine", "reserve"), &WeaponSimulation::configure, DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("reset", "magazine", "reserve"), &WeaponSimulation::reset, DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_equipped", "equipped"), &WeaponSimulation::set_equipped);
	ClassDB::bind_method(D_METHOD("is_equipped"), &WeaponSimulation::is_equipped);
	ClassDB::bind_method(D_METHOD("step", "tick", "trigger_held", "reload_requested"), &WeaponSimulation::step, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_magazine"), &WeaponSimulation::get_magazine);
	ClassDB::bind_method(D_METHOD("get_reserve"), &WeaponSimulation::get_reserve);
	ClassDB::bind_method(D_METHOD("get_last_tick"), &WeaponSimulation::get_last_tick);
	ClassDB::bind_method(D_METHOD("is_reloading"), &WeaponSimulation::is_reloading);
	ClassDB::bind_method(D_METHOD("get_snapshot"), &WeaponSimulation::get_snapshot);
	ClassDB::bind_method(D_METHOD("restore_snapshot", "snapshot"), &WeaponSimulation::restore_snapshot);
	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(SHOT);
	BIND_ENUM_CONSTANT(DRY_FIRE);
	BIND_ENUM_CONSTANT(RELOAD_STARTED);
	BIND_ENUM_CONSTANT(RELOAD_COMPLETED);
}

bool WeaponSimulation::configure(const Ref<WeaponSimulationSettings> &p_settings, int p_tick_rate, int p_magazine, int p_reserve) {
	if (p_settings.is_null() || !p_settings->validation_errors().is_empty() || p_tick_rate < 1 || p_tick_rate > 1000 || p_magazine < -1 || p_magazine > p_settings->get_magazine_capacity() || p_reserve < 0 || p_reserve > 1000000) {
		return false;
	}
	mode = p_settings->get_fire_mode();
	capacity = p_settings->get_magazine_capacity();
	tick_rate = p_tick_rate;
	shot_ticks = MAX(1, (int)Math::ceil(p_settings->get_shot_interval() * tick_rate));
	reload_ticks = MAX(1, (int)Math::ceil(p_settings->get_reload_duration() * tick_rate));
	equip_ticks = (int)Math::ceil(p_settings->get_equip_delay() * tick_rate);
	configured = true;
	return reset(p_magazine, p_reserve);
}

bool WeaponSimulation::reset(int p_magazine, int p_reserve) {
	if (!configured || p_magazine < -1 || p_magazine > capacity || p_reserve < 0 || p_reserve > 1000000) {
		return false;
	}
	magazine = p_magazine == -1 ? capacity : p_magazine;
	reserve = p_reserve;
	cooldown = reload_remaining = equip_remaining = 0;
	last_tick = -1;
	equipped = trigger_held = false;
	return true;
}

void WeaponSimulation::set_equipped(bool p_equipped) {
	if (!configured || equipped == p_equipped) {
		return;
	}
	equipped = p_equipped;
	reload_remaining = 0;
	equip_remaining = equipped ? equip_ticks : 0;
}

int WeaponSimulation::step(int64_t p_tick, bool p_trigger_held, bool p_reload_requested) {
	if (!configured || last_tick == INT64_MAX || p_tick < 0 || p_tick == INT64_MAX || p_tick != last_tick + 1) {
		return -1;
	}
	last_tick = p_tick;
	const bool pressed = p_trigger_held && !trigger_held;
	trigger_held = p_trigger_held;
	const bool equipping = equip_remaining > 0;
	cooldown = MAX(0, cooldown - 1);
	equip_remaining = MAX(0, equip_remaining - 1);
	int events = NONE;
	if (reload_remaining > 0) {
		reload_remaining--;
		if (reload_remaining == 0) {
			const int transferred = MIN(capacity - magazine, reserve);
			magazine += transferred;
			reserve -= transferred;
			events |= RELOAD_COMPLETED;
		}
	}
	if (!equipped || equipping || reload_remaining > 0) {
		return events;
	}
	if (p_reload_requested && magazine < capacity && reserve > 0) {
		reload_remaining = reload_ticks;
		return events | RELOAD_STARTED;
	}
	if (cooldown == 0 && (mode == WeaponSimulationSettings::AUTOMATIC ? p_trigger_held : pressed)) {
		if (magazine > 0) {
			magazine--;
			cooldown = shot_ticks;
			events |= SHOT;
		} else if (pressed) {
			events |= DRY_FIRE;
		}
	}
	return events;
}

Dictionary WeaponSimulation::configuration() const {
	Dictionary result;
	result["mode"] = mode;
	result["capacity"] = capacity;
	result["tick_rate"] = tick_rate;
	result["shot_ticks"] = shot_ticks;
	result["reload_ticks"] = reload_ticks;
	result["equip_ticks"] = equip_ticks;
	return result;
}

Dictionary WeaponSimulation::get_snapshot() const {
	if (!configured) {
		return Dictionary();
	}
	Dictionary result;
	result["version"] = 1;
	result["configuration"] = configuration();
	result["magazine"] = magazine;
	result["reserve"] = reserve;
	result["cooldown"] = cooldown;
	result["reload_remaining"] = reload_remaining;
	result["equip_remaining"] = equip_remaining;
	result["last_tick"] = last_tick;
	result["equipped"] = equipped;
	result["trigger_held"] = trigger_held;
	return result;
}

bool WeaponSimulation::restore_snapshot(const Dictionary &p_snapshot) {
	if (!configured || p_snapshot.size() != 10) {
		return false;
	}
	const char *integer_keys[] = { "version", "magazine", "reserve", "cooldown", "reload_remaining", "equip_remaining", "last_tick" };
	for (const char *key : integer_keys) {
		if (p_snapshot.get(key, Variant()).get_type() != Variant::INT) {
			return false;
		}
	}
	if ((int64_t)p_snapshot["version"] != 1 || p_snapshot.get("configuration", Variant()).get_type() != Variant::DICTIONARY || p_snapshot.get("equipped", Variant()).get_type() != Variant::BOOL || p_snapshot.get("trigger_held", Variant()).get_type() != Variant::BOOL) {
		return false;
	}
	const Dictionary config = p_snapshot["configuration"];
	const Dictionary expected = configuration();
	if (config.size() != expected.size()) {
		return false;
	}
	for (const Variant *key = expected.next(nullptr); key; key = expected.next(key)) {
		if (config.get(*key, Variant()).get_type() != Variant::INT || config[*key] != expected[*key]) {
			return false;
		}
	}
	const int64_t next_magazine = p_snapshot["magazine"];
	const int64_t next_reserve = p_snapshot["reserve"];
	const int64_t next_cooldown = p_snapshot["cooldown"];
	const int64_t next_reload = p_snapshot["reload_remaining"];
	const int64_t next_equip = p_snapshot["equip_remaining"];
	const int64_t next_tick = p_snapshot["last_tick"];
	const bool next_equipped = p_snapshot["equipped"];
	if (next_magazine < 0 || next_magazine > capacity || next_reserve < 0 || next_reserve > 1000000 || next_cooldown < 0 || next_cooldown > shot_ticks || next_reload < 0 || next_reload > reload_ticks || next_equip < 0 || next_equip > equip_ticks || next_tick < -1 || next_tick == INT64_MAX || (!next_equipped && (next_reload > 0 || next_equip > 0)) || (next_reload > 0 && (next_magazine == capacity || next_reserve == 0 || next_equip > 0))) {
		return false;
	}
	magazine = (int)next_magazine;
	reserve = (int)next_reserve;
	cooldown = (int)next_cooldown;
	reload_remaining = (int)next_reload;
	equip_remaining = (int)next_equip;
	last_tick = next_tick;
	equipped = next_equipped;
	trigger_held = p_snapshot["trigger_held"];
	return true;
}
