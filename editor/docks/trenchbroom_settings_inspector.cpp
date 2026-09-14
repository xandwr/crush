/**************************************************************************/
/*  trenchbroom_settings_inspector.cpp                                    */
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

#include "trenchbroom_settings_inspector.h"

#include "core/object/callable_mp.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"

void EditorPropertyTrenchBroomList::_change(const Variant &p_value, int p_index, const String &p_key) {
	if (p_index < 0 || p_index >= entries.size()) {
		return;
	}
	entries = entries.duplicate(true);
	Dictionary entry = entries[p_index];
	entry[p_key] = p_value;
	emit_changed(get_edited_property(), entries, StringName(), true);
}

void EditorPropertyTrenchBroomList::_select_format(int p_value, int p_index) {
	_change(PackedStringArray({ "Valve", "Standard", "Quake2", "Quake3" })[p_value], p_index, "format");
}

void EditorPropertyTrenchBroomList::_add() {
	entries = entries.duplicate(true);
	Dictionary entry;
	if (kind == "trenchbroom_map_formats") {
		entry["format"] = "Valve";
		entry["initial_map"] = "";
	} else {
		entry["name"] = "";
		entry["pattern"] = "";
		entry["transparent"] = true;
		if (kind == "trenchbroom_brush_tags") {
			entry["material"] = "";
		}
	}
	entries.push_back(entry);
	emit_changed(get_edited_property(), entries);
	_rebuild();
}

void EditorPropertyTrenchBroomList::_remove(int p_index) {
	entries = entries.duplicate(true);
	entries.remove_at(p_index);
	emit_changed(get_edited_property(), entries);
	_rebuild();
}

void EditorPropertyTrenchBroomList::_move(int p_index, int p_offset) {
	entries = entries.duplicate(true);
	SWAP(entries[p_index], entries[p_index + p_offset]);
	emit_changed(get_edited_property(), entries);
	_rebuild();
}

void EditorPropertyTrenchBroomList::_browse(int p_index) {
	file_index = p_index;
	file_dialog->popup_file_dialog();
}

void EditorPropertyTrenchBroomList::_file_selected(const String &p_path) {
	_change(p_path, file_index, "initial_map");
	_rebuild();
}

void EditorPropertyTrenchBroomList::_rebuild() {
	for (int i = rows->get_child_count() - 1; i >= 0; i--) {
		Node *child = rows->get_child(i);
		rows->remove_child(child);
		child->queue_free();
	}
	for (int i = 0; i < entries.size(); i++) {
		Dictionary entry = entries[i].get_type() == Variant::DICTIONARY ? Dictionary(entries[i]) : Dictionary();
		HBoxContainer *header = memnew(HBoxContainer);
		rows->add_child(header);
		Label *index = memnew(Label);
		index->set_text(itos(i));
		header->add_child(index);
		if (kind == "trenchbroom_map_formats") {
			OptionButton *format = memnew(OptionButton);
			PackedStringArray formats({ "Valve", "Standard", "Quake2", "Quake3" });
			for (const String &name : formats) {
				format->add_item(name);
			}
			format->select(formats.find(entry.get("format", "Valve")));
			format->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			format->connect("item_selected", callable_mp(this, &EditorPropertyTrenchBroomList::_select_format).bind(i));
			header->add_child(format);
		} else {
			LineEdit *name = memnew(LineEdit);
			name->set_placeholder(TTR("Tag Name"));
			name->set_text(entry.get("name", ""));
			name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			name->connect("text_changed", callable_mp(this, &EditorPropertyTrenchBroomList::_change).bind(i, "name"));
			header->add_child(name);
		}
		for (int offset : { -1, 1, 0 }) {
			Button *button = memnew(Button);
			button->set_button_icon(get_editor_theme_icon(offset == -1 ? "MoveUp" : (offset == 1 ? "MoveDown" : "Remove")));
			button->set_tooltip_text(offset == -1 ? TTR("Move Up") : (offset == 1 ? TTR("Move Down") : TTR("Remove")));
			if (offset == 0) {
				button->connect("pressed", callable_mp(this, &EditorPropertyTrenchBroomList::_remove).bind(i));
			} else {
				button->set_disabled(i + offset < 0 || i + offset >= entries.size());
				button->connect("pressed", callable_mp(this, &EditorPropertyTrenchBroomList::_move).bind(i, offset));
			}
			header->add_child(button);
		}
		PackedStringArray keys;
		if (kind == "trenchbroom_map_formats") {
			keys.push_back("initial_map");
		} else {
			keys.push_back("pattern");
			if (kind == "trenchbroom_brush_tags") {
				keys.push_back("material");
			}
		}
		for (const String &key : keys) {
			HBoxContainer *row = memnew(HBoxContainer);
			rows->add_child(row);
			LineEdit *field = memnew(LineEdit);
			field->set_placeholder(key == "initial_map" ? TTR("Initial Map (optional)") : (key == "pattern" ? TTR("Match Pattern") : TTR("Material (optional)")));
			field->set_text(entry.get(key, ""));
			field->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			field->connect("text_changed", callable_mp(this, &EditorPropertyTrenchBroomList::_change).bind(i, key));
			row->add_child(field);
			if (key == "initial_map") {
				Button *browse = memnew(Button);
				browse->set_button_icon(get_editor_theme_icon("Folder"));
				browse->set_tooltip_text(TTR("Select Initial Map"));
				browse->connect("pressed", callable_mp(this, &EditorPropertyTrenchBroomList::_browse).bind(i));
				row->add_child(browse);
			}
		}
		if (kind != "trenchbroom_map_formats") {
			CheckBox *transparent = memnew(CheckBox);
			transparent->set_text(TTR("Transparent"));
			transparent->set_pressed(entry.get("transparent", true));
			transparent->connect("toggled", callable_mp(this, &EditorPropertyTrenchBroomList::_change).bind(i, "transparent"));
			rows->add_child(transparent);
		}
	}
	Button *add = memnew(Button);
	add->set_text(TTR("Add Element"));
	add->connect("pressed", callable_mp(this, &EditorPropertyTrenchBroomList::_add));
	rows->add_child(add);
}

void EditorPropertyTrenchBroomList::update_property() {
	Array value = get_edited_object()->get(get_edited_property());
	if (entries.recursive_equal(value, 0) && rows->get_child_count() > 0) {
		return;
	}
	entries = value.duplicate(true);
	_rebuild();
}

EditorPropertyTrenchBroomList::EditorPropertyTrenchBroomList(const String &p_kind) {
	kind = p_kind;
	rows = memnew(VBoxContainer);
	add_child(rows);
	set_bottom_editor(rows);
	file_dialog = memnew(EditorFileDialog);
	file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_dialog->add_filter("*.map", TTR("Map Files"));
	file_dialog->connect("file_selected", callable_mp(this, &EditorPropertyTrenchBroomList::_file_selected));
	add_child(file_dialog);
}

bool TrenchBroomSettingsInspector::can_handle(Object *p_object) {
	return true;
}

bool TrenchBroomSettingsInspector::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_type == Variant::ARRAY && (p_hint_text == "trenchbroom_map_formats" || p_hint_text == "trenchbroom_brush_tags" || p_hint_text == "trenchbroom_face_tags")) {
		add_property_editor(p_path, memnew(EditorPropertyTrenchBroomList(p_hint_text)));
		return true;
	}
	return false;
}
