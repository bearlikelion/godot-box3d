# Box3D Web quick start

Godot Web exports can load `godot-box3d` when the extension side module and the Godot Web template are built with matching dynamic-link settings. This repository uses a supported single-threaded profile with Godot 4.7 and Emscripten 4.0.20.

## Fastest route: use a prebuilt Web bundle

Download the `godot-box3d-web-release` artifact from a successful **Portable Android iOS Web Release** workflow run, or use the same ZIP attached to a project release when available.

The archive contains:

```text
godot-box3d-portable/
├── addons/godot-box3d/
│   ├── godot-box3d.gdextension
│   └── bin/web/
└── web-export-templates/
    ├── godot-box3d-web-debug.zip
    └── godot-box3d-web-release.zip
```

Copy `addons/godot-box3d` into the root of the Godot project:

```text
your-project/
└── addons/godot-box3d/
```

Open the project with Godot 4.7 when using the supplied templates. Then:

1. Open **Project Settings → Physics → 3D → Physics Engine**.
2. Select **Box3D Physics (Extension)** and restart the editor.
3. Create or edit a Web export preset.
4. Enable **Extensions Support** and disable **Thread Support**.
5. Set the custom debug and release templates to the two ZIPs from `web-export-templates/`.
6. Export the project normally.

The equivalent project setting is:

```ini
[physics]

3d/physics_engine="Box3D Physics (Extension)"
```

## Build the Web bundle from source

Requirements:

- Git and Python 3.
- A C/C++ build toolchain available on Linux or macOS.
- Enough free disk space for Emscripten, Godot source, and build outputs.

From the repository root, run:

```bash
MAX_JOBS=4 scripts/quickstart_web.sh
```

That command installs the pinned local SCons environment, checks out the exact dependency revisions, installs the pinned Emscripten toolchain, builds the debug/release extension modules and matching Godot templates, validates them, and creates:

```text
dist/godot-box3d-web-release.zip
```

The first template build compiles Godot itself and can take a while. Later runs are incremental. Set `MAX_JOBS` to a conservative value if the machine has limited memory.

## Serve an exported project locally

Web exports with extension support should be served over HTTP with cross-origin opener and embedder policies. The included development server adds those headers:

```bash
python3 /path/to/godot-box3d/scripts/serve_web_export.py \
  /path/to/your-project/build/web \
  --port 8060
```

Open <http://127.0.0.1:8060/>. Do not test by opening `index.html` directly from the filesystem.

## Verify the included smoke project

Point `GODOT_BIN` at a Godot 4.7 executable, then export the release smoke scene:

```bash
GODOT_BIN=/path/to/godot
mkdir -p build/web-smoke-release
"$GODOT_BIN" --headless \
  --path test_project \
  --export-release "Web Box3D Smoke" \
  ../build/web-smoke-release/index.html
```

For the automated browser assertion:

```bash
.venv/bin/python -m pip install playwright
.venv/bin/python -m playwright install chromium
.venv/bin/python scripts/run_web_smoke.py \
  --dir build/web-smoke-release \
  --screenshot build/web-smoke-release.png
```

The test passes only when the extension registers, Box3D is the requested backend, a rigid body settles on the floor, an area callback fires, and a hinge moves.

## Important compatibility rules

- Use the supplied custom templates. Standard Godot Web templates do not enable GDExtension dynamic linking.
- Keep extension support enabled and thread support disabled for this profile.
- Do not mix a side module and Godot template built with different Emscripten versions.
- Rebuild both the side module and templates when changing the Godot or Emscripten pins.
- The default build uses WebAssembly SIMD128. Use `BOX3D_DISABLE_SIMD=1 scripts/build_web.sh` only as a diagnostic fallback.

See [`MOBILE_WEB_INTEGRATION.md`](MOBILE_WEB_INTEGRATION.md) for architecture details, mobile targets, packaging, CI, and release validation.
