extends "res://regression.gd"

var mirrors: Array[Mirror3D] = []

func measure(label: String) -> Dictionary:
	await settle()
	var calls := 0
	var started := Time.get_ticks_usec()
	for _frame in 120:
		await get_tree().process_frame
		calls += RenderingServer.get_rendering_info(RenderingServer.RENDERING_INFO_TOTAL_DRAW_CALLS_IN_FRAME)
	return {"label": label, "mean_frame_ms": (Time.get_ticks_usec() - started) / 120000.0, "mean_draw_calls": calls / 120.0}

func _ready() -> void:
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	camera = Camera3D.new()
	add_child(camera)
	camera.position = Vector3(0, 0, 5)
	camera.current = true
	for mirror_position in [Vector3(-1.1, -0.8, 0), Vector3(1.1, -0.8, 0), Vector3(-1.1, 0.8, 0), Vector3(1.1, 0.8, 0)]:
		var surface := Mirror3D.new()
		add_child(surface)
		surface.position = mirror_position
		surface.size = Vector2(1.8, 1.2)
		mirrors.append(surface)
	for index in 128:
		box(Vector3((index % 8) * 0.6 - 2.1, (index / 8.0 as int) % 4 * 0.6 - 0.9, 6 + (index / 32.0 as int) * 1.5), Color(0.3, 0.4, 0.5), Vector3(0.3, 0.3, 0.3))
	secondary = SubViewport.new()
	secondary.size = Vector2i(320, 240)
	secondary.world_3d = get_world_3d()
	secondary.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(secondary)
	secondary_camera = Camera3D.new()
	secondary.add_child(secondary_camera)
	secondary_camera.position = Vector3(0.3, 0, 5)
	secondary_camera.current = true
	for surface in mirrors:
		surface.enabled = false
	var baseline := await measure("disabled")
	for surface in mirrors:
		surface.enabled = true
	var reflected := await measure("4 mirrors, 2 cameras, 128 objects")
	var extra_calls: float = reflected.mean_draw_calls - baseline.mean_draw_calls
	var inferred_views := extra_calls / 132.0
	if absf(inferred_views - 8) > 0.01:
		failures.append("expected 8 reflection views, inferred " + str(inferred_views))
	for surface in mirrors:
		surface.rotation.y = PI
	var backfacing := await measure("backfacing")
	if absf(backfacing.mean_draw_calls - baseline.mean_draw_calls) > 0.01:
		failures.append("backfacing mirrors scheduled reflection draw calls")
	var result := {"renderer": RenderingServer.get_current_rendering_method(), "baseline": baseline, "reflection": reflected, "backfacing": backfacing, "inferred_reflection_views": inferred_views}
	print("MIRROR_PERFORMANCE ", JSON.stringify(result))
	if failures.is_empty():
		print("MIRROR_PERFORMANCE_PASS")
	else:
		for failure in failures:
			push_error(failure)
	get_tree().quit(0 if failures.is_empty() else 1)
