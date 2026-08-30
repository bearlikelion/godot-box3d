---
title: Comparison with box3d-godot
---

# Comparison with box3d-godot

[box3d-godot](https://github.com/Stink-O/box3d-godot) is another Box3D binding for Godot. It exposes Box3D as 14 custom nodes (`Box3DWorld`, `Box3DBody`, eight joint types, a character controller) running alongside Godot's physics. This project replaces Godot's physics instead.

Different projects solving different problems, so this is a comparison rather than a ranking.

| | godot-box3d (this) | box3d-godot |
|---|---|---|
| Approach | Implements `PhysicsServer3D` | Custom `Box3DWorld` / `Box3DBody` nodes |
| Stock Godot nodes | Work unchanged | Not supported; scenes are rewritten |
| Adopting it | Change a project setting | Port every physics node |
| Existing addons | Keep working | Do not apply |
| Joints | 3 (pin, hinge, slider) | 8 (adds ball, fixed, motor, wheel, parallel, distance) |
| Vehicles | `VehicleBody3D` (raycast) | `Box3DWheelJoint` (real constraint) |
| Heightfields | Yes | No |
| Platforms | Linux, Windows (macOS universal, experimental) | + Android, web |
| Box3D-only features | Not reachable | Explosions, gyroscopic torque, solver profiling, async stepping |

## The trade

**box3d-godot exposes more of Box3D. This project keeps your project working.**

The difference is structural, not a matter of effort. `PhysicsServer3D` has no entry point for `b3World_Explode`, no wheel joint in its `JointType` enum, and no hook for solver profiling, so a drop-in backend cannot surface them at all.

Equally, box3d-godot's nodes are a separate type hierarchy. `RigidBody3D`, `CharacterBody3D`, and third-party physics addons do not apply there.

## Which to use

Use **godot-box3d** if you have an existing project, or want stock nodes and addons to keep working.

Use **box3d-godot** if you want Box3D's own feature surface and don't mind building scenes around its nodes.

Neither is production-ready, and both say so.

## Where this project is behind

Joint types, ConeTwist and 6DOF, per-shape indices in query results, profiling, and platforms.
