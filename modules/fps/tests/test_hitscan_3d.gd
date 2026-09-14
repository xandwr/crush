extends SceneTree

var failures := 0

func expect(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		printerr("FAIL: " + message)

func _initialize() -> void:
	run.call_deferred()

func box(parent: Node3D, position: Vector3, layer: int, area := false) -> CollisionObject3D:
	var body: CollisionObject3D = Area3D.new() if area else StaticBody3D.new()
	body.collision_layer = layer
	body.collision_mask = 0
	var collision := CollisionShape3D.new()
	collision.shape = BoxShape3D.new()
	body.add_child(collision)
	parent.add_child(body)
	body.position = position
	return body

func run() -> void:
	var settings := HitscanSettings.new()
	settings.damage = 100.0
	settings.max_distance = 20.0
	settings.falloff_start_distance = 4.0
	settings.falloff_end_distance = 12.0
	settings.falloff_min_multiplier = 0.2
	expect(settings.validation_errors().is_empty(), "settings valid")
	for pair in [[0.0, 100.0], [4.0, 100.0], [8.0, 60.0], [12.0, 20.0], [20.0, 20.0], [20.1, 0.0]]:
		expect(is_equal_approx(settings.damage_at_distance(pair[0]), pair[1]), "falloff boundary " + str(pair[0]))
	for distance in [-1.0, INF, NAN]:
		expect(settings.damage_at_distance(distance) == -1.0, "invalid distance rejected")
	for field in ["damage", "max_distance", "falloff_start_distance", "falloff_end_distance", "falloff_min_multiplier"]:
		for invalid in [-1.0, INF, NAN]:
			var broken := settings.duplicate() as HitscanSettings
			broken.set(field, invalid)
			expect(not broken.validation_errors().is_empty() and broken.damage_at_distance(0) == -1, "invalid " + field)
	var copy := settings.duplicate() as HitscanSettings
	copy.damage = 1
	expect(settings.damage == 100, "resource duplicates isolate values")
	expect(ResourceSaver.save(settings, "user://hitscan_settings.tres") == OK, "settings save")
	var reopened := ResourceLoader.load("user://hitscan_settings.tres", "", ResourceLoader.CACHE_MODE_IGNORE) as HitscanSettings
	expect(reopened != null and reopened.damage_at_distance(8) == settings.damage_at_distance(8), "settings round trip")
	DirAccess.remove_absolute("user://hitscan_settings.tres")
	var world := Node3D.new()
	root.add_child(world)
	var near := box(world, Vector3(0, 0, -5), 1)
	var far := box(world, Vector3(0, 0, -10), 4)
	var area := box(world, Vector3(0, 0, -2), 2, true)
	var inside := box(world, Vector3.ZERO, 8)
	var triangle := StaticBody3D.new()
	triangle.collision_layer = 16
	var triangle_shape := ConcavePolygonShape3D.new()
	triangle_shape.backface_collision = true
	triangle_shape.set_faces(PackedVector3Array([Vector3(-2, -2, -7), Vector3(2, -2, -7), Vector3(0, 2, -7)]))
	var triangle_collision := CollisionShape3D.new()
	triangle_collision.shape = triangle_shape
	triangle.add_child(triangle_collision)
	world.add_child(triangle)
	await physics_frame
	await physics_frame
	var space := world.get_world_3d().direct_space_state
	var result := Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings, 5)
	expect(result.hit and result.collider == near and result.rid == near.get_rid(), "nearest body blocks farther target")
	expect(result.collider_id == near.get_instance_id() and result.shape == 0 and result.face_index == -1, "physics identity and convex face sentinel")
	expect(result.endpoint.is_equal_approx(Vector3(0, 0, -4.5)) and result.normal.is_equal_approx(Vector3.BACK), "hit endpoint and normal")
	expect(is_equal_approx(result.distance, 4.5) and is_equal_approx(result.damage, 95.0), "damage uses traveled distance")
	var excluded: Array[RID] = [near.get_rid()]
	result = Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings, 5, excluded)
	expect(result.hit and result.collider == far, "RID exclusion reveals next collider")
	result = Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings, 4)
	expect(result.collider == far, "mask filters nearest body")
	result = Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings, 2)
	expect(not result.hit, "areas excluded by default")
	result = Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings, 2, [], true)
	expect(result.hit and result.collider == area, "areas included explicitly")
	result = Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings, 8)
	expect(result.hit and result.collider == inside and is_zero_approx(result.distance) and result.normal == Vector3.ZERO, "starting inside solid hits immediately")
	result = Hitscan3D.trace(space, Vector3.ZERO, Vector3.RIGHT, settings, 5)
	expect(not result.hit and result.endpoint == Vector3(20, 0, 0) and result.distance == 20 and result.damage == 0, "miss reaches range without damage")
	expect(result.collider == null and result.collider_id == 0 and not result.rid.is_valid() and result.shape == -1 and result.face_index == -1 and result.normal == Vector3.ZERO, "miss identity sentinels")
	expect(not Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings, 0).hit, "zero mask is a valid miss")
	result = Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings, 16)
	expect(result.hit and result.collider == triangle and is_equal_approx(result.distance, 7.0), "concave triangle hit")
	var query := PhysicsRayQueryParameters3D.create(Vector3.ZERO, Vector3(0, 0, -20), 16)
	var raw := space.intersect_ray(query)
	expect(result.face_index == raw.face_index and result.shape == raw.shape, "face and shape indices match backend query")
	result = Hitscan3D.trace(space, Vector3(0, 0, -8), Vector3.BACK, settings, 16)
	expect(result.hit and is_equal_approx(result.distance, 1.0), "back face blocks trace")
	var short_range := settings.duplicate() as HitscanSettings
	short_range.max_distance = 3
	short_range.falloff_start_distance = 1
	short_range.falloff_end_distance = 2
	expect(not Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, short_range, 5).hit, "colliders beyond range do not hit")
	result = Hitscan3D.trace(space, Vector3.ZERO, Vector3(0, 0, -1.0001), settings, 5)
	expect(result.hit and is_equal_approx(result.distance, 4.5), "near-unit input normalized")
	var enormous := settings.duplicate() as HitscanSettings
	enormous.max_distance = 1e300
	expect(Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, enormous).is_empty(), "unrepresentable endpoint rejected")
	for direction in [Vector3.ZERO, Vector3(0, 0, -2), Vector3(INF, 0, 0), Vector3(NAN, 0, 0)]:
		expect(Hitscan3D.trace(space, Vector3.ZERO, direction, settings).is_empty(), "invalid direction rejected")
	expect(Hitscan3D.trace(null, Vector3.ZERO, Vector3.FORWARD, settings).is_empty(), "null space rejected")
	expect(Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, null).is_empty(), "null settings rejected")
	expect(Hitscan3D.trace(space, Vector3(INF, 0, 0), Vector3.FORWARD, settings).is_empty(), "invalid origin rejected")
	settings.max_distance = 0
	expect(Hitscan3D.trace(space, Vector3.ZERO, Vector3.FORWARD, settings).is_empty(), "invalid settings do not produce a miss")
	world.free()
	if failures == 0: print("Hitscan3D contract passed")
	quit(0 if failures == 0 else 1)
