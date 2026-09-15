#!/usr/bin/env python3
"""Run with: python tests/editor/test_trenchbroom_map_import.py --engine <editor executable>."""

import argparse
import subprocess
import tempfile
from pathlib import Path

SCRIPT = r"""@tool
extends EditorPlugin

func _enter_tree() -> void:
	call_deferred("run")

func check(condition: bool, message: String) -> void:
	if not condition:
		printerr("FAILED: " + message)
		get_tree().quit(1)
		assert(condition, message)

func run() -> void:
	await get_tree().process_frame
	while EditorInterface.get_resource_filesystem().is_scanning():
		await get_tree().process_frame
	var packed: PackedScene = load("res://standard.map")
	check(packed != null, "Map imports as PackedScene")
	var scene := packed.instantiate()
	check(scene.get_child_count() == 3, "World, point entity, and brush entity assembled")
	var world_mesh: MeshInstance3D = scene.get_node("World_0/Brush_0/Mesh")
	check(world_mesh.mesh.get_surface_count() == 6, "Six brush faces")
	check(world_mesh.mesh.surface_get_material(0).resource_path == "res://materials/stone.tres", "Configured material overrides texture")
	var arrays := world_mesh.mesh.surface_get_arrays(0)
	check(arrays[Mesh.ARRAY_TANGENT].size() == 16, "Tangents generated through scene pipeline")
	var positions: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var uvs: PackedVector2Array = arrays[Mesh.ARRAY_TEX_UV]
	for i in positions.size():
		check(uvs[i].distance_to(Vector2(positions[i].z * 16, -positions[i].x * 8)) < 0.001, "UVs use actual 2x4 texture size: " + str(positions[i]) + " " + str(uvs[i]))
	var collision: CollisionShape3D = scene.get_node("World_0/Brush_0/Body/Collision")
	check(collision.shape is ConvexPolygonShape3D, "Convex collider imported")
	check(collision.shape.points.size() == 8, "Eight collision vertices")
	check(collision.shape.margin == 0, "Brush collision margin preserved")
	var entity: Entity3D = scene.get_node("ent_test_1")
	check(entity.definition.classname == "ent_test", "Definition resolved")
	check(entity.position.is_equal_approx(Vector3(1, 1.5, 0.5)), "Entity origin converted")
	check(entity.basis.x.is_equal_approx(Vector3(0, 0, 1)), "Yaw converted")
	check(entity.entity_properties.team == 2, "Enum integer preserved")
	check(entity.entity_properties.custom == "keep me", "Unknown properties retained")
	check(entity.get_meta("map_properties").team == "2", "Raw properties retained")
	add_child(scene)
	var generated := entity.get_child(0, true)
	check(generated.get("team") == 2, "Team applied to spawned root")
	check(generated.get_meta("team_on_enter") == 2, "Team applied before entering tree")
	check(entity.get_child_count() == 0, "Definition child remains internal")
	var brush_mesh: MeshInstance3D = scene.get_node("func_detail_2/Brush_0/Mesh")
	check(brush_mesh.global_transform.is_equal_approx(Transform3D.IDENTITY), "Brush world geometry survives entity origin and rotation")
	var brush_shape: CollisionShape3D = scene.get_node("func_detail_2/Brush_0/Body/Collision")
	check(brush_shape.global_transform.is_equal_approx(brush_mesh.global_transform), "Mesh and collision transforms agree")
	var properties := entity.entity_properties
	properties.team = 1
	entity.entity_properties = properties
	await get_tree().process_frame
	check(entity.get_child(0, true).get("team") == 1, "Property changes rebuild placement")
	entity.entity_properties = {}
	await get_tree().process_frame
	check(entity.get_child(0, true).get("team") == 0, "Omitted enum uses definition default")
	remove_child(scene)
	scene.free()
	var valve: PackedScene = load("res://valve.map")
	check(valve != null, "Valve format auto detected")
	var valve_scene := valve.instantiate()
	check(valve_scene.get_node("World_0/Brush_0/Mesh").mesh.get_surface_count() == 6, "Valve geometry assembled")
	valve_scene.free()
	var source := FileAccess.get_file_as_string("res://standard.map")
	source = source.replace('"team" "2"', '"team" "1"').replace('"16 32 48"', '"128 32 48"')
	var brush_start := source.find("{\n(") + 2
	var brush_end := source.find("\n}", brush_start)
	var original_brush := source.substr(brush_start, brush_end - brush_start)
	source = source.replace(original_brush, original_brush.replace("32", "64"))
	var file := FileAccess.open("res://standard.map", FileAccess.WRITE)
	file.store_string(source)
	file.close()
	var filesystem := EditorInterface.get_resource_filesystem()
	filesystem.scan()
	await filesystem.resources_reimported
	var updated: PackedScene = ResourceLoader.load("res://standard.map", "", ResourceLoader.CACHE_MODE_IGNORE)
	var updated_scene := updated.instantiate()
	check(updated_scene.get_node("ent_test_1").entity_properties.team == 1, "Map edit reimports enum")
	check(updated_scene.get_node("ent_test_1").position.is_equal_approx(Vector3(1, 1.5, 4)), "Map edit reimports origin")
	var updated_mesh: MeshInstance3D = updated_scene.get_node("World_0/Brush_0/Mesh")
	check(updated_mesh.mesh.get_aabb().size.distance_to(Vector3(2, 2, 2)) < 0.001, "Map edit recompiles brush geometry")
	updated_scene.free()
	print("TrenchBroom map import checks passed")
	get_tree().quit()
"""

