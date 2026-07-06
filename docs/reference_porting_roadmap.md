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

## P1 - Next practical ports

- Resolve `get_contact_collider_object()` to the Godot object instance when RID-to-object ownership is available in this extension layer.
- Expand `body_test_motion()` to test all enabled shapes on a multi-shape body, returning the earliest blocker instead of the first enabled shape.
- Return richer `collide_shape()` contact pairs when Box3D exposes enough manifold detail for a direct overlap query; current support reports overlapping shape origins.
- Add focused Godot integration scenes that exercise contact callbacks, exceptions, ray pickability, cylinder queries, and multi-shape motion.

## P2 - Larger follow-up projects

- Add recording/replay or timewarp tooling on top of Box3D snapshots only after deterministic state ownership is documented.
- Revisit soft-body API stubs as explicit unsupported capability reporting unless a real Box3D-backed implementation is planned.
- Add editor/debug visual overlays for contact normals, active islands, broadphase pairs, and sleeping state.
- Consider a generic shape-hull factory to share polygonal approximation code if more analytic shapes are added.
