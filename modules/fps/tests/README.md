# FPS module validation

Run the native class contract through the compiled engine's public bindings:

```powershell
bin/godot.windows.editor.x86_64.console.exe --headless --path modules/fps/tests --script test_movement_history_3d.gd
```

The contract checks independent little-endian byte fixtures, signed 64-bit IDs,
packet bounds and flags, atomic rejection, ring wrap, snapshot isolation,
interpolation, loss holding, teleport resets, and node disposal. No game project
or autoloads are required. The module can be excluded with `module_fps_enabled=no`.

## WeaponSimulation

```powershell
bin/godot.windows.editor.x86_64.console.exe --headless --path modules/fps/tests --script test_weapon_simulation.gd
```

Checks semi/automatic cadence, consecutive ticks, ammunition conservation,
reload completion/cancellation, equip delay, switching cooldown, settings
isolation, replay, malformed snapshots, and Resource save/reopen/duplication.
Timing is rounded up to fixed ticks. Gameplay authority and presentation are
outside this class. Replay returns events again; callers deduplicate effects.

## Hitscan3D

```powershell
bin/godot.windows.editor.x86_64.console.exe --headless --path modules/fps/tests --script test_hitscan_3d.gd
```

Checks nearest-hit obstruction, masks, RID exclusions, optional areas, inside-solid
origins, concave faces, back faces, finite range, miss sentinels, invalid queries,
linear falloff boundaries, and Resource duplication/save/reopen. Face indices are
compared with the active backend's raw physics query, not render mesh indices.
Run with either Godot Physics or Jolt Physics selected in the test project.

## CharacterMotor3D

```powershell
bin/godot.windows.editor.x86_64.console.exe --headless --path modules/fps/tests --script test_character_motor_3d.gd
```

Checks movement settings, invalid-step rejection, stair ascent/descent, tall obstacles,
jump release from floor attachment, stance clearance, and airborne crouch transitions.
The fixture uses a flat-bottom hull and runs on Godot Physics and Jolt. The project
supplies collision geometry and a synchronous clearance callback; the motor owns
stance decisions. Rounded hulls require a walkable contact at the candidate landing.
