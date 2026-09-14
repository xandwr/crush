# Rope3D acceptance

Run this project with the matching native engine. The scene shows a hanging rope,
a cable between hard anchors, an independently posed tube with coincident points,
and a simulated rope under mirrored nonuniform scale. All use standard materials.

Use `-- --rope-capture` to capture the settled scene and exit. Run separately with
Forward+, Mobile, and Compatibility. Captures go to the project's user directory.

Use `-- --rope-performance` for one-rope and 64-rope samples at 9 and 33 particles,
256 Hz, and six solver iterations. Each sample warms for 32 ticks and measures 128
ticks. Variants separate solver-only, solver plus tube updates, and collision plus
tube updates. Reported CPU time includes the GDScript loop and synchronous native
advance; it excludes asynchronous GPU execution. Retained memory changes are not
allocation counts. Headless runs use the dummy renderer and cannot measure real
GPU upload costs.

The P.P. `tests/physics/rope_3d_contract.gd` checks sweeps, overlap recovery, moving
floor recovery, constraint-correction contacts, masks, exclusions, and hard-pin
conflicts during actual physics callbacks. Run that contract on Jolt Physics and
Godot Physics. Collision is one-way particle collision, not full-span or self
collision.
