extends Node3D

var failures: Array[String] = []
var output := "res://captures"
var camera: Camera3D
var mirror: Mirror3D
var red: MeshInstance3D
var green: MeshInstance3D
var secondary: SubViewport
var secondary_camera: Camera3D

func box(position_value: Vector3, color: Color, dimensions := Vector3(0.4, 0.4, 0.4)) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = dimensions
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	mesh.material = material
	instance.mesh = mesh
	add_child(instance)
	instance.position = position_value
	return instance

func settle() -> void:
	for _frame in 12:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw

func capture(label: String, viewport: Viewport = null) -> Image:
	if viewport == null:
		viewport = get_viewport()
	var image := viewport.get_texture().get_image()
	image.save_png(output.path_join(label + ".png"))
	return image

func check_color(image: Image, source_camera: Camera3D, point: Vector3, channel: int, label: String) -> void:
	var pixel := Vector2i(source_camera.unproject_position(point))
	var found := false
	for y in range(maxi(0, pixel.y - 4), mini(image.get_height(), pixel.y + 5)):
		for x in range(maxi(0, pixel.x - 4), mini(image.get_width(), pixel.x + 5)):
			var color := image.get_pixel(x, y)
			if color[channel] > 0.6 and color[(channel + 1) % 3] < 0.2 and color[(channel + 2) % 3] < 0.2:
				found = true
	if not found:
		failures.append(label + " at " + str(pixel))

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
	red = box(Vector3(-0.9, 0.6, 2), Color.RED)
	green = box(Vector3(0.9, -0.6, 2), Color.GREEN)
	box(Vector3(0, 0, -1), Color.MAGENTA, Vector3(0.7, 0.7, 0.7))
	await settle()
	var image := capture("center")
	check_color(image, camera, Vector3(-0.9, 0.6, -2), 0, "center red")
	check_color(image, camera, Vector3(0.9, -0.6, -2), 1, "center green")
	var center := image.get_pixelv(Vector2i(image.get_size() / 2.0))
	if center.r > 0.2 or center.b > 0.2:
		failures.append("geometry behind mirror plane leaked")
	var occluder_position := camera.position.lerp(Vector3(-0.9, 0.6, -2), (camera.position.z - 1.0) / (camera.position.z + 2.0))
	var occluder := box(occluder_position, Color.BLUE, Vector3(0.25, 0.25, 0.25))
	await settle()
	image = capture("world_occlusion")
	check_color(image, camera, Vector3(-0.9, 0.6, -2), 2, "world occlusion")
	occluder.queue_free()
	await settle()

	secondary = SubViewport.new()
	secondary.size = Vector2i(400, 300)
	secondary.world_3d = get_world_3d()
	secondary.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(secondary)
	secondary_camera = Camera3D.new()
	secondary.add_child(secondary_camera)
	secondary_camera.position = Vector3(1.5, 0.3, 5)
	secondary_camera.look_at(Vector3.ZERO)
	secondary_camera.current = true
	await settle()
	image = capture("two_camera_primary")
	check_color(image, camera, Vector3(-0.9, 0.6, -2), 0, "two camera primary red")
	image = capture("two_camera_secondary", secondary)
	check_color(image, secondary_camera, Vector3(-0.9, 0.6, -2), 0, "secondary red")
	check_color(image, secondary_camera, Vector3(0.9, -0.6, -2), 1, "secondary green")

	secondary_camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	secondary_camera.size = 5
	await settle()
	image = capture("orthogonal", secondary)
	check_color(image, secondary_camera, Vector3(-0.9, 0.6, -2), 0, "orthogonal red")
	check_color(image, secondary_camera, Vector3(0.9, -0.6, -2), 1, "orthogonal green")
	secondary.size = Vector2i(480, 320)
	await settle()
	image = capture("viewport_resized", secondary)
	check_color(image, secondary_camera, Vector3(-0.9, 0.6, -2), 0, "resized red")
	check_color(image, secondary_camera, Vector3(0.9, -0.6, -2), 1, "resized green")
	secondary_camera.projection = Camera3D.PROJECTION_FRUSTUM
	secondary_camera.size = 0.12
	secondary_camera.frustum_offset = Vector2(0.025, 0.01)
	await settle()
	image = capture("asymmetric_frustum", secondary)
	check_color(image, secondary_camera, Vector3(-0.9, 0.6, -2), 0, "frustum red")
	check_color(image, secondary_camera, Vector3(0.9, -0.6, -2), 1, "frustum green")
	secondary_camera.projection = Camera3D.PROJECTION_PERSPECTIVE
	var temporary_parent := Node3D.new()
	add_child(temporary_parent)
	mirror.reparent(temporary_parent, true)
	await settle()
	image = capture("reparented")
	check_color(image, camera, Vector3(-0.9, 0.6, -2), 0, "reparented red")
	mirror.reparent(self, true)
	temporary_parent.queue_free()
	secondary.own_world_3d = true
	mirror.reparent(secondary, true)
	await settle()
	image = capture("world_changed")
	var removed_pixel := Vector2i(camera.unproject_position(Vector3(-0.9, 0.6, -2)))
	if image.get_pixelv(removed_pixel).r > 0.2:
		failures.append("old world retained mirror")
	mirror.reparent(self, true)
	await settle()

	mirror.enabled = false
	await settle()
	image = capture("disabled")
	var expected := Vector2i(camera.unproject_position(Vector3(-0.9, 0.6, -2)))
	if image.get_pixelv(expected).r > 0.2:
		failures.append("disabled mirror retained reflection")
	mirror.enabled = true
	secondary.queue_free()
	await settle()
	image = capture("camera_deleted")
	check_color(image, camera, Vector3(-0.9, 0.6, -2), 0, "camera deletion red")

	for iteration in 6:
		var temporary := Mirror3D.new()
		add_child(temporary)
		temporary.position = Vector3(8, 0, 0)
		temporary.queue_free()
		mirror.hide()
		await settle()
		mirror.show()
		mirror.resolution_scale = 0.5 if iteration % 2 else 1.0
		await settle()
	image = capture("lifecycle")
	check_color(image, camera, Vector3(-0.9, 0.6, -2), 0, "lifecycle red")
	var packed := PackedScene.new()
	if packed.pack(mirror) != OK:
		failures.append("runtime mirror scene serialization failed")
	var reloaded := packed.instantiate() as Mirror3D
	mirror.queue_free()
	await settle()
	add_child(reloaded)
	mirror = reloaded
	await settle()
	image = capture("scene_reloaded")
	check_color(image, camera, Vector3(-0.9, 0.6, -2), 0, "scene reload")
	mirror.scale = Vector3(1.2, 0.8, 1)
	await settle()
	image = capture("nonuniform_scale")
	check_color(image, camera, Vector3(-0.9, 0.6, -2), 0, "nonuniform scale")
	mirror.cull_mask = 0
	await settle()
	image = capture("empty_cull_mask")
	var culled_pixel := Vector2i(camera.unproject_position(Vector3(-0.9, 0.6, -2)))
	if image.get_pixelv(culled_pixel).r > 0.2:
		failures.append("reflection ignored cull mask")
	mirror.cull_mask = 1048575
	mirror.layers = 2
	camera.cull_mask = 1
	await settle()
	image = capture("source_surface_culled")
	if image.get_pixelv(culled_pixel).r > 0.2:
		failures.append("source camera ignored mirror surface layers")
	if failures.is_empty():
		print("MIRROR_REGRESSION_PASS ", RenderingServer.get_current_rendering_method())
	else:
		for failure in failures:
			push_error(failure)
	get_tree().quit(0 if failures.is_empty() else 1)
