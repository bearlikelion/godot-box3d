---
title: Installation
---

# Installation

## Requirements

- Godot 4.3 or newer (the demo project targets 4.7)

## From a release

Download the addon zip from [Releases](https://github.com/bearlikelion/godot-box3d/releases) and copy `addons/godot-box3d/` into your project:

```
your-project/
  addons/godot-box3d/
    godot-box3d.gdextension
    bin/
      libgodot-box3d.so      (Linux)
      godot-box3d.dll        (Windows)
      libgodot-box3d.dylib   (macOS)
```

Then set **Project Settings → Physics → 3D → Physics Engine** to `Box3D Physics` and restart Godot.

The restart is required: the physics server is constructed once during engine startup, so changing the setting has no effect until the process restarts.

## Worker threads

The solver is multithreaded and defaults to one worker per physical core (efficiency cores and hyperthreads are excluded, since they slow the solver's synchronised stages down).
To override it, set `physics/box3d/worker_count` (visible with Advanced Settings on) to an explicit count; `0` means auto.
Worker count changes also need a restart, and simulation results are identical at any worker count.
The supported no-thread Web profile always uses one worker, regardless of this setting.

## Platform support

| Platform | Status |
|---|---|
| Linux x86_64 | Tested |
| Windows x86_64 | Tested |
| macOS arm64 | Compiles, untested |
| macOS x86_64 | Not built |

macOS is a work in progress. Builds are Apple Silicon only and are not notarized, so Intel Macs cannot load the library and Gatekeeper will need convincing on any Mac.

## Verifying it loaded

The setting records what you *asked* for, not what actually loaded, so reading it back is not a real check. Print the active server instead:

```gdscript
func _ready() -> void:
	print(ProjectSettings.get_setting("physics/3d/physics_engine"))
	print(PhysicsServer3D.get_class())
```

If the extension failed to load, Godot silently falls back to `GodotPhysics3D` and logs a warning at startup. A missing or mismatched library is the usual cause, so check the console output first.

## Uninstalling

Set the physics engine back to `DEFAULT`, restart, then delete `addons/godot-box3d/`. Scenes need no changes, since they only ever used stock Godot nodes.
