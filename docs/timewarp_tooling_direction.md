# Timewarp Tooling Direction

The `temp/box3d_timewarp_extension` reference is useful, but it should not be merged directly into the core `PhysicsServer3D` backend. Its `Box3DWorld3D` and `Box3DBody3D` nodes duplicate Godot's regular physics node stack, while this repository is intended to stay a drop-in Box3D-backed server for `RigidBody3D`, `StaticBody3D`, `CharacterBody3D`, areas, joints, and direct queries.

## Recommended Shape

- Keep timewarp/replay work in a companion extension or separate repository until its API has stabilized.
- Let the core backend expose only narrow hooks if they become necessary, such as optional recording lifecycle controls on a `Box3DSpace3D` wrapper.
- Avoid adding a parallel `Box3DWorld3D` / `Box3DBody3D` node model to this repository unless the project intentionally stops being only a PhysicsServer replacement.

## Design Questions Before Porting

- How are recorded Box3D body creation indices mapped back to Godot RIDs and object IDs during replay?
- What happens when nodes are created or freed while a recording is active?
- Is replay intended for debugging, deterministic validation, gameplay rewind, or editor tooling?
- How are queries, area callbacks, contact callbacks, and state-sync callbacks represented during replay?
- What is the supported worker-count and SIMD policy for deterministic validation?

## Priority Order

1. Write a companion-extension API sketch around recording, validation, frame seek, divergence info, and keyframe policy.
2. Add deterministic replay tests using Box3D's recording validation APIs before exposing gameplay-facing controls.
3. Prototype editor/debug visualization on replay data separately from the core backend.
4. Only after the above, consider whether the core backend needs a small optional bridge for active `Box3DSpace3D` recording.
