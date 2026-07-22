# godot-box3d

A [GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html) that integrates [Box3D](https://github.com/erincatto/box3d), Erin Catto's 3D physics engine, into Godot 4 as a drop-in replacement for the built-in `PhysicsServer3D`.

The structure of this extension is based on [godot-jolt](https://github.com/godot-jolt/godot-jolt), which pioneered swapping Godot's 3D physics backend through GDExtension.

> **Status: early and experimental.** Box3D itself is young, and this wrapper still has missing features. Production adoption requires device/browser testing and performance budgets.

For browser support, start with [`WEB_QUICKSTART.md`](WEB_QUICKSTART.md). The complete platform setup and release guide is [`MOBILE_WEB_INTEGRATION.md`](MOBILE_WEB_INTEGRATION.md).

## Supported build matrix

| Platform | Architectures | Variants | Notes |
|---|---|---|---|
| Linux | x86-64, arm64 | debug, release | Desktop regression target |
| Windows | x86-64 | debug, release | Desktop target |
| macOS | universal | debug, release | Desktop target |
| Android | arm64, x86-64 | debug, release | NDK 23.2.8568313, API 21+ |
| iOS | arm64 device | debug, release | macOS/Xcode required |
| Web | wasm32 | debug, release | No threads; custom dynamic-link Web templates required |

The portable profile embeds Box3D into the GDExtension and uses one Box3D worker. The Web build uses Emscripten 4.0.20 and WebAssembly SIMD128 by default.

## What works

- Rigid, static, and kinematic bodies
- Shapes: box, sphere, capsule, cylinder, convex polygon, concave polygon (trimesh), heightmap, and world boundary
- Areas, including overlap events and gravity/damping overrides
- Direct space-state queries: ray casts, point and shape intersection, shape casts (`cast_motion`), `collide_shape`, and `rest_info`
- `body_test_motion`, so `CharacterBody3D` and `move_and_slide()` work
- Collision exceptions, collision layers/masks, ray pickability, contact reports, and debug contacts
- Joints: pin, hinge, and slider
- Headless regression tests and an exported-app portable smoke scene

## Not yet implemented

- Separation-ray shapes, which Box3D does not support
- ConeTwist and Generic6DOF joints
- `SoftBody3D`
- Changing a `PinJoint3D` anchor after creation; recreate the joint instead
- Production-scale mobile/browser performance tuning

## Requirements

- Godot 4.3 or newer
- Godot 4.7 when using the supplied prebuilt Web templates
- Python 3 and Git
- Platform SDK/toolchain for the intended export target

Dependency commits and toolchain versions are pinned in [`dependencies.lock`](dependencies.lock).
The reduced binding set used by portable builds is pinned in [`godot_cpp_build_profile.json`](godot_cpp_build_profile.json).
Other Godot versions require matching custom Web templates and their own validation pass.

## Web quick start

The fastest route is the prebuilt `godot-box3d-web-release` artifact from the **Portable Android iOS Web Release** workflow. Copy its `addons/godot-box3d` directory into the project, select **Box3D Physics (Extension)** as the 3D physics engine, and configure the supplied custom debug/release templates in the Web export preset.

To build the complete Web bundle from source with pinned tools and dependencies:

```bash
MAX_JOBS=4 scripts/quickstart_web.sh
```

The output is `dist/godot-box3d-web-release.zip`. The first run builds matching Godot 4.7 Web templates and can take a while; later builds are incremental.

Serve an exported project with the required development headers:

```bash
python3 scripts/serve_web_export.py /path/to/export --port 8060
```

See [`WEB_QUICKSTART.md`](WEB_QUICKSTART.md) for the exact project layout, export settings, and automated browser smoke test.

## Build other platforms

Prepare the local SCons environment and exact source dependencies:

```bash
scripts/setup_python_build_env.sh
scripts/bootstrap_dependencies.sh
python3 scripts/verify_port.py --require-dependencies
```

Build a desktop development binary for the host platform:

```bash
scons platform=linux target=template_debug arch=x86_64
```

For Android and iOS, use the platform scripts:

```bash
scripts/build_android.sh
scripts/build_ios.sh
```

The iOS script must run on macOS.

See [`MOBILE_WEB_INTEGRATION.md`](MOBILE_WEB_INTEGRATION.md) for exact setup, application integration, CI, packaging, and release validation instructions.

## Validation snapshot

The complete Android matrix has been cross-compiled with the pinned NDK: arm64 and x86-64, debug and release. Linux x86-64 debug and release also compile and link. Web debug/release side modules and matching Godot 4.7 templates compile, and the exported smoke scene passes in Chrome. iOS compilation plus physical-device and cross-browser acceptance remain. See [`VALIDATION_REPORT.md`](VALIDATION_REPORT.md).

## Install in a Godot project

Copy the complete addon tree into the project:

```text
addons/godot-box3d/
├── godot-box3d.gdextension
└── bin/<platform>/...
```

Then select **Box3D Physics (Extension)** under **Project Settings → Physics → 3D → Physics Engine** and restart the editor.

## Package a portable release

After all mobile/Web binaries and Web templates are built:

```bash
python3 scripts/package_addon.py \
  --mode bundle \
  --platform android \
  --platform ios \
  --platform web \
  --web-templates-dir dist/web-export-templates \
  --output dist/godot-box3d-portable-release.zip
```

The GitHub Actions workflow at `.github/workflows/portable-release.yml` automates the complete build and emits the same single release bundle.

## Test project

Run the desktop headless suite:

```bash
scripts/run_headless_tests.sh
```

Open `test_project/portable_smoke_test.tscn` as the main scene for exported Android, iOS, and Web acceptance testing. It checks backend registration, falling-body contact, an area event, and hinge movement, and displays the result on screen.

## CMake

CMake remains available as a secondary build path for IDEs and existing toolchain integrations:

```bash
scripts/bootstrap_dependencies.sh
cmake -S . -B build -G Ninja
cmake --build build --parallel
```

SCons is the recommended path for Android, iOS, and Web because it follows Godot's official platform toolchains and filename conventions.

## License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
