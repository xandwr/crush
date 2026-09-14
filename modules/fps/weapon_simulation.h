/**************************************************************************/
/*  weapon_simulation.h                                                   */
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
#include "core/variant/dictionary.h"

class WeaponSimulationSettings : public Resource {
	GDCLASS(WeaponSimulationSettings, Resource);
	int fire_mode = 0;
	int magazine_capacity = 30;
	double shot_interval = 0.1;
	double reload_duration = 2.0;
	double equip_delay = 0.25;

protected:
	static void _bind_methods();

public:
	enum FireMode {
		SEMI_AUTOMATIC,
		AUTOMATIC,
	};
	void set_fire_mode(int p_value) {
		fire_mode = p_value;
		emit_changed();
	}
	int get_fire_mode() const { return fire_mode; }
	void set_magazine_capacity(int p_value) {
		magazine_capacity = p_value;
		emit_changed();
	}
	int get_magazine_capacity() const { return magazine_capacity; }
	void set_shot_interval(double p_value) {
		shot_interval = p_value;
		emit_changed();
	}
	double get_shot_interval() const { return shot_interval; }
	void set_reload_duration(double p_value) {
		reload_duration = p_value;
		emit_changed();
	}
	double get_reload_duration() const { return reload_duration; }
	void set_equip_delay(double p_value) {
		equip_delay = p_value;
		emit_changed();
	}
	double get_equip_delay() const { return equip_delay; }
	PackedStringArray validation_errors() const;
};
VARIANT_ENUM_CAST(WeaponSimulationSettings::FireMode);

class WeaponSimulation : public RefCounted {
	GDCLASS(WeaponSimulation, RefCounted);
	bool configured = false;
	int mode = 0;
	int capacity = 0;
	int shot_ticks = 0;
	int reload_ticks = 0;
	int equip_ticks = 0;
	int tick_rate = 0;
	int magazine = 0;
	int reserve = 0;
	int cooldown = 0;
	int reload_remaining = 0;
	int equip_remaining = 0;
	int64_t last_tick = -1;
	bool equipped = false;
	bool trigger_held = false;
	Dictionary configuration() const;

protected:
	static void _bind_methods();

public:
	enum Event {
		NONE = 0,
		SHOT = 1,
		DRY_FIRE = 2,
		RELOAD_STARTED = 4,
		RELOAD_COMPLETED = 8,
	};
	bool configure(const Ref<WeaponSimulationSettings> &p_settings, int p_tick_rate, int p_magazine = -1, int p_reserve = 0);
	bool reset(int p_magazine = -1, int p_reserve = 0);
	void set_equipped(bool p_equipped);
	bool is_equipped() const { return equipped; }
	int step(int64_t p_tick, bool p_trigger_held, bool p_reload_requested = false);
	int get_magazine() const { return magazine; }
	int get_reserve() const { return reserve; }
	int64_t get_last_tick() const { return last_tick; }
	bool is_reloading() const { return reload_remaining > 0; }
	Dictionary get_snapshot() const;
	bool restore_snapshot(const Dictionary &p_snapshot);
};
VARIANT_ENUM_CAST(WeaponSimulation::Event);
