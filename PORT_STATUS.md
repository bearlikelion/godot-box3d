# Portable Port Status

## Implementation status

| Target | Source/build implementation | CI build | Local compile validation | Runtime validation |
|---|---|---|---|---|
| Android arm64 | Complete | Included | Debug and release passed | Physical-device gate remains |
| Android x86-64 | Complete | Included | Debug and release passed | Emulator gate remains |
| iOS arm64 device | Complete | Included on macOS | Debug and release passed on macOS | Signing/device gate remains |
| iOS arm64/x86-64 simulator | Complete | Included on macOS | Debug and release XCFramework slices passed on macOS | Exported simulator smoke remains |
| Web wasm32, no threads | Complete | Included | Debug and release passed | Chromium smoke passed; cross-browser gate remains |
| Custom Godot Web templates | Complete | Included | Debug and release passed | Chromium smoke passed; cross-browser gate remains |

“Complete” means the source graph, descriptor mapping, pinned toolchain setup, build scripts, validation, CI, and packaging are present. It does not mean an untested target is certified for production deployment.

## Implemented in this package

- Cross-platform SCons build based on Godot's official `godot-cpp` platform model.
- Reduced `godot-cpp` build profile containing the classes needed by this physics backend and their dependencies.
- Box3D C17 sources compiled directly into the GDExtension library, avoiding a second runtime native library.
- Android arm64 and x86-64 debug/release targets.
- iOS arm64 device and universal arm64/x86-64 simulator debug/release
  XCFramework targets.
- Web wasm32 debug/release side modules using the supported no-thread profile.
- WebAssembly SIMD128 flags with an opt-in scalar diagnostic fallback.
- Matching custom Godot Web export-template build with `dlink_enabled=yes` and `threads=no`.
- Platform-qualified `.gdextension` feature tags and relative library paths.
- Exact commits for Box3D, `godot-cpp`, the Godot Web-template source, and the Emscripten SDK manager.
- Exact SCons, Android NDK/API, iOS minimum-version, and Emscripten compiler pins.
- One-command dependency bootstrap and platform build scripts.
- Static source validation plus binary magic/architecture validation.
- Deterministic addon, runtime-bundle, and source-archive packaging with SHA-256 checksums.
- GitHub Actions release workflow for all three targets and Web templates.
- Existing desktop CMake path updated so dependencies inherit the same cross toolchain.
- Portable exported-app smoke-test scene in the test project.
- Explicit source includes correcting transitive-header assumptions discovered during real compilation.

## Validation completed for this repository

- Linux x86-64 debug and release compiled and linked against the pinned sources.
- Android arm64 debug and release cross-compiled with NDK 23.2.8568313 for API 21.
- Android x86-64 debug and release cross-compiled with the same NDK/API.
- iOS arm64 device and universal simulator debug/release binaries compiled,
  linked, and packaged into XCFrameworks with an iOS 14 minimum target.
- All compiled outputs have the advertised ELF architecture and shared-library type.
- All compiled outputs export `godot_box3d_main`.
- The secondary CMake graph configured successfully with pinned dependencies.
- Python, SConstruct, shell, descriptor, lock-file, workflow, source-layout, and package checks passed.
- The deterministic release packager was exercised against a complete synthetic artifact layout.
- Godot 4.7 headless backend, regression, physics, ray, fall, settle, area, and joint tests passed on macOS.
- Web debug and release side modules compiled with Emscripten 4.0.20.
- Matching Godot 4.7 debug and release Web templates compiled with dynamic linking and no threads.
- The standalone exported Web smoke test passed in Chrome, including body settling, area callbacks, and a hinge joint.
- A downstream Godot Web project loaded both debug and release exports and reported `Box3D Physics (Extension)` as the active 3D backend.

Detailed hashes and limitations are in `VALIDATION_REPORT.md`.

## Validation still required in target environments

- Android loading, lifecycle, and performance on physical arm64 devices.
- Android x86-64 emulator loading when emulator support is required.
- iOS Godot/Xcode embedding, simulator execution, code signing, App Store
  validation, and physical-device execution.
- Web loading in the product's supported Firefox and Safari versions and in release CI.
- Production-scale physics benchmarks on representative low-end mobile hardware.

The supplied `portable-release.yml` workflow performs the cross-compiles in controlled Linux and macOS runners. Device/browser acceptance remains a release gate.

## Supported release profile

| Setting | Supported value |
|---|---|
| Godot API baseline | 4.3 |
| Godot precision | Single |
| Box3D workers | 1 |
| Android | arm64 and x86-64, API 21+, NDK 23.2.8568313 |
| iOS | arm64 device plus arm64/x86-64 simulator, minimum iOS 14.0 |
| Web | wasm32, no threads, dynamic linking, Emscripten 4.0.20 |
| Default build parallelism | At most 8 jobs unless overridden |

Any other combination is experimental until it has its own descriptor tags, CI artifacts, and acceptance results.
