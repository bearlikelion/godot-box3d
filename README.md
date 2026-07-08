# godot-box3d

A [GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html) that integrates [Box3D](https://github.com/erincatto/box3d), Erin Catto's 3D physics engine, into Godot 4 as a drop-in replacement for the built-in `PhysicsServer3D`.

The structure of this extension is based on [godot-jolt](https://github.com/godot-jolt/godot-jolt), which pioneered swapping out Godot's 3D physics backend via GDExtension.

> **Status: early and experimental.** Box3D itself is a young engine, and this extension is a work in progress. Expect missing features and rough edges.

## What works

- Rigid, static, and kinematic bodies
- Shapes: box, sphere, capsule, convex polygon, concave polygon (trimesh), heightmap, and world boundary
- Cylinder shapes (approximated as a 16-slice convex hull; Box3D has no native cylinder)
- Scaled body transforms: scale on a body's transform is baked into the engine-side collision geometry, matching Godot semantics
- Areas, including overlap events and gravity/damping overrides
- Collision layers and masks with Godot's exact one-directional query semantics
- Per-pair collision exceptions (`add_collision_exception_with`)
- Direct space state queries: ray casts, point and shape intersection, shape casts (`cast_motion`), `collide_shape`, and `rest_info`
- `body_test_motion` built on Box3D's native character-mover API, so `CharacterBody3D` and `move_and_slide()` work — including slopes, trimesh terrain, and dense crowds of kinematic characters
- Joints: pin, hinge, slider, and cone-twist (mapped to Box3D's spherical joint); cone-twist + hinge chains are sufficient for ragdolls
- `Box3DRecording` singleton: record physics queries to a file and replay them to detect divergence, for determinism debugging
- A test project (`test_project/`) with a headless regression suite and a visual demo scene (`demo_scene.tscn`) that runs visual tests with pass/fail readouts

## What's left to do

- Separation ray shapes
- Generic6DOF joints
- `SoftBody3D`
- Changing a `PinJoint3D` anchor after creation (recreate the joint instead)
- More platforms and architectures (currently Linux, Windows, and macOS builds)
- Performance benchmarking and tuning
- Documentation

## Requirements

- Godot 4.3 or newer

## Building

The project builds with CMake:

```sh
cmake -B build
cmake --build build
```

The resulting library is placed in `bin/` and loaded via `godot-box3d.gdextension`. Copy `bin/` and the `.gdextension` file into your project, then select the Box3D physics server in your project settings.

MSVC is supported; if you hit `_gde_UnexistingClass` cast errors after changing toolchains, delete `build/CMakeCache.txt` and reconfigure.

## Testing

`test_project/` serves two purposes:

- **Headless regression suite** — scripts extending `SceneTree`, run as
  `godot --headless --path test_project --script <test>.gd`. Covers bodies,
  shapes, layers, joints, areas, character motion on slopes and trimesh
  terrain, transform scale, crowd stability, and CSG shape updates. Each test
  prints `PASSED`/`FAILED` and sets its exit code, so the suite is CI-friendly.
- **Visual demo** — open `demo_scene.tscn` and run; a dropdown lists the visual
  tests (scripts extending `Node3D`) and shows their pass/fail result on screen.

## Changelog

- Fixed reverse-facing world collision on concave (trimesh) shapes: Godot and
  Box3D use opposite triangle front-face winding, so rays and contacts resolved
  against the back side of trimesh geometry.
- Fixed collision layer/mask semantics: Box3D's filter is a bidirectional AND
  while Godot's queries are one-directional, which made some valid
  layer/mask configurations invisible to queries and motion. The extension now
  drives Box3D's custom pair filtering to reproduce Godot's rules exactly.
- Fixed shape data changes not applying to bodies already in the world
  (e.g. `ConcavePolygonShape3D.set_faces` after the shape was attached, as CSG
  nodes do).
- Fixed a character-mover depth-convention bug that could make characters sink
  into sloped surfaces, lock in place, or fall through geometry. Box3D's mover
  planes are mover-relative constraints, not world planes; the motion code now
  uses the native convention everywhere.
- Fixed oscillation in dense crowds of kinematic characters: mover sweeps could
  end slightly inside other characters and be ejected the next frame, making
  pressed groups vibrate. Motion is now clamped to stop at contact.
- Fixed default-area parameter queries (`area_get_param` /
  `area_set_param` with a space RID), restoring project gravity settings and
  removing warning spam.
- Fixed MSVC builds: a required godot-cpp compile definition was dropped by the
  CMake external-project import, causing method-bind cast errors.
- Added support for scale in body transforms (baked into shape geometry;
  non-uniform scale on primitive shapes warns once and uses the closest fit).
- Added per-pair collision exceptions, wired to both the solver and all queries.
- Added cone-twist joints and cylinder shapes (see feature list for the mapping
  and approximation used).
- Added the `Box3DRecording` record/replay singleton for determinism debugging.
- Performance: character motion uses fewer world queries per `body_test_motion`
  call in the common grounded case, and high-frequency body transform updates
  use a cheaper scale-change test.

## Contributing

Help wanted! This is a big surface area for one person, and contributions of any size are very welcome: missing features from the list above, bug reports with reproduction scenes, benchmarks, documentation, or just trying it in your project and reporting what breaks. Open an issue or a pull request.

## License

MIT. See [LICENSE](LICENSE) for details.

Box3D is licensed under the [MIT license](https://github.com/erincatto/box3d/blob/main/LICENSE). This project takes structural inspiration from [godot-jolt](https://github.com/godot-jolt/godot-jolt), also MIT licensed.
