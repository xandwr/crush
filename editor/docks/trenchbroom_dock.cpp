/**************************************************************************/
/*  trenchbroom_dock.cpp                                                  */
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

#include "trenchbroom_dock.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "editor/docks/trenchbroom_game_config_exporter.h"
#include "editor/settings/editor_settings.h"
#include "editor/settings/project_settings_editor.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/scroll_container.h"

void TrenchBroomDock::_refresh_destination() {
	TrenchBroomGameConfigExporter exporter;
	String path = exporter.get_export_directory(EDITOR_GET("filesystem/external_programs/trenchbroom/game_config_export_directory"));
	destination->set_text(path.is_empty() ? TTR("Set the game config export directory in Editor Settings > FileSystem > External Programs > TrenchBroom.") : path);
}

void TrenchBroomDock::_export() {
	_refresh_destination();
	TrenchBroomGameConfigExporter exporter;
	Dictionary result = exporter.export_game_config(EDITOR_GET("filesystem/external_programs/trenchbroom/game_config_export_directory"));
	status->set_text(result["message"]);
}

void TrenchBroomDock::_open_settings() {
	ProjectSettingsEditor::get_singleton()->popup_project_settings(true);
	ProjectSettingsEditor::get_singleton()->set_filter("trenchbroom");
	ProjectSettingsEditor::get_singleton()->set_general_page("trenchbroom/general");
}

void TrenchBroomDock::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &TrenchBroomDock::_refresh_destination));
		EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &TrenchBroomDock::_refresh_destination));
		_refresh_destination();
	}
}

TrenchBroomDock::TrenchBroomDock() {
	set_name("TrenchBroom");
	set_icon_name("Node3D");
	set_default_slot(DOCK_SLOT_LEFT_BR);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	add_child(scroll);
	VBoxContainer *layout = memnew(VBoxContainer);
	layout->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->add_child(layout);
	Button *export_button = memnew(Button);
	export_button->set_text(TTR("Export TrenchBroom Game Config"));
	export_button->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	export_button->connect("pressed", callable_mp(this, &TrenchBroomDock::_export));
	layout->add_child(export_button);
	Button *settings_button = memnew(Button);
	settings_button->set_text(TTR("Project Settings"));
	settings_button->connect("pressed", callable_mp(this, &TrenchBroomDock::_open_settings));
	layout->add_child(settings_button);
	Label *heading = memnew(Label);
	heading->set_text(TTR("Export Destination"));
	layout->add_child(heading);
	destination = memnew(Label);
	destination->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	destination->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	layout->add_child(destination);
	status = memnew(Label);
	status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	status->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	layout->add_child(status);
}
