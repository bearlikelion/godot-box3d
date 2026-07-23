# Godot Box3D Android, iOS, and Web Integration Guide

This document is the detailed implementation and release guide for Android, iOS, and browser exports, including the custom Godot Web export templates required by GDExtension dynamic linking. For the shortest browser setup, start with `WEB_QUICKSTART.md`.

## 1. Delivered scope

The port adds the following build and packaging support while preserving the existing Godot `PhysicsServer3D` integration:

| Target | Architectures | Build variants | Output format | Status in this source kit |
|---|---|---|---|---|
| Android | arm64, x86-64 | template debug, template release | `.so` | Implementation/CI complete; all four variants cross-compiled during validation |
| iOS | arm64 device | template debug, template release | `.dylib` | Implementation/CI complete; macOS compile and device acceptance remain |
| Web | wasm32, no threads | template debug, template release | side-module `.wasm` | Debug/release side modules, custom templates, and local Chrome smoke passed; cross-browser acceptance remains |

The portable profile keeps Box3D at one worker. This matches the existing source code and avoids introducing a new thread scheduler into mobile and browser exports. Web builds are intentionally single-threaded, so they do not require `SharedArrayBuffer` or a worker-enabled Godot export template. Serve extension-enabled exports with the opener/embedder headers described below.

The optional source archive contains the repository source while excluding generated native/Web binaries, downloaded dependency trees, platform SDKs, and signing material. The included GitHub Actions workflow builds all target binaries, builds matching Web templates, and emits Web-only and complete portable release bundles. Validation results are recorded in `VALIDATION_REPORT.md`.

## 2. Important compatibility decisions

### Godot baseline

The extension retains:

```ini
compatibility_minimum = "4.3"
```

The `godot-cpp` binding source is pinned to the exact commit behind `godot-4.3-stable`. This preserves the existing project baseline. Portable builds also use `godot_cpp_build_profile.json` to generate only the Godot classes required by this backend and their transitive dependencies. Any new wrapper use of a Godot class must be added to that profile before the target matrix is rebuilt. Later Godot 4.x releases can generally load an extension generated against an earlier compatible API, but every platform binary must still be rebuilt for the intended precision, target, and architecture.

### Box3D baseline

Box3D is pinned to the exact commit for v0.1.0. The wrapper source uses APIs present in that release. Do not float Box3D to `main` in production builds; Box3D is young and its C API can change.

### Web toolchain

This port pins the Emscripten SDK manager to an exact commit and installs Emscripten 4.0.20. It uses the same compiler version for both:

1. `godot-box3d` Web GDExtension side modules.
2. Custom Godot Web export templates built with `dlink_enabled=yes`.

Using the same version for both sides removes a major source of WebAssembly linker and loader incompatibility.

## 3. Files added or materially changed

The main integration points are:

| Path | Purpose |
|---|---|
| `SConstruct` | Primary cross-platform build. Compiles the wrapper, Box3D C17 sources, and `godot-cpp` into one GDExtension library. |
| `godot_cpp_build_profile.json` | Reduced Godot binding class set used by portable builds. |
| `godot-box3d.gdextension` | Godot feature-tag mapping for desktop, Android, iOS, and Web binaries. |
| `dependencies.lock` | Exact dependency commits and toolchain versions. |
| `.gitmodules` | Adds `godot-cpp` as a source dependency alongside Box3D. |
| `scripts/bootstrap_dependencies.sh` | Clones and checks out the exact Box3D and `godot-cpp` revisions. |
| `scripts/build_android.sh` | Builds Android arm64 and x86-64, debug and release. |
| `scripts/build_ios.sh` | Builds iOS arm64, debug and release. |
| `scripts/build_web.sh` | Builds single-threaded wasm32 side modules, debug and release. |
| `scripts/build_web_templates.sh` | Builds matching dynamic-link-enabled Godot Web templates. |
| `scripts/quickstart_web.sh` | Sets up, builds, validates, and packages all Web support in one command. |
| `scripts/serve_web_export.py` | Serves a local Web export with extension-compatible HTTP headers. |
| `scripts/package_addon.py` | Verifies and packages one addon or complete runtime release bundle. |
| `scripts/package_source_archive.py` | Produces a deterministic source ZIP while excluding dependencies, SDKs, caches, and binaries. |
| `scripts/verify_port.py` | Checks dependency pins, descriptor entries, portable build settings, and optional binary formats/architectures. |
| `.github/workflows/portable-release.yml` | Builds all three targets and the Web templates, then emits one release ZIP. |
| `CMakeLists.txt` | Retained secondary build path. Dependencies now inherit the caller's cross toolchain instead of using a nested host build. |
| `WEB_QUICKSTART.md` | Start-here instructions for browser builds and project integration. |
| `VALIDATION_REPORT.md` | Exact compile results, hashes, limitations, and remaining release gates. |

