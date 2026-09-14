extends SceneTree

var failures := 0

func expect(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		printerr("FAIL: " + message)

func settings(automatic := false) -> WeaponSimulationSettings:
	var spec := WeaponSimulationSettings.new()
	spec.fire_mode = WeaponSimulationSettings.AUTOMATIC if automatic else WeaponSimulationSettings.SEMI_AUTOMATIC
	spec.magazine_capacity = 3
	spec.shot_interval = 0.2
	spec.reload_duration = 0.3
	spec.equip_delay = 0.0
	return spec

func weapon(spec: WeaponSimulationSettings, ammo := -1, reserve := 5) -> WeaponSimulation:
	var runtime := WeaponSimulation.new()
	expect(runtime.configure(spec, 10, ammo, reserve), "configure")
	runtime.set_equipped(true)
	return runtime

func _initialize() -> void:
	var spec := settings()
	var runtime := weapon(spec)
	expect(runtime.step(0, true) == WeaponSimulation.SHOT, "semi press fires immediately")
	var snapshot := runtime.get_snapshot()
	expect(runtime.step(0, true) == -1 and runtime.step(2, true) == -1, "duplicate and skipped ticks rejected")
	expect(runtime.get_snapshot() == snapshot, "rejected ticks are atomic")
	expect(runtime.step(1, true) == 0 and runtime.step(2, true) == 0, "semi hold never repeats")
	runtime.step(3, false)
	expect(runtime.step(4, true) == WeaponSimulation.SHOT, "release rearms semi")
	expect(runtime.get_magazine() == 1, "exact ammo consumption")
	var automatic := weapon(settings(true))
	var shots: Array[int] = []
	for tick in range(8):
		if automatic.step(tick, true) & WeaponSimulation.SHOT: shots.append(tick)
	expect(shots == [0, 2, 4] and automatic.get_magazine() == 0, "automatic tick cadence and empty stop")
	automatic.step(8, false)
	expect(automatic.step(9, true) == WeaponSimulation.DRY_FIRE and automatic.step(10, true) == 0, "dry fire once per press")
	var partial := weapon(spec, 1, 1)
	expect(partial.step(0, false, true) == WeaponSimulation.RELOAD_STARTED, "partial reload starts")
	expect(partial.step(1, true) == 0 and partial.step(2, true) == 0, "reload blocks fire")
	expect(partial.step(3, true) == WeaponSimulation.RELOAD_COMPLETED, "semi held during reload does not fire on completion")
	expect(partial.get_magazine() == 2 and partial.get_reserve() == 0, "partial reserve transferred exactly at completion")
	expect(partial.step(4, false, true) == 0, "no reserve cannot reload")
	var cancel := weapon(spec, 1)
	cancel.step(0, false, true)
	cancel.set_equipped(false)
	expect(not cancel.is_reloading() and cancel.get_magazine() == 1 and cancel.get_reserve() == 5, "holster cancels without transferring ammo")
	cancel.step(1, true)
	cancel.set_equipped(true)
	expect(cancel.step(2, true) == 0, "equip does not manufacture semi press")
	var cooldown := weapon(settings(true))
	cooldown.step(0, true)
	cooldown.set_equipped(false)
	cooldown.set_equipped(true)
	expect(cooldown.step(1, true) == 0 and cooldown.step(2, true) == WeaponSimulation.SHOT, "switch cannot bypass cooldown")
	var delayed_spec := settings(true)
	delayed_spec.equip_delay = 0.2
	var delayed := weapon(delayed_spec)
	expect(delayed.step(0, true) == 0 and delayed.step(1, true) == 0 and delayed.step(2, true) == WeaponSimulation.SHOT, "equip waits full rounded duration")
	var isolated := weapon(spec)
	spec.magazine_capacity = 100
	spec.fire_mode = WeaponSimulationSettings.AUTOMATIC
	isolated.step(0, true)
	expect(isolated.step(1, true) == 0 and isolated.get_magazine() == 2, "configured settings are copied")
	var replay := weapon(settings(true), 1)
	replay.step(0, false, true)
	snapshot = replay.get_snapshot()
	var expected_events: Array[int] = []
	for tick in range(1, 7): expected_events.append(replay.step(tick, true))
	var expected_state := replay.get_snapshot()
	expect(replay.restore_snapshot(snapshot), "restore earlier reload state")
	var actual_events: Array[int] = []
	for tick in range(1, 7): actual_events.append(replay.step(tick, true))
	expect(actual_events == expected_events and replay.get_snapshot() == expected_state, "replay reproduces state and events")
	for field in ["magazine", "reserve", "cooldown", "reload_remaining", "equip_remaining", "last_tick"]:
		var invalid := expected_state.duplicate(true)
		invalid[field] = -2
		expect(not replay.restore_snapshot(invalid) and replay.get_snapshot() == expected_state, "reject invalid " + field)
	var bad := expected_state.duplicate(true)
	bad.configuration.tick_rate = 11
	expect(not replay.restore_snapshot(bad), "reject mismatched simulation configuration")
	bad = expected_state.duplicate(true)
	bad.magazine = 1.0
	expect(not replay.restore_snapshot(bad), "reject wrong numeric type")
	bad = expected_state.duplicate(true)
	bad.equipped = false
	bad.reload_remaining = 1
	expect(not replay.restore_snapshot(bad), "reject holstered reload")
	expect(not replay.configure(null, 10) and not replay.reset(4) and replay.get_snapshot() == expected_state, "invalid configure and reset preserve state")
	expect(replay.reset(0, 4) and replay.get_last_tick() == -1 and not replay.is_equipped() and replay.get_magazine() == 0, "explicit reset is silent and holstered")
	var invalid_spec := settings()
	invalid_spec.shot_interval = NAN
	expect(not invalid_spec.validation_errors().is_empty() and not replay.configure(invalid_spec, 10), "nonfinite timing rejected")
	var fractional := settings(true)
	fractional.shot_interval = 0.21
	var quantized := weapon(fractional)
	expect(quantized.step(0, true) == WeaponSimulation.SHOT and quantized.step(1, true) == 0 and quantized.step(2, true) == 0 and quantized.step(3, true) == WeaponSimulation.SHOT, "fractional durations round up")
	var full := weapon(settings())
	expect(full.step(0, false, true) == 0, "full magazine cannot reload")
	var buffered := weapon(settings())
	buffered.step(0, true)
	buffered.step(1, false)
	buffered.set_equipped(false)
	buffered.step(2, true)
	buffered.set_equipped(true)
	expect(buffered.step(3, true) == 0, "holstered presses do not buffer semi fire")
	var slow_spec := settings()
	slow_spec.shot_interval = 0.3
	var slow := weapon(slow_spec)
	slow.step(0, true)
	slow.step(1, false)
	expect(slow.step(2, true) == 0 and slow.step(3, true) == 0, "semi press during cooldown is consumed without buffering")
	var stable := full.get_snapshot()
	var edited := full.get_snapshot()
	edited.configuration.capacity = 100
	edited.magazine = 100
	expect(full.get_snapshot() == stable, "snapshots are independent")
	for rate in [0, 1001]: expect(not full.configure(settings(), rate), "invalid tick rate rejected")
	var changed_spec := settings()
	changed_spec.magazine_capacity = 0
	expect(not full.configure(changed_spec, 10), "invalid capacity rejected")
	changed_spec.magazine_capacity = 3
	changed_spec.reload_duration = INF
	expect(not full.configure(changed_spec, 10), "invalid reload timing rejected")
	changed_spec.reload_duration = 0.3
	changed_spec.equip_delay = -1
	expect(not full.configure(changed_spec, 10), "invalid equip timing rejected")
	changed_spec.equip_delay = 0
	changed_spec.fire_mode = 2
	expect(not full.configure(changed_spec, 10) and full.get_snapshot() == stable, "invalid mode rejected atomically")
	var roundtrip := settings(true)
	expect(ResourceSaver.save(roundtrip, "user://weapon_settings.tres") == OK, "save native settings")
	var loaded := ResourceLoader.load("user://weapon_settings.tres", "", ResourceLoader.CACHE_MODE_IGNORE) as WeaponSimulationSettings
	expect(loaded != null and loaded.fire_mode == roundtrip.fire_mode and loaded.reload_duration == roundtrip.reload_duration, "native settings reopen")
	var copy := roundtrip.duplicate() as WeaponSimulationSettings
	copy.magazine_capacity = 2
	expect(roundtrip.magazine_capacity == 3, "native settings duplicate independently")
	if failures == 0: print("Weapon simulation contract passed")
	quit(0 if failures == 0 else 1)
