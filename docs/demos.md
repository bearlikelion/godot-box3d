---
title: Demos
---

# Demos

The `test_project/` directory is a runnable Godot project with one scene per feature, plus a benchmark. Open it in Godot and run `main.tscn` for the hub, or press F6 on any individual scene.

The hub's backend picker switches between Box3D, Godot Physics, Jolt, and any other installed backend. Switching relaunches the project, because the physics server is built once at startup.

Every screenshot below was captured with Box3D active.

## Shapes

![Shapes demo]({{ '/images/demos/shapes.png' | relative_url }})

Every shape Box3D supports, dropped onto a floor: box, sphere, capsule, cylinder, and convex hull. If a shape settles at the wrong height or sinks, that is the first thing to check after a change to shape creation.

## Joints

![Joints demo]({{ '/images/demos/joints.png' | relative_url }})

The three joint types that map onto Box3D. A hinge swings a motorised door, a pin joint's anchor sweeps every frame via `pin_joint_set_local_a` (so anchors can move after creation), and a slider is pushed between its limits.

Godot exposes joint parameters that Box3D has no equivalent for, such as `LIMIT_RELAXATION` and `IMPULSE_CLAMP`. Those log a warning and are ignored rather than silently doing something different.

## Areas

![Areas demo]({{ '/images/demos/areas.png' | relative_url }})

Two `Area3D` overrides: one replaces gravity with zero and damps bodies to a halt, the other applies point gravity toward a centre offset from the area's own origin.

Area priority ordering follows Godot's rule, where the highest-priority area that a body overlaps wins.

## Contacts

![Contacts demo]({{ '/images/demos/contacts.png' | relative_url }})

Contact monitoring. The dropped box reports real contact points, normals, and impulses, with a yellow marker on each reported point. Press Space to drop it again.

Contact normals follow Godot's convention, which is the opposite sign to Box3D's native A-to-B direction.

## Collision exceptions

![Collision exceptions demo]({{ '/images/demos/exceptions.png' | relative_url }})

Per-pair collision exceptions via `add_collision_exception_with`. The blue pair passes through each other, the orange pair collides normally. Press Space to toggle the exception and watch the excepted pair push apart again.

## Trimesh

![Trimesh demo]({{ '/images/demos/trimesh.png' | relative_url }})

A `ConcavePolygonShape3D` ramp with crates sliding down it. Trimesh bodies collide normally but are never detected by an `Area3D`, which is a [documented difference]({{ '/behavior-differences/' | relative_url }}).

## Trimesh transforms

![Trimesh transform demo]({{ '/images/demos/trimesh_transform.png' | relative_url }})

Three trimesh pads with different transforms: plain, offset inside its own body, and scaled by its parent. Each ball should rest on its own pad, and the on-screen readout says OK when it does.

This scene exists because of a real bug. Trimesh shapes were built with a hardcoded identity transform, so collision geometry sat at the body origin instead of where the mesh was drawn. Anything relying on an offset or scaled trimesh fell straight through. This is the regression test for that fix.

## Benchmark

![Benchmark]({{ '/images/benchmarks/box3d.png' | relative_url }})

A deterministic drop test: 36 boxes every 0.5s onto a fixed lattice from a fixed seed, up to 4096 bodies, capped at 60 seconds. Identical placement on every backend, so the only variable is the physics engine.

The readout tracks FPS, physics step time, memory, and how many bodies were in the world when the step first exceeded the 16.66 ms budget for 60 FPS. The export button writes a screenshot and a per-frame CSV to `user://`.

Results and caveats are on the [Benchmarks]({{ '/benchmarks/' | relative_url }}) page.

## Regenerating the screenshots

```sh
godot --path test_project --script res://tools/capture_demos.gd
```

This needs a real GPU ran without `--headless`.