The old `cmake/GodotBox3DExternalGodotCpp.cmake` helper was removed. A nested `ExternalProject_Add` build does not reliably inherit Android NDK, Apple SDK, or Emscripten settings.

## 4. Clone and initialize the repository

Clone the repository, then initialize the pinned dependencies:

```bash
scripts/setup_python_build_env.sh
scripts/bootstrap_dependencies.sh
scripts/bootstrap_dependencies.sh --check
python3 scripts/verify_port.py --require-dependencies
```

For Web-only development, the one-command path performs all setup and creates a ready-to-copy bundle:

```bash
MAX_JOBS=4 scripts/quickstart_web.sh
```

The `box3d` and `godot-cpp` directories remain pinned by `dependencies.lock`. Treat revision changes as deliberate upgrades and validate every target before committing them.

## 5. Initial setup common to all targets

Run these commands from the repository root:

```bash
scripts/setup_python_build_env.sh
scripts/bootstrap_dependencies.sh
python3 scripts/verify_port.py --require-dependencies
```

The first script creates a local `.venv` and installs the pinned SCons version. The build scripts automatically locate that environment, so no shell activation is required. Build parallelism is capped at eight jobs by default to avoid memory exhaustion on high-core runners; set `MAX_JOBS` or `JOBS` explicitly when a different cap is appropriate.

The dependency bootstrap script verifies these exact commits:

- `godot-cpp`: `fbbf9ec4efd8f1055d00edb8d926eef8ba4c2cce`
- Box3D: `8441b4a06d6d09dcfb0b0f704df4d847d1437b92`

A changed dependency revision should be treated as a deliberate upgrade and tested on all targets before the lock file is updated.

## 6. Android build and integration

### Requirements

- Android SDK Command-line Tools.
- Android NDK 23.2.8568313.
- Python 3 and Git.
- A Godot Android export template compatible with the Godot version used by the application.

Set `ANDROID_HOME` or `ANDROID_SDK_ROOT` before running the setup script. The following block uses the conventional Linux SDK location:

```bash
export ANDROID_HOME="$HOME/Android/Sdk"
scripts/setup_android_toolchain.sh
scripts/build_android.sh
```

On macOS with the conventional Android Studio location, use this complete block instead:

```bash
export ANDROID_HOME="$HOME/Library/Android/sdk"
scripts/setup_android_toolchain.sh
scripts/build_android.sh
```

Use your actual SDK directory if Android Studio was configured elsewhere. The setup script installs the pinned NDK and Android API level; it does not install Android Studio itself.

### Android outputs

A successful build produces:

```text
bin/android/libgodot-box3d.android.template_debug.arm64.so
bin/android/libgodot-box3d.android.template_release.arm64.so
bin/android/libgodot-box3d.android.template_debug.x86_64.so
bin/android/libgodot-box3d.android.template_release.x86_64.so
```

The same files are copied into:

```text
test_project/addons/godot-box3d/bin/android/
```

### Android export settings

