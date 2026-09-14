#!/usr/bin/env python3
"""Run with: python tests/editor/test_trenchbroom_export.py --engine <editor executable>."""

import argparse
from pathlib import Path
import subprocess
import tempfile


SCRIPT = r"""@tool
extends EditorPlugin

func _enter_tree() -> void:
	call_deferred("wait_for_scan")

func wait_for_scan() -> void:
	await get_tree().process_frame
	while EditorInterface.get_resource_filesystem().is_scanning():
		await get_tree().process_frame
	run()

func check(condition: bool, message: String) -> void:
	if not condition:
		printerr("FAILED: " + message)
		get_tree().quit(1)
		assert(condition, message)

func save_text(path: String, content: String) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(content)

func read_config(path: String) -> Dictionary:
	return JSON.parse_string(FileAccess.get_file_as_string(path.path_join("GameConfig.cfg")))

func run() -> void:
	var exporter = ClassDB.instantiate("TrenchBroomGameConfigExporter")
	check(exporter != null, "Exporter is registered")
	var export_root := ProjectSettings.globalize_path("res://exports")
	ProjectSettings.set_setting("application/config/name", "Export Test")
	ProjectSettings.set_setting("application/config/icon", "")
	check(not exporter.export_game_config("").success, "Empty export path rejected")
	check(not exporter.export_game_config("relative/path").success, "Relative export path rejected")
	DirAccess.make_dir_recursive_absolute("res://source_a/stone")
	DirAccess.make_dir_recursive_absolute("res://source_b/metal")
	var image := Image.create_empty(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(Color.RED)
	check(image.save_png("res://source_a/stone/red.png") == OK, "First texture saved")
	check(image.save_png("res://source_b/metal/red.png") == OK, "Second texture saved")
	check(image.save_png("res://icon.png") == OK, "Icon saved")
	ProjectSettings.set_setting("application/config/icon", "res://icon.png")
	ProjectSettings.set_setting("trenchbroom/general/texture_source_directories", PackedStringArray(["res://source_a", "res://source_b"]))
	var scene := PackedScene.new()
	var source := Node3D.new()
	check(scene.pack(source) == OK, "Scene packed")
	source.free()
	check(ResourceSaver.save(scene, "res://entity.tscn") == OK, "Scene saved")
	var definition := EntityDefinition.new()
	definition.classname = "ent_test"
	definition.description = "Quote \" / backslash \\ / newline\ntext"
	definition.scene = scene
	check(ResourceSaver.save(definition, "res://entity.tres") == OK, "Definition saved")
	ProjectSettings.set_setting("trenchbroom/general/entity_definitions", PackedStringArray(["res://entity.tres"]))
	var result: Dictionary = exporter.export_game_config(export_root)
	check(result.success, str(result))
	var directory: String = result.directory
	var config := read_config(directory)
	check(config.version == 9, "Latest schema is 9")
	check(config.name == "Export Test", "Project name fallback")
	check(config.fileformats.size() == 4, "Four map formats")
	check(not config.materials.has("palette"), "Unset palette omitted")
	check(config.tags.brush.is_empty(), "No default brush tags")
	check(config.tags.brushface.size() == 3, "Three face tags")
	check(config.tags.brushface[0].match == "material", "Version 9 tag terminology")
	check(config.materials.excludes.size() == 9, "Default exclusions exported")
	check(FileAccess.file_exists(directory.path_join("textures/stone/red.png")), "First texture copied")
	check(FileAccess.file_exists(directory.path_join("textures/metal/red.png")), "Second texture copied")
	check(Image.load_from_file(directory.path_join("icon.png")).get_size() == Vector2i(32, 32), "Icon resized")
	var fgd := FileAccess.get_file_as_string(directory.path_join("Entities.fgd"))
	check(fgd.contains("@PointClass size(-16.0 -16.0 -16.0, 16.0 16.0 16.0)"), "Box uses map units")
	check(fgd.contains("= ent_test :"), "Entity exported")
	check(fgd.contains('Quote \\"'), "FGD quotes escaped")
	var before := FileAccess.get_file_as_string(directory.path_join("GameConfig.cfg"))
	ProjectSettings.set_setting("trenchbroom/general/entity_definitions", PackedStringArray(["res://entity.tres", "res://entity.tres"]))
	check(not exporter.export_game_config(export_root).success, "Duplicate classname rejected")
	check(FileAccess.get_file_as_string(directory.path_join("GameConfig.cfg")) == before, "Failed export preserves config")
	ProjectSettings.set_setting("trenchbroom/general/entity_definitions", PackedStringArray(["res://entity.tres"]))
	DirAccess.make_dir_recursive_absolute("res://source_b/stone")
	image.save_png("res://source_b/stone/red.png")
	check(not exporter.export_game_config(export_root).success, "Material collision rejected")
	DirAccess.remove_absolute("res://source_b/stone/red.png")
	ProjectSettings.set_setting("trenchbroom/textures/palette", "res://missing.lmp")
	check(not exporter.export_game_config(export_root).success, "Missing palette rejected")
	save_text("res://palette.lmp", "palette fixture")
	ProjectSettings.set_setting("trenchbroom/textures/palette", "res://palette.lmp")
	save_text("res://start.map", '{\n"classname" "worldspawn"\n}\n')
	ProjectSettings.set_setting("trenchbroom/map_formats/formats", [{"format": "Valve", "initial_map": "res://start.map"}])
	ProjectSettings.set_setting("trenchbroom/tags/brush_tags", [{"name": "Trigger", "pattern": "trigger*", "transparent": true, "material": "trigger"}])
	for version in [4, 8, 9]:
		ProjectSettings.set_setting("trenchbroom/compatibility/game_config_version", version)
		result = exporter.export_game_config(export_root)
		check(result.success, str(result))
		config = read_config(directory)
		var texture_key := "materials" if version == 9 else "textures"
		check(config.version == version, "Explicit schema exported")
		check(config[texture_key].palette == "palette.lmp", "Palette relative to exported game")
		check(FileAccess.file_exists(directory.path_join(config.fileformats[0].initialmap)), "Initial map copied")
		check(config.tags.brush[0].match == "classname", "Brush tag match")
		check(config.tags.brush[0].get("material" if version == 9 else "texture") == "trigger", "Brush tag material")
	ProjectSettings.set_setting("trenchbroom/general/texture_source_directories", PackedStringArray(["res://source_a"]))
	check(exporter.export_game_config(export_root).success, "Re-export succeeds")
	check(not FileAccess.file_exists(directory.path_join("textures/metal/red.png")), "Re-export removes stale copied textures")
	ProjectSettings.set_setting("trenchbroom/entities/map_units_per_meter", 0.0)
	check(not exporter.export_game_config(export_root).success, "Invalid scale rejected")
	ProjectSettings.set_setting("trenchbroom/entities/map_units_per_meter", 32.0)
	save_text(directory.path_join(".godot-trenchbroom-project"), "another project")
	check(not exporter.export_game_config(export_root).success, "Unowned directory preserved")
	check(FileAccess.get_file_as_string(directory.path_join(".godot-trenchbroom-project")) == "another project", "Ownership marker unchanged")
	check(ProjectSettings.save() == OK, "Settings saved")
	var settings := ConfigFile.new()
	check(settings.load("res://project.godot") == OK, "Settings reloaded")
	check(settings.get_value("trenchbroom", "map_formats/formats")[0].format == "Valve", "Structured map settings round trip")
	EditorInterface.inspect_object(ProjectSettings)
	await get_tree().process_frame
	var editors := EditorInterface.get_inspector().find_children("*", "EditorPropertyTrenchBroomList", true, false)
	check(editors.size() == 3, "Structured settings editors created")
	var map_editor: EditorProperty
	for property_editor in editors:
		if property_editor.get_edited_property() == "trenchbroom/map_formats/formats":
			map_editor = property_editor
	check(map_editor != null, "Map format editor found")
	if map_editor:
		var options := map_editor.find_children("*", "OptionButton", true, false).filter(func(option: OptionButton) -> bool: return option.item_count == 4 and option.get_item_text(0) == "Valve")
		check(options.size() == 1, "Map format dropdown created")
		options[0].select(1)
		options[0].emit_signal("item_selected", 1)
		await get_tree().process_frame
		check(ProjectSettings.get_setting("trenchbroom/map_formats/formats")[0].format == "Standard", "Dropdown updates settings")
	var docks := EditorInterface.get_base_control().find_children("*", "TrenchBroomDock", true, false)
	check(docks.size() == 1, "TrenchBroom dock installed")
	print("TrenchBroom export checks passed")
	get_tree().quit()
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="godot-trenchbroom-test-") as directory:
        project = Path(directory)
        (project / "project.godot").write_text(
            'config_version=5\n[editor_plugins]\nenabled=PackedStringArray("res://addons/export_test/plugin.cfg")\n',
            encoding="utf-8",
        )
        addon = project / "addons" / "export_test"
        addon.mkdir(parents=True)
        (addon / "plugin.cfg").write_text(
            '[plugin]\nname="Export Test"\ndescription=""\nauthor=""\nversion="1"\nscript="test.gd"\n', encoding="utf-8"
        )
        (addon / "test.gd").write_text(SCRIPT, encoding="utf-8")
        result = subprocess.run(
            [str(args.engine.resolve()), "--headless", "--editor", "--path", directory],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=60,
        )
        print(result.stdout, end="")
        print(result.stderr, end="")
        if result.returncode or "ERROR:" in result.stderr or "WARNING:" in result.stderr:
            raise SystemExit(result.returncode or 1)
        if "TrenchBroom export checks passed" not in result.stdout:
            raise SystemExit("Exporter test did not finish")


if __name__ == "__main__":
    main()
