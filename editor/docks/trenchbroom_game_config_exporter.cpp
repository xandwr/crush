/**************************************************************************/
/*  trenchbroom_game_config_exporter.cpp                                  */
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

#include "trenchbroom_game_config_exporter.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/resources/entity_definition.h"

namespace {

Dictionary failure(const String &p_message) {
	Dictionary result;
	result["success"] = false;
	result["message"] = p_message;
	return result;
}

String project_path() {
	return ProjectSettings::get_singleton()->globalize_path("res://").simplify_path().trim_suffix("/");
}

String game_name() {
	String name = GLOBAL_GET("trenchbroom/general/game_name");
	if (name.strip_edges().is_empty()) {
		name = GLOBAL_GET("application/config/name");
	}
	return name.strip_edges().is_empty() ? "Untitled Project" : name.strip_edges();
}

bool valid_project_path(const String &p_path) {
	if (!p_path.begins_with("res://")) {
		return false;
	}
	String absolute = ProjectSettings::get_singleton()->globalize_path(p_path).simplify_path();
	return absolute == project_path() || absolute.begins_with(project_path() + "/");
}

bool valid_classname(const String &p_name) {
	if (p_name.is_empty()) {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		char32_t c = p_name[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
			return false;
		}
	}
	return true;
}

String fgd_string(const String &p_text) {
	return p_text.replace("\\", "\\\\").replace("\"", "\\\"").replace("\r", " ").replace("\n", " ").replace("\t", " ");
}

Error write_text(const String &p_path, const String &p_text) {
	Error error;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &error);
	if (file.is_null()) {
		return error;
	}
	file->store_string(p_text);
	file->flush();
	return file->get_error();
}

String collect_textures(const String &p_root, const String &p_relative, HashMap<String, String> &r_files, HashMap<String, String> &r_material_names, int p_depth = 0) {
	if (p_depth > 64) {
		return vformat("Texture directories are nested too deeply: %s", p_root);
	}
	String path = p_root.path_join(p_relative);
	Ref<DirAccess> dir = DirAccess::open(path);
	if (dir.is_null()) {
		return vformat("Cannot read texture directory: %s", path);
	}
	PackedStringArray entries = dir->get_files();
	for (const String &entry : entries) {
		String extension = entry.get_extension().to_lower();
		if (!PackedStringArray({ "bmp", "exr", "hdr", "jpeg", "jpg", "png", "tga", "webp" }).has(extension)) {
			continue;
		}
		String source = path.path_join(entry);
		String relative = p_relative.path_join(entry);
		String key = relative.get_basename().to_lower();
		if (r_material_names.has(key)) {
			if (r_material_names[key] == source) {
				continue;
			}
			return vformat("Texture name collision for '%s': %s and %s. Rename one texture or separate them into differently named subfolders.", key, r_material_names[key], source);
		}
		r_material_names[key] = source;
		r_files["textures/" + relative] = source;
	}
	for (const String &entry : dir->get_directories()) {
		if (entry.begins_with(".")) {
			continue;
		}
		if (dir->is_link(entry)) {
			return vformat("Linked texture directories are not supported: %s", path.path_join(entry));
		}
		String error = collect_textures(p_root, p_relative.path_join(entry), r_files, r_material_names, p_depth + 1);
		if (!error.is_empty()) {
			return error;
		}
	}
	return String();
}

String build_tags(const Array &p_tags, bool p_brush, int p_version, Array &r_tags) {
	HashSet<String> names;
	for (int i = 0; i < p_tags.size(); i++) {
		if (p_tags[i].get_type() != Variant::DICTIONARY) {
			return "Each tag must be a dictionary.";
		}
		Dictionary source = p_tags[i];
		if (source.get("name", Variant()).get_type() != Variant::STRING || source.get("pattern", Variant()).get_type() != Variant::STRING || source.get("transparent", true).get_type() != Variant::BOOL) {
			return "Tags require a string name, string pattern, and boolean transparent field.";
		}
		String name = source["name"];
		String pattern = source["pattern"];
		if (name.strip_edges().is_empty() || pattern.is_empty() || names.has(name)) {
			return "Tag names must be nonempty and unique within their group, and patterns must be nonempty.";
		}
		names.insert(name);
		Dictionary tag;
		tag["name"] = name;
		tag["pattern"] = pattern;
		tag["match"] = p_brush ? "classname" : (p_version == 9 ? "material" : "texture");
		tag["attribs"] = bool(source.get("transparent", true)) ? PackedStringArray({ "transparent" }) : PackedStringArray();
		if (p_brush) {
			if (source.get("material", "").get_type() != Variant::STRING) {
				return "Brush tag material must be a string.";
			}
			String material = source.get("material", "");
			if (!material.is_empty()) {
				tag[p_version == 9 ? "material" : "texture"] = material;
			}
		}
		r_tags.push_back(tag);
	}
	return String();
}

