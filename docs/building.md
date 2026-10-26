---
title: Building from source
---

# Building from source

## Requirements

- CMake 3.22 or newer
- A C++17 compiler (GCC, Clang, or MSVC)
- Git, for the Box3D submodule

`godot-cpp` is fetched automatically during configure, so it does not need to be installed.

## Clone and build

```sh
git clone --recurse-submodules https://github.com/bearlikelion/godot-box3d.git
cd godot-box3d
cmake -B build
cmake --build build --parallel
```

If you cloned without `--recurse-submodules`:

```sh
git submodule update --init --recursive
```

The library lands in `bin/` and is copied into `test_project/addons/godot-box3d/bin/`, so the demo project always runs against a fresh build.

## Cross-compiling a Windows DLL from Linux

Needs MinGW-w64 installed.

```sh
cmake -B build-win --toolchain "$(pwd)/cmake/mingw-w64.cmake" -G Ninja
cmake --build build-win --parallel
```

This produces `bin/godot-box3d.dll`, statically linked against the MinGW runtime so no extra DLLs ship beside it. Release builds use the MSVC DLL from CI instead; this exists for exporting a Windows demo locally without waiting on a CI run.

The toolchain file also generates a capitalised `Windows.h` symlink, because `box3d/src/timer.c` includes it capitalised while MinGW ships lowercase `windows.h` and Linux filesystems are case-sensitive.

## Android

Android builds use the existing CMake project and official Godot export templates.
Install Android NDK r28b (`28.1.13356709`), CMake 3.22 or newer, and Ninja.
The supported ABIs are `arm64-v8a` (phones/tablets) and `x86_64` (emulators).

```sh
export ANDROID_NDK_ROOT="$ANDROID_HOME/ndk/28.1.13356709"
for abi in arm64-v8a x86_64; do
	for config in Debug Release; do
		cmake -B "build/android-$abi-$config" -G Ninja \
			-DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
			-DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-24 \
			-DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE="$config"
		CMAKE_BUILD_PARALLEL_LEVEL=4 cmake --build "build/android-$abi-$config" --parallel 4
	done
done
```

The four libraries land in `bin/android/` and are copied into the demo addon's
`bin/android/` directory. Separate build directories keep generated godot-cpp
bindings and native objects isolated. Box3D and the C++ runtime are linked into
the extension; no additional extension dependency library is needed.

To use the prebuilt addon, copy `addons/godot-box3d/` from the release ZIP and
select `Box3D Physics` before exporting. In an Android export preset, enable
`arm64-v8a`, optionally enable `x86_64`, and disable `armeabi-v7a` and `x86`.
Use the official templates matching your editor. The Godot 4.7 templates already
require API 24; use a minimum SDK of 24 or higher when configuring a Gradle build.
For the included demo, select the Compatibility renderer and enable
`Rendering > Textures > VRAM Compression > Import ETC2 ASTC` before export.
Follow Godot's [Android export setup](https://docs.godotengine.org/en/4.7/tutorials/export/exporting_for_android.html) for the SDK and signing configuration.
The existing demo's backend selector is disabled on Android because changing
the physics backend requires restarting the process.

CI builds both configurations for both ABIs and checks the entry point and 16 KB
ELF alignment. The final APK/AAB also needs correct packaging alignment and
16 KB-compatible engine/dependency libraries; follow Android's [page-size guide](https://developer.android.com/guide/practices/page-sizes).
Compilation and emulator checks do not qualify physical ARM64 devices, sustained
performance, or application lifecycle behavior on every supported Android version.

## Running the tests

```sh
GODOT_BIN=/path/to/godot scripts/run_headless_tests.sh
```

The runner builds the extension, registers it, checks the Box3D backend actually loaded, then runs the headless regression tests. It exits non-zero if a test fails or leaks a Box3D RID. `GODOT_BIN` can be omitted when a suitable `godot` is on `PATH`.

The runner registers only Box3D. A second GDExtension built against a different Godot version can abort the process before Box3D registers, which would fail every test for an unrelated reason.

Tests live in `test_project/tests/`, one file per behavior, each extending `SceneTree`. Assertions were validated against stock Godot Physics first, so they encode real engine behavior rather than whatever this backend happened to do.

## Regenerating demo screenshots

```sh
godot --path test_project --script res://tools/capture_demos.gd
```

Needs a real GPU. `--headless` forces the dummy rasterizer and writes blank images, so the script refuses to run there.
