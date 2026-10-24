#!/usr/bin/env python3
"""Validate the supported Web build configuration and optional artifacts."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
WEB_LIBRARIES = {
    "web.wasm32.single.debug": "./bin/web/libgodot-box3d.web.template_debug.wasm32.nothreads.wasm",
    "web.wasm32.single.release": "./bin/web/libgodot-box3d.web.template_release.wasm32.nothreads.wasm",
}
PINNED_REFS = {
    "BOX3D_REF": "3fc20f5b453ba9e14cdf54ecafa87a2a4bcdf53c",
    "GODOT_CPP_REF": "fbbf9ec4efd8f1055d00edb8d926eef8ba4c2cce",
    "GODOT_ENGINE_REF": "5b4e0cb0fd279832bbdd69fed5354d4e5ad26f88",
    "EMSDK_REF": "e4fe26ef59168ff44f4c23c466e497bf60b3411e",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", action="append", choices=("web",))
    parser.add_argument("--require-binaries", action="store_true")
    parser.add_argument("--require-dependencies", action="store_true")
    return parser.parse_args()


def parse_descriptor(path: Path) -> dict[str, str]:
    libraries: dict[str, str] = {}
    in_libraries = False
    assignment = re.compile(r'^([^;#][^=]+?)\s*=\s*"([^"]+)"\s*$')
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line == "[libraries]":
            in_libraries = True
            continue
        if line.startswith("[") and line != "[libraries]":
            in_libraries = False
        if not in_libraries:
            continue
        match = assignment.match(line)
        if match:
            libraries[match.group(1).strip()] = match.group(2)
    return libraries


def parse_lock(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    assignment = re.compile(r'^([A-Z0-9_]+)="([^"]*)"$')
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        match = assignment.match(raw_line.strip())
        if match:
            values[match.group(1)] = match.group(2)
    return values


def main() -> int:
    args = parse_args()
    errors: list[str] = []
    checks: list[str] = []

    descriptors = (
        REPO_ROOT / "godot-box3d.gdextension",
        REPO_ROOT / "test_project/addons/godot-box3d/godot-box3d.gdextension",
        REPO_ROOT / "web_smoke_project/addons/godot-box3d/godot-box3d.gdextension",
    )
    for descriptor in descriptors:
        if not descriptor.is_file():
            errors.append(f"Missing descriptor: {descriptor.relative_to(REPO_ROOT)}")
            continue
        descriptor_text = descriptor.read_text(encoding="utf-8")
        if 'compatibility_minimum = "4.3"' not in descriptor_text:
            errors.append(f"{descriptor.relative_to(REPO_ROOT)} must support Godot 4.3 or newer")
        libraries = parse_descriptor(descriptor)
        for feature, expected in WEB_LIBRARIES.items():
            if libraries.get(feature) != expected:
                errors.append(
                    f"{descriptor.relative_to(REPO_ROOT)} has the wrong path for {feature}"
                )
    checks.append("Web GDExtension feature tags")

    lock_path = REPO_ROOT / "dependencies.lock"
    if not lock_path.is_file():
        errors.append("Missing dependencies.lock")
        lock: dict[str, str] = {}
    else:
        lock = parse_lock(lock_path)
        for key, expected in PINNED_REFS.items():
            if lock.get(key) != expected:
                errors.append(f"{key} must be pinned to {expected}")
        if lock.get("BOX3D_REPOSITORY") != "https://github.com/erincatto/box3d.git":
            errors.append("BOX3D_REPOSITORY must use the original maintainer repository")
        for key in ("SCONS_VERSION", "EMSCRIPTEN_VERSION"):
            if not lock.get(key):
                errors.append(f"Missing toolchain pin: {key}")
    checks.append("Reproducible source and toolchain pins")

    gitmodules = (REPO_ROOT / ".gitmodules").read_text(encoding="utf-8")
    if "https://github.com/erincatto/box3d.git" not in gitmodules:
        errors.append(".gitmodules must use the original Box3D repository")
    if "https://github.com/godotengine/godot-cpp.git" not in gitmodules:
        errors.append(".gitmodules must define the godot-cpp source dependency")
    checks.append("Official dependency repositories")

    sconstruct_path = REPO_ROOT / "SConstruct"
    if not sconstruct_path.is_file():
        errors.append("Missing SConstruct")
    else:
        sconstruct = sconstruct_path.read_text(encoding="utf-8")
        try:
            compile(sconstruct, str(sconstruct_path), "exec")
        except SyntaxError as exc:
            errors.append(f"SConstruct syntax error: {exc}")
        for fragment in (
            'ARGUMENTS.get("platform") == "web"',
            'local_env["threads"] = False',
            'box3d_env.AppendUnique(CCFLAGS=["-msimd128", "-msse2"])',
            'SConscript("godot-cpp/SConstruct"',
        ):
            if fragment not in sconstruct:
                errors.append(f"SConstruct is missing: {fragment}")
    checks.append("Single-threaded Web build configuration")

    required_files = (
        "WEB_QUICKSTART.md",
        ".github/workflows/web.yml",
        "scripts/build_web.sh",
        "scripts/build_web_templates.sh",
        "scripts/quickstart_web.sh",
        "scripts/run_web_smoke.py",
        "scripts/serve_web_export.py",
        "web_smoke_project/project.godot",
        "web_smoke_project/export_presets.cfg",
        "web_smoke_project/smoke_test.gd",
        "web_smoke_project/smoke_test.tscn",
    )
    for relative in required_files:
        if not (REPO_ROOT / relative).is_file():
            errors.append(f"Missing Web support file: {relative}")
    checks.append("Web quick start, CI, and Chromium smoke project")

    if args.require_dependencies:
        for dependency in ("box3d", "godot-cpp"):
            path = REPO_ROOT / dependency
            if not path.is_dir() or not any(path.iterdir()):
                errors.append(f"Dependency is not initialized: {dependency}")
        checks.append("Initialized dependency trees")

    if args.require_binaries:
        for relative in WEB_LIBRARIES.values():
            binary = REPO_ROOT / relative.removeprefix("./")
            if not binary.is_file():
                errors.append(f"Missing Web binary: {binary.relative_to(REPO_ROOT)}")
                continue
            header = binary.read_bytes()[:8]
            if header != b"\x00asm\x01\x00\x00\x00":
                errors.append(f"Invalid WebAssembly binary: {binary.relative_to(REPO_ROOT)}")
        checks.append("Debug and release WebAssembly side modules")

    for check in checks:
        print(f"PASS: {check}")
    for error in errors:
        print(f"FAIL: {error}", file=sys.stderr)
    if errors:
        print("Web support validation failed.", file=sys.stderr)
        return 1
    print("Web support validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
