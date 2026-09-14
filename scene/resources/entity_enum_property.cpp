/**************************************************************************/
/*  entity_enum_property.cpp                                                  */
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

#include "entity_enum_property.h"

#include "core/object/class_db.h"

void EntityEnumProperty::set_key(const String &p_key) {
	key = p_key;
	emit_changed();
}

String EntityEnumProperty::get_key() const {
	return key;
}

void EntityEnumProperty::set_display_name(const String &p_display_name) {
	display_name = p_display_name;
	emit_changed();
}

String EntityEnumProperty::get_display_name() const {
	return display_name;
}

void EntityEnumProperty::set_description(const String &p_description) {
	description = p_description;
	emit_changed();
}

String EntityEnumProperty::get_description() const {
	return description;
}

void EntityEnumProperty::set_default_value(const int64_t &p_default_value) {
	default_value = p_default_value;
	emit_changed();
}

int64_t EntityEnumProperty::get_default_value() const {
	return default_value;
}

void EntityEnumProperty::set_choices(const TypedDictionary<String, int64_t> &p_choices) {
	choices = p_choices.duplicate();
	emit_changed();
}

TypedDictionary<String, int64_t> EntityEnumProperty::get_choices() const {
	return choices.duplicate();
}

void EntityEnumProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_key", "key"), &EntityEnumProperty::set_key);
	ClassDB::bind_method(D_METHOD("get_key"), &EntityEnumProperty::get_key);
	ClassDB::bind_method(D_METHOD("set_display_name", "display_name"), &EntityEnumProperty::set_display_name);
	ClassDB::bind_method(D_METHOD("get_display_name"), &EntityEnumProperty::get_display_name);
	ClassDB::bind_method(D_METHOD("set_description", "description"), &EntityEnumProperty::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &EntityEnumProperty::get_description);
	ClassDB::bind_method(D_METHOD("set_default_value", "default_value"), &EntityEnumProperty::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &EntityEnumProperty::get_default_value);
	ClassDB::bind_method(D_METHOD("set_choices", "choices"), &EntityEnumProperty::set_choices);
	ClassDB::bind_method(D_METHOD("get_choices"), &EntityEnumProperty::get_choices);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "key"), "set_key", "get_key");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "display_name"), "set_display_name", "get_display_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "default_value"), "set_default_value", "get_default_value");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "choices", PROPERTY_HINT_DICTIONARY_TYPE, "String;int"), "set_choices", "get_choices");
}
