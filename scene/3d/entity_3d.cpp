/**************************************************************************/
/*  entity_3d.cpp                                                         */
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

#include "entity_3d.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

void Entity3D::_queue_rebuild() {
	dirty = true;
	if (!is_inside_tree() || rebuild_pending) {
		return;
	}
	rebuild_pending = true;
	callable_mp(this, &Entity3D::_rebuild).call_deferred();
}

void Entity3D::_rebuild() {
	rebuild_pending = false;
	if (!is_inside_tree() || !dirty) {
		return;
	}
	dirty = false;
	Node *previous = Object::cast_to<Node>(ObjectDB::get_instance(instance_id));
	instance_id = ObjectID();
	if (previous) {
		if (previous->get_parent() == this) {
			remove_child(previous);
		}
		previous->queue_free();
	}
	invalid_root = false;
	recursive_definition = false;
	invalid_entity_property = false;
	if (definition.is_valid() && definition->get_scene().is_valid()) {
		for (Node *ancestor = get_parent(); ancestor; ancestor = ancestor->get_parent()) {
			Entity3D *entity = Object::cast_to<Entity3D>(ancestor);
			if (entity && entity->get_definition() == definition) {
				recursive_definition = true;
				break;
			}
		}
		if (!recursive_definition) {
			Node *instance = definition->get_scene()->instantiate();
			if (Object::cast_to<Node3D>(instance)) {
				TypedArray<EntityEnumProperty> properties = definition->get_enum_properties();
				for (int i = 0; i < properties.size(); i++) {
					Ref<EntityEnumProperty> property = properties[i];
					if (property.is_valid()) {
						String key = property->get_key();
						bool valid = false;
						instance->set(key, entity_properties.get(key, property->get_default_value()), &valid);
						invalid_entity_property |= !valid;
					}
				}
				instance_id = instance->get_instance_id();
				add_child(instance, false, INTERNAL_MODE_BACK);
			} else {
				invalid_root = true;
				if (instance) {
					memdelete(instance);
				}
			}
		}
	}
	update_configuration_warnings();
}

void Entity3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE && dirty) {
		_rebuild();
	}
}

void Entity3D::set_definition(const Ref<EntityDefinition> &p_definition) {
	if (definition == p_definition) {
		return;
	}
	if (definition.is_valid()) {
		definition->disconnect_changed(callable_mp(this, &Entity3D::_queue_rebuild));
	}
	definition = p_definition;
	if (definition.is_valid()) {
		definition->connect_changed(callable_mp(this, &Entity3D::_queue_rebuild));
	}
	_queue_rebuild();
	update_configuration_warnings();
}

Ref<EntityDefinition> Entity3D::get_definition() const {
	return definition;
}

void Entity3D::set_entity_properties(const Dictionary &p_properties) {
	entity_properties = p_properties.duplicate(true);
	_queue_rebuild();
}

Dictionary Entity3D::get_entity_properties() const {
	return entity_properties.duplicate(true);
}

PackedStringArray Entity3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (invalid_root) {
		warnings.push_back(RTR("The entity scene must have a Node3D root."));
	}
	if (invalid_entity_property) {
		warnings.push_back(RTR("An entity definition property does not exist on the instantiated scene root."));
	}
	if (recursive_definition) {
		warnings.push_back(RTR("The entity definition is already used by an ancestor Entity3D. Recursive instantiation is disabled."));
	}
	return warnings;
}

void Entity3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_definition", "definition"), &Entity3D::set_definition);
	ClassDB::bind_method(D_METHOD("get_definition"), &Entity3D::get_definition);
	ClassDB::bind_method(D_METHOD("set_entity_properties", "properties"), &Entity3D::set_entity_properties);
	ClassDB::bind_method(D_METHOD("get_entity_properties"), &Entity3D::get_entity_properties);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "entity_properties"), "set_entity_properties", "get_entity_properties");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "definition", PROPERTY_HINT_RESOURCE_TYPE, "EntityDefinition"), "set_definition", "get_definition");
}