Error remove_tree(const String &p_path) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return ERR_CANT_OPEN;
	}
	dir->set_include_hidden(true);
	for (const String &entry : dir->get_files()) {
		Error error = dir->remove(entry);
		if (error != OK) {
			return error;
		}
	}
	for (const String &entry : dir->get_directories()) {
		Error error = dir->is_link(entry) ? dir->remove(entry) : remove_tree(p_path.path_join(entry));
		if (error != OK) {
			return error;
		}
	}
	return DirAccess::remove_absolute(p_path);
}

} // namespace

String TrenchBroomGameConfigExporter::get_export_directory(const String &p_parent_directory) const {
	if (p_parent_directory.strip_edges().is_empty() || !p_parent_directory.is_absolute_path() || p_parent_directory.begins_with("res://") || p_parent_directory.begins_with("user://")) {
		return String();
	}
	String slug;
	String name = game_name().to_lower();
	for (int i = 0; i < name.length(); i++) {
		char32_t c = name[i];
		slug += ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') ? String::chr(c) : "_";
	}
	return p_parent_directory.simplify_path().path_join(slug.substr(0, 48) + "_native_" + project_path().sha256_text().substr(0, 8));
}

Dictionary TrenchBroomGameConfigExporter::export_game_config(const String &p_parent_directory) const {
	String destination = get_export_directory(p_parent_directory);
	if (destination.is_empty()) {
		return failure("Set an absolute game config export directory in Editor Settings > FileSystem > External Programs > TrenchBroom.");
	}
	int version = GLOBAL_GET("trenchbroom/compatibility/game_config_version");
	if (version == 0) {
		version = 9;
	}
	if (version != 4 && version != 8 && version != 9) {
		return failure("Supported game config versions are 4, 8, and 9.");
	}
	double units = GLOBAL_GET("trenchbroom/entities/map_units_per_meter");
	Vector2 uv_scale = GLOBAL_GET("trenchbroom/face_attributes/default_uv_scale");
	if (!Math::is_finite(units) || units <= 0 || !uv_scale.is_finite() || uv_scale.x == 0 || uv_scale.y == 0) {
		return failure("Map units per meter must be positive and finite. UV scale components must be finite and nonzero.");
	}
	HashMap<String, String> files;
	HashMap<String, String> material_names;
	PackedStringArray sources = GLOBAL_GET("trenchbroom/general/texture_source_directories");
	for (const String &source : sources) {
		if (!valid_project_path(source)) {
			return failure(vformat("Texture sources must be project directories: %s", source));
		}
		String absolute = ProjectSettings::get_singleton()->globalize_path(source).simplify_path();
		if (destination.to_lower().begins_with(absolute.trim_suffix("/").to_lower() + "/") || destination.to_lower() == absolute.to_lower()) {
			return failure("The export destination must be outside every texture source directory.");
		}
		String error = collect_textures(absolute, "", files, material_names);
		if (!error.is_empty()) {
			return failure(error);
		}
	}

	Dictionary config;
	config["version"] = version;
	config["name"] = game_name();
	Array formats = GLOBAL_GET("trenchbroom/map_formats/formats");
	if (formats.is_empty()) {
		return failure("Select at least one map format.");
	}
	Array exported_formats;
	HashSet<String> format_names;
	for (int i = 0; i < formats.size(); i++) {
		if (formats[i].get_type() != Variant::DICTIONARY) {
			return failure("Each map format must be a dictionary.");
		}
		Dictionary entry = formats[i];
		if (entry.get("format", Variant()).get_type() != Variant::STRING || entry.get("initial_map", "").get_type() != Variant::STRING) {
			return failure("Map formats require string format and initial_map fields.");
		}
		String format = entry["format"];
		if (!PackedStringArray({ "Valve", "Standard", "Quake2", "Quake3" }).has(format) || format_names.has(format)) {
			return failure("Map formats must be unique and one of Valve, Standard, Quake2, or Quake3.");
		}
		format_names.insert(format);
		Dictionary exported;
		exported["format"] = format;
		String initial_map = entry.get("initial_map", "");
		if (!initial_map.is_empty()) {
			if (!valid_project_path(initial_map) || initial_map.get_extension().to_lower() != "map" || !FileAccess::exists(initial_map)) {
				return failure(vformat("Initial map must be an existing project .map file: %s", initial_map));
			}
			String target = "initial_" + format.to_lower() + ".map";
			files[target] = initial_map;
			exported["initialmap"] = target;
		}
		exported_formats.push_back(exported);
	}
	config["fileformats"] = exported_formats;
	Dictionary package;
	package["extension"] = ".zip";
	package["format"] = "zip";
	Dictionary filesystem;
	filesystem["searchpath"] = ".";
	filesystem["packageformat"] = package;
	config["filesystem"] = filesystem;
	Dictionary materials;
	PackedStringArray extensions({ ".bmp", ".exr", ".hdr", ".jpeg", ".jpg", ".png", ".tga", ".webp" });
	if (version == 4) {
		Dictionary texture_package;
		texture_package["type"] = "directory";
		texture_package["root"] = "textures";
		materials["package"] = texture_package;
		Dictionary texture_format;
		texture_format["format"] = "image";
		PackedStringArray old_extensions;
		for (const String &extension : extensions) {
			old_extensions.push_back(extension.trim_prefix("."));
		}
		texture_format["extensions"] = old_extensions;
		materials["format"] = texture_format;
	} else {
		materials["root"] = "textures";
		materials["extensions"] = extensions;
	}
	materials["excludes"] = GLOBAL_GET("trenchbroom/textures/exclusion_patterns");
	materials["attribute"] = "wad";
	String palette = GLOBAL_GET("trenchbroom/textures/palette");
	if (!palette.is_empty()) {
		if (!valid_project_path(palette) || !FileAccess::exists(palette)) {
			return failure(vformat("Palette must be an existing project file: %s", palette));
		}
		String target = "palette." + palette.get_extension();
		files[target] = palette;
		materials["palette"] = target;
	}
	config[version == 9 ? "materials" : "textures"] = materials;

	String fgd = "// Generated from native EntityDefinition resources.\n@SolidClass = worldspawn : \"World\"\n[\n]\n\n";
	PackedStringArray definitions = GLOBAL_GET("trenchbroom/general/entity_definitions");
	HashSet<String> classnames;
	classnames.insert("worldspawn");
	for (const String &path : definitions) {
		if (!valid_project_path(path) || !ResourceLoader::exists(path, "EntityDefinition")) {
			return failure(vformat("Entity definition must be an existing project resource: %s", path));
		}
		Ref<EntityDefinition> definition = ResourceLoader::load(path, "EntityDefinition");
		if (definition.is_null()) {
			return failure(vformat("Not an EntityDefinition resource: %s", path));
		}
		String classname = definition->get_classname();
		if (!valid_classname(classname) || classnames.has(classname.to_lower())) {
			return failure(vformat("Entity classname '%s' must be unique, use only letters, digits and underscores, and cannot be worldspawn. Resource: %s", classname, path));
		}
		classnames.insert(classname.to_lower());
		if (definition->get_scene().is_null() || !definition->get_scene()->can_instantiate()) {
			return failure(vformat("Entity definition requires a valid scene: %s", path));
		}
		Ref<SceneState> state = definition->get_scene()->get_state();
		String root_type;
		for (int depth = 0; state.is_valid() && state->get_node_count() > 0 && depth < 64; depth++) {
			root_type = state->get_node_type(0);
			if (!root_type.is_empty()) {
				break;
			}
			state = state->get_base_scene_state();
		}
		if (root_type.is_empty() || !ClassDB::is_parent_class(root_type, "Node3D")) {
			return failure(vformat("Entity scene must have a Node3D root: %s", path));
		}
		String half_size = String::num(units * 0.5);
		fgd += "@PointClass size(-" + half_size + " -" + half_size + " -" + half_size + ", " + half_size + " " + half_size + " " + half_size + ") color(0.6 0.8 1) = " + classname + " : \"" + fgd_string(definition->get_description()) + "\"\n[\n]\n\n";
	}
	Dictionary entities;
	entities["definitions"] = PackedStringArray({ "Entities.fgd" });
	entities["defaultcolor"] = "0.6 0.6 0.6 1.0";
	entities["scale"] = units;
	if (version == 4) {
		entities["modelformats"] = PackedStringArray({ "mdl", "md2", "obj" });
	}
	config["entities"] = entities;
	Dictionary tags;
	for (bool brush : { true, false }) {
		Array exported;
		String error = build_tags(GLOBAL_GET(brush ? "trenchbroom/tags/brush_tags" : "trenchbroom/tags/face_tags"), brush, version, exported);
		if (!error.is_empty()) {
			return failure(error);
		}
		tags[brush ? "brush" : "brushface"] = exported;
	}
	config["tags"] = tags;
	Dictionary defaults;
	defaults["scale"] = Array({ uv_scale.x, uv_scale.y });
	Dictionary face_attributes;
	face_attributes["defaults"] = defaults;
	face_attributes["contentflags"] = Array();
	face_attributes["surfaceflags"] = Array();
	config["faceattribs"] = face_attributes;

	String icon_path = GLOBAL_GET("trenchbroom/general/icon");
	if (icon_path.is_empty()) {
		icon_path = GLOBAL_GET("application/config/icon");
	}
	Ref<Image> icon;
	if (!icon_path.is_empty()) {
		if (!valid_project_path(icon_path) || !FileAccess::exists(icon_path)) {
			return failure(vformat("Icon must be an existing project image: %s", icon_path));
		}
		icon = Image::load_from_file(icon_path);
		if (icon.is_null() || icon->is_empty()) {
			return failure(vformat("Cannot load icon image: %s", icon_path));
		}
		icon->resize(32, 32, Image::INTERPOLATE_LANCZOS);
		config["icon"] = "icon.png";
	}

	String marker = ".godot-trenchbroom-project";
	bool replacing = DirAccess::dir_exists_absolute(destination);
	Ref<DirAccess> parent = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (parent->is_link(destination) || (replacing && (!FileAccess::exists(destination.path_join(marker)) || FileAccess::get_file_as_string(destination.path_join(marker)) != project_path()))) {
		return failure(vformat("Refusing to replace an export directory not owned by this project: %s", destination));
	}
	String staging = destination + ".staging-" + itos(OS::get_singleton()->get_ticks_usec());
	String backup = staging + ".previous";
	if (DirAccess::dir_exists_absolute(staging) || DirAccess::dir_exists_absolute(backup)) {
		return failure("An export is already using the staging directory. Try again.");
	}
	Error error = DirAccess::make_dir_recursive_absolute(staging.path_join("textures"));
	if (error != OK) {
		return failure(vformat("Cannot create export staging directory: %s", staging));
	}
	for (const KeyValue<String, String> &file : files) {
		String target = staging.path_join(file.key);
		error = DirAccess::make_dir_recursive_absolute(target.get_base_dir());
		if (error == OK) {
			error = DirAccess::copy_absolute(file.value, target);
		}
		if (error != OK) {
			remove_tree(staging);
			return failure(vformat("Failed to copy %s into the export. Previous export preserved.", file.value));
		}
	}
	if (icon.is_valid()) {
		error = icon->save_png(staging.path_join("icon.png"));
	}
	if (error == OK) {
		error = write_text(staging.path_join("Entities.fgd"), fgd);
	}
	if (error == OK) {
		error = write_text(staging.path_join(marker), project_path());
	}
	if (error == OK) {
		error = write_text(staging.path_join("GameConfig.cfg"), JSON::stringify(config, "\t") + "\n");
	}
	if (error != OK) {
		remove_tree(staging);
		return failure("Failed to write game configuration files. Previous export preserved.");
	}
	if (replacing && DirAccess::rename_absolute(destination, backup) != OK) {
		remove_tree(staging);
		return failure("Cannot replace the previous export. Close files using it and try again.");
	}
	if (DirAccess::rename_absolute(staging, destination) != OK) {
		if (replacing) {
			DirAccess::rename_absolute(backup, destination);
		}
		remove_tree(staging);
		return failure(vformat("Cannot install the export. Check %s and %s.", destination, backup));
	}
	String cleanup_warning;
	if (replacing && remove_tree(backup) != OK) {
		cleanup_warning = vformat("\nCould not remove the previous export backup: %s", backup);
	}
	Dictionary result;
	result["success"] = true;
	result["directory"] = destination;
	result["message"] = vformat("Exported %d entities and %d textures.\nSelect '%s' in TrenchBroom and set its Game Path to:\n%s\nKeep authored maps outside this generated directory.%s", definitions.size(), material_names.size(), game_name(), destination, cleanup_warning);
	return result;
}

void TrenchBroomGameConfigExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_export_directory", "parent_directory"), &TrenchBroomGameConfigExporter::get_export_directory);
	ClassDB::bind_method(D_METHOD("export_game_config", "parent_directory"), &TrenchBroomGameConfigExporter::export_game_config);
}
