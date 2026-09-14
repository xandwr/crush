@tool
extends EditorPlugin

func _enter_tree() -> void:
	if "--mirror-editor-qa" in OS.get_cmdline_user_args():
		_run.call_deferred()

func _run() -> void:
	for _frame in 40:
		await get_tree().process_frame
	EditorInterface.open_scene_from_path("res://editor_preview.tscn")
	for _frame in 40:
		await get_tree().process_frame
	var scene := EditorInterface.get_edited_scene_root()
	var mirror := scene.get_node("Mirror3D") as Mirror3D
	EditorInterface.set_main_screen_editor("3D")
	EditorInterface.get_selection().clear()
	EditorInterface.get_selection().add_node(mirror)
	EditorInterface.inspect_object(mirror)
	for _frame in 20:
		await get_tree().process_frame
	if EditorInterface.get_inspector().get_edited_object() != mirror:
		push_error("Mirror Inspector did not select native node")
		get_tree().quit(1)
		return
	mirror.size = Vector2(3.8, 2.8)
	mirror.enabled = false
	mirror.enabled = true
	mirror.size = Vector2(4, 3)
	var viewport := EditorInterface.get_editor_viewport_3d(0)
	viewport.get_camera_3d().global_transform = scene.get_node("Camera3D").global_transform
	await RenderingServer.frame_post_draw
	var output := "res://captures/editor"
	var arguments := OS.get_cmdline_user_args()
	for index in arguments.size() - 1:
		if arguments[index] == "--output":
			output = arguments[index + 1]
	DirAccess.make_dir_recursive_absolute(output)
	viewport.get_texture().get_image().save_png(output.path_join("viewport.png"))
	get_tree().root.get_texture().get_image().save_png(output.path_join("editor.png"))
	var packed := PackedScene.new()
	if packed.pack(scene) != OK:
		push_error("Editor scene serialization failed")
		get_tree().quit(1)
		return
	var restored := packed.instantiate()
	var restored_mirror := restored.get_node("Mirror3D") as Mirror3D
	if restored_mirror == null or restored_mirror.size != Vector2(4, 3) or restored_mirror.get_child_count(true) != 0:
		push_error("Native Mirror3D serialization failed in editor")
		get_tree().quit(1)
		return
	restored.free()
	print("MIRROR_EDITOR_QA_PASS")
	get_tree().quit()
