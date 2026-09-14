extends SceneTree

var failures := 0
var motor: CharacterMotor3D
var hull: CollisionShape3D
var obstacles: Array[Node] = []

func expect(value: bool, message: String) -> void:
	if value: return
	failures += 1
	printerr("FAIL: " + message)

func _initialize() -> void:
	run.call_deferred()

func box(position: Vector3, size: Vector3) -> StaticBody3D:
	var body := StaticBody3D.new()
	var collision := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	collision.shape = shape
	body.add_child(collision)
	body.position = position
	root.add_child(body)
	obstacles.append(body)
	return body

func apply_height(height: float) -> void:
	(hull.shape as BoxShape3D).size = Vector3(0.5, height, 0.5)
	hull.position.y = height * 0.5

func clearance(height: float, at: Transform3D) -> bool:
	var shape := BoxShape3D.new()
	shape.size = Vector3(0.5, height - 0.002, 0.5)
	var query := PhysicsShapeQueryParameters3D.new()
	query.shape = shape
	query.transform = at * Transform3D(Basis.IDENTITY, Vector3(0, height * 0.5, 0))
	query.exclude = [motor.get_rid()]
	return motor.get_world_3d().direct_space_state.intersect_shape(query).is_empty()

func tick(input := Vector2.ZERO, jump := false, crouch := false) -> void:
	await physics_frame
	expect(motor.step(1.0 / 60.0, input, Basis.IDENTITY, jump, crouch), "valid step accepted")

func reset(at := Vector3.ZERO) -> void:
	motor.global_position = at
	motor.velocity = Vector3.ZERO
	motor.reset_stance()
	for index in 4: await tick()

func run() -> void:
	var settings := MovementSettings.new()
	expect(settings.validation_errors().is_empty(), "defaults valid")
	settings.crouching_height = settings.standing_height + 1
	expect(not settings.validation_errors().is_empty(), "inverted heights rejected")
	settings.crouching_height = 1.15
	settings.step_height = NAN
	expect(not settings.validation_errors().is_empty(), "nonfinite settings rejected")
	settings.step_height = 0.3
	var copy := settings.duplicate() as MovementSettings
	copy.step_height = 0.2
	expect(is_equal_approx(settings.step_height, 0.3), "resource duplicate isolated")
	motor = CharacterMotor3D.new()
	motor.settings = settings
	hull = CollisionShape3D.new()
	hull.shape = BoxShape3D.new()
	motor.add_child(hull)
	motor.stance_changed.connect(apply_height)
	motor.set_clearance_test(clearance)
	root.add_child(motor)
	box(Vector3(0, -0.5, 0), Vector3(30, 1, 30))
	await reset()
	expect(motor.is_on_floor(), "settles on floor")
	var before := motor.global_transform
	expect(not motor.step(NAN, Vector2.ZERO, Basis.IDENTITY), "invalid delta rejected")
	expect(motor.global_transform == before, "invalid step does not move")
	var stair := box(Vector3(0, 0.1, -1.5), Vector3(3, 0.2, 2))
	var stepped := false
	for index in 35:
		await tick(Vector2(0, -1))
		stepped = stepped or motor.get_step_height() > 0.1
		expect(motor.get_position_delta().is_equal_approx(motor.get_displacement()), "inherited displacement includes step")
		expect((motor.get_real_velocity() / 60.0).is_equal_approx(motor.get_displacement()), "real velocity includes step")
	expect(stepped, "climbs a low stair")
	expect(motor.global_position.z < -1.0 and motor.global_position.y > 0.19, "advances onto stair")
	var stayed_grounded := true
	for index in 25:
		await tick(Vector2(0, -1))
		stayed_grounded = stayed_grounded and motor.is_on_floor()
	expect(stayed_grounded and motor.position.y < 0.01, "snaps down a descending stair")
	await tick(Vector2.ZERO, true)
	expect(motor.has_jumped() and not motor.is_on_floor(), "jump releases floor snap")
	stair.queue_free()
	await reset()
	var tall := box(Vector3(0, 0.3, -1.5), Vector3(3, 0.6, 2))
	for index in 30: await tick(Vector2(0, -1))
	expect(motor.global_position.z > -0.4, "cannot climb obstacle above step height")
	tall.queue_free()
	await reset()
	var low_step := box(Vector3(0, 0.1, -1.5), Vector3(3, 0.2, 2))
	var low_roof := box(Vector3(0, 2.0, -1.5), Vector3(3, 0.3, 3))
	for index in 30: await tick(Vector2(0, -1))
	expect(motor.position.z > -0.4, "headroom blocks elevated path")
	low_roof.queue_free()
	await reset()
	settings.step_height = 0
	for index in 30: await tick(Vector2(0, -1))
	expect(motor.position.z > -0.4, "zero step height disables stair climbing")
	settings.step_height = 0.3
	low_step.queue_free()
	await reset()
	await tick(Vector2.ZERO, false, true)
	expect(motor.is_crouching(), "enters crouch")
	var feet := motor.global_position.y
	var ceiling := box(Vector3(0, 1.45, 0), Vector3(3, 0.4, 3))
	await tick()
	expect(motor.is_crouching(), "low ceiling blocks standing")
	expect(absf(motor.global_position.y - feet) < 0.01, "ground crouch preserves feet")
	ceiling.queue_free()
	await tick()
	await tick()
	expect(not motor.is_crouching(), "stands when clearance returns")
	await tick(Vector2.ZERO, false, true)
	await tick(Vector2.ZERO, true, true)
	expect(motor.global_position.y > 0.6, "air crouch tucks hull")
	await tick()
	expect(not motor.is_crouching(), "air crouch restores standing when clear")
	for obstacle in obstacles:
		if is_instance_valid(obstacle): obstacle.queue_free()
	motor.queue_free()
	await process_frame
	if failures == 0: print("CharacterMotor3D contract passed")
	quit(0 if failures == 0 else 1)
