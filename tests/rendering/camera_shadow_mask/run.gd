extends SceneTree

var viewport: SubViewport
var camera: Camera3D
var caster: MeshInstance3D
var failures := 0

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	viewport = SubViewport.new()
	viewport.size = Vector2i(384, 384)
	viewport.own_world_3d = true
	viewport.transparent_bg = true
	viewport.positional_shadow_atlas_size = 2048
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	camera = Camera3D.new()
	camera.cull_mask = 1
	camera.position = Vector3(5, 7, 8)
	camera.far = 30
	viewport.add_child(camera)
	camera.look_at(Vector3.ZERO)
	camera.current = true
	var receiver := MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(10, 10)
	receiver.mesh = plane
	var white := StandardMaterial3D.new()
	white.albedo_color = Color.WHITE
	white.roughness = 1
	receiver.material_override = white
	viewport.add_child(receiver)
	caster = MeshInstance3D.new()
	caster.mesh = BoxMesh.new()
	caster.position = Vector3(0, 1.5, 0)
	caster.layers = 2
	var red := StandardMaterial3D.new()
	red.albedo_color = Color.RED
	caster.material_override = red
	viewport.add_child(caster)
	assert(camera.additional_shadow_cull_mask == 0)
	var packed := PackedScene.new()
	camera.additional_shadow_cull_mask = 2
	assert(packed.pack(camera) == OK)
	var restored := packed.instantiate() as Camera3D
	assert(restored.additional_shadow_cull_mask == 2 and restored.cull_mask == 1)
	restored.free()
	var other_viewport := SubViewport.new()
	other_viewport.size = viewport.size
	other_viewport.world_3d = camera.get_world_3d()
	other_viewport.transparent_bg = true
	other_viewport.positional_shadow_atlas_size = 2048
	other_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(other_viewport)
	var other_camera := camera.duplicate() as Camera3D
	other_camera.additional_shadow_cull_mask = 0
	other_viewport.add_child(other_camera)
	for kind in ["directional", "directional_cascades", "omni_cube", "omni_dual", "spot"]:
		if kind == "omni_dual" and RenderingServer.get_current_rendering_method() == "gl_compatibility":
			print("SKIP omni_dual: unsupported by Compatibility renderer")
			continue
		var light: Light3D
		if kind.begins_with("directional"):
			var directional := DirectionalLight3D.new()
			directional.directional_shadow_mode = 0 if kind == "directional" else 2
			directional.directional_shadow_max_distance = 25
			light = directional
		elif kind.begins_with("omni"):
			var omni := OmniLight3D.new()
			omni.omni_range = 20
			omni.omni_shadow_mode = 1 if kind == "omni_cube" else 0
			light = omni
		else:
			var spot := SpotLight3D.new()
			spot.spot_range = 20
			spot.spot_angle = 65
			light = spot
		light.position = Vector3(-3, 6, 2)
		light.shadow_enabled = true
		light.shadow_bias = 0.02
		light.shadow_normal_bias = 0.1
		viewport.add_child(light)
		light.look_at(Vector3.ZERO)
		camera.additional_shadow_cull_mask = 0
		var baseline := await _capture()
		var other_baseline := await _capture(other_viewport)
		camera.additional_shadow_cull_mask = 2
		var shadowed := await _capture()
		baseline.save_png("user://%s_baseline.png" % kind)
		shadowed.save_png("user://%s_shadowed.png" % kind)
		_check(_difference(other_baseline, await _capture(other_viewport)) < 30, "%s keeps shared-world camera independent" % kind)
		other_camera.additional_shadow_cull_mask = 2
		_check(_difference(shadowed, await _capture(other_viewport)) < 30, "%s supports shared-world shadows" % kind)
		other_camera.additional_shadow_cull_mask = 0
		var shadow_pixels := _difference(baseline, shadowed)
		_check(shadow_pixels > 100, "%s adds shadow: %d pixels" % [kind, shadow_pixels])
		_check(_red_pixels(shadowed) == 0, "%s caster stays excluded" % kind)
		light.shadow_caster_mask = 1
		_check(_difference(baseline, await _capture()) < 30, "%s respects light caster mask" % kind)
		light.shadow_caster_mask = 0xffffffff
		caster.hide()
		_check(_difference(baseline, await _capture()) < 30, "%s respects node visibility" % kind)
		caster.show()
		caster.position.x = 2
		_check(_difference(shadowed, await _capture()) > 100, "%s tracks moving caster" % kind)
		caster.position.x = 0
		camera.additional_shadow_cull_mask = 0
		_check(_difference(baseline, await _capture()) < 30, "%s resets to legacy behavior" % kind)
		camera.cull_mask = 3
		_check(_red_pixels(await _capture()) > 100, "%s visible mask still renders caster" % kind)
		camera.cull_mask = 1
		light.queue_free()
		await process_frame
	print("CAMERA_SHADOW_MASK_RESULT failures=%d" % failures)
	quit(0 if failures == 0 else 1)

func _capture(target: SubViewport = null) -> Image:
	if target == null:
		target = viewport
	for _frame in range(12):
		await process_frame
	await RenderingServer.frame_post_draw
	return target.get_texture().get_image()

func _difference(first: Image, second: Image) -> int:
	var changed := 0
	for y in range(first.get_height()):
		for x in range(first.get_width()):
			var a := first.get_pixel(x, y)
			var b := second.get_pixel(x, y)
			if absf(a.r - b.r) + absf(a.g - b.g) + absf(a.b - b.b) > 0.08:
				changed += 1
	return changed

func _red_pixels(image: Image) -> int:
	var count := 0
	for y in range(image.get_height()):
		for x in range(image.get_width()):
			var pixel := image.get_pixel(x, y)
			if pixel.r > 0.05 and pixel.r > pixel.g * 2 and pixel.r > pixel.b * 2:
				count += 1
	return count

func _check(condition: bool, description: String) -> void:
	print("%s %s" % ["PASS" if condition else "FAIL", description])
	if not condition:
		failures += 1