BRUSH = r"""( 0 0 0 ) ( 0 32 0 ) ( 32 32 0 ) stone 0 0 0 1 1
( 0 0 32 ) ( 32 32 32 ) ( 0 32 32 ) stone 0 0 0 1 1
( 0 0 0 ) ( 32 0 32 ) ( 0 0 32 ) stone 0 0 0 1 1
( 0 32 0 ) ( 0 32 32 ) ( 32 32 32 ) stone 0 0 0 1 1
( 0 0 0 ) ( 0 0 32 ) ( 0 32 32 ) stone 0 0 0 1 1
( 32 0 0 ) ( 32 32 32 ) ( 32 0 32 ) stone 0 0 0 1 1"""


FORCE_IMPORT = r"""@tool
extends EditorPlugin

func _enter_tree() -> void:
	call_deferred("run")

func run() -> void:
	await get_tree().process_frame
	while EditorInterface.get_resource_filesystem().is_scanning():
		await get_tree().process_frame
	EditorInterface.get_resource_filesystem().reimport_files(PackedStringArray(["res://standard.map"]))
	print("TrenchBroom manual reimport finished")
	get_tree().quit()
"""

RUNTIME = r"""extends SceneTree

func _initialize() -> void:
	call_deferred("run")

func check(condition: bool, message: String) -> void:
	if not condition:
		printerr("FAILED: " + message)
		quit(1)
		assert(condition, message)

func run() -> void:
	var packed: PackedScene = load("res://standard.map")
	var scene := packed.instantiate()
	root.add_child(scene)
	var entity: Entity3D = scene.get_node("ent_test_1")
	check(entity.get_child(0, true).get("team") == 1, "Runtime entity has reimported Team")
	check(entity.get_child(0, true).get_meta("team_on_enter") == 1, "Runtime Team applied before tree entry")
	await physics_frame
	await process_frame
	var query := PhysicsRayQueryParameters3D.create(Vector3(0.5, 3, 0.5), Vector3(0.5, -1, 0.5))
	var hit: Dictionary = scene.get_world_3d().direct_space_state.intersect_ray(query)
	check(not hit.is_empty(), "Runtime brush collision raycast hits")
	check(hit.position.distance_to(Vector3(0.5, 2, 0.5)) < 0.001, "Runtime collision matches brush top")
	scene.free()
	print("TrenchBroom runtime scene checks passed")
	quit()
"""


def run_engine(engine, project, *arguments):
    return subprocess.run(
        [str(engine.resolve()), "--headless", "--path", str(project), *arguments],
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=60,
    )


def require_success(result, marker=None):
    if (
        result.returncode
        or "ERROR:" in result.stderr
        or "WARNING:" in result.stderr
        or (marker and marker not in result.stdout)
    ):
        print(result.stdout, end="")
        print(result.stderr, end="")
        raise SystemExit(result.returncode or "Test did not finish successfully")
    if marker:
        print(marker)


