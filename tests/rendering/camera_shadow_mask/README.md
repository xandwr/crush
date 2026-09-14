# Camera shadow mask rendering regression

Run with a graphical display and the patched editor binary:

```
godot --path tests/rendering/camera_shadow_mask --script run.gd --rendering-method gl_compatibility
godot --path tests/rendering/camera_shadow_mask --script run.gd --rendering-method forward_plus
```

The test renders a transparent SubViewport and compares GPU-produced images for directional shadows (single and four cascades), both omni shadow modes, and spot shadows. It verifies excluded caster geometry remains invisible, additional caster layers produce shadows, moving casters update, node visibility and light caster masks remain authoritative, and resetting the extra mask restores legacy behavior. It also checks scene serialization and two cameras sharing one World3D with independent shadow masks. Headless rendering is not sufficient for this test.

Compatibility skips dual-paraboloid omni shadows, which that renderer does not support. Baseline and shadowed PNGs are saved in the project user data directory for visual inspection.
