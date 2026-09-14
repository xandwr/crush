# Mirror3D rendering regressions

Run from the engine checkout in PowerShell. GPU runs open a window and exit with a nonzero status on a failed assertion. Headless validation does not replace these runs.

```powershell
uv tool run scons platform=windows target=editor arch=x86_64 dev_build=yes tests=yes fast_unsafe=no
& bin/godot.windows.editor.dev.x86_64.console.exe --headless --test --test-case='*Mirror3D*,*Camera3D*,*Viewmodel3D*,*Projection*'
gdkit check tests/rendering/mirror_3d --godot P:/godot/bin/godot.windows.editor.dev.x86_64.exe
python doc/tools/make_rst.py doc/classes --filter Mirror3D --dry-run
```

Wait for each build to finish before launching its executable. Windows cannot replace a running launcher.

```powershell
$mirrorEngine = 'P:/godot/bin/godot.windows.editor.dev.x86_64.console.exe'
foreach ($mirrorRenderer in @('gl_compatibility', 'forward_plus', 'mobile')) {
    $mirrorDriver = if ($mirrorRenderer -eq 'gl_compatibility') { 'opengl3' } else { 'vulkan' }
    $mirrorOutput = "P:/godot/.scratchpad/mirror-captures/$mirrorRenderer"
    & $mirrorEngine --path tests/rendering/mirror_3d --rendering-method $mirrorRenderer --rendering-driver $mirrorDriver --resolution 640x480 -- --output $mirrorOutput
    if ($LASTEXITCODE) { throw "Basic regression failed: $mirrorRenderer" }
    & $mirrorEngine --path tests/rendering/mirror_3d advanced.tscn --rendering-method $mirrorRenderer --rendering-driver $mirrorDriver --resolution 640x480 -- --output $mirrorOutput
    if ($LASTEXITCODE) { throw "Advanced regression failed: $mirrorRenderer" }
    & $mirrorEngine --path tests/rendering/mirror_3d performance.tscn --rendering-method $mirrorRenderer --rendering-driver $mirrorDriver --resolution 640x480
    if ($LASTEXITCODE) { throw "Performance regression failed: $mirrorRenderer" }
}
& $mirrorEngine --editor --path tests/rendering/mirror_3d --rendering-method gl_compatibility -- --mirror-editor-qa --output P:/godot/.scratchpad/mirror-captures/editor
```

The editor plugin is inert unless `--mirror-editor-qa` is supplied. Open `editor_preview.tscn` normally for interactive Inspector and gizmo testing.

## Coverage

The basic scene checks asymmetric colored markers against projected virtual positions, plane clipping, world occlusion, two cameras, perspective/orthographic/asymmetric projections, viewport resizing, world changes, reparenting, scene reload, visibility, enable toggles, camera deletion, nonuniform scaling, and source/reflection visibility masks.

The advanced scene checks directional shadows with one and four cascades, omni and spot shadows with changing atlas size/precision, tilted surfaces, private/public Viewmodel3D geometry, facing-mirror black fallback, exposure against an independent ordinary-camera control, render scaling and 4x MSAA, moving geometry, GPU particles, alpha blending, alpha cutout, and emissive content with glow. It also retains close and grazing-view captures for inspection.

The editor check selects the native node in the Inspector, changes size and enabled state, captures its surface/gizmo, and checks serialization without internal children. Inspector and gizmo captures were inspected visually. Manual dragging and undo/redo interaction were not exercised.

The performance scene contains four visible mirrors, two cameras, and 128 separately materialized objects. Each reflection draws those objects and four fallback surfaces. The increase of 1056 draw calls confirms eight reflection views; back-facing surfaces add no reflection draws. Timings use 120 steady frames with VSync disabled and include CPU, GPU, presentation, and scheduling overhead. They are informational, not a hardware-independent performance threshold. Scheduling variability was visible in the Vulkan baselines; back-facing runs measured 0.421 / 0.690 / 0.602 ms per frame for Compatibility / Forward+ / Mobile.

## Recorded results

Windows 11, RTX 4070 Ti, NVIDIA OpenGL 616.92 / Vulkan 1.4.351, conservative dev build, September 13, 2026:

| Renderer | Basic | Advanced | Performance | Disabled ms/frame | Four mirrors / two cameras ms/frame |
| --- | --- | --- | --- | --- | --- |
| Compatibility / OpenGL | Pass | Pass | Pass | 0.407 | 3.131 |
| Forward+ / Vulkan | Pass | Pass | Pass | 1.718 | 3.273 |
| Mobile / Vulkan | Pass | Pass | Pass | 1.608 | 2.214 |

All three performance runs reported 8 baseline draw calls, 1064 reflected draw calls, and exactly 8 inferred reflection views. The related unit suite passed 43 cases and 427 assertions. XML documentation and project validation passed without project-owned warnings. The automated editor check passed on Compatibility. The standard conservative editor build and a Compatibility rendered smoke test also passed.

Representative retained images are in `captures/`. Full run captures and logs are kept locally in `.scratchpad/mirror-captures/` and `.scratchpad/mirror-*.log`.

Vulkan reports a loader registry lookup warning before renderer startup on this machine. Both Vulkan renderers completed the GPU checks; the warning is outside project scripts and the Mirror3D rendering path. Physical mobile devices, other operating systems, D3D12, XR/stereo, and exhaustive screen/depth-reading shader combinations remain unverified.

## Implementation boundaries

Reflection views belong to the renderer and are keyed by mirror instance, source camera, and source viewport. Their render buffers, HDR targets, and positional shadow atlases are released on camera/viewport deletion, mirror disable/removal/world changes, or after 120 rendered frames without use. Steady dimensions reuse allocations. Reflections complete before the source draw; all mirror materials are explicitly rebound and flushed per consuming view. Reflected draws use black fallback and cannot schedule more mirror renders.

The reflected camera preserves the source projection and reflects its full transform. A camera-space fragment clip plane excludes geometry behind the mirror; standard renderer winding support handles reflected cameras. Light-culling silhouette winding also follows camera handedness. Reflection buffers inherit source MSAA, and positional shadow atlases inherit source size, precision, and subdivisions. Reflection color skips exposure, tonemapping, glow, and camera depth-of-field before the consuming view applies its effects once. Compatibility retains its normal internal sRGB encoding and albedo conversion; RD reflection targets contain linear HDR color.

Screen/depth-reading shaders see reflection buffers, not the source camera's buffers. Clipping happens in the fragment pass, so geometry behind the mirror can still incur vertex/culling work. XR shows black fallback. Recursive rendering, collisions, ricochets, curved mirrors, and portal traversal are outside V1.
