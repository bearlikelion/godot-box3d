# Portable Port Changelog

## 2026-07-21 — Web developer onboarding

### Added

- One-command Web setup, build, validation, and packaging through `scripts/quickstart_web.sh`.
- A concise `WEB_QUICKSTART.md` for installing the addon and configuring Godot Web exports.
- A local Web export server that supplies extension-compatible opener/embedder headers.
- A standalone Web bundle artifact in the portable release workflow.

### Changed

- Source packaging is repository-native and no longer depends on a separate integration patch.
- Integration and validation documentation is product-neutral.

## 2026-07-21 — Android, iOS, and Web source port

### Added

- Official-template-style SCons build for desktop, Android, iOS, and Web.
- Exact dependency/toolchain lock file.
- `godot-cpp` submodule declaration.
- Android arm64/x86-64 build automation.
- iOS arm64 device and universal simulator XCFramework build automation with
  verified deployment targets.
- Web wasm32 no-thread side-module build automation.
- Dynamic-link-enabled Godot Web template build automation.
- Portable release GitHub Actions workflow.
- Strict source/binary verifier and deterministic release packager.
- Portable exported-app smoke-test scene.
- Complete mobile/Web integration and release guidance.

### Changed

- `.gdextension` paths now follow Godot's platform/target/architecture naming convention and are relative to the descriptor.
- `reloadable` is disabled for portable native packaging.
- CMake now includes `godot-cpp` in the same build graph so cross-toolchain settings propagate.
- Desktop CI and headless tests use the same SCons naming/layout as portable targets.
- README now documents mobile and browser support.

### Removed

- Nested `ExternalProject_Add` download/build of `godot-cpp`.
- Desktop-only unqualified binary names in the `.gdextension` descriptor.
