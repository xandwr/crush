/**************************************************************************/
/*  hitscan_3d.cpp                                                        */
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

#include "hitscan_3d.h"

#include "core/object/class_db.h"

void HitscanSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_damage", "value"), &HitscanSettings::set_damage);
	ClassDB::bind_method(D_METHOD("get_damage"), &HitscanSettings::get_damage);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damage", PROPERTY_HINT_RANGE, "0,1024,0.1,or_greater"), "set_damage", "get_damage");
	ClassDB::bind_method(D_METHOD("set_max_distance", "value"), &HitscanSettings::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &HitscanSettings::get_max_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "0.1,1024,0.1,or_greater"), "set_max_distance", "get_max_distance");
	ClassDB::bind_method(D_METHOD("set_falloff_start_distance", "value"), &HitscanSettings::set_falloff_start_distance);
	ClassDB::bind_method(D_METHOD("get_falloff_start_distance"), &HitscanSettings::get_falloff_start_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "falloff_start_distance", PROPERTY_HINT_RANGE, "0,1024,0.1,or_greater"), "set_falloff_start_distance", "get_falloff_start_distance");
	ClassDB::bind_method(D_METHOD("set_falloff_end_distance", "value"), &HitscanSettings::set_falloff_end_distance);
	ClassDB::bind_method(D_METHOD("get_falloff_end_distance"), &HitscanSettings::get_falloff_end_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "falloff_end_distance", PROPERTY_HINT_RANGE, "0,1024,0.1,or_greater"), "set_falloff_end_distance", "get_falloff_end_distance");
	ClassDB::bind_method(D_METHOD("set_falloff_min_multiplier", "value"), &HitscanSettings::set_falloff_min_multiplier);
	ClassDB::bind_method(D_METHOD("get_falloff_min_multiplier"), &HitscanSettings::get_falloff_min_multiplier);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "falloff_min_multiplier", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_falloff_min_multiplier", "get_falloff_min_multiplier");
	ClassDB::bind_method(D_METHOD("validation_errors"), &HitscanSettings::validation_errors);
	ClassDB::bind_method(D_METHOD("damage_at_distance", "distance"), &HitscanSettings::damage_at_distance);
}

PackedStringArray HitscanSettings::validation_errors() const {
	PackedStringArray errors;
	if (!Math::is_finite(damage) || damage < 0) {
		errors.push_back("Damage must be finite and nonnegative.");
	}
	if (!Math::is_finite(max_distance) || max_distance <= 0) {
		errors.push_back("Range must be finite and positive.");
	}
	if (!Math::is_finite(falloff_start_distance) || falloff_start_distance < 0) {
		errors.push_back("Falloff start must be finite and nonnegative.");
	}
	if (!Math::is_finite(falloff_end_distance) || falloff_end_distance <= falloff_start_distance || falloff_end_distance > max_distance) {
		errors.push_back("Falloff end must follow its start and stay within range.");
	}
	if (!Math::is_finite(falloff_min_multiplier) || falloff_min_multiplier < 0 || falloff_min_multiplier > 1) {
		errors.push_back("Minimum damage multiplier must be between zero and one.");
	}
	return errors;
}

double HitscanSettings::damage_at_distance(double p_distance) const {
	if (!validation_errors().is_empty() || !Math::is_finite(p_distance) || p_distance < 0) {
		return -1.0;
	}
	if (p_distance > max_distance) {
		return 0.0;
	}
	double fraction = CLAMP((p_distance - falloff_start_distance) / (falloff_end_distance - falloff_start_distance), 0.0, 1.0);
	return damage * (1.0 - fraction * (1.0 - falloff_min_multiplier));
}

void Hitscan3D::_bind_methods() {
	ClassDB::bind_static_method("Hitscan3D", D_METHOD("trace", "space", "origin", "direction", "settings", "collision_mask", "exclude", "collide_with_areas"), &Hitscan3D::trace, DEFVAL(UINT32_MAX), DEFVAL(TypedArray<RID>()), DEFVAL(false));
}

Dictionary Hitscan3D::trace(PhysicsDirectSpaceState3D *p_space, const Vector3 &p_origin, const Vector3 &p_direction, const Ref<HitscanSettings> &p_settings, uint32_t p_collision_mask, const TypedArray<RID> &p_exclude, bool p_collide_with_areas) {
	if (!p_space || p_settings.is_null() || !p_settings->validation_errors().is_empty() || !p_origin.is_finite() || !p_direction.is_finite()) {
		return Dictionary();
	}
	real_t length_squared = p_direction.length_squared();
	if (!Math::is_finite(length_squared) || Math::abs(length_squared - 1.0) > 0.001) {
		return Dictionary();
	}
	PhysicsDirectSpaceState3D::RayParameters parameters;
	parameters.from = p_origin;
	parameters.to = p_origin + p_direction.normalized() * p_settings->get_max_distance();
	if (!parameters.to.is_finite() || parameters.to == p_origin) {
		return Dictionary();
	}
	parameters.collision_mask = p_collision_mask;
	parameters.collide_with_areas = p_collide_with_areas;
	parameters.hit_from_inside = true;
	parameters.hit_back_faces = true;
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude.insert(p_exclude[i]);
	}
	PhysicsDirectSpaceState3D::RayResult hit;
	bool collided = p_space->intersect_ray(parameters, hit);
	double distance = collided ? p_origin.distance_to(hit.position) : p_settings->get_max_distance();
	Dictionary result;
	result["hit"] = collided;
	result["origin"] = p_origin;
	result["endpoint"] = collided ? hit.position : parameters.to;
	result["distance"] = distance;
	result["normal"] = collided ? hit.normal : Vector3();
	result["collider"] = collided ? Variant(hit.collider) : Variant();
	result["collider_id"] = collided ? uint64_t(hit.collider_id) : uint64_t(0);
	result["rid"] = collided ? hit.rid : RID();
	result["shape"] = collided ? hit.shape : -1;
	result["face_index"] = collided ? hit.face_index : -1;
	result["damage"] = collided ? p_settings->damage_at_distance(MIN(distance, p_settings->get_max_distance())) : 0.0;
	return result;
}
