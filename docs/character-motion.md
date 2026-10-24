---
title: Character motion recovery
---

# Character motion recovery

`body_test_motion` uses Box3D's contact manifolds to recover initial overlaps before casting the requested motion.
Sphere, capsule, box, cylinder and convex query shapes work against primitive/hull, trimesh, heightfield and world-boundary fixtures.
Compound attachments, their local transforms and disabled shape slots are preserved.
Terrain recovery only visits triangles in the query's local bounding box; bounding boxes do not determine penetration direction or depth.

Recovery solves contact planes together.
Movable obstacles have bounded pushes, and static/kinematic supporting surfaces take precedence when contacts compete.
Higher `collision_priority` takes precedence within the same class of contacts.
This is an adapter recovery policy, not a change to Box3D's rigid-body solver or a promise of bit-for-bit parity with GodotPhysics3D.
A body trapped in a gap smaller than itself cannot satisfy both surfaces; recovery remains bounded but does not guarantee escape from impossible overlaps.

Motion queries are read-only: they do not move the character or collider, apply forces, or wake a rigid body.
Per-call body/object exclusions apply to both recovery and casting.
`recovery_as_collision=false` suppresses recovery-only collision reporting, not the recovery displacement itself.

The motion cast reports a hit only when the shapes would actually touch, as GodotPhysics3D and Jolt do, not when they come within Box3D's linear slop.
Motion that starts a few millimeters from a surface still sees it, and sliding flush across the seam between coplanar shapes does not snag on the next shape's edge.

The regressions exercise geometric normals and bounded displacement for five convex shapes, a large query margin, compound floor/wall contacts, priority in a too-narrow passage, rigid loads on stationary/moving characters, and actual `move_and_slide` grounding across slopes and terrain seams.
These are behavioral checks, not exact cross-engine trajectory comparisons.

These fixes address adapter behavior.
They do not add dynamic concave bodies, concave query shapes or sensor visits by concave bodies; those remain separate Box3D restrictions.
Non-uniform collision-shape scaling is not certified by this milestone; use dimensions on the shape resource and unit-scale body transforms.
