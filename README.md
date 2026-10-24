# godot-box3d

A [GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html) that integrates [Box3D](https://github.com/erincatto/box3d), Erin Catto's 3D physics engine, into Godot 4 as a drop-in replacement for the built-in `PhysicsServer3D`.

Stock Godot physics nodes keep working.
You change a project setting, not your scenes.

> **Status: early and experimental.** Box3D itself is a young engine, and this extension is a work in progress. Expect missing features and rough edges.

**Documentation:** https://bearlikelion.github.io/godot-box3d/

## Why

I'm building [SurfsUp](https://store.steampowered.com/app/3454830/SurfsUp/), a recreation of Source-engine "SkillSurf" in Godot.
Surf maps are hard on a physics engine: long ramps built from concave trimesh, corners taken at speed, and head surf along the underside of geometry.
Godot's built-in physics and godot-jolt both have trouble with parts of that.

Box3D is worth trying here because of where it came from.
[Erin Catto](https://x.com/erin_catto) started it from Valve's Rubikon-lite, the physics engine of Source 2 ([Announcing Box3D](https://box2d.org/posts/2026/06/announcing-box3d/#valve-to-the-rescue)), so a Rubikon-lineage solver seemed like a reasonable bet.

New tech is cool and Godot is great.
That's most of it.
This is my first time doing physics programming, and I am building the bridge between Godot's [PhysicsServer3D](https://docs.godotengine.org/en/stable/classes/class_physicsserver3d.html) and Box3D based on the architecture of [godot-jolt](https://github.com/godot-jolt/godot-jolt).

## Quick start

1. Download the addon zip from [Releases](https://github.com/bearlikelion/godot-box3d/releases) and copy `addons/godot-box3d/` into your project.
2. Set **Project Settings → Physics → 3D → Physics Engine** to `Box3D Physics`.
3. Restart Godot.

Requires Godot 4.3 or newer.
Linux and Windows are tested; macOS builds are Apple Silicon only and untested.
Worker threads, length units, and checking that the backend loaded are covered in [Installation](https://bearlikelion.github.io/godot-box3d/installation/).

## What works

- Rigid, static, and kinematic bodies
- Box, sphere, capsule, cylinder, convex, trimesh, heightmap, and world boundary shapes, with live updates of shared resources
- Areas with overlap events, gravity and damping overrides, and point gravity
- Ray casts, shape casts, and the other direct space state queries
- `CharacterBody3D` and `move_and_slide()`
- Contact monitoring with real contact points, normals, and impulses
- Pin, hinge, and slider joints
- A multithreaded solver with identical results at any worker count

Not done yet: separation ray shapes, ConeTwist and `Generic6DOFJoint3D`, `SoftBody3D`, solver profiling, and more platforms.
A few things also behave differently from Godot's built-in physics, listed in [Behavior differences](https://bearlikelion.github.io/godot-box3d/behavior-differences/).

## Benchmark

A 4096-box drop test, same scene and seed on every backend.
Box3D held the 60 FPS budget longest and had the tightest tail.

| Backend | Bodies at 16.66 ms | Median step | p95 step | Peak step | Memory |
|---|---|---|---|---|---|
| **Box3D Physics** | **1404** | **27.3 ms** | **63.1 ms** | **66.8 ms** | **117 MB** |
| Jolt Physics | 1080 | 56.9 ms | 99.9 ms | 139.9 ms | 150 MB |
| Godot Physics | 1044 | 54.6 ms | 235.5 ms | 306.0 ms | 177 MB |
| Rapier3D | 684 | 79.2 ms | 249.3 ms | 303.2 ms | 177 MB |

Methodology, caveats, and graphs are on the [Benchmarks](https://bearlikelion.github.io/godot-box3d/benchmarks/) page.

## Compared to box3d-godot

[box3d-godot](https://github.com/Stink-O/box3d-godot) exposes Box3D as its own set of nodes instead of replacing Godot's physics.
It reaches more of Box3D; this project keeps stock nodes and addons working.
The [comparison](https://bearlikelion.github.io/godot-box3d/comparison/) covers which fits your project.

## Building

See [Building from source](https://bearlikelion.github.io/godot-box3d/building/) for build, cross-compile, and test instructions.

## Contributing

Help wanted!
This is a big project for one person, and contributions of any size are welcome: missing features from the list above, bug reports with reproduction scenes, benchmarks, documentation, or just trying it in your project and reporting what breaks.
Open an issue or a pull request.

## License

MIT. See [LICENSE](LICENSE) for details.

Box3D is licensed under the [MIT license](https://github.com/erincatto/box3d/blob/main/LICENSE).
This project takes structural inspiration from [godot-jolt](https://github.com/godot-jolt/godot-jolt), also MIT licensed.
