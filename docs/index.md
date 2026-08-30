---
title: godot-box3d
permalink: /
---

# godot-box3d

A [GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html) that integrates [Box3D](https://github.com/erincatto/box3d), Erin Catto's 3D physics engine, into Godot 4 as a drop-in replacement for the built-in `PhysicsServer3D`.

Stock Godot physics nodes keep working. You change a project setting, not your scenes.

> **Status: early and experimental.** Box3D itself is a young engine, and this extension is a work in progress. Expect missing features and rough edges.

- [Installation]({{ '/installation/' | relative_url }})
- [Demos]({{ '/demos/' | relative_url }})
- [Behavior differences]({{ '/behavior-differences/' | relative_url }})
- [Comparison with box3d-godot]({{ '/comparison/' | relative_url }})
- [Building from source]({{ '/building/' | relative_url }})

## Why this exists

I'm building [SurfsUp](https://store.steampowered.com/app/3454830/SurfsUp/), a recreation of Source-engine "SkillSurf" in Godot. Surf maps are hard on a physics engine: long ramps built from concave trimesh, corners taken at speed, and head surf along the underside of geometry. Godot's built-in physics and godot-jolt both have trouble with parts of that.

Box3D is worth trying because of where it came from. [Erin Catto](https://x.com/erin_catto) started it from Valve's Rubikon-lite, the physics engine of Source 2 ([Announcing Box3D](https://box2d.org/posts/2026/06/announcing-box3d/#valve-to-the-rescue)), so a Rubikon-lineage solver seemed like a reasonable bet.

## What works

- Rigid, static, and kinematic bodies
- Shapes: box, sphere, capsule, cylinder, convex polygon, concave polygon (trimesh), heightmap, and world boundary
- Areas, including overlap events, gravity/damping overrides, priority ordering, and point gravity
- Direct space state queries: ray casts, point and shape intersection, shape casts (`cast_motion`), `collide_shape`, and `rest_info`
- `body_test_motion`, so `CharacterBody3D` and `move_and_slide()` work
- Contact monitoring, so `RigidBody3D` reports real contact points, normals, and impulses
- Per-pair collision exceptions
- Joints: pin, hinge, and slider (pin anchors can be moved after creation)
- Multithreaded solver: auto-detects physical cores, overridable via `physics/box3d/worker_count`

## What's left

- Separation ray shapes
- ConeTwist joints
- `Generic6DOFJoint3D` (Box3D has no per-axis lock/limit/motor constraint, so there is no faithful mapping)
- `SoftBody3D`
- Per-shape indices in query and contact results (multi-shape bodies always report shape 0)
- Solver profiling
- macOS notarization and runtime validation of both architecture slices

## Benchmark

A 4096-box drop test, same scene and seed on every backend. Box3D held the 60 FPS budget longest and had the tightest tail.

| Backend | Bodies at 16.66 ms | Median step | p95 step | Peak step | Memory |
|---|---|---|---|---|---|
| **Box3D Physics** | **1404** | **27.3 ms** | **63.1 ms** | **66.8 ms** | **117 MB** |
| Jolt Physics | 1080 | 56.9 ms | 99.9 ms | 139.9 ms | 150 MB |
| Godot Physics | 1044 | 54.6 ms | 235.5 ms | 306.0 ms | 177 MB |
| Rapier3D | 684 | 79.2 ms | 249.3 ms | 303.2 ms | 177 MB |

One workload on one machine, and all four are past budget well before 4096 bodies, so this measures degradation under overload rather than comfortable capacity. The Box3D column predates the multithreaded solver and was measured with a single worker; threading improves its step time at high body counts by roughly 1.5x. See the [README](https://github.com/bearlikelion/godot-box3d#benchmarks) for the full caveats.

## License

MIT. Box3D is likewise [MIT licensed](https://github.com/erincatto/box3d/blob/main/LICENSE). This project takes structural inspiration from [godot-jolt](https://github.com/godot-jolt/godot-jolt).
