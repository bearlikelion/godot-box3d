# Validation Report

**Validation date:** July 21, 2026

**Scope:** Portable Godot Box3D source port for Android, iOS, and Web

## Result summary

The portable source and build system are internally consistent. Native compilation passed for the complete supported Android matrix plus Linux regression builds, and local Godot 4.7 Web compilation and Chrome runtime smoke tests passed for both debug and release profiles.

| Validation area | Result |
|---|---|
| Source/layout validation | Passed |
| Python and SConstruct syntax | Passed |
| Shell-script syntax | Passed |
| Workflow YAML parsing | Passed |
| Root/test `.gdextension` synchronization | Passed |
| Dependency and toolchain pins | Passed |
| CMake configure with pinned dependencies | Passed |
| Linux x86-64 debug compile/link | Passed |
| Linux x86-64 release compile/link | Passed |
| Android arm64 debug compile/link | Passed |
| Android arm64 release compile/link | Passed |
| Android x86-64 debug compile/link | Passed |
| Android x86-64 release compile/link | Passed |
| Exported `godot_box3d_main` symbol | Passed for all compiled binaries |
| Android ELF architecture and API metadata | Passed |
| iOS compile/link | Not executed in the Linux validation environment |
| Web extension compile/link | Passed for debug and release with Emscripten 4.0.20 |
| Custom Godot Web template compile | Passed for Godot 4.7 debug and release, dynamic linking, no threads |
| Godot runtime/headless tests | Passed locally with Godot 4.7 |
| Physical-device/browser acceptance | Chrome Web smoke passed; mobile-device and cross-browser acceptance remain |

## Exact source inputs

| Component | Revision/version |
|---|---|
| Godot API baseline | 4.3 |
| `godot-cpp` | `fbbf9ec4efd8f1055d00edb8d926eef8ba4c2cce` |
| Box3D | `8441b4a06d6d09dcfb0b0f704df4d847d1437b92` (v0.1.0) |
| Godot source for Web templates | `5b4e0cb0fd279832bbdd69fed5354d4e5ad26f88` (4.7-stable) |
| Emscripten SDK manager | `e4fe26ef59168ff44f4c23c466e497bf60b3411e` |
| Emscripten compiler target | 4.0.20 |
| SCons | 4.8.1 |
| Android NDK | 23.2.8568313 (r23c) |
| Android API level | 21 |
| iOS minimum version | 12.0 |

## Validation host and compilers

- Linux x86-64 validation host.
- GCC 14.2.0 for host compilation.
- CMake 3.31.6 and Ninja 1.12.1 for CMake configuration.
- Android NDK r23c Clang 12.0.9 for Android cross-compilation.
- SCons 4.8.1.
- Local Web follow-up used macOS, Godot 4.7-stable, Emscripten 4.0.20, and Google Chrome.

## Real compiled artifacts

These checksums identify the exact binaries produced during validation. They are recorded for traceability but are not embedded in the source archive; the release workflow rebuilds them from the pinned source.

| Artifact | Bytes | SHA-256[^1] |
|---|---:|---|
| `bin/android/libgodot-box3d.android.template_debug.arm64.so` | 1,506,024 | `7ecb372e16dca1bdb97bb15bf3bca62f5fe6df5bcf2ff28e48f8a6375d032070` |
| `bin/android/libgodot-box3d.android.template_release.arm64.so` | 1,498,536 | `a3240c08871e0e0d668d7e1bd91de34b7fe8612f685f3cf4c62779ec51cc9034` |
| `bin/android/libgodot-box3d.android.template_debug.x86_64.so` | 1,705,616 | `6566278758c5e5b8fd9ec3e223059166517b2313510546d971c93ea02d509451` |
| `bin/android/libgodot-box3d.android.template_release.x86_64.so` | 1,719,456 | `f426e863462cbb04d8cc58c2368b2beec87ddc964049e230b1fc6bd7377f9fb9` |
| `bin/linux/libgodot-box3d.linux.template_debug.x86_64.so` | 1,707,032 | `5870746d16d91b6142cec6eeadf4e7eb4269bc6694576d496b7439f2f68e1387` |
| `bin/linux/libgodot-box3d.linux.template_release.x86_64.so` | 1,804,960 | `6ad5ac19d7bbbd00c4e1a843adb6fccbca4e6a96368a098769480578a01d002e` |
| `bin/web/libgodot-box3d.web.template_debug.wasm32.nothreads.wasm` | 952,226 | `e8029a59de546e73c0228af8674dc02b004d8113b4b25b2afffd8446fd1c5aa7` |
| `bin/web/libgodot-box3d.web.template_release.wasm32.nothreads.wasm` | 964,671 | `05157f241437f9eb2c87ebf6c72d9c219ba6fb3e69a95e21b6d385b6209918b2` |

`file`, `readelf`, and dynamic-symbol inspection confirmed:

- Android arm64 outputs are 64-bit AArch64 shared objects for Android API 21, built by NDK r23c.
- Android x86-64 outputs are 64-bit x86-64 shared objects for Android API 21, built by NDK r23c.
- Linux outputs are 64-bit x86-64 shared objects.
- Web outputs are wasm32 side modules built for the single-threaded dynamic-link profile.
- Every compiled library exports `godot_box3d_main`.

## Source defects found and corrected by compilation

Real compilation exposed transitive-include assumptions that could vary by platform and compiler. The port adds explicit includes for:

- Box3D transform/math types in the joint base header.
- Godot `HashSet` in the physics server and direct-space-state headers.
- Godot `LocalVector` in the convex and concave polygon implementations.

These are source-correctness fixes, not platform conditionals, and should remain in every target branch.

## Reproduction commands

After initializing dependencies:

```bash
scripts/setup_python_build_env.sh
scripts/bootstrap_dependencies.sh
python3 scripts/verify_port.py --require-dependencies
```

Linux regression build:

```bash
scons -j 8 platform=linux target=template_debug arch=x86_64
scons -j 8 platform=linux target=template_release arch=x86_64
```

Android matrix:

```bash
scripts/setup_android_toolchain.sh
scripts/build_android.sh
python3 scripts/verify_port.py --platform android --require-binaries
```

Web side modules, matching templates, and browser smoke:

```bash
scripts/setup_web_toolchain.sh
scripts/build_web.sh
scripts/build_web_templates.sh
python3 scripts/verify_port.py --platform web --require-binaries
python3 scripts/run_web_smoke.py --dir build/web-smoke-release
```

The portable GitHub Actions workflow supplies the remaining platform-specific validation:

- iOS compile/link on macOS.
- Repeatable Web extension/template builds and browser smoke in CI.
- Strict complete-matrix packaging.

## Remaining release acceptance

The following are intentionally release gates rather than claims made by this repository:

1. Install debug and release Android exports on a physical arm64 device.
2. Launch the x86-64 debug export on an emulator when emulator support is part of the product matrix.
3. Build, sign, install, suspend, resume, and terminate an iOS release export on physical hardware.
4. Load both Web variants with the matching custom templates in the product's supported Chromium, Firefox, and Safari versions.
5. Run `portable_smoke_test.tscn` on every target and retain screenshots/logs with `BUILD_MANIFEST.json`.
6. Establish physics-body/contact budgets on representative low-end mobile devices and browsers.

Successful compilation does not replace these runtime, lifecycle, signing, browser-loader, or performance checks.

[^1]: **SHA-256** means Secure Hash Algorithm 256-bit, used to identify exact artifact bytes.
