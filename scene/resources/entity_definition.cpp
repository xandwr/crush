/**************************************************************************/
/*  entity_definition.cpp                                                */
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

#include "entity_definition.h"

#include "core/object/class_db.h"

void EntityDefinition::set_classname(const String &p_classname) {
	if (classname == p_classname) {
		return;
	}
	classname = p_classname;
	emit_changed();
}

String EntityDefinition::get_classname() const {
	return classname;
}

void EntityDefinition::set_description(const String &p_description) {
	if (description == p_description) {
		return;
	}
	description = p_description;
	emit_changed();
}

String EntityDefinition::get_description() const {
	return description;
}

void EntityDefinition::set_scene(const Ref<PackedScene> &p_scene) {
	if (scene == p_scene) {
		return;
	}
	scene = p_scene;
	emit_changed();
}

Ref<PackedScene> EntityDefinition::get_scene() const {
	return scene;
}

void EntityDefinition::set_enum_properties(const TypedArray<EntityEnumProperty> &p_properties) {
	enum_properties = p_properties.duplicate();
	emit_changed();
}

TypedArray<EntityEnumProperty> EntityDefinition::get_enum_properties() const {
	return enum_properties.duplicate();
}

void EntityDefinition::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enum_properties", "properties"), &EntityDefinition::set_enum_properties);
	ClassDB::bind_method(D_METHOD("get_enum_properties"), &EntityDefinition::get_enum_properties);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "enum_properties", PROPERTY_HINT_ARRAY_TYPE, "EntityEnumProperty"), "set_enum_properties", "get_enum_properties");
	ClassDB::bind_method(D_METHOD("set_classname", "classname"), &EntityDefinition::set_classname);
	ClassDB::bind_method(D_METHOD("get_classname"), &EntityDefinition::get_classname);
	ClassDB::bind_method(D_METHOD("set_description", "description"), &EntityDefinition::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &EntityDefinition::get_description);
	ClassDB::bind_method(D_METHOD("set_scene", "scene"), &EntityDefinition::set_scene);
	ClassDB::bind_method(D_METHOD("get_scene"), &EntityDefinition::get_scene);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "classname"), "set_classname", "get_classname");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_scene", "get_scene");
}
