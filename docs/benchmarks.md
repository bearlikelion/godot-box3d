---
title: Benchmarks
---

# Benchmarks

## Box drop

The demo project's benchmark scene drops boxes onto a fixed lattice from a fixed seed, 36 at a time every 0.5s, until 4096 bodies are in the world.
Same scene, same placement, same seed on every backend, so the only variable is the physics engine.

Run on Linux, Godot 4.7.2.rc, 4096 boxes over 57s.
Lower is better except for the first column.
These numbers predate the multithreaded solver, so the Box3D column was measured with a single worker; with threading enabled its step time at high body counts improves by roughly 1.5x.

| Backend | Bodies at 16.66 ms | Median step | p95 step | Peak step | Memory |
|---|---|---|---|---|---|
| **Box3D Physics** | **1404** | **27.3 ms** | **63.1 ms** | **66.8 ms** | **117 MB** |
| Jolt Physics | 1080 | 56.9 ms | 99.9 ms | 139.9 ms | 150 MB |
| Godot Physics | 1044 | 54.6 ms | 235.5 ms | 306.0 ms | 177 MB |
| Rapier3D | 684 | 79.2 ms | 249.3 ms | 303.2 ms | 177 MB |

"Bodies at 16.66 ms" is how many boxes were in the world when the physics step first blew the 60 FPS budget.
None of the four recover below budget afterward, so it is a stable knee rather than a one-frame spike.

Box3D held the frame budget longest and stayed the most consistent under load: its p95 is close to its peak, while Godot Physics and Rapier both spike to roughly 4x their median.
That tail matters more than the median for a game, since it is what shows up as a stutter.

## Caveats

All four are past the budget well before 4096 bodies, so this measures how they degrade under overload, not a workload any of them handles comfortably.
A dense box pile is also only one workload: it rewards a solver tuned for stacked contacts and says nothing about raycasts, character controllers, or joints.
Take it as a starting point, not a ranking.

## Graphs

| Box3D Physics | Jolt Physics |
|---|---|
| [![Box3D benchmark]({{ '/images/benchmarks/box3d.png' | relative_url }})]({{ '/images/benchmarks/box3d.png' | relative_url }}) | [![Jolt benchmark]({{ '/images/benchmarks/jolt.png' | relative_url }})]({{ '/images/benchmarks/jolt.png' | relative_url }}) |
| **Godot Physics** | **Rapier3D** |
| [![Godot Physics benchmark]({{ '/images/benchmarks/godot-physics.png' | relative_url }})]({{ '/images/benchmarks/godot-physics.png' | relative_url }}) | [![Rapier3D benchmark]({{ '/images/benchmarks/rapier.png' | relative_url }})]({{ '/images/benchmarks/rapier.png' | relative_url }}) |

## Reproducing

Open the demo project, pick a backend on the main menu, and run the benchmark scene.
The export button writes a PNG and a per-frame CSV to `user://`.
