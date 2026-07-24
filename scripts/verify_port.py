#!/usr/bin/env python3
"""Validate the portable Godot Box3D source and optional build artifacts."""

from __future__ import annotations

import argparse
import json
import plistlib
import re
import struct
import sys
from pathlib import Path
from typing import Iterable

REPO_ROOT = Path(__file__).resolve().parents[1]

EXPECTED_LIBRARIES: dict[str, dict[str, str]] = {
    "android": {
        "android.arm64.single.debug": "./bin/android/libgodot-box3d.android.template_debug.arm64.so",
        "android.arm64.single.release": "./bin/android/libgodot-box3d.android.template_release.arm64.so",
        "android.x86_64.single.debug": "./bin/android/libgodot-box3d.android.template_debug.x86_64.so",
        "android.x86_64.single.release": "./bin/android/libgodot-box3d.android.template_release.x86_64.so",
    },
    "ios": {
        "ios.debug": "./bin/ios/libgodot-box3d.ios.template_debug.xcframework",
        "ios.release": "./bin/ios/libgodot-box3d.ios.template_release.xcframework",
    },
    "web": {
        "web.wasm32.single.debug": "./bin/web/libgodot-box3d.web.template_debug.wasm32.nothreads.wasm",
        "web.wasm32.single.release": "./bin/web/libgodot-box3d.web.template_release.wasm32.nothreads.wasm",
    },
    "desktop": {
        "macos.single.debug": "./bin/macos/libgodot-box3d.macos.template_debug.dylib",
        "macos.single.release": "./bin/macos/libgodot-box3d.macos.template_release.dylib",
        "windows.x86_64.single.debug": "./bin/windows/godot-box3d.windows.template_debug.x86_64.dll",
        "windows.x86_64.single.release": "./bin/windows/godot-box3d.windows.template_release.x86_64.dll",
        "linux.x86_64.single.debug": "./bin/linux/libgodot-box3d.linux.template_debug.x86_64.so",
        "linux.x86_64.single.release": "./bin/linux/libgodot-box3d.linux.template_release.x86_64.so",
        "linux.arm64.single.debug": "./bin/linux/libgodot-box3d.linux.template_debug.arm64.so",
        "linux.arm64.single.release": "./bin/linux/libgodot-box3d.linux.template_release.arm64.so",
    },
}

PINNED_REFS = {
    "GODOT_CPP_REF": "fbbf9ec4efd8f1055d00edb8d926eef8ba4c2cce",
    "BOX3D_REF": "8441b4a06d6d09dcfb0b0f704df4d847d1437b92",
    "GODOT_ENGINE_REF": "5b4e0cb0fd279832bbdd69fed5354d4e5ad26f88",
    "EMSDK_REF": "e4fe26ef59168ff44f4c23c466e497bf60b3411e",
}

ELF_MACHINE = {
    "arm64": 183,  # EM_AARCH64
    "x86_64": 62,  # EM_X86_64
}
MACHO_CPU_TYPE_ARM64 = 0x0100000C
MACHO_CPU_TYPE_X86_64 = 0x01000007
MACHO_FILETYPE_DYLIB = 6
MACHO_LC_BUILD_VERSION = 0x32
MACHO_PLATFORM_IOS = 2
MACHO_PLATFORM_IOS_SIMULATOR = 7
PE_MACHINE_AMD64 = 0x8664


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--platform",
        action="append",
        choices=sorted(EXPECTED_LIBRARIES),
        help="Limit binary checks to a platform group. May be repeated.",
    )
    parser.add_argument(
        "--require-binaries",
        action="store_true",
        help="Fail unless all selected platform binaries exist, are non-empty, and match their advertised format/architecture.",
    )
    parser.add_argument(
        "--require-dependencies",
        action="store_true",
        help="Fail unless Box3D and godot-cpp source trees are populated.",
    )
    parser.add_argument("--json", action="store_true", help="Emit a JSON result.")
    return parser.parse_args()


