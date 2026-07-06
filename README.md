# godot-box3d

A [GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html) that integrates [Box3D](https://github.com/erincatto/box3d), Erin Catto's 3D physics engine, into Godot 4 as a drop-in replacement for the built-in `PhysicsServer3D`.

The structure of this extension is based on [godot-jolt](https://github.com/godot-jolt/godot-jolt), which pioneered swapping out Godot's 3D physics backend via GDExtension.

> **Status: early and experimental.** Box3D itself is a young engine, and this extension is a work in progress. Expect missing features and rough edges.

## What works

- Rigid, static, and kinematic bodies
- Shapes: box, sphere, capsule, cylinder, convex polygon, concave polygon (trimesh), heightmap, and world boundary
- Areas, including overlap events and gravity/damping overrides
- Direct space state queries: ray casts, point and shape intersection, shape casts (`cast_motion`), `collide_shape`, and `rest_info`
- `body_test_motion`, so `CharacterBody3D` and `move_and_slide()` work
- Collision exceptions, collision layers/masks, ray pickability, contact reports, and debug contacts
- Joints: pin, hinge, and slider
- A small test project (`test_project/`) with demo and stress scenes

## What's left to do

- Separation ray shapes (not supported by Box3D)
- ConeTwist and Generic6DOF joints
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

## Test Project

The `test_project/` folder contains headless smoke tests and a playable demo scene. For example:

```sh
scripts/run_headless_tests.sh
```

Set `GODOT_BIN=/path/to/godot` to test with a specific Godot executable.
Set `BUILD_DIR=/path/to/build` to use a non-default CMake build directory.

## Contributing

Help wanted! This is a big surface area for one person, and contributions of any size are very welcome: missing features from the list above, bug reports with reproduction scenes, benchmarks, documentation, or just trying it in your project and reporting what breaks. Open an issue or a pull request.

## License

MIT. See [LICENSE](LICENSE) for details.

Box3D is licensed under the [MIT license](https://github.com/erincatto/box3d/blob/main/LICENSE). This project takes structural inspiration from [godot-jolt](https://github.com/godot-jolt/godot-jolt), also MIT licensed.