For production phones and tablets, include arm64. Include x86-64 when the team needs Android emulator or ChromeOS x86-64 coverage. The `.gdextension` file selects the correct library by Godot feature tags; do not rename these files after building.

Build a debug export first, install it on an arm64 device, and confirm the log contains the backend registration from `backend_activation_test.gd` or the portable smoke scene. Then build and test the release export, because Godot loads separate `template_debug` and `template_release` extension files.

## 7. iOS build and integration

### Requirements

- macOS.
- Xcode with the iPhoneOS SDK installed.
- Xcode command-line tools selected with `xcode-select`.
- An Apple development team and signing configuration for device installation.

Run this block on the Mac that performs iOS builds:

```bash
scripts/setup_python_build_env.sh
scripts/bootstrap_dependencies.sh
scripts/build_ios.sh
```

The script validates the iPhoneOS SDK with `xcrun` and builds both variants for arm64 devices.

### iOS outputs

```text
bin/ios/libgodot-box3d.ios.template_debug.arm64.dylib
bin/ios/libgodot-box3d.ios.template_release.arm64.dylib
```

The Godot iOS exporter is responsible for embedding and signing the selected dynamic library as part of the generated Xcode project. Do not ad-hoc sign the GDExtension before Godot export unless your organization's signing pipeline explicitly requires it.

The supplied descriptor targets physical arm64 devices. Simulator libraries are intentionally not part of the supported release matrix. A simulator build can be added later with Godot's `ios_simulator=yes` SCons option and separate `.gdextension` feature tags, but it must not replace the device library.

## 8. Web build and integration

### Why custom templates are mandatory

Godot's standard Web export templates do not include GDExtension dynamic-linking support. A Web project that uses this extension must use custom templates built with:

```text
dlink_enabled=yes
```

This port also uses:

```text
threads=no
```

The GDExtension side module and the Godot main module must be built with compatible Emscripten settings.

### One-command toolchain and builds

Run:

```bash
MAX_JOBS=4 scripts/quickstart_web.sh
```

The quick-start script creates the pinned SCons environment, initializes source dependencies, checks out the Emscripten SDK manager at its exact locked commit, installs Emscripten 4.0.20, builds the extension and custom templates, validates the result, and creates `dist/godot-box3d-web-release.zip`. The underlying build scripts automatically source the local Emscripten environment and reject a different compiler version unless `ALLOW_EMSCRIPTEN_VERSION_MISMATCH=1` is deliberately set for an experiment.

### Web extension outputs

```text
bin/web/libgodot-box3d.web.template_debug.wasm32.nothreads.wasm
bin/web/libgodot-box3d.web.template_release.wasm32.nothreads.wasm
```

### Custom Web template outputs

```text
dist/web-export-templates/godot-box3d-web-debug.zip
dist/web-export-templates/godot-box3d-web-release.zip
dist/web-export-templates/manifest.json
```

### Configure the Godot Web export preset

In the project's Web export preset:

1. Set the custom debug template to `godot-box3d-web-debug.zip`.
2. Set the custom release template to `godot-box3d-web-release.zip`.
3. Enable extension support.
4. Keep thread support disabled for this supported profile.
5. Export debug and verify the browser console before testing release.

Serve extension-enabled exports with cross-origin opener and embedder policies. This is required by Godot's Web extension-support path even when this profile keeps threads disabled.

### WebAssembly SIMD fallback

The default build enables WebAssembly SIMD128 through Box3D's upstream SSE2 path. Modern supported browsers implement this feature. For diagnostics on a problematic browser, build a slower scalar version with:

```bash
BOX3D_DISABLE_SIMD=1 scripts/build_web.sh
```

Do not mix a diagnostic scalar binary with an old cached export. Clear the browser cache or change the export path before comparing behavior.

## 9. Create the complete release bundle

After all platform binaries and Web templates exist, run:

