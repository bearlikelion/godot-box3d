# Reference Porting Roadmap

This tracks features reviewed from `temp/` reference projects that are useful but should be ported deliberately after the core physics contract is stable.

## P0 - Implemented in current work

- Ignore the `temp/` reference directory so copied experiments do not enter source control.
- Preserve shape indices and ray face indices in direct-space query results where Box3D exposes enough information.
- Respect Godot collision layers, masks, ray pickability, and body collision exceptions through Box3D custom filtering.
- Populate body contact reports and debug contact points from live Box3D manifolds.
- Route `PhysicsDirectBodyState3D` contact getters to cached contact data.
- Support `CylinderShape3D` as a Box3D hull for attached bodies and direct-space shape queries.
- Use query margin for direct-space shape proxies where Box3D exposes a finite proxy.
- Invoke the body force integration callback during the space step before the integration pass.
- Report basic process counters from Box3D world counters.

## P1 - Implemented follow-ups

- Resolve `get_contact_collider_object()` to the Godot object instance when RID-to-object ownership is available in this extension layer.
- Expand `body_test_motion()` to test all enabled shapes on a multi-shape body, returning the earliest blocker instead of the first enabled shape.
- Return `collide_shape()` contact point pairs for convex Box3D overlaps, with conservative AABB fallback pairs for non-convex world shapes.
- Honor `PhysicsShapeQueryParameters3D.exclude` through the extension query-exclusion hook in direct-space callbacks.
- Add focused headless Godot integration coverage for contact callbacks, collision exceptions, cylinder queries, query exclusions, `collide_shape()` pairs, and multi-shape motion.
- Report all `SoftBody3D` server APIs as explicitly unsupported while keeping the methods inert and returning safe defaults.
- Add viewport object-picking coverage for body and area ray pickability; this exercises Godot's `pick_ray` path instead of only public direct-space ray queries.
- Apply Area3D linear and angular damping override modes through the same priority-ordered area pass used for gravity, while preserving each body's configured base damping.
- Report unsupported ConeTwist and Generic6DOF joint API calls consistently instead of silently accepting setter/getter calls after construction fails.

## P2 - Larger follow-up projects

- Keep recording/replay or timewarp tooling out of the core backend until a design doc defines deterministic state ownership, node/RID mapping, and whether it ships as a companion extension or a narrow optional API on `Box3DSpace3D`.
- Add editor/debug visual overlays for contact normals, active islands, broadphase pairs, and sleeping state.
- Consider a generic shape-hull factory to share polygonal approximation code if more analytic shapes are added.
