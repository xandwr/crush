/**************************************************************************/
/*  editor_importer_trenchbroom.cpp                                       */
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

#include "editor_importer_trenchbroom.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/templates/hash_set.h"
#include "editor/import/3d/trenchbroom_brush_compiler.h"
#include "scene/3d/entity_3d.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/surface_tool.h"

#include <locale>
#include <sstream>

namespace {
using Parser = TrenchBroomMapParser;
using Compiler = TrenchBroomBrushCompiler;

Variant option(const HashMap<StringName, Variant> &p_options, const StringName &p_name, const Variant &p_default) {
	const Variant *value = p_options.getptr(p_name);
	return value ? *value : p_default;
}
bool read_numbers(const String &p_value, int p_count, Vector3 &r_value) {
	PackedStringArray parts = p_value.split_spaces();
	if (parts.size() != p_count) {
		return false;
	}
	for (int i = 0; i < p_count; i++) {
		if (!parts[i].is_valid_float()) {
			return false;
		}
		std::istringstream stream(parts[i].utf8().get_data());
		stream.imbue(std::locale::classic());
		double value;
		stream >> value;
		if (stream.fail() || !stream.eof() || !Math::is_finite(value) || !Math::is_finite(real_t(value))) {
			return false;
		}
		r_value[i] = value;
	}
	return true;
}
bool valid_identifier(const String &p_name) {
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
Error collect_files(const String &p_root, const String &p_relative, HashMap<String, String> &r_files, const PackedStringArray &p_extensions, int p_depth = 0) {
	if (!p_root.begins_with("res://") || p_depth > 64) {
		return ERR_INVALID_PARAMETER;
	}
	Ref<DirAccess> directory = DirAccess::open(p_root.path_join(p_relative));
	if (directory.is_null()) {
		return ERR_CANT_OPEN;
	}
	for (const String &file : directory->get_files()) {
		if (!p_extensions.has(file.get_extension().to_lower())) {
			continue;
		}
		String name = p_relative.path_join(file).get_basename().to_lower();
		String path = p_root.path_join(p_relative).path_join(file);
		if (r_files.has(name) && r_files[name] != path) {
			ERR_PRINT(vformat("TrenchBroom asset name collision: %s", name));
			return ERR_ALREADY_EXISTS;
		}
		r_files[name] = path;
	}
	for (const String &child : directory->get_directories()) {
		if (child.begins_with(".")) {
			continue;
		}
		if (directory->is_link(child)) {
			return ERR_INVALID_PARAMETER;
		}
		Error error = collect_files(p_root, p_relative.path_join(child), r_files, p_extensions, p_depth + 1);
		if (error != OK) {
			return error;
		}
	}
	return OK;
}
void attach(Node *p_parent, Node *p_child, Node *p_owner) {
	p_parent->add_child(p_child, true);
	p_child->set_owner(p_owner);
}
} // namespace

void EditorTrenchBroomImporter::get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("map");
}
void EditorTrenchBroomImporter::get_import_options(const String &p_path, List<ResourceImporter::ImportOption> *r_options) {
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "trenchbroom/cull_interior_faces"), true));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "trenchbroom/format", PROPERTY_HINT_ENUM, "Auto,Standard,Valve 220"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "trenchbroom/map_units_per_meter", PROPERTY_HINT_RANGE, "0.001,1024,0.001,or_greater"), GLOBAL_GET("trenchbroom/entities/map_units_per_meter")));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "trenchbroom/entity_definitions", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:*.tres,*.res", Variant::STRING, PROPERTY_HINT_FILE)), GLOBAL_GET("trenchbroom/general/entity_definitions")));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "trenchbroom/texture_source_directories", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:", Variant::STRING, PROPERTY_HINT_DIR)), GLOBAL_GET("trenchbroom/general/texture_source_directories")));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::STRING, "trenchbroom/materials_directory", PROPERTY_HINT_DIR), GLOBAL_GET("trenchbroom/general/materials_output_directory")));
}