```bash
python3 scripts/verify_port.py --platform android --platform ios --platform web --require-binaries
python3 scripts/package_addon.py \
  --mode bundle \
  --platform android \
  --platform ios \
  --platform web \
  --web-templates-dir dist/web-export-templates \
  --output dist/godot-box3d-portable-release.zip
```

The ZIP contains:

```text
godot-box3d-portable/
├── addons/godot-box3d/
│   ├── godot-box3d.gdextension
│   ├── bin/android/...
│   ├── bin/ios/...
│   └── bin/web/...
├── web-export-templates/
│   ├── godot-box3d-web-debug.zip
│   ├── godot-box3d-web-release.zip
│   └── manifest.json
├── BUILD_MANIFEST.json
├── WEB_QUICKSTART.md
├── MOBILE_WEB_INTEGRATION.md
├── PORT_STATUS.md
├── VALIDATION_REPORT.md
├── CHANGELOG_PORT.md
├── THIRD_PARTY_NOTICES.md
└── dependencies.lock
```

The manifest contains SHA-256[^1] checksums for the included binaries and templates.

To create an addon-only ZIP, use:

```bash
python3 scripts/package_addon.py \
  --mode addon \
  --platform android \
  --platform ios \
  --platform web \
  --output dist/godot-box3d-addon.zip
```

To create a deterministic source archive without downloaded dependencies or build outputs:

```bash
python3 scripts/package_source_archive.py
```

## 10. Install the addon in an application

Copy the complete `addons/godot-box3d` directory from the release bundle into the application's project root so the final path is:

```text
<project>/addons/godot-box3d/godot-box3d.gdextension
```

Do not copy only the binary for the current development machine. Godot's exporter uses the descriptor to locate target-specific libraries during export.

Set the 3D physics engine in the Godot editor under **Project Settings → Physics → 3D → Physics Engine** to:

```text
Box3D Physics (Extension)
```

The equivalent `project.godot` setting is:

```ini
[physics]

3d/physics_engine="Box3D Physics (Extension)"
```

Close and reopen the editor after adding or replacing native extension binaries. Confirm that the output does not report a missing library or missing `godot_box3d_main` entry symbol.

## 11. CI release workflow

The supplied `.github/workflows/portable-release.yml` is the reference release pipeline. It performs these jobs:

1. Validates scripts, descriptors, dependency pins, and source configuration.
2. Builds Android debug/release for arm64 and x86-64.
3. Builds iOS debug/release for arm64 on macOS.
4. Builds Web debug/release side modules with Emscripten 4.0.20.
5. Builds dynamic-link-enabled, no-thread Godot 4.7 Web export templates.
6. Creates a Web-only `godot-box3d-web-release.zip` artifact as soon as the Web jobs finish.
7. Downloads all platform artifacts into the complete packaging job.
8. Runs strict binary validation.
9. Creates one `godot-box3d-portable-release.zip` artifact.

Run the workflow manually from GitHub Actions while the port is being stabilized. After device and browser acceptance, it can also be triggered for version tags.

## 12. Validation procedure

### Source validation

Run on every change:

```bash
python3 scripts/verify_port.py --require-dependencies
```

After platform builds, run strict validation. In addition to checking existence, this inspects ELF, Mach-O, PE/COFF, or WebAssembly magic and the advertised architecture where applicable:

```bash
python3 scripts/verify_port.py \
  --platform android \
  --platform ios \
  --platform web \
  --require-binaries
```

### Validation completed for this repository

The complete Android arm64/x86-64 debug/release matrix and Linux x86-64 debug/release builds compiled and linked from the pinned sources. Web debug/release side modules, matching Godot 4.7 templates, and local Chrome smoke tests also passed. Architecture metadata and the `godot_box3d_main` export were verified. iOS compilation and physical-device/cross-browser acceptance remain target-environment gates. See `VALIDATION_REPORT.md` for checksums and exact details.

### Desktop regression tests

The existing headless suite remains the fastest functional regression test for the physics wrapper:

