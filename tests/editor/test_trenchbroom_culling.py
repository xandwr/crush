#!/usr/bin/env python3
"""Run with: python tests/editor/test_trenchbroom_culling.py --engine <editor executable>."""

import argparse
from pathlib import Path
import re
import tempfile

from test_trenchbroom_map_import import BRUSH, require_success, run_engine


SCRIPT = r'''@tool
extends EditorPlugin

func _enter_tree() -> void:
	call_deferred("run")

func check(condition: bool, message: String) -> void:
	if not condition:
		printerr("FAILED: " + message)
		get_tree().quit(1)
		assert(condition, message)

func inspect_map(name: String, counts: Array, collision_vertices: int = 8) -> Array:
	var packed: PackedScene = ResourceLoader.load("res://" + name + ".map", "", ResourceLoader.CACHE_MODE_IGNORE)
	check(packed != null, name + " imports")
	var scene := packed.instantiate()
	var brushes := scene.find_children("Brush_*", "Node3D", true, false)
	check(brushes.size() == counts.size(), name + " preserves brush nodes")
	var snapshots: Array = []
	for index in brushes.size():
		var brush: Node3D = brushes[index]
		var mesh_node := brush.get_node_or_null("Mesh") as MeshInstance3D
		var count := mesh_node.mesh.get_surface_count() if mesh_node != null else 0
		check(count == counts[index], name + " brush " + str(index) + " faces: " + str(count))
		var shape: ConvexPolygonShape3D = brush.get_node("Body/Collision").shape
		check(shape.points.size() == collision_vertices, name + " preserves collision vertices")
		check(shape.margin == 0, name + " preserves collision margin")
		var surfaces: Array = []
		for face in count:
			var arrays: Array = mesh_node.mesh.surface_get_arrays(face)
			check(arrays[Mesh.ARRAY_TANGENT].size() > 0, name + " keeps tangents")
			surfaces.append(arrays)
		snapshots.append([surfaces, shape.points])
	scene.free()
	return snapshots

func run() -> void:
	await get_tree().process_frame
	while EditorInterface.get_resource_filesystem().is_scanning():
		await get_tree().process_frame
	var culled := inspect_map("adjacent", [5, 5])
	inspect_map("overlap", [5, 5])
	inspect_map("partial", [6, 6])
	inspect_map("gap", [6, 6])
	inspect_map("duplicate", [6, 6])
	inspect_map("contained", [6, 0])
	inspect_map("contained_reversed", [0, 6])
	inspect_map("slopes", [4, 4], 6)
	inspect_map("collective", [6, 4, 4])
	inspect_map("separate_entities", [6, 6])
	inspect_map("clip", [6, 0])
	inspect_map("skip", [6, 5])
	var config := ConfigFile.new()
	check(config.load("res://adjacent.map.import") == OK, "Read import settings")
	config.set_value("params", "trenchbroom/cull_interior_faces", false)
	check(config.save("res://adjacent.map.import") == OK, "Save culling override")
	EditorInterface.get_resource_filesystem().reimport_files(PackedStringArray(["res://adjacent.map"]))
	var unculled := inspect_map("adjacent", [6, 6])
	for brush in culled.size():
		check(culled[brush][1] == unculled[brush][1], "Culling leaves exact collision data intact")
		for surface in culled[brush][0]:
			check(unculled[brush][0].has(surface), "Surviving vertex, normal, UV and tangent arrays stay intact")
	print("TrenchBroom whole-face culling checks passed")
	get_tree().quit()
'''


def transform(brush, scale=(1, 1, 1), offset=(0, 0, 0)):
    def point(match):
        values = [float(value) for value in match.group(1).split()]
        return "( " + " ".join(f"{values[i] * scale[i] + offset[i]:g}" for i in range(3)) + " )"

    return re.sub(r"\( ([^)]+) \)", point, brush)


def entity(brushes, classname="worldspawn"):
    return '{\n"classname" "' + classname + '"\n' + "".join("{\n" + brush + "\n}\n" for brush in brushes) + "}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    args = parser.parse_args()
    cube = BRUSH
    large = transform(cube, (2, 2, 2), (-16, -16, -16))
    wedge = "\n".join([cube.splitlines()[i] for i in (0, 2, 3, 4)])
    wedge += "\n( 0 0 32 ) ( 32 0 0 ) ( 0 32 32 ) stone 0 0 0 1 1"
    fixtures = {
        "adjacent": entity([cube, transform(cube, offset=(32, 0, 0))]),
        "overlap": entity([cube, transform(cube, offset=(16, 0, 0))]),
        "partial": entity([cube, transform(cube, offset=(32, 16, 0))]),
        "gap": entity([cube, transform(cube, offset=(32.1, 0, 0))]),
        "duplicate": entity([cube, cube]),
        "contained": entity([large, cube]),
        "contained_reversed": entity([cube, large]),
        "slopes": entity([wedge, transform(wedge, (-1, 1, -1), (32, 0, 32))]),
        "collective": entity([cube, transform(cube, (1, 0.5, 1), (32, 0, 0)), transform(cube, (1, 0.5, 1), (32, 16, 0))]),
        "separate_entities": entity([cube]) + entity([transform(cube, offset=(32, 0, 0))], "func_detail"),
        "clip": entity([cube, large.replace("stone", "clip")]),
        "skip": entity([cube, large.replace("stone", "skip", 1)]),
    }
    with tempfile.TemporaryDirectory(prefix="godot-trenchbroom-culling-") as directory:
        project = Path(directory)
        (project / "project.godot").write_text(
            'config_version=5\n[editor_plugins]\nenabled=PackedStringArray("res://addons/culling_test/plugin.cfg")\n',
            encoding="utf-8",
        )
        for name, source in fixtures.items():
            (project / (name + ".map")).write_text(source, encoding="utf-8")
        addon = project / "addons/culling_test"
        addon.mkdir(parents=True)
        (addon / "plugin.cfg").write_text(
            '[plugin]\nname="Culling Test"\ndescription=""\nauthor=""\nversion="1"\nscript="test.gd"\n',
            encoding="utf-8",
        )
        (addon / "test.gd").write_text(SCRIPT, encoding="utf-8")
        require_success(run_engine(args.engine, project, "--editor"), "TrenchBroom whole-face culling checks passed")


if __name__ == "__main__":
    main()
