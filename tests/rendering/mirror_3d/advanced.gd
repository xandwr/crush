extends "res://regression.gd"

func changed_pixels(before: Image, after: Image, area: Rect2i) -> int:
	var count := 0
	for y in range(area.position.y, area.end.y):
		for x in range(area.position.x, area.end.x):
			var a := before.get_pixel(x, y)
			var b := after.get_pixel(x, y)
			if absf(a.r - b.r) + absf(a.g - b.g) + absf(a.b - b.b) > 0.12:
				count += 1
	return count

func _ready() -> void:
	var arguments := OS.get_cmdline_user_args()
	for index in arguments.size() - 1:
		if arguments[index] == "--output":
			output = arguments[index + 1]
	DirAccess.make_dir_recursive_absolute(output)
	camera = Camera3D.new()
	add_child(camera)
	camera.position = Vector3(0, 0, 5)
	camera.current = true
	mirror = Mirror3D.new()
	add_child(mirror)
	mirror.size = Vector2(4, 3)
	mirror.resolution_scale = 1
	var floor_mesh := box(Vector3(0, -1, 6), Color.WHITE, Vector3(8, 0.2, 10))
	var floor_material := floor_mesh.mesh.material as StandardMaterial3D
	floor_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
	var caster := box(Vector3(0, -0.3, 7), Color.WHITE, Vector3(0.6, 1.2, 0.6))
	var caster_material := caster.mesh.material as StandardMaterial3D
	caster_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
	var world_environment := WorldEnvironment.new()
	world_environment.environment = Environment.new()
	world_environment.environment.background_mode = Environment.BG_COLOR
	world_environment.environment.background_color = Color(0.01, 0.01, 0.01)
	world_environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world_environment.environment.ambient_light_color = Color.WHITE
	world_environment.environment.ambient_light_energy = 0.15
	add_child(world_environment)
	var directional := DirectionalLight3D.new()
	add_child(directional)
	directional.rotation_degrees = Vector3(-55, -25, 0)
	directional.shadow_enabled = true
	for mode in [DirectionalLight3D.SHADOW_ORTHOGONAL, DirectionalLight3D.SHADOW_PARALLEL_4_SPLITS]:
		directional.directional_shadow_mode = mode
		await settle()
		var shadowed := capture("directional_" + str(mode))
		directional.shadow_enabled = false
		await settle()
		var unshadowed := capture("directional_unshadowed_" + str(mode))
		if changed_pixels(shadowed, unshadowed, Rect2i(210, 245, 220, 50)) < 20:
			failures.append("directional shadow missing mode " + str(mode))
		directional.shadow_enabled = true
	directional.hide()
	for kind in ["omni", "spot"]:
		get_viewport().positional_shadow_atlas_size = 1024 if kind == "omni" else 2048
		get_viewport().positional_shadow_atlas_16_bits = kind != "omni"
		var light: Light3D = OmniLight3D.new() if kind == "omni" else SpotLight3D.new()
		add_child(light)
		light.position = Vector3(-1.5, 2, 6)
		light.light_energy = 4
		if light is OmniLight3D:
			light.omni_range = 15
		else:
			light.spot_range = 15
			light.spot_angle = 65
			light.look_at(Vector3(0, -1, 7))
		light.shadow_enabled = true
		await settle()
		var shadowed := capture(kind + "_shadow")
		light.shadow_enabled = false
		await settle()
		var unshadowed := capture(kind + "_unshadowed")
		if changed_pixels(shadowed, unshadowed, Rect2i(210, 245, 220, 50)) < 20:
			failures.append(kind + " shadow missing")
		light.queue_free()
		await settle()
	get_viewport().positional_shadow_atlas_size = 2048
	get_viewport().positional_shadow_atlas_16_bits = true
	caster.hide()
	floor_mesh.hide()
	world_environment.environment.ambient_light_energy = 0
	red = box(Vector3(-0.9, 0.6, 7), Color.RED)
	green = box(Vector3(0.9, -0.6, 7), Color.GREEN)
	mirror.rotation_degrees = Vector3(8, 12, 0)
	await settle()
	var plane := Plane(mirror.global_basis.z.normalized(), mirror.global_position)
	var red_reflected := red.global_position - plane.normal * plane.distance_to(red.global_position) * 2
	var green_reflected := green.global_position - plane.normal * plane.distance_to(green.global_position) * 2
	var image := capture("tilted")
	check_color(image, camera, red_reflected, 0, "tilted red")
	check_color(image, camera, green_reflected, 1, "tilted green")
	mirror.rotation = Vector3.ZERO
	for view_position in [Vector3(1.2, 0.4, 4), Vector3(0, 0, 0.04), Vector3(4, 0, 0.4)]:
		camera.position = view_position
		camera.look_at(Vector3.ZERO)
		await settle()
		capture("viewer_" + str(view_position))
	camera.position = Vector3(0, 0, 5)
	camera.rotation = Vector3.ZERO

	var viewmodel := Viewmodel3D.new()
	camera.add_child(viewmodel)
	var private_geometry := box(Vector3(0, 0.4, 7), Color.BLUE)
	private_geometry.reparent(viewmodel, true)
	await settle()
	image = capture("private_viewmodel")
	var private_pixel := Vector2i(camera.unproject_position(Vector3(0, 0.4, -7)))
	if image.get_pixelv(private_pixel).b > 0.4:
		failures.append("private viewmodel leaked into reflection")
	viewmodel.visible_to_other_cameras = true
	await settle()
	image = capture("public_viewmodel")
	check_color(image, camera, Vector3(0, 0.4, -7), 2, "public viewmodel")
	private_geometry.hide()

	var nested := Mirror3D.new()
	add_child(nested)
	nested.position = Vector3(0, 0, 7)
	nested.rotation.y = PI
	nested.size = Vector2(1.5, 1.5)
	await settle()
	image = capture("facing_mirrors")
	var nested_pixel := Vector2i(camera.unproject_position(Vector3(0, 0, -7)))
	var nested_color := image.get_pixelv(nested_pixel)
	if nested_color.r > 0.05 or nested_color.g > 0.05 or nested_color.b > 0.05:
		failures.append("nested mirror did not use black fallback")
	nested.queue_free()

	var reference := SubViewport.new()
	reference.size = Vector2i(640, 480)
	reference.own_world_3d = true
	reference.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(reference)
	var reference_camera := Camera3D.new()
	reference.add_child(reference_camera)
	reference_camera.position = camera.position
	reference_camera.current = true
	var reference_environment := world_environment.duplicate() as WorldEnvironment
	reference.add_child(reference_environment)
	reference_environment.environment = world_environment.environment
	var material := ShaderMaterial.new()
	material.shader = Shader.new()
	material.shader.code = "shader_type spatial; render_mode unshaded; void fragment() { ALBEDO = vec3(0.4, 0.2, 0.1); }"
	var color_marker := box(Vector3(0, 0, 7), Color.WHITE)
	color_marker.material_override = material
	var reference_marker := color_marker.duplicate() as MeshInstance3D
	reference.add_child(reference_marker)
	reference_marker.position.z = -7
	for exposure in [0.5, 2.0]:
		world_environment.environment.tonemap_mode = Environment.TONE_MAPPER_ACES
		world_environment.environment.tonemap_exposure = exposure
		await settle()
		image = capture("exposure_" + str(exposure))
		var control := capture("exposure_reference_" + str(exposure), reference)
		var reflection_color := image.get_pixel(320, 240)
		var reference_color := control.get_pixel(320, 240)
		if absf(reflection_color.r - reference_color.r) + absf(reflection_color.g - reference_color.g) + absf(reflection_color.b - reference_color.b) > 0.06:
			failures.append("exposure processed twice " + str(exposure) + " reflection " + str(reflection_color) + " reference " + str(reference_color))
	reference.queue_free()
	color_marker.hide()
	world_environment.environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	world_environment.environment.tonemap_exposure = 1

	for msaa in [Viewport.MSAA_DISABLED, Viewport.MSAA_4X]:
		get_viewport().msaa_3d = msaa
		get_viewport().scaling_3d_scale = 0.75
		get_viewport().scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
		await settle()
		image = capture("scaled_msaa_" + str(msaa))
		check_color(image, camera, Vector3(-0.9, 0.6, -7), 0, "scaled MSAA red " + str(msaa))
	get_viewport().scaling_3d_scale = 1
	get_viewport().msaa_3d = Viewport.MSAA_DISABLED
	for marker_position in [Vector3(-0.5, 0.2, 7), Vector3(0.5, 0.2, 7)]:
		red.position = marker_position
		await settle()
		image = capture("animated_" + str(marker_position.x))
		check_color(image, camera, Vector3(marker_position.x, marker_position.y, -marker_position.z), 0, "moving geometry")

	var particles := GPUParticles3D.new()
	add_child(particles)
	particles.position = Vector3(0, -0.5, 7)
	particles.amount = 16
	particles.lifetime = 1
	particles.preprocess = 1
	var particle_material := ParticleProcessMaterial.new()
	particle_material.gravity = Vector3.ZERO
	particle_material.initial_velocity_min = 0.1
	particle_material.initial_velocity_max = 0.2
	particle_material.color = Color.YELLOW
	particles.process_material = particle_material
	var particle_mesh := SphereMesh.new()
	particle_mesh.radius = 0.16
	particle_mesh.height = 0.32
	var particle_surface := StandardMaterial3D.new()
	particle_surface.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	particle_surface.vertex_color_use_as_albedo = true
	particle_surface.emission_enabled = true
	particle_surface.emission = Color.YELLOW
	particle_surface.emission_energy_multiplier = 2
	particle_mesh.material = particle_surface
	particles.draw_pass_1 = particle_mesh
	var transparent := box(Vector3(1, 0.5, 7), Color.BLUE)
	var transparent_material := transparent.mesh.material as StandardMaterial3D
	transparent_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	transparent_material.albedo_color = Color(0, 0, 1, 0.5)
	world_environment.environment.glow_enabled = true
	await settle()
	image = capture("particles_transparency_glow")
	var transparent_pixel := Vector2i(camera.unproject_position(Vector3(1, 0.5, -7)))
	var transparent_color := image.get_pixelv(transparent_pixel)
	if transparent_color.b < 0.3 or transparent_color.b > 0.85 or transparent_color.r > 0.2 or transparent_color.g > 0.2:
		failures.append("transparent geometry did not blend in reflection")
	var particle_pixel := Vector2i(camera.unproject_position(Vector3(0, -0.5, -7)))
	var particle_pixels := 0
	for y in range(particle_pixel.y - 8, particle_pixel.y + 9):
		for x in range(particle_pixel.x - 8, particle_pixel.x + 9):
			var color := image.get_pixel(x, y)
			if color.r > 0.5 and color.g > 0.5 and color.b < 0.3:
				particle_pixels += 1
	if particle_pixels < 5:
		failures.append("emissive particles missing in reflection")
	var cutout := box(Vector3(-0.9, -0.5, 7), Color.RED)
	var cutout_material := ShaderMaterial.new()
	cutout_material.shader = Shader.new()
	cutout_material.shader.code = "shader_type spatial; render_mode unshaded; uniform float coverage = 0.0; void fragment() { ALBEDO = vec3(1.0, 0.0, 0.0); ALPHA = coverage; ALPHA_SCISSOR_THRESHOLD = 0.5; }"
	cutout.material_override = cutout_material
	await settle()
	image = capture("alpha_cutout_hidden")
	var cutout_pixel := Vector2i(camera.unproject_position(Vector3(-0.9, -0.5, -7)))
	if image.get_pixelv(cutout_pixel).r > 0.2:
		failures.append("alpha-cutout geometry failed to discard")
	cutout_material.set_shader_parameter("coverage", 1.0)
	await settle()
	image = capture("alpha_cutout_visible")
	check_color(image, camera, Vector3(-0.9, -0.5, -7), 0, "alpha-cutout geometry")

	if failures.is_empty():
		print("MIRROR_ADVANCED_PASS ", RenderingServer.get_current_rendering_method())
	else:
		for failure in failures:
			push_error(failure)
	get_tree().quit(0 if failures.is_empty() else 1)
