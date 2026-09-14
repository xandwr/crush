# Recorded validation

2026-09-13, Windows 11, Ryzen 9 7900X, RTX 4070 Ti. Engine baseline is
`caa9454ede`, including Mirror3D. Development editor, debug optimization, 256 Hz.

Native tests cover mass distribution, arc-length sampling, collapsed particles,
hard/soft/intermediate pins, overstretch, rest-angle bend, scheduling, topology,
reset, node removal, world-space state under parent motion, posed isolation,
persistent topology, cap winding, normals, UVs, and bounds.

Quantitative solver tolerances at 5, 9, and 17 particles and 60, 120, and 256 Hz:

- Hard hanging chain after eight seconds and 64 iterations: segment error below
  3 mm and endpoint within 35 mm of the one-meter vertical equilibrium.
- Uniform one-kilogram hanging chain with 0.01 m/N whole-chain compliance:
  endpoint within 6 mm of the analytic 1.049-meter equilibrium.
- Whole-chain stretch compliance 0.001 m/N against a 0.01 m/N soft endpoint:
  endpoint within 40 mm of the 1.090909-meter equilibrium at 64 iterations,
  improving to within 12 mm with the internal 256-iteration reference solve.
  Public node iteration limits remain 64. Finite convergence error is measurable.
- Small-angle cantilever under 0.98 m/s squared gravity with inverse flexural
  rigidity 0.2 per N m squared: endpoint within 8 mm of the discrete analytic
  equilibrium using the internal 1024-iteration reference. This verifies bend
  compliance scaling, not node accuracy at its bounded iteration count. Stiff
  bend configurations require adequate substeps and iterations; convergence
  error can be large even with 256 internal iterations at coarse tick rates.

GPU captures inspected on Compatibility/OpenGL, Forward+/Vulkan, and
Mobile/Vulkan show hanging, anchored cable, posed/coincident-point tubes, and
mirrored nonuniform scale with standard materials. Native P.P. viewmodel GPU
checks cover exact grips, projection independence, camera isolation, world
lighting, and shadow reception on all three rendering methods.

Development CPU measurements, six iterations, 128 sampled ticks after 32 warmup
ticks. Values include the GDScript loop and synchronous native updates. They
exclude asynchronous GPU execution and are not release performance claims.

| Particles | Ropes | Solver only, us/tick | Tube enabled, us/tick | Collision and tube, us/tick |
| --- | --- | --- | --- | --- |
| 9 | 1 | 19.4 | 81.3 | 216.0 |
| 9 | 64 | 735.8 | 3913.9 | 12649.6 |
| 33 | 1 | 50.5 | 170.8 | 681.1 |
| 33 | 64 | 2543.0 | 9260.7 | 41983.9 |

All samples showed zero retained static-memory growth. Allocation counts were
not measured. No comparison with the old nine-point shader was performed.
At 256 Hz the entire tick budget is 3906 us; crowded collision samples exceed
that budget. Arms use two nine-particle ropes with collision off. Keep collision
opt-in and choose rope density, iterations, and scene population from profiling.

The physics contract exercises floors, walls, concave geometry, starting overlap,
fast particles, moving collider overlap, projection contacts, friction, masks,
exclusions, and hard-pin conflicts. Particle collision does not guarantee full
span collision, moving-obstacle continuous collision, or knot preservation.

Interactive multiplayer playtesting and allocation-count profiling remain
separate validation boundaries. Automated network contracts are not a substitute
for visual inspection of a real multiplayer session.