Node *EditorTrenchBroomImporter::import_scene(const String &p_path, uint32_t p_flags, const HashMap<StringName, Variant> &p_options, List<String> *r_missing_deps, Error *r_err) {
	Node3D *root = nullptr;
	auto fail = [&](Error p_error, const String &p_message, int p_line = 0) -> Node * {
		if (r_err) {
			*r_err = p_error;
		}
		ERR_PRINT(vformat("%s:%d: %s", p_path, p_line, p_message));
		if (root) {
			memdelete(root);
		}
		return nullptr;
	};
	Error error;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &error);
	if (file.is_null()) {
		return fail(error, "Cannot read map.");
	}
	String source = file->get_as_utf8_string();
	int format = option(p_options, "trenchbroom/format", 0);
	Parser::Map map;
	Parser::Diagnostic diagnostic;
	if (format < 0 || format > 2) {
		return fail(ERR_INVALID_PARAMETER, "Invalid map format option.");
	}
	error = Parser::parse(source, format == 2 ? Parser::VALVE_220 : Parser::STANDARD, map, diagnostic);
	if (error != OK && format == 0) {
		Parser::Diagnostic standard_diagnostic = diagnostic;
		error = Parser::parse(source, Parser::VALVE_220, map, diagnostic);
		if (error != OK && standard_diagnostic.line > diagnostic.line) {
			diagnostic = standard_diagnostic;
		}
	}
	if (error != OK) {
		return fail(error, vformat("%s (column %d)", diagnostic.message, diagnostic.column), diagnostic.line);
	}
	double units = option(p_options, "trenchbroom/map_units_per_meter", GLOBAL_GET("trenchbroom/entities/map_units_per_meter"));
	if (!Math::is_finite(units) || units <= 0 || !Math::is_finite(real_t(1.0 / units)) || real_t(1.0 / units) <= 0) {
		return fail(ERR_INVALID_PARAMETER, "Map units per meter must be positive and finite.");
	}
	Compiler::Options compiler_options;
	compiler_options.unit_scale = 1.0 / units;
	HashMap<String, Ref<EntityDefinition>> definitions;
	HashSet<String> classnames;
	classnames.insert("worldspawn");
	PackedStringArray definition_paths = option(p_options, "trenchbroom/entity_definitions", GLOBAL_GET("trenchbroom/general/entity_definitions"));
	for (const String &path : definition_paths) {
		Ref<EntityDefinition> definition = ResourceLoader::load(path);
		if (definition.is_null()) {
			if (r_missing_deps) {
				r_missing_deps->push_back(path);
			}
			return fail(ERR_CANT_OPEN, vformat("Cannot load entity definition: %s", path));
		}
		String classname = definition->get_classname();
		if (!valid_identifier(classname) || classnames.has(classname.to_lower())) {
			return fail(ERR_INVALID_DATA, vformat("Invalid, reserved, or duplicate entity classname: %s", classname));
		}
		if (definition->get_scene().is_null() || !definition->get_scene()->can_instantiate()) {
			return fail(ERR_INVALID_DATA, vformat("Entity definition requires a valid scene: %s", path));
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
			return fail(ERR_INVALID_DATA, vformat("Entity scene must have a Node3D root: %s", path));
		}
		classnames.insert(classname.to_lower());
		HashSet<String> keys;
		for (const Ref<EntityEnumProperty> property : definition->get_enum_properties()) {
			if (property.is_null()) {
				return fail(ERR_INVALID_DATA, vformat("Null enum property in %s.", path));
			}
			String key = property->get_key().to_lower();
			if (!valid_identifier(key) || keys.has(key) || key == "classname" || key == "origin" || key == "angle" || key == "angles" || key == "spawnflags") {
				return fail(ERR_INVALID_DATA, vformat("Invalid, reserved, or duplicate enum property '%s' in %s.", key, path));
			}
			keys.insert(key);
			HashSet<int64_t> values;
			TypedDictionary<String, int64_t> choices = property->get_choices();
			for (const Variant &label : choices.keys()) {
				int64_t value = choices[label];
				if (String(label).strip_edges().is_empty() || values.has(value)) {
					return fail(ERR_INVALID_DATA, vformat("Empty or duplicate enum choice in %s.", path));
				}
				values.insert(value);
			}
			if (!values.has(property->get_default_value())) {
				return fail(ERR_INVALID_DATA, vformat("Enum default has no matching choice in %s.", path));
			}
		}
		Node *scene_root = definition->get_scene()->instantiate(PackedScene::GEN_EDIT_STATE_DISABLED);
		if (!Object::cast_to<Node3D>(scene_root)) {
			if (scene_root) {
				memdelete(scene_root);
			}
			return fail(ERR_INVALID_DATA, vformat("Entity scene must instantiate a Node3D root: %s", path));
		}
		for (const Ref<EntityEnumProperty> property : definition->get_enum_properties()) {
			bool valid = false;
			Variant value = scene_root->get(property->get_key(), &valid);
			if (!valid || value.get_type() != Variant::INT) {
				memdelete(scene_root);
				return fail(ERR_INVALID_DATA, vformat("Entity scene root requires an integer '%s' property: %s", property->get_key(), path));
			}
		}
		memdelete(scene_root);
		definitions[classname] = definition;
	}
	HashMap<String, String> textures;
	HashMap<String, String> materials;
	PackedStringArray texture_roots = option(p_options, "trenchbroom/texture_source_directories", GLOBAL_GET("trenchbroom/general/texture_source_directories"));
	for (const String &path : texture_roots) {
		error = collect_files(path, "", textures, PackedStringArray({ "bmp", "exr", "hdr", "jpeg", "jpg", "png", "tga", "webp" }));
		if (error != OK) {
			return fail(error, vformat("Cannot catalog textures: %s", path));
		}
	}
	String material_root = option(p_options, "trenchbroom/materials_directory", GLOBAL_GET("trenchbroom/general/materials_output_directory"));
	if (!material_root.is_empty()) {
		error = collect_files(material_root, "", materials, PackedStringArray({ "tres", "res" }));
		if (error != OK) {
			return fail(error, vformat("Cannot catalog materials: %s", material_root));
		}
	}
	for (const auto &entity : map.entities) {
		for (const auto &brush : entity.brushes) {
			for (const auto &face : brush.faces) {
				if (compiler_options.materials.has(face.material) || PackedStringArray({ "clip", "skip", "origin" }).has(face.material.to_lower())) {
					continue;
				}
				Compiler::MaterialInfo info;
				String key = face.material.to_lower();
				if (materials.has(key)) {
					info.material = ResourceLoader::load(materials[key]);
					if (info.material.is_null()) {
						return fail(ERR_INVALID_DATA, vformat("Not a material: %s", materials[key]), face.line);
					}
					Ref<BaseMaterial3D> base = info.material;
					if (base.is_valid() && base->get_texture(BaseMaterial3D::TEXTURE_ALBEDO).is_valid()) {
						info.texture_size = base->get_texture(BaseMaterial3D::TEXTURE_ALBEDO)->get_size();
					}
				} else if (textures.has(key)) {
					Ref<Texture2D> texture = ResourceLoader::load(textures[key]);
					if (texture.is_null()) {
						return fail(ERR_CANT_OPEN, vformat("Cannot load texture: %s", textures[key]), face.line);
					}
					Ref<StandardMaterial3D> material;
					material.instantiate();
					material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
					material->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
					material->set_name(face.material);
					info.material = material;
					info.texture_size = texture->get_size();
				}
				compiler_options.materials[face.material] = info;
			}
		}
	}
	int worldspawn_count = 0;
	for (int i = 0; i < map.entities.size(); i++) {
		const Parser::Entity &entity = map.entities[i];
		if (!entity.properties.has("classname") || !valid_identifier(entity.properties["classname"])) {
			return fail(ERR_INVALID_DATA, "Entity requires a valid classname.", entity.line);
		}
		if (entity.properties["classname"] == "worldspawn") {
			worldspawn_count++;
			if (i != 0 || entity.properties.has("origin") || entity.properties.has("angle") || entity.properties.has("angles")) {
				return fail(ERR_INVALID_DATA, "worldspawn must be the first entity and cannot have a transform.", entity.line);
			}
		}
	}
	if (worldspawn_count != 1) {
		return fail(ERR_INVALID_DATA, "Map requires exactly one worldspawn entity.");
	}
	root = memnew(Node3D);
	root->set_name(p_path.get_file().get_basename().validate_node_name());
	for (int i = 0; i < map.entities.size(); i++) {
		const Parser::Entity &entity = map.entities[i];
		String classname = entity.properties.has("classname") ? entity.properties["classname"] : "unknown";
		Dictionary raw_properties;
		for (const auto &property : entity.properties) {
			raw_properties[property.key] = property.value;
		}
		Vector3 origin;
		if (entity.properties.has("origin") && !read_numbers(entity.properties["origin"], 3, origin)) {
			return fail(ERR_INVALID_DATA, "Invalid entity origin.", entity.line);
		}
		Vector<Compiler::Result> brushes;
		bool has_origin_brush = false;
		for (const auto &brush : entity.brushes) {
			Compiler::Result compiled;
			error = Compiler::compile(brush, map.format, compiler_options, compiled, diagnostic);
			if (error != OK) {
				return fail(error, diagnostic.message, diagnostic.line);
			}
			if (compiled.origin) {
				if (classname == "worldspawn" || has_origin_brush) {
					return fail(ERR_INVALID_DATA, "Origin brushes require a non-world entity and must be unique per entity.", brush.line);
				}
				has_origin_brush = true;
				origin = compiled.origin_center;
			}
			brushes.push_back(compiled);
		}
		if (option(p_options, "trenchbroom/cull_interior_faces", true)) {
			Compiler::cull_interior_faces(brushes, compiler_options.tolerance * compiler_options.unit_scale);
		}
		Basis orientation;
		if (entity.properties.has("angles")) {
			Vector3 angles;
			if (!read_numbers(entity.properties["angles"], 3, angles)) {
				return fail(ERR_INVALID_DATA, "Invalid entity angles.", entity.line);
			}
			orientation = Basis::from_euler(Vector3(
					Math::deg_to_rad(Math::fmod(-angles.x, real_t(360))),
					Math::deg_to_rad(Math::fmod(angles.y + 180, real_t(360))),
					Math::deg_to_rad(Math::fmod(-angles.z, real_t(360)))));
		} else if (entity.properties.has("angle")) {
			Vector3 angle;
			if (!read_numbers(entity.properties["angle"], 1, angle)) {
				return fail(ERR_INVALID_DATA, "Invalid entity angle.", entity.line);
			}
			if (angle.x == -1 || angle.x == -2) {
				orientation = Basis::from_euler(Vector3(angle.x == -1 ? Math::PI / 2 : -Math::PI / 2, Math::PI, 0));
			} else {
				orientation = Basis(Vector3(0, 1, 0), Math::deg_to_rad(Math::fmod(angle.x + 180, real_t(360))));
			}
		}
		Vector3 position = Vector3(origin.y, origin.z, origin.x) * compiler_options.unit_scale;
		if (!position.is_finite()) {
			return fail(ERR_INVALID_DATA, "Entity origin exceeds numeric range after scaling.", entity.line);
		}
		Node3D *placement;
		if (classname == "worldspawn") {
			placement = memnew(Node3D);
			placement->set_name(vformat("World_%d", i));
		} else {
			Entity3D *entity_node = memnew(Entity3D);
			placement = entity_node;
			placement->set_name(vformat("%s_%d", classname.validate_node_name(), i));
			Dictionary values = raw_properties.duplicate();
			if (definitions.has(classname)) {
				Ref<EntityDefinition> definition = definitions[classname];
				TypedArray<EntityEnumProperty> properties = definition->get_enum_properties();
				for (int j = 0; j < properties.size(); j++) {
					Ref<EntityEnumProperty> property = properties[j];
					if (property.is_null()) {
						memdelete(placement);
						return fail(ERR_INVALID_DATA, "Null enum property in entity definition.", entity.line);
					}
					String key = property->get_key();
					int64_t value = property->get_default_value();
					if (entity.properties.has(key)) {
						String text = entity.properties[key];
						if (!text.is_valid_int()) {
							memdelete(placement);
							return fail(ERR_INVALID_DATA, vformat("Invalid integer for entity property '%s'.", key), entity.line);
						}
						std::istringstream stream(text.utf8().get_data());
						stream.imbue(std::locale::classic());
						stream >> value;
						if (stream.fail() || !stream.eof()) {
							memdelete(placement);
							return fail(ERR_INVALID_DATA, vformat("Integer out of range for entity property '%s'.", key), entity.line);
						}
					}
					bool valid = false;
					TypedDictionary<String, int64_t> choices = property->get_choices();
					for (const Variant &label : choices.keys()) {
						valid |= int64_t(choices[label]) == value;
					}
					if (!valid) {
						memdelete(placement);
						return fail(ERR_INVALID_DATA, vformat("Unknown value for entity property '%s'.", key), entity.line);
					}
					values[key] = value;
				}
				entity_node->set_definition(definition);
			}
			entity_node->set_entity_properties(values);
		}
		placement->set_meta("map_properties", raw_properties);
		placement->set_position(position);
		placement->set_basis(orientation);
		attach(root, placement, root);
		for (int j = 0; j < entity.brushes.size(); j++) {
			const Compiler::Result &compiled = brushes[j];
			if (compiled.origin) {
				continue;
			}
			Node3D *brush_node = memnew(Node3D);
			brush_node->set_name(vformat("Brush_%d", j));
			brush_node->set_transform(placement->get_transform().affine_inverse());
			attach(placement, brush_node, root);
			Ref<ImporterMesh> import_mesh;
			import_mesh.instantiate();
			for (int surface = 0; surface < compiled.mesh->get_surface_count(); surface++) {
				Array arrays = compiled.mesh->surface_get_arrays(surface);
				if (p_flags & IMPORT_GENERATE_TANGENT_ARRAYS) {
					Ref<SurfaceTool> tool;
					tool.instantiate();
					tool->create_from_triangle_arrays(arrays);
					tool->generate_tangents();
					arrays = tool->commit_to_arrays();
				}
				import_mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, arrays, TypedArray<Array>(), Dictionary(), compiled.mesh->surface_get_material(surface), compiled.mesh->surface_get_name(surface), p_flags & IMPORT_FORCE_DISABLE_MESH_COMPRESSION ? 0 : Mesh::ARRAY_FLAG_COMPRESS_ATTRIBUTES);
			}
			if (compiled.mesh->get_surface_count() > 0) {
				ImporterMeshInstance3D *mesh = memnew(ImporterMeshInstance3D);
				mesh->set_name("Mesh");
				mesh->set_mesh(import_mesh);
				attach(brush_node, mesh, root);
			}
			StaticBody3D *body = memnew(StaticBody3D);
			body->set_name("Body");
			attach(brush_node, body, root);
			CollisionShape3D *shape = memnew(CollisionShape3D);
			shape->set_name("Collision");
			shape->set_shape(compiled.collision);
			attach(body, shape, root);
		}
	}
	if (r_err) {
		*r_err = OK;
	}
	return root;
}
