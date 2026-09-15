extends SceneTree

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var world := Node3D.new()
	root.add_child(world)
	var floor_body := StaticBody3D.new()
	floor_body.collision_layer = 1 << 31
	floor_body.position.y = -2.0
	var shape := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(10, 3, 10)
	shape.shape = box
	floor_body.add_child(shape)
	world.add_child(floor_body)
	var shoulder := Node3D.new()
	shoulder.name = "Shoulder"
	world.add_child(shoulder)
	var arm := Arm3D.new()
	arm.shoulder_target = NodePath("../Shoulder")
	arm.hand_target_enabled = false
	arm.collision_enabled = true
	arm.collision_mask = 1 << 31
	world.add_child(arm)
	for index in 30:
		await physics_frame
	assert(arm.get_joint_positions()[2].y > -0.5, "Selected layer must block the hand")
	arm.collision_mask = 1
	arm.reset_simulation()
	for index in 30:
		await physics_frame
	assert(arm.get_joint_positions()[2].y < -0.8, "Unselected layer must not block the hand")
	for node in [arm, Rope3D.new()]:
		var found := false
		for property in node.get_property_list():
			if property.name == "collision_mask":
				assert(property.hint == PROPERTY_HINT_LAYERS_3D_PHYSICS)
				found = true
		assert(found)
		if node != arm:
			node.free()
	print("PASS: Arm3D layer 32 collision, mask exclusion, and both native layer pickers")
	world.queue_free()
	quit()
