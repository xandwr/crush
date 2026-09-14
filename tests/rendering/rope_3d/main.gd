extends Node3D

var _ropes: Array[Rope3D] = []
var _measure := false
var _ticks := 0
var _elapsed_usec := 0
var _memory_before := 0


func _ready() -> void:
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 7
	add_child(camera)
	camera.position = Vector3(4, 3.5, 7)
	camera.look_at(Vector3(0, 1.4, 0))
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-50, -30, 0)
	light.shadow_enabled = true
	add_child(light)
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_color = Color(0.55, 0.65, 0.85)
	environment.environment.ambient_light_energy = 0.5
	add_child(environment)
	var floor_body := StaticBody3D.new()
	var floor_shape := CollisionShape3D.new()
	floor_shape.shape = BoxShape3D.new()
	(floor_shape.shape as BoxShape3D).size = Vector3(20, 0.2, 20)
	floor_body.add_child(floor_shape)
	floor_body.position.y = -0.1
	add_child(floor_body)
	var floor_mesh := MeshInstance3D.new()
	floor_mesh.mesh = BoxMesh.new()
	(floor_mesh.mesh as BoxMesh).size = Vector3(8, 0.2, 6)
	floor_mesh.position.y = -0.1
	floor_mesh.material_override = _material(Color(0.25, 0.3, 0.4))
	add_child(floor_mesh)
	if "--rope-performance" in OS.get_cmdline_user_args():
		_performance()
		return
	var hanging := _rope(17, Color(0.9, 0.45, 0.2))
	hanging.rest_length = 2.2
	hanging.reset_to_points(_curve(17, Vector3(-2, 3, 0), Vector3(-1, 1.1, 0)))
	hanging.set_particle_target(0, Vector3(-2, 3, 0))
	var cable := _rope(33, Color(0.3, 0.8, 0.65))
	cable.rest_length = 3
	cable.reset_to_points(_curve(33, Vector3(-0.5, 3, 0), Vector3(2, 3, 0), Vector3(0, -0.6, 0)))
	cable.set_particle_target(0, Vector3(-0.5, 3, 0))
	cable.set_particle_target(32, Vector3(2, 3, 0))
	var posed := _rope(9, Color(0.55, 0.65, 1))
	posed.simulation_enabled = false
	posed.set_render_points(PackedVector3Array([Vector3(-1.5, 0.2, 1), Vector3(-1.1, 0.7, 1), Vector3(-0.6, 1, 1), Vector3(0, 1.1, 1), Vector3(0.5, 1, 1), Vector3(0.9, 0.7, 1), Vector3(1.1, 0.2, 1), Vector3(1.1, 0.2, 1), Vector3(0.8, 0.3, 1)]))
	var mirrored := _rope(9, Color(0.85, 0.3, 0.55))
	mirrored.scale = Vector3(-1.5, 0.8, 1.2)
	mirrored.rest_length = 1.5
	mirrored.reset_to_points(_curve(9, Vector3(2.5, 2, -0.5), Vector3(3.3, 0.7, -0.5)))
	mirrored.set_particle_target(0, Vector3(2.5, 2, -0.5))
	await get_tree().create_timer(3).timeout
	await RenderingServer.frame_post_draw
	var capture := "user://rope_" + str(RenderingServer.get_current_rendering_method()) + ".png"
	var image := get_viewport().get_texture().get_image()
	var result := image.save_png(capture)
	print("Rope3D capture: ", ProjectSettings.globalize_path(capture), " error=", result)
	if "--rope-capture" in OS.get_cmdline_user_args(): get_tree().quit(result)


func _material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.65
	return material


func _curve(count: int, start: Vector3, end: Vector3, guide := Vector3.ZERO) -> PackedVector3Array:
	var points := PackedVector3Array()
	for index in count:
		var fraction := float(index) / (count - 1)
		points.append(start.lerp(end, fraction) + guide * sin(PI * fraction))
	return points


func _rope(count: int, color: Color) -> Rope3D:
	var rope := Rope3D.new()
	rope.particle_count = count
	rope.radius = 0.065
	rope.collision_radius = 0.065
	rope.cap_mode = Rope3D.CAP_ROUND
	rope.solver_iterations = 12
	rope.damping = 5
	rope.collision_enabled = true
	rope.simulation_process_mode = Rope3D.SIMULATION_PROCESS_MANUAL
	rope.material_override = _material(color)
	add_child(rope)
	_ropes.append(rope)
	return rope


func _physics_process(delta: float) -> void:
	var started := Time.get_ticks_usec()
	for rope in _ropes:
		if rope.simulation_enabled: rope.advance_simulation(delta)
	if _measure:
		_elapsed_usec += Time.get_ticks_usec() - started
		_ticks += 1


func _performance() -> void:
	for count in [9, 33]:
		for rope_count in [1, 64]:
			for variant in [0, 1, 2]:
				for index in rope_count:
					var rope := _rope(count, Color(0.4, 0.7, 0.9))
					rope.solver_iterations = 6
					rope.rest_length = 1.5
					rope.render_enabled = variant > 0
					rope.collision_enabled = variant > 1
					var anchor := Vector3(-2 + index * 0.06, 1.5, 0)
					rope.reset_to_points(_curve(count, anchor, anchor + Vector3(0.1, -1.4, 0)))
					rope.set_particle_target(0, anchor)
				for tick in 32: await get_tree().physics_frame
				_memory_before = OS.get_static_memory_usage()
				_ticks = 0
				_elapsed_usec = 0
				_measure = true
				for tick in 128: await get_tree().physics_frame
				_measure = false
				print("ROPE_PROFILE ", JSON.stringify({ "particles": count, "ropes": rope_count, "render": variant > 0, "collision": variant > 1, "iterations": 6, "rate": Engine.physics_ticks_per_second, "usec_per_tick": float(_elapsed_usec) / _ticks, "retained_bytes_delta": OS.get_static_memory_usage() - _memory_before, "renderer": RenderingServer.get_current_rendering_method() }))
				for rope in _ropes: rope.queue_free()
				_ropes.clear()
				await get_tree().process_frame
	get_tree().quit()