def png(width, height):
    import struct
    import zlib

    def chunk(name, data):
        return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name + data))

    pixels = b"".join(b"\x00" + b"\xff\x00\x00\xff" * width for _ in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(pixels))
        + chunk(b"IEND", b"")
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="godot-trenchbroom-import-") as directory:
        project = Path(directory)
        (project / "project.godot").write_text(
            'config_version=5\n[trenchbroom]\ngeneral/entity_definitions=PackedStringArray("res://entity.tres")\ngeneral/texture_source_directories=PackedStringArray("res://textures")\ngeneral/materials_output_directory="res://materials"\n[editor_plugins]\nenabled=PackedStringArray("res://addons/import_test/plugin.cfg")\n',
            encoding="utf-8",
        )
        (project / "textures").mkdir()
        (project / "materials").mkdir()
        (project / "textures/stone.png").write_bytes(png(2, 4))
        (project / "materials/stone.tres").write_text(
            '[gd_resource type="StandardMaterial3D" load_steps=2 format=3]\n[ext_resource type="Texture2D" path="res://textures/stone.png" id="1"]\n[resource]\nalbedo_texture=ExtResource("1")\n',
            encoding="utf-8",
        )
        (project / "entity.gd").write_text(
            '@tool\nextends Node3D\n@export var team: int = 99\nfunc _enter_tree() -> void:\n\tset_meta("team_on_enter", team)\n',
            encoding="utf-8",
        )
        (project / "entity.tscn").write_text(
            '[gd_scene load_steps=2 format=3]\n[ext_resource type="Script" path="res://entity.gd" id="1"]\n[node name="Entity" type="Node3D"]\nscript=ExtResource("1")\n',
            encoding="utf-8",
        )
        (project / "entity.tres").write_text(
            '[gd_resource type="EntityDefinition" load_steps=3 format=3]\n[ext_resource type="PackedScene" path="res://entity.tscn" id="1"]\n[sub_resource type="EntityEnumProperty" id="Team"]\nkey="team"\nchoices=Dictionary[String, int]({"None": 0, "Good": 1, "Evil": 2})\n[resource]\nclassname="ent_test"\nscene=ExtResource("1")\nenum_properties=Array[EntityEnumProperty]([SubResource("Team")])\n',
            encoding="utf-8",
        )
        source = (
            '{\n"classname" "worldspawn"\n{\n'
            + BRUSH
            + '\n}\n}\n{\n"classname" "ent_test"\n"origin" "16 32 48"\n"angle" "90"\n"team" "2"\n"custom" "keep me"\n}\n{\n"classname" "func_detail"\n"origin" "16 16 16"\n"angle" "90"\n{\n'
            + BRUSH
            + "\n}\n}\n"
        )
        (project / "standard.map").write_text(source, encoding="utf-8")
        axes = ["[ 1 0 0 0 ] [ 0 -1 0 0 ]"] * 2 + ["[ 1 0 0 0 ] [ 0 0 -1 0 ]"] * 2 + ["[ 0 1 0 0 ] [ 0 0 -1 0 ]"] * 2
        valve_brush = "\n".join(
            line.split("stone")[0] + "stone " + axis + " 17 1 1" for line, axis in zip(BRUSH.splitlines(), axes)
        )
        (project / "valve.map").write_text(
            '{\n"classname" "worldspawn"\n{\n' + valve_brush + "\n}\n}\n", encoding="utf-8"
        )
        addon = project / "addons/import_test"
        addon.mkdir(parents=True)
        (addon / "plugin.cfg").write_text(
            '[plugin]\nname="Map Import Test"\ndescription=""\nauthor=""\nversion="1"\nscript="test.gd"\n',
            encoding="utf-8",
        )
        (addon / "test.gd").write_text(SCRIPT, encoding="utf-8")
        require_success(run_engine(args.engine, project, "--editor"), "TrenchBroom map import checks passed")
        (project / "runtime.gd").write_text(RUNTIME, encoding="utf-8")
        require_success(
            run_engine(args.engine, project, "--script", "res://runtime.gd"), "TrenchBroom runtime scene checks passed"
        )
        (addon / "test.gd").write_text(FORCE_IMPORT, encoding="utf-8")
        valid_source = (project / "standard.map").read_text(encoding="utf-8")
        compiled = next((project / ".godot/imported").glob("standard.map-*.scn"))
        previous = compiled.read_bytes()
        failures = [
            (valid_source.replace('"128 32 48"', '"broken"'), "Invalid entity origin."),
            (valid_source.replace('"team" "1"', '"team" "9999999999999999999999999"'), "Integer out of range"),
            (valid_source.replace('"team" "1"', '"team" "9"'), "Unknown value for entity property"),
            (valid_source.replace(BRUSH.replace("32", "64").splitlines()[1] + "\n", ""), "Brush is open"),
            (valid_source + "\ntrailing", "column"),
            (valid_source.replace('"classname" "worldspawn"', '"classname" "not_world"'), "exactly one worldspawn"),
        ]
        for invalid_source, diagnostic in failures:
            (project / "standard.map").write_text(invalid_source, encoding="utf-8")
            result = run_engine(args.engine, project, "--editor")
            if (
                diagnostic not in result.stderr
                or "WARNING:" in result.stderr
                or "TrenchBroom manual reimport finished" not in result.stdout
            ):
                print(result.stdout, end="")
                print(result.stderr, end="")
                raise SystemExit("Missing expected import diagnostic: " + diagnostic)
            if compiled.read_bytes() != previous:
                raise SystemExit("Failed import replaced compiled scene")
        (project / "standard.map").write_text(valid_source, encoding="utf-8")
        require_success(run_engine(args.engine, project, "--editor"), "TrenchBroom manual reimport finished")
        require_success(
            run_engine(args.engine, project, "--script", "res://runtime.gd"), "TrenchBroom runtime scene checks passed"
        )
        print("TrenchBroom failed import and recovery checks passed")


if __name__ == "__main__":
    main()
