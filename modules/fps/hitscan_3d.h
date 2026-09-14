/**************************************************************************/
/*  hitscan_3d.h                                                          */
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
#include "core/variant/typed_array.h"
#include "servers/physics_3d/physics_server_3d.h"

class HitscanSettings : public Resource {
	GDCLASS(HitscanSettings, Resource);
	double damage = 10.0;
	double max_distance = 1024.0;
	double falloff_start_distance = 32.0;
	double falloff_end_distance = 1024.0;
	double falloff_min_multiplier = 0.3;

protected:
	static void _bind_methods();

public:
	void set_damage(double p_value) {
		damage = p_value;
		emit_changed();
	}
	double get_damage() const { return damage; }
	void set_max_distance(double p_value) {
		max_distance = p_value;
		emit_changed();
	}
	double get_max_distance() const { return max_distance; }
	void set_falloff_start_distance(double p_value) {
		falloff_start_distance = p_value;
		emit_changed();
	}
	double get_falloff_start_distance() const { return falloff_start_distance; }
	void set_falloff_end_distance(double p_value) {
		falloff_end_distance = p_value;
		emit_changed();
	}
	double get_falloff_end_distance() const { return falloff_end_distance; }
	void set_falloff_min_multiplier(double p_value) {
		falloff_min_multiplier = p_value;
		emit_changed();
	}
	double get_falloff_min_multiplier() const { return falloff_min_multiplier; }
	PackedStringArray validation_errors() const;
	double damage_at_distance(double p_distance) const;
};

class Hitscan3D : public RefCounted {
	GDCLASS(Hitscan3D, RefCounted);

protected:
	static void _bind_methods();

public:
	static Dictionary trace(PhysicsDirectSpaceState3D *p_space, const Vector3 &p_origin, const Vector3 &p_direction, const Ref<HitscanSettings> &p_settings, uint32_t p_collision_mask = UINT32_MAX, const TypedArray<RID> &p_exclude = TypedArray<RID>(), bool p_collide_with_areas = false);
};
