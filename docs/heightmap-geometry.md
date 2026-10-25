---
title: Heightmap geometry
---

# Heightmap geometry

`HeightMapShape3D` uses a grid centered on the shape origin, with one-unit sample
spacing and row-major heights. Local attachment translations and rotations must
move the collision surface just as they move its Godot debug geometry.

The pinned Box3D heightfield API creates a corner-origin grid and cannot apply a
fixture-local transform. The adapter therefore builds a centered static mesh:
identity attachments share its geometry, and transformed attachments reuse the
existing owned-mesh baking/lifetime path. The original height array remains the
resource's data. Grid cells use Godot's diagonal and winding; triangles touching
non-finite heights are omitted, so NaN vertices create holes without entering
the native mesh builder. An entirely empty grid creates no collision fixture.

This fixes collision placement; it is not the compressed native-heightfield
fast path. Mesh construction and baked copies can use more memory on large maps.
Qualify representative terrain sizes before treating this as a performance
improvement. Dynamic concave bodies and concave sensor visitors remain unsupported.

`test_project/tests/heightmap_geometry_test.gd` checks independently calculated
surface heights/normals, both sides of the centered grid, bounds, shared owners,
body/attachment rotations, an attachment move, rigid support, a non-planar cell,
NaN holes and an all-hole grid. Live resource replacement is a separate adapter lifetime concern.
