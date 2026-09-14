extends SceneTree

var failures := 0

func expect(condition: bool, message: String) -> void:
	if condition: return
	failures += 1
	printerr("FAIL: " + message)

func sample(time: float, position := Vector3.ZERO, epoch := 0) -> Dictionary:
	return {"time": time, "position": position, "velocity": Vector3.RIGHT, "yaw": 0.0, "ack": 1, "epoch": epoch, "gait_phase": 0.25, "gait_advancing": true, "grounded": true, "crouching": false, "jumping": false}

func _initialize() -> void:
	var history := MovementHistory3D.new()
	expect(history.is_class("Node") and not history.is_class("Node3D"), "history owns no spatial transform or automatic simulation")
	expect(history.get_latest_sample().is_empty() and history.to_packet().is_empty(), "empty history has no state")
	var command := {"sequence": 9007199254740993, "jump_id": 2, "movement": Vector2(0.25, -0.5), "yaw": 0.5, "jump": true, "crouch": true, "sprint": true, "slow": true}
	var command_bytes := "010000000000200002000000000000000000803e000000bf0000003f0f".hex_decode()
	expect(MovementHistory3D.encode_commands([command]) == command_bytes, "command matches independent little-endian fixture including exact 64-bit sequence")
	expect(MovementHistory3D.decode_commands(command_bytes) == [command], "command fixture decodes exactly")
	var fixture := sample(1.25, Vector3(1, 2, 3), 7)
	fixture.velocity = Vector3(-4, 5, -6)
	fixture.yaw = 0.5
	fixture.ack = 9007199254740993
	fixture.crouching = true
	fixture.jumping = true
	var sample_bytes := "000000000000f43f0000803f0000004000004040000080c00000a0400000c0c00000003f0100000000002000070000000000803e0107".hex_decode()
	expect(MovementHistory3D.encode_samples([fixture]) == sample_bytes, "sample matches independent 54-byte fixture")
	expect(MovementHistory3D.decode_samples(sample_bytes) == [fixture], "sample fixture preserves ack and all flags")
	expect(history.push_packet(sample_bytes) and history.to_packet() == sample_bytes, "direct packet ingestion and output preserve bytes")
	fixture.position = Vector3(100, 100, 100)
	var snapshot := history.get_latest_sample()
	snapshot.position = Vector3(200, 200, 200)
	expect(history.get_latest_sample().position == Vector3(1, 2, 3), "input and output dictionaries cannot mutate history")
	expect(not history.push_packet(sample_bytes), "duplicate packet cannot rewind history")
	for offset in [52, 53]:
		var bad := sample_bytes.duplicate()
		bad[offset] = 255
		expect(not history.push_packet(bad) and MovementHistory3D.decode_samples(bad).is_empty(), "invalid sample flags rejected")
	for offset in [0, 8, 20, 32, 48]:
		var bad := sample_bytes.duplicate()
		if offset == 0: bad.encode_double(offset, NAN)
		else: bad.encode_float(offset, INF)
		expect(not history.push_packet(bad), "nonfinite packet field rejected")
	var oversized := sample_bytes.duplicate()
	oversized.resize(MovementHistory3D.SAMPLE_BYTES * 25)
	expect(not history.push_packet(oversized), "oversized packet rejected")
	expect(not history.push_packet(sample_bytes.slice(0, 53)), "truncated packet rejected")
	expect(not history.push_packet(PackedByteArray()), "empty packet rejected")
	expect(history.get_sample_count() == 1 and history.to_packet() == sample_bytes, "malformed input leaves history untouched")
	var bad_command := command_bytes.duplicate()
	bad_command[28] = 16
	expect(MovementHistory3D.decode_commands(bad_command).is_empty(), "unknown command flags rejected")
	bad_command = command_bytes.duplicate()
	bad_command.encode_float(16, INF)
	expect(MovementHistory3D.decode_commands(bad_command).is_empty(), "nonfinite command rejected")
	bad_command = command_bytes.duplicate()
	bad_command[7] = 128
	expect(MovementHistory3D.decode_commands(bad_command).is_empty(), "negative signed sequence rejected")
	expect(MovementHistory3D.encode_commands([command, command]).is_empty(), "unordered commands rejected")
	command.jump_id = command.sequence + 1
	expect(MovementHistory3D.encode_commands([command]).is_empty(), "impossible jump counter rejected")
	history.clear()
	var left := sample(1.0)
	var right := sample(2.0, Vector3(2, 0, 0))
	left.yaw = deg_to_rad(179.0)
	right.yaw = deg_to_rad(-179.0)
	left.gait_phase = 0.9
	right.gait_phase = 0.1
	right.grounded = false
	right.jumping = true
	right.gait_advancing = false
	expect(history.push_samples([left, right]), "two samples accepted")
	var middle := history.sample_at(1.5)
	expect(middle.position == Vector3.RIGHT, "position interpolates")
	expect(absf(absf(middle.yaw) - PI) < 0.00001, "yaw crosses shortest arc")
	expect(minf(middle.gait_phase, 1.0 - middle.gait_phase) < 0.00001, "gait phase wraps across zero")
	expect(middle.grounded and not middle.jumping and middle.gait_advancing, "discrete state holds left sample")
	expect(not history.sample_at(2.0).gait_advancing, "gait advancement selects right at interval endpoint")
	expect(history.sample_at(0.0) == left, "sampling before history holds first")
	expect(history.sample_at(3.0) == right, "sampling after history holds last")
	expect(history.advance(100.0).position == right.position, "packet loss holds last position without extrapolation")
	expect(history.advance(-1.0).is_empty() and history.advance(NAN).is_empty(), "invalid delta rejected")
	expect(history.sample_at(INF).is_empty(), "invalid sample time rejected")
	var invalid := sample(4.0)
	invalid.position = Vector3(NAN, 0, 0)
	expect(not history.push_samples([sample(3.0), invalid]) and history.get_sample_count() == 2, "batch validation is atomic")
	invalid = sample(4.0)
	invalid.jumping = 1
	expect(not history.push_sample(invalid), "wrong flag type rejected")
	expect(not history.push_samples([sample(4.0), sample(3.0)]), "descending timestamps rejected")
	expect(not history.push_samples([sample(3.0), sample(3.0)]), "equal timestamps rejected")
	var teleport := sample(3.0, Vector3(100, 0, 0), 1)
	expect(history.push_samples([right, teleport]) and history.get_sample_count() == 1, "epoch transition removes old path")
	expect(history.advance(0.0) == teleport, "teleport resets playback cursor")
	history.clear()
	for index in 50:
		expect(history.push_sample(sample(float(index), Vector3(index, 0, 0))), "ring accepts ordered sample")
	expect(history.get_sample_count() == 24, "ring stays bounded")
	expect(history.sample_at(0.0).time == 26.0 and history.get_latest_sample().time == 49.0, "ring evicts oldest sample in order")
	var full := history.to_packet()
	expect(full.size() == 1296, "full history remains within existing packet budget")
	var replica := MovementHistory3D.new()
	expect(replica.push_packet(full) and replica.to_packet() == full, "wrapped ring serializes chronologically")
	var near_wrap := sample(50.0)
	near_wrap.gait_phase = 0.999999999
	expect(replica.push_packet(MovementHistory3D.encode_samples([near_wrap])), "float quantization cannot turn a valid phase into invalid 1.0")
	replica.clear()
	replica.render_delay = 0.25
	expect(replica.push_samples([sample(0.0), sample(1.0, Vector3.RIGHT)]), "custom delay history accepted")
	expect(replica.advance(0.0).position.is_equal_approx(Vector3(0.75, 0, 0)), "render delay sets initial cursor")
	var scene := PackedScene.new()
	expect(scene.pack(replica) == OK, "native history can be packed as a scene node")
	var restored := scene.instantiate() as MovementHistory3D
	expect(restored != null and restored.render_delay == 0.25 and restored.get_sample_count() == 0, "scene restores settings without persisting runtime samples")
	restored.free()
	history.free()
	replica.free()
	if failures == 0: print("MovementHistory3D native contract passed")
	quit(0 if failures == 0 else 1)