```bash
scripts/run_headless_tests.sh
```

It checks backend activation, rigid-body behavior, collision contracts, ray pickability, settling, areas, and joints.

### Device and browser acceptance checklist

Run both debug and release exports. Record the device, operating-system version, Godot version, and binary manifest checksum for each result.

- Backend registration succeeds and the project reports `Box3D Physics (Extension)`.
- A dynamic body falls, contacts the ground, and settles without tunneling.
- Area enter/exit signals fire.
- Ray queries and pickability behave as expected.
- Pin, hinge, and slider joints remain stable.
- Suspending and resuming the mobile app does not crash or duplicate the physics server.
- Rotating the device or resizing the browser does not corrupt the physics world.
- Returning to the main scene and freeing the world exits cleanly.
- Web debug and release exports load without missing-symbol or side-module errors in browser developer tools.
- Android release is tested on a physical arm64 device.
- iOS release is tested on a physical arm64 device.
- Web release is tested in current Chromium, Firefox, and Safari versions used by the product.

## 13. Known constraints and non-goals

- Android, iOS, and Web need project-owned device/browser testing even when compilation succeeds.
- The wrapper currently sets `workerCount = 1`. This is intentional for the portable profile but limits Box3D parallelism in large scenes.
- Web exports require custom Godot templates. Standard downloaded Web templates will not load this GDExtension.
- iOS simulator libraries are not included in the supported matrix.
- Android 32-bit ARM and x86 are not included. Add them only if the application's distribution requirements still need those ABIs.[^2]
- This work does not implement wrapper features that were already absent, including `SoftBody3D`, ConeTwist joints, Generic6DOF joints, and separation-ray shapes.
- Performance budgets must be established on representative low-end devices. A successful build is not a performance certification.

## 14. Safe upgrade procedure

Do not update Godot, `godot-cpp`, Box3D, Emscripten, or the Android NDK independently in the release branch.

For a dependency upgrade:

1. Create an isolated upgrade branch.
2. Update the exact SHA and label in `dependencies.lock`.
3. Update the corresponding submodule checkout.
4. Rebuild desktop headless tests.
5. Rebuild all Android, iOS, and Web variants.
6. Rebuild the custom Web export templates with the same Emscripten version used for the extension.
7. Run the complete device/browser acceptance checklist.
8. Generate a new release manifest and compare file checksums and sizes.
9. Commit the lock file and submodule pointer in the same commit.

When raising the Godot compatibility baseline, update all of these together:

- `compatibility_minimum` in `godot-box3d.gdextension`.
- `GODOT_CPP_REF` and label.
- `GODOT_ENGINE_REF` and label for Web templates.
- Test project's `config/features` value if needed.
- CI Godot test executable version.
- This guide and `PORT_STATUS.md`.

## 15. Official technical references

- Godot GDExtension overview: <https://docs.godotengine.org/en/4.3/tutorials/scripting/gdextension/what_is_gdextension.html>
- Godot `.gdextension` file format: <https://docs.godotengine.org/en/4.3/tutorials/scripting/gdextension/gdextension_file.html>
- Godot Web compilation and dynamic linking: <https://docs.godotengine.org/en/4.3/contributing/development/compiling/compiling_for_web.html>
- Godot Android export documentation: <https://docs.godotengine.org/en/4.3/tutorials/export/exporting_for_android.html>
- Godot iOS export documentation: <https://docs.godotengine.org/en/4.3/tutorials/export/exporting_for_ios.html>
- Official `godot-cpp` template: <https://github.com/godotengine/godot-cpp-template>
- Box3D source and build documentation: <https://github.com/erincatto/box3d>
- Emscripten SDK: <https://github.com/emscripten-core/emsdk>

[^1]: **SHA-256** means Secure Hash Algorithm 256-bit, used here to identify exact artifact bytes.
[^2]: **ABI** means application binary interface, the machine-level contract for a processor architecture and operating system.
