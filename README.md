# godot but cooler

it's the `4.7` branch of Godot upstream, with a few custom rendering features.

## Custom features

| Feature | What it does |
| --- | --- |
| Mirror shadows | Fixes directional shadow cascades for asymmetric camera frustums, so mirrors render without borked shadows. |
| [Procedural normals](doc/classes/BaseMaterial3D.xml) | Native normal map generation from albedo textures, with adjustable strength, smoothing, and height inversion. Enable Procedural Normal on a material to add surface relief without authoring a separate normal map; generated textures update when the source or settings change, and existing normal map settings are restored when disabled. Also available through [Image.generate_normal_map()](doc/classes/Image.xml). |
| [Viewmodel3D](doc/classes/Viewmodel3D.xml) | Renders first-person arms, weapons, and held objects in a separate pass with their own FOV and clipping planes, while keeping world lighting and shadow reception. |
| [Camera shadow mask](doc/classes/Camera3D.xml) | `additional_shadow_cull_mask` lets geometry excluded from a camera's view still cast shadows. |
| [Mirror3D](doc/classes/Mirror3D.xml) | Native planar mirrors with independent camera reflections, world lighting and shadows, and a bounded single bounce. [Rendering checks](tests/rendering/mirror_3d/README.md). |
| [MovementHistory3D](modules/fps/doc_classes/MovementHistory3D.xml) | Native bounded movement histories, delayed interpolation, and compact command/sample packets. Optional FPS module with [binding contracts](modules/fps/tests/README.md). |
| [CharacterMotor3D](modules/fps/doc_classes/CharacterMotor3D.xml) | Explicit character movement with MovementSettings, directional air acceleration, stair stepping, floor attachment, and height-based crouch decisions. Projects supply stance geometry and clearance. |
| [WeaponSimulation](modules/fps/doc_classes/WeaponSimulation.xml) | Native per-firearm cadence, ammunition, reload/equip timers and snapshot replay, configured by WeaponSimulationSettings. |
| [Hitscan3D](modules/fps/doc_classes/Hitscan3D.xml) | Native first-hit physics traces with range and linear damage falloff from HitscanSettings. Input, authority, and damage application belong to the game. |
