---
title: Behavior differences
---

# Behavior differences

Places where Box3D behaves differently from Godot's built-in physics. These are design differences in the underlying engine, not bugs, and none of them are things a drop-in backend can paper over.

## `Area3D` does not detect trimesh or heightmap bodies

In Box3D a concave shape (`ConcavePolygonShape3D`) or `HeightMapShape3D` can never act as a sensor *visitor*, by design, since testing an arbitrary mesh against a sensor is too expensive.

So an `Area3D` silently ignores a body whose shape is a trimesh or heightmap: no `body_entered` or `body_exited` fires for it. Godot's built-in physics and Jolt both report such a body on the first physics frame.

If you need a body to be detected by an area, give it a convex shape. A trimesh may still be used *as* an `Area3D`'s own shape to detect convex bodies passing through it.

## `collide_shape()` reports contact points without penetration depth

Box3D exposes GJK, which finds the closest points between two shapes but cannot measure how far they already overlap.

For a genuinely overlapping pair, both returned points are the same witness point, so the pair's separation reads as zero. Godot's built-in physics returns two points whose distance is the penetration depth.

Whether shapes overlap, which bodies they are, `max_results`, and `collision_mask` all behave normally. Use it to answer "is anything here, and roughly where" rather than "how deep".

## Shape queries need a convex query shape

`intersect_shape`, `cast_motion`, `collide_shape`, `rest_info`, and `body_test_motion` build a point-cloud proxy of the shape being queried *with*. Trimesh, heightmap, and world-boundary shapes have no finite point cloud.

Passing one of those as the query shape returns no results. They work normally as targets in the world.

## Friction and restitution combine differently

Box3D uses `sqrt(a * b)` for friction and `max(a, b)` for restitution. Godot's built-in physics uses `min(a, b)` and a clamped sum.

Materials tuned against Godot's defaults will not feel the same, so a port usually needs its friction values revisited.

## Ignored joint parameters

Godot's joint API exposes parameters Box3D has no equivalent for, including `HingeJoint3D`'s `LIMIT_RELAXATION` and `LIMIT_SOFTNESS`, `PinJoint3D`'s `IMPULSE_CLAMP`, and most of `SliderJoint3D`'s per-axis tuning (Box3D's prismatic joint is a pure 1-DOF slider).

Setting one logs a warning and is ignored, rather than silently mapping onto something that behaves differently.