def parse_descriptor(path: Path) -> dict[str, str]:
    entries: dict[str, str] = {}
    in_libraries = False
    assignment = re.compile(r'^([^;#][^=]+?)\s*=\s*"([^"]+)"\s*$')
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line == "[libraries]":
            in_libraries = True
            continue
        if line.startswith("[") and line != "[libraries]":
            in_libraries = False
        if not in_libraries or not line or line.startswith((";", "#")):
            continue
        match = assignment.match(line)
        if match:
            entries[match.group(1).strip()] = match.group(2)
    return entries


def parse_shell_assignments(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    assignment = re.compile(r'^([A-Z0-9_]+)="([^"]*)"$')
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        match = assignment.match(raw_line.strip())
        if match:
            result[match.group(1)] = match.group(2)
    return result


def selected_groups(platforms: Iterable[str] | None) -> list[str]:
    return list(dict.fromkeys(platforms)) if platforms else list(EXPECTED_LIBRARIES)


def _inspect_elf(path: Path, feature_tag: str, data: bytes) -> str | None:
    if len(data) < 20 or data[:4] != b"\x7fELF":
        return "not an ELF binary"
    if data[4] != 2:
        return "ELF binary is not 64-bit"
    if data[5] not in (1, 2):
        return "ELF binary has an unsupported byte order"
    endian = "<" if data[5] == 1 else ">"
    file_type, machine = struct.unpack_from(endian + "HH", data, 16)
    if file_type != 3:
        return f"ELF type is {file_type}, expected ET_DYN (3)"

    expected_arch = "arm64" if ".arm64." in feature_tag else "x86_64" if ".x86_64." in feature_tag else None
    if expected_arch and machine != ELF_MACHINE[expected_arch]:
        return f"ELF machine is {machine}, expected {ELF_MACHINE[expected_arch]} for {expected_arch}"
    return None


def _inspect_macho(path: Path, feature_tag: str, data: bytes) -> str | None:
    if len(data) < 8:
        return "Mach-O binary is too small"

    magic = data[:4]
    # 64-bit little-endian Mach-O, used for the supported iOS arm64 output.
    if magic == b"\xcf\xfa\xed\xfe":
        cpu_type = struct.unpack_from("<I", data, 4)[0]
        if feature_tag.startswith("ios.") and cpu_type != MACHO_CPU_TYPE_ARM64:
            return f"Mach-O CPU type is 0x{cpu_type:08x}, expected arm64"
        return None

    # Accept other valid thin/fat Mach-O encodings for the architecture-neutral
    # macOS descriptor entries. The iOS path must remain a thin arm64 dylib.
    valid_macho_magics = {
        b"\xfe\xed\xfa\xcf",  # 64-bit big-endian
        b"\xce\xfa\xed\xfe",  # 32-bit little-endian
        b"\xfe\xed\xfa\xce",  # 32-bit big-endian
        b"\xca\xfe\xba\xbe",  # fat big-endian
        b"\xbe\xba\xfe\xca",  # fat little-endian
        b"\xca\xfe\xba\xbf",  # fat64 big-endian
        b"\xbf\xba\xfe\xca",  # fat64 little-endian
    }
    if feature_tag.startswith("ios."):
        return "iOS library is not a thin 64-bit little-endian arm64 Mach-O binary"
    if magic not in valid_macho_magics:
        return "not a recognized Mach-O binary"
    return None


def _decode_macho_version(value: int) -> tuple[int, int, int]:
    return (value >> 16, (value >> 8) & 0xFF, value & 0xFF)


def _parse_version(value: str) -> tuple[int, int, int]:
    components = value.split(".")
    if not components or len(components) > 3 or any(not item.isdigit() for item in components):
        raise ValueError(f"invalid version: {value!r}")
    numbers = [int(item) for item in components]
    numbers.extend([0] * (3 - len(numbers)))
    return tuple(numbers)


def _thin_macho_build(
    data: bytes,
) -> tuple[int, int, tuple[int, int, int]] | str:
    if len(data) < 32 or data[:4] != b"\xcf\xfa\xed\xfe":
        return "slice is not a 64-bit little-endian Mach-O binary"

    cpu_type, file_type, command_count, command_bytes = struct.unpack_from("<I4xIII", data, 4)
    if file_type != MACHO_FILETYPE_DYLIB:
        return f"Mach-O file type is {file_type}, expected MH_DYLIB ({MACHO_FILETYPE_DYLIB})"

    offset = 32
    command_end = offset + command_bytes
    if command_end > len(data):
        return "Mach-O load-command region exceeds the binary"

    for _ in range(command_count):
        if offset + 8 > command_end:
            return "Mach-O load command header is truncated"
        command, command_size = struct.unpack_from("<II", data, offset)
        if command_size < 8 or offset + command_size > command_end:
            return "Mach-O load command has an invalid size"
        if command == MACHO_LC_BUILD_VERSION:
            if command_size < 24:
                return "Mach-O LC_BUILD_VERSION command is truncated"
            platform, minimum_os = struct.unpack_from("<II", data, offset + 8)
            return cpu_type, platform, _decode_macho_version(minimum_os)
        offset += command_size

    return "Mach-O binary has no LC_BUILD_VERSION command"


def _macho_slices(data: bytes) -> list[bytes] | str:
    if data[:4] == b"\xcf\xfa\xed\xfe":
        return [data]

    fat_formats = {
        b"\xca\xfe\xba\xbe": (">", False),
        b"\xbe\xba\xfe\xca": ("<", False),
        b"\xca\xfe\xba\xbf": (">", True),
        b"\xbf\xba\xfe\xca": ("<", True),
    }
    format_info = fat_formats.get(data[:4])
    if not format_info or len(data) < 8:
        return "binary is neither a thin 64-bit Mach-O nor a supported fat Mach-O"

    endian, is_64_bit = format_info
    architecture_count = struct.unpack_from(endian + "I", data, 4)[0]
    entry_size = 32 if is_64_bit else 20
    table_end = 8 + architecture_count * entry_size
    if architecture_count == 0 or table_end > len(data):
        return "fat Mach-O architecture table is invalid"

    slices: list[bytes] = []
    for index in range(architecture_count):
        entry_offset = 8 + index * entry_size
        if is_64_bit:
            _, _, slice_offset, slice_size = struct.unpack_from(endian + "IIQQ", data, entry_offset)
        else:
            _, _, slice_offset, slice_size = struct.unpack_from(endian + "IIII", data, entry_offset)
        slice_end = slice_offset + slice_size
        if slice_size == 0 or slice_end > len(data):
            return "fat Mach-O slice is outside the binary"
        slices.append(data[slice_offset:slice_end])
    return slices


def inspect_ios_xcframework(path: Path, minimum_version: str) -> tuple[list[str], int]:
    errors: list[str] = []
    plist_path = path / "Info.plist"
    if not plist_path.is_file():
        return ["XCFramework is missing Info.plist"], 0

    try:
        plist = plistlib.loads(plist_path.read_bytes())
    except (OSError, plistlib.InvalidFileException) as exc:
        return [f"could not parse XCFramework Info.plist: {exc}"], 0

    libraries = plist.get("AvailableLibraries")
    if not isinstance(libraries, list):
        return ["XCFramework Info.plist has no AvailableLibraries array"], 0

    try:
        expected_minimum = _parse_version(minimum_version)
    except ValueError as exc:
        return [str(exc)], 0

    expected_slices = {
        ("ios", ""): ({MACHO_CPU_TYPE_ARM64}, {"arm64"}, MACHO_PLATFORM_IOS),
        (
            "ios",
            "simulator",
        ): (
            {MACHO_CPU_TYPE_ARM64, MACHO_CPU_TYPE_X86_64},
            {"arm64", "x86_64"},
            MACHO_PLATFORM_IOS_SIMULATOR,
        ),
    }
    seen: set[tuple[str, str]] = set()
    inspected_binary_count = 0

    for library in libraries:
        if not isinstance(library, dict):
            errors.append("XCFramework AvailableLibraries contains a non-dictionary entry")
            continue
        platform = str(library.get("SupportedPlatform", ""))
        variant = str(library.get("SupportedPlatformVariant", ""))
        key = (platform, variant)
        expected = expected_slices.get(key)
        if expected is None:
            errors.append(f"XCFramework contains unsupported platform slice {key!r}")
            continue
        if key in seen:
            errors.append(f"XCFramework contains duplicate platform slice {key!r}")
            continue
        seen.add(key)

        expected_cpu_types, expected_architectures, expected_platform = expected
        architectures = set(library.get("SupportedArchitectures", []))
        if architectures != expected_architectures:
            errors.append(
                f"XCFramework slice {key!r} architectures are {sorted(architectures)!r}; "
                f"expected {sorted(expected_architectures)!r}"
            )

        identifier = str(library.get("LibraryIdentifier", ""))
        binary_path = str(library.get("BinaryPath", ""))
        if (
            not identifier
            or not binary_path
            or Path(identifier).is_absolute()
            or Path(binary_path).is_absolute()
            or ".." in Path(identifier).parts
            or ".." in Path(binary_path).parts
        ):
            errors.append(f"XCFramework slice {key!r} has an unsafe library path")
            continue

        binary = path / identifier / binary_path
        if not binary.is_file() or binary.stat().st_size == 0:
            errors.append(f"XCFramework slice {key!r} binary is missing or empty: {binary}")
            continue

        slices = _macho_slices(binary.read_bytes())
        if isinstance(slices, str):
            errors.append(f"XCFramework slice {key!r}: {slices}")
            continue

        actual_cpu_types: set[int] = set()
        for macho_slice in slices:
            build = _thin_macho_build(macho_slice)
            if isinstance(build, str):
                errors.append(f"XCFramework slice {key!r}: {build}")
                continue
            cpu_type, macho_platform, encoded_minimum = build
            actual_cpu_types.add(cpu_type)
            if macho_platform != expected_platform:
                errors.append(
                    f"XCFramework slice {key!r} Mach-O platform is {macho_platform}; "
                    f"expected {expected_platform}"
                )
            if encoded_minimum != expected_minimum:
                errors.append(
                    f"XCFramework slice {key!r} minimum OS is "
                    f"{'.'.join(str(item) for item in encoded_minimum)}; "
                    f"expected {minimum_version}"
                )
        if actual_cpu_types != expected_cpu_types:
            errors.append(
                f"XCFramework slice {key!r} CPU types are "
                f"{sorted(hex(item) for item in actual_cpu_types)!r}; expected "
                f"{sorted(hex(item) for item in expected_cpu_types)!r}"
            )
        inspected_binary_count += 1

    missing_slices = set(expected_slices) - seen
    for key in sorted(missing_slices):
        errors.append(f"XCFramework is missing required platform slice {key!r}")

    return errors, inspected_binary_count


def _inspect_pe(path: Path, data: bytes) -> str | None:
    if len(data) < 0x40 or data[:2] != b"MZ":
        return "not a PE/COFF binary"
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 6 > len(data):
        return "PE header offset is outside the file"
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        return "PE signature is missing"
    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    if machine != PE_MACHINE_AMD64:
        return f"PE machine is 0x{machine:04x}, expected AMD64"
    return None


def inspect_binary(path: Path, feature_tag: str) -> str | None:
    try:
        data = path.read_bytes()
    except OSError as exc:
        return f"could not read binary: {exc}"

    suffix = path.suffix.lower()
    if suffix == ".wasm":
        if len(data) < 8 or data[:4] != b"\0asm":
            return "not a WebAssembly module"
        if data[4:8] != b"\x01\x00\x00\x00":
            return "unsupported WebAssembly binary version"
        return None
    if suffix == ".so":
        return _inspect_elf(path, feature_tag, data)
    if suffix == ".dylib":
        return _inspect_macho(path, feature_tag, data)
    if suffix == ".dll":
        return _inspect_pe(path, data)
    return f"unsupported binary suffix: {suffix or '<none>'}"


def main() -> int:
    args = parse_args()
    errors: list[str] = []
    warnings: list[str] = []
    checks: list[str] = []
    inspected_binary_count = 0
    lock_path = REPO_ROOT / "dependencies.lock"
    early_lock = parse_shell_assignments(lock_path) if lock_path.is_file() else {}
    ios_min_version = early_lock.get("IOS_MIN_VERSION", "")

    descriptor = REPO_ROOT / "godot-box3d.gdextension"
    test_descriptor = REPO_ROOT / "test_project/addons/godot-box3d/godot-box3d.gdextension"
    if not descriptor.is_file():
        errors.append("Missing root godot-box3d.gdextension descriptor.")
    else:
        text = descriptor.read_text(encoding="utf-8")
        if 'compatibility_minimum = "4.3"' not in text:
            errors.append("Descriptor compatibility_minimum must remain 4.3.")
        if "reloadable = false" not in text:
            errors.append("Descriptor must use reloadable = false for portable packaging.")
        libraries = parse_descriptor(descriptor)
        for group in selected_groups(args.platform):
            for feature_tag, expected_path in EXPECTED_LIBRARIES[group].items():
                actual = libraries.get(feature_tag)
                if actual != expected_path:
                    errors.append(
                        f"Descriptor mismatch for {feature_tag}: expected {expected_path!r}, got {actual!r}."
                    )
                    continue
                if not args.require_binaries:
                    continue

                binary = (descriptor.parent / expected_path).resolve()
                if expected_path.endswith(".xcframework"):
                    if not binary.is_dir():
                        errors.append(f"Missing {group} XCFramework: {binary.relative_to(REPO_ROOT)}")
                        continue
                    xcframework_errors, binary_count = inspect_ios_xcframework(
                        binary,
                        ios_min_version,
                    )
                    for xcframework_error in xcframework_errors:
                        errors.append(
                            f"Invalid XCFramework for {feature_tag} "
                            f"({binary.relative_to(REPO_ROOT)}): {xcframework_error}."
                        )
                    if not xcframework_errors:
                        inspected_binary_count += binary_count
                elif not binary.is_file():
                    errors.append(f"Missing {group} binary: {binary.relative_to(REPO_ROOT)}")
                elif binary.stat().st_size == 0:
                    errors.append(f"Binary is empty: {binary.relative_to(REPO_ROOT)}")
                else:
                    format_error = inspect_binary(binary, feature_tag)
                    if format_error:
                        errors.append(
                            f"Invalid binary for {feature_tag} ({binary.relative_to(REPO_ROOT)}): {format_error}."
                        )
                    else:
                        inspected_binary_count += 1
        checks.append("GDExtension feature tags and paths")
        if args.require_binaries and inspected_binary_count:
            checks.append(f"Native/Web binary formats and architectures ({inspected_binary_count} files)")

    if descriptor.is_file() and test_descriptor.is_file():
        if descriptor.read_bytes() != test_descriptor.read_bytes():
            errors.append("The root and test-project .gdextension descriptors differ.")
        else:
            checks.append("Test-project descriptor synchronization")
    else:
        errors.append("Missing test-project GDExtension descriptor.")

    lock: dict[str, str] = {}
    if not lock_path.is_file():
        errors.append("Missing dependencies.lock.")
    else:
        lock = parse_shell_assignments(lock_path)
        for key, expected in PINNED_REFS.items():
            actual = lock.get(key)
            if actual != expected:
                errors.append(f"Dependency pin {key} must be {expected}; got {actual!r}.")
            elif not re.fullmatch(r"[0-9a-f]{40}", actual):
                errors.append(f"Dependency pin {key} is not a full 40-character commit SHA.")
        for key in ("SCONS_VERSION", "ANDROID_NDK_VERSION", "ANDROID_API_LEVEL", "IOS_MIN_VERSION", "EMSCRIPTEN_VERSION"):
            if not lock.get(key):
                errors.append(f"Toolchain pin {key} is missing from dependencies.lock.")
        if lock.get("IOS_MIN_VERSION") != "14.0":
            errors.append(
                "IOS_MIN_VERSION must remain 14.0 so Box3D allocation APIs and the "
                "downstream Godot Light deployment target agree."
            )
        checks.append("Pinned dependency and toolchain revisions")

    gitmodules = REPO_ROOT / ".gitmodules"
    if not gitmodules.is_file():
        errors.append("Missing .gitmodules.")
    else:
        gitmodules_text = gitmodules.read_text(encoding="utf-8")
        for dependency in ("box3d", "godot-cpp"):
            if f'path = {dependency}' not in gitmodules_text:
                errors.append(f".gitmodules does not define the {dependency} dependency path.")
        checks.append("Dependency submodule declarations")

    sconstruct = REPO_ROOT / "SConstruct"
    if not sconstruct.is_file():
        errors.append("Missing SConstruct portable build.")
    else:
        source = sconstruct.read_text(encoding="utf-8")
        try:
            compile(source, str(sconstruct), "exec")
        except SyntaxError as exc:
            errors.append(f"SConstruct syntax error: {exc}")
        required_fragments = (
            'local_env["build_profile"] = os.path.abspath("godot_cpp_build_profile.json")',
            'local_env["generate_bindings"] = True',
            'ARGUMENTS.get("platform") == "web"',
            'local_env["threads"] = False',
            'box3d_env.get("is_msvc", False)',
            'box3d_env.Prepend(CFLAGS=["/std:c17"])',
            'box3d_env.Prepend(CFLAGS=["-std=gnu17"])',
            'box3d_env.AppendUnique(CCFLAGS=["-msimd128", "-msse2"])',
            'is_ios_simulator = platform_name == "ios"',
            'env.AppendUnique(LINKFLAGS=[deployment_flag])',
            'SConscript("godot-cpp/SConstruct"',
        )
        for fragment in required_fragments:
            if fragment not in source:
                errors.append(f"SConstruct is missing required portable-build fragment: {fragment}")
        checks.append("Portable SCons configuration")

    build_profile = REPO_ROOT / "godot_cpp_build_profile.json"
    if not build_profile.is_file():
        errors.append("Missing godot_cpp_build_profile.json.")
    else:
        try:
            profile = json.loads(build_profile.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            errors.append(f"Invalid godot_cpp_build_profile.json: {exc}")
        else:
            required_classes = {
                "PhysicsDirectBodyState3DExtension",
                "PhysicsDirectSpaceState3DExtension",
                "PhysicsServer3DExtension",
                "PhysicsServer3DManager",
            }
            enabled = set(profile.get("enabled_classes", []))
            missing_classes = sorted(required_classes - enabled)
            if missing_classes:
                errors.append(
                    "godot-cpp build profile omits required classes: " + ", ".join(missing_classes)
                )
            else:
                checks.append("Reduced godot-cpp binding profile")

    space_source = REPO_ROOT / "src/spaces/box3d_space_3d.cpp"
    if not space_source.is_file() or "def.workerCount = 1;" not in space_source.read_text(encoding="utf-8"):
        errors.append("Box3D world creation must keep workerCount = 1 for the supported portable profile.")
    else:
        checks.append("Single-worker portable physics profile")

    asymmetric_filter_fragments = {
        REPO_ROOT / "src/misc/box3d_collision_filter.hpp": (
            "BOX3D_GODOT_BROADPHASE_BIT",
            "box3d_godot_shape_filter",
            "box3d_godot_query_filter",
        ),
        REPO_ROOT / "src/spaces/box3d_space_3d.cpp": (
            "collision_filters_interact",
            "get_collision_mask() & p_b->get_collision_layer()) != 0 ||",
        ),
        REPO_ROOT / "src/spaces/box3d_physics_direct_space_state_3d.cpp": (
            "object->get_collision_layer() & p_filter.collision_mask",
            "filter.set_collision_mask(p_body.get_collision_mask())",
        ),
        REPO_ROOT / "test_project/physics_contract_test.gd": (
            "body_test_motion uses the moving body's mask without requiring the target mask",
            "physical contacts occur when either body's mask scans the other body's layer",
        ),
    }
    asymmetric_filter_missing: list[str] = []
    for path, fragments in asymmetric_filter_fragments.items():
        source = path.read_text(encoding="utf-8") if path.is_file() else ""
        for fragment in fragments:
            if fragment not in source:
                asymmetric_filter_missing.append(f"{path.relative_to(REPO_ROOT)}: {fragment}")
    if asymmetric_filter_missing:
        errors.append(
            "Godot-compatible asymmetric collision filtering is incomplete: "
            + "; ".join(asymmetric_filter_missing)
        )
    else:
        checks.append("Godot-compatible asymmetric collision filtering")

    old_external = REPO_ROOT / "cmake/GodotBox3DExternalGodotCpp.cmake"
    if old_external.exists():
        errors.append("The old nested ExternalProject godot-cpp helper must be removed.")
    cmake = REPO_ROOT / "CMakeLists.txt"
    if cmake.is_file() and "add_subdirectory(godot-cpp EXCLUDE_FROM_ALL)" in cmake.read_text(encoding="utf-8"):
        checks.append("Cross-toolchain-aware CMake dependency graph")
    else:
        errors.append("CMake must build godot-cpp in the same cross-compilation graph.")

    required_scripts = (
        "bootstrap_dependencies.sh",
        "build_android.sh",
        "build_ios.sh",
        "build_web.sh",
        "build_web_templates.sh",
        "quickstart_web.sh",
        "serve_web_export.py",
        "package_addon.py",
        "package_source_archive.py",
    )
    for filename in required_scripts:
        if not (REPO_ROOT / "scripts" / filename).is_file():
            errors.append(f"Missing required build/package script: scripts/{filename}")
    if not any(error.startswith("Missing required build/package script") for error in errors):
        checks.append("Portable build and package scripts")

    ios_build = REPO_ROOT / "scripts/build_ios.sh"
    if ios_build.is_file():
        ios_build_source = ios_build.read_text(encoding="utf-8")
        for fragment in (
            "arch=arm64",
            "arch=universal",
            "ios_simulator=yes",
            "xcodebuild -create-xcframework",
            "libgodot-box3d.ios.${target}.xcframework",
        ):
            if fragment not in ios_build_source:
                errors.append(f"iOS build script is missing required XCFramework fragment: {fragment}")

    required_docs = (
        "WEB_QUICKSTART.md",
        "MOBILE_WEB_INTEGRATION.md",
        "PORT_STATUS.md",
        "VALIDATION_REPORT.md",
        "THIRD_PARTY_NOTICES.md",
    )
    for filename in required_docs:
        if not (REPO_ROOT / filename).is_file():
            errors.append(f"Missing required developer document: {filename}")
    if not any(error.startswith("Missing required developer document") for error in errors):
        checks.append("Developer integration documentation")

    portable_workflow = REPO_ROOT / ".github/workflows/portable-release.yml"
    if not portable_workflow.is_file():
        errors.append("Missing portable release workflow.")
    else:
        workflow = portable_workflow.read_text(encoding="utf-8")
        for job_name in (
            "android:",
            "ios:",
            "web-extension:",
            "web-templates:",
            "web-package:",
            "package:",
        ):
            if job_name not in workflow:
                errors.append(f"Portable release workflow is missing job {job_name.rstrip(':')}.")
        checks.append("Android/iOS/Web release workflow")

    if args.require_dependencies:
        for dependency in ("box3d", "godot-cpp"):
            path = REPO_ROOT / dependency
            if not path.is_dir() or not any(path.iterdir()):
                errors.append(f"Dependency tree is not populated: {dependency}")
        checks.append("Populated dependency source trees")

    result = {
        "ok": not errors,
        "checks": checks,
        "warnings": warnings,
        "errors": errors,
        "selected_platforms": selected_groups(args.platform),
        "required_binaries": args.require_binaries,
        "inspected_binary_count": inspected_binary_count,
    }

    if args.json:
        print(json.dumps(result, indent=2))
    else:
        for check in checks:
            print(f"PASS: {check}")
        for warning in warnings:
            print(f"WARN: {warning}", file=sys.stderr)
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        print("Portable port validation passed." if not errors else "Portable port validation failed.")

    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
