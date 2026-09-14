# godot but cooler

it's the `4.7` branch of Godot upstream, with a few custom rendering features.

## Custom features

| Feature | What it does |
| --- | --- |
| Mirror shadows | Fixes directional shadow cascades for asymmetric camera frustums, so mirrors render without borked shadows. |
| [Viewmodel3D](doc/classes/Viewmodel3D.xml) | Renders first-person arms, weapons, and held objects in a separate pass with their own FOV and clipping planes, while keeping world lighting and shadow reception. |
| [Camera shadow mask](doc/classes/Camera3D.xml) | `additional_shadow_cull_mask` lets geometry excluded from a camera's view still cast shadows. |
| [Mirror3D](doc/classes/Mirror3D.xml) | Native planar mirrors with independent camera reflections, world lighting and shadows, and a bounded single bounce. [Rendering checks](tests/rendering/mirror_3d/README.md). |
| [MovementHistory3D](modules/fps/doc_classes/MovementHistory3D.xml) | Native bounded movement histories, delayed interpolation, and compact command/sample packets. Optional FPS module with [binding contracts](modules/fps/tests/README.md). |
| [WeaponSimulation](modules/fps/doc_classes/WeaponSimulation.xml) | Native per-firearm cadence, ammunition, reload/equip timers and snapshot replay, configured by WeaponSimulationSettings. |
