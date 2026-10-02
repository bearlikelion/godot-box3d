# Box3D Binding Generator

Generates C++ GDExtension bindings and struct data classes for the Box3D C API,
producing the static `Box3DAPI` class exposed to GDScript.

- Binding methods → `src/api/bindings/box3d_api_*.gen.cpp` (+ `box3d_api_generated.gen.hpp`,
  `box3d_api_register.gen.cpp`)
- Struct data classes → `src/api/bindings/data_classes/`
- Godot class reference XML → `doc_classes/` (embedded into the binary at build time)

All generated output is gitignored and **swept before each run**, so what's on disk
always matches the current generator + `config.yaml`.

## Setup (one-time)

```bash
tools/generator/setup.sh
```

Creates a Python venv at `tools/generator/.venv` with the required dependencies (pyyaml).

## Workflow

From the repo root:

```bash
# Run everything (data classes → audit → bindings → docs)
tools/generator/.venv/bin/python tools/generator/generate.py all

# Or individual steps:
tools/generator/.venv/bin/python tools/generator/generate.py audit         # coverage report
tools/generator/.venv/bin/python tools/generator/generate.py bindings      # C++ binding methods
tools/generator/.venv/bin/python tools/generator/generate.py data_classes  # struct data classes
tools/generator/.venv/bin/python tools/generator/generate.py docs          # class reference XML
```

The project root is auto-detected from the script location; the optional `root`
argument overrides it.

Then build:

```bash
cmake -B build && cmake --build build
```

Output: `bin/libgodot-box3d.{so,dynlib,dll}`

## What Each Step Does

### `audit`
Parses the Box3D headers and reports coverage: bound / skipped / missing, per
domain, plus any *implicit* skips (structural skips with no `config.yaml` entry)
that need a human decision. Exits with error code 1 if any functions are missing.
It also verifies:
- **Hand-written coverage** — every function listed in `handwritten_functions`
  must have a matching declaration in `src/api/*.hpp`; missing ones are reported
  and count toward the failure exit code.
- **Config drift** — entries in the config lists are checked against the parsed
  headers so an upstream rename (e.g. `b3CreateHeightfieldShape` →
  `b3CreateHeightFieldShape`) can never silently stop matching again.
- **Unused types** — generated data classes and bound enums that no bound or
  hand-written function (transitively) references — the checklist for scope
  trimming.

### `bindings`
Parses the Box3D headers → generates `Box3DAPI` static method bindings →
`src/api/bindings/`. Handles C-style array parameters (auto-detected direction/count,
with overrides), output structs with internal arrays (collision manifolds), and
owning RefCounted wrappers for heap-blob returns.

### `data_classes`
Parses the Box3D headers → generates a `RefCounted` wrapper class per bindable
struct → `src/api/bindings/data_classes/`. Embedded structs get typed getters/setters,
`to_b3()`/`from_b3()`, and nested-view support; heap blobs (hull/mesh/heightfield/
compound) get owning wrappers that free their allocation in the destructor.

### `docs`
Parses the Box3D headers → generates Godot class reference XML for `Box3DAPI` and
every data class → `doc_classes/`. The XML is compiled into the extension at build
time via `embed_doc_data.py` (wired up in `cmake/GodotBox3DEmbedDocs.cmake`).

## Configuration

All configuration is in `config.yaml`:

- `skip_structs` — structs that should NOT get data classes (opaque handles,
  event iteration types, collision internals)
- `skip_functions` — functions that are deliberately OUT OF SCOPE and will never
  be bound (debug draw, allocator, OS helpers). This is a policy list,
  NOT for functions with hand-written implementations.
- `handwritten_functions` — functions that are intentionally NOT auto-generated
  because a human implements them in `src/api/` (creation/destruction, callbacks,
  events, ID lookups, user data, query-with-callback functions). The audit
  verifies each has a declaration in `src/api/*.hpp`.
- `skip_enums` — enums that should NOT map to `int` (debug-draw enums)
- `skip_types` — parameter types that make a function unbindable (callback
  function-pointer types, opaque handles)
- `id_types` — Box3D ID types and their RID/pack mappings
- `math_types` — Box3D math types and their conversion function names
- `array_params` — overrides for array parameters that auto-detection gets wrong
  (direction, count param/function, fixed counts, output structs with internal arrays)
- `domain_rules` — ordered `[domain, pattern]` pairs used to classify every
  function into a domain (audit display, generated file naming, docs grouping).
  Matched top-to-bottom, first match wins; specific exceptions are listed first
  so they beat the broad common-naming rules below them. Anything unmatched
  falls into `other`
- `domain_headers` — extra `#include`s per domain for generated
  `box3d_api_{domain}.gen.cpp` files (helpers declared outside the base includes)

**Philosophy**: the parser and classification do the heavy lifting; config only
handles exceptions (blacklists).

## Classification

`classification.py` is the single source of truth for whether a function can be
auto-generated. Both the binding generator and the audit use
`classification.classify_function`, so they can never disagree about what is
generatable, what is deliberately skipped, and why. Skipping is only allowed when
backed by an explicit `config.yaml` entry; structural skips without one are
reported by the audit as *implicit* skips.

## Adding Hand-Written Bindings

For functions that can't be auto-generated (output pointers, user data, complex logic):

1. Add the declaration to `src/api/box3d_api.hpp`
2. Add the implementation to `src/api/box3d_api.cpp` (or a sibling, e.g. `box3d_api_collision.cpp`)
3. Add the `ClassDB::bind_static_method()` call in `Box3DAPI::_bind_methods()`
4. Add the b3 function name to `handwritten_functions` in `config.yaml` so the
   generator never auto-generates it and the audit verifies your implementation
   exists (it reports a failure until the declaration is present)

## Architecture

```
Box3D C headers
    ↓ parser.py (extract functions, structs, enums, docs)
    ├── classification.py  (decide generate vs. skip — shared with audit)
    ├── array_detection.py (detect C-style array params)
    ├── array_emit.py      (emit array-aware method bodies)
    ├── binding_generator.py → src/api/bindings/box3d_api_*.gen.cpp (+ header + register)
    ├── data_class_generator.py → src/api/bindings/data_classes/
    ├── audit.py           → coverage report
    └── docs.py            → doc_classes/*.xml (embedded at build)
    ↓
src/api/box3d_api.hpp/.cpp (hand-written methods + includes generated bindings)
    ↓
cmake → bin/libgodot-box3d.{so,dynlib,dll}
```