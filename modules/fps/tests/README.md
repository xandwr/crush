# MovementHistory3D validation

Run the native class contract through the compiled engine's public bindings:

```powershell
bin/godot.windows.editor.x86_64.console.exe --headless --path modules/fps/tests --script test_movement_history_3d.gd
```

The contract checks independent little-endian byte fixtures, signed 64-bit IDs,
packet bounds and flags, atomic rejection, ring wrap, snapshot isolation,
interpolation, loss holding, teleport resets, and node disposal. No game project
or autoloads are required. The module can be excluded with `module_fps_enabled=no`.
