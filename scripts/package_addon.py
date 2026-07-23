#!/usr/bin/env python3
"""Create a deterministic Godot addon ZIP or full developer release bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
ADDON_ROOT = Path("addons/godot-box3d")

PLATFORM_BINARIES: dict[str, tuple[str, ...]] = {
    "android": (
        "bin/android/libgodot-box3d.android.template_debug.arm64.so",
        "bin/android/libgodot-box3d.android.template_release.arm64.so",
        "bin/android/libgodot-box3d.android.template_debug.x86_64.so",
        "bin/android/libgodot-box3d.android.template_release.x86_64.so",
    ),
    "ios": (
        "bin/ios/libgodot-box3d.ios.template_debug.arm64.dylib",
        "bin/ios/libgodot-box3d.ios.template_release.arm64.dylib",
    ),
    "web": (
        "bin/web/libgodot-box3d.web.template_debug.wasm32.nothreads.wasm",
        "bin/web/libgodot-box3d.web.template_release.wasm32.nothreads.wasm",
    ),
    "desktop": (
        "bin/macos/libgodot-box3d.macos.template_debug.dylib",
        "bin/macos/libgodot-box3d.macos.template_release.dylib",
        "bin/windows/godot-box3d.windows.template_debug.x86_64.dll",
        "bin/windows/godot-box3d.windows.template_release.x86_64.dll",
        "bin/linux/libgodot-box3d.linux.template_debug.x86_64.so",
        "bin/linux/libgodot-box3d.linux.template_release.x86_64.so",
        "bin/linux/libgodot-box3d.linux.template_debug.arm64.so",
        "bin/linux/libgodot-box3d.linux.template_release.arm64.so",
    ),
}

WEB_TEMPLATE_FILENAMES = (
    "godot-box3d-web-debug.zip",
    "godot-box3d-web-release.zip",
    "manifest.json",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=REPO_ROOT / "dist/godot-box3d-portable-release.zip",
        help="Destination ZIP path.",
    )
    parser.add_argument(
        "--mode",
        choices=("addon", "bundle"),
        default="bundle",
        help="addon: only the Godot addon tree; bundle: addon plus integration documentation and Web templates.",
    )
    parser.add_argument(
        "--platform",
        action="append",
        choices=sorted(PLATFORM_BINARIES),
        help="Platform group to include. Defaults to android, ios, and web. May be repeated.",
    )
    parser.add_argument(
        "--web-templates-dir",
        type=Path,
        help="Directory containing custom dynamic-link Web export template ZIPs.",
    )
    parser.add_argument(
        "--allow-missing",
        action="store_true",
        help="Create a source/partial package instead of failing on missing native binaries or templates.",
    )
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_required(source: Path, destination: Path, *, allow_missing: bool, missing: list[str]) -> None:
    if not source.is_file() or source.stat().st_size == 0:
        missing.append(str(source.relative_to(REPO_ROOT) if source.is_relative_to(REPO_ROOT) else source))
        if allow_missing:
            return
        raise FileNotFoundError(f"Required file is missing or empty: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def zip_tree(root: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    source_date_epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "315532800"))
    timestamp = datetime.fromtimestamp(max(source_date_epoch, 315532800), tz=timezone.utc)
    zip_date = (timestamp.year, timestamp.month, timestamp.day, timestamp.hour, timestamp.minute, timestamp.second)

    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(item for item in root.rglob("*") if item.is_file()):
            relative = path.relative_to(root).as_posix()
            info = zipfile.ZipInfo(relative, date_time=zip_date)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o644 & 0xFFFF) << 16
            archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)


def main() -> int:
    args = parse_args()
    selected = args.platform or ["android", "ios", "web"]
    missing: list[str] = []

    with tempfile.TemporaryDirectory(prefix="godot-box3d-package-") as temp_dir:
        staging = Path(temp_dir) / ("godot-box3d-portable" if args.mode == "bundle" else "package")
        addon = staging / ADDON_ROOT if args.mode == "bundle" else staging / ADDON_ROOT
        addon.mkdir(parents=True, exist_ok=True)

        copy_required(
            REPO_ROOT / "godot-box3d.gdextension",
            addon / "godot-box3d.gdextension",
            allow_missing=False,
            missing=missing,
        )

        uid = REPO_ROOT / "test_project/addons/godot-box3d/godot-box3d.gdextension.uid"
        if uid.is_file():
            shutil.copy2(uid, addon / uid.name)

        for filename in ("LICENSE", "README.md", "THIRD_PARTY_NOTICES.md"):
            source = REPO_ROOT / filename
            if source.is_file():
                shutil.copy2(source, addon / filename)

        included_binaries: list[dict[str, str | int]] = []
        for platform in selected:
            for relative_text in PLATFORM_BINARIES[platform]:
                relative = Path(relative_text)
                source = REPO_ROOT / relative
                destination = addon / relative
                before_missing_count = len(missing)
                copy_required(source, destination, allow_missing=args.allow_missing, missing=missing)
                if len(missing) == before_missing_count:
                    included_binaries.append(
                        {
                            "path": (ADDON_ROOT / relative).as_posix(),
                            "size": source.stat().st_size,
                            "sha256": sha256(source),
                        }
                    )

        web_templates: list[dict[str, str | int]] = []
        if args.mode == "bundle":
            for filename in (
                "WEB_QUICKSTART.md",
                "MOBILE_WEB_INTEGRATION.md",
                "PORT_STATUS.md",
                "VALIDATION_REPORT.md",
                "CHANGELOG_PORT.md",
                "THIRD_PARTY_NOTICES.md",
                "README.md",
                "dependencies.lock",
            ):
                source = REPO_ROOT / filename
                copy_required(
                    source,
                    staging / filename,
                    allow_missing=False,
                    missing=missing,
                )

            if args.web_templates_dir:
                template_dir = args.web_templates_dir.resolve()
                for filename in WEB_TEMPLATE_FILENAMES:
                    source = template_dir / filename
                    destination = staging / "web-export-templates" / filename
                    before_missing_count = len(missing)
                    copy_required(source, destination, allow_missing=args.allow_missing, missing=missing)
                    if len(missing) == before_missing_count:
                        web_templates.append(
                            {
                                "path": f"web-export-templates/{filename}",
                                "size": source.stat().st_size,
                                "sha256": sha256(source),
                            }
                        )
            elif not args.allow_missing:
                raise FileNotFoundError(
                    "Bundle mode requires --web-templates-dir unless --allow-missing is used."
                )

        lock_values: dict[str, str] = {}
        for raw_line in (REPO_ROOT / "dependencies.lock").read_text(encoding="utf-8").splitlines():
            if "=" in raw_line and not raw_line.lstrip().startswith("#"):
                key, value = raw_line.split("=", 1)
                lock_values[key] = value.strip().strip('"')

        manifest = {
            "package": "godot-box3d portable release",
            "mode": args.mode,
            "platform_groups": selected,
            "complete": not missing,
            "missing": missing,
            "dependencies": {
                "godot_cpp_ref": lock_values.get("GODOT_CPP_REF"),
                "box3d_ref": lock_values.get("BOX3D_REF"),
                "godot_engine_ref": lock_values.get("GODOT_ENGINE_REF"),
                "emsdk_ref": lock_values.get("EMSDK_REF"),
                "emscripten_version": lock_values.get("EMSCRIPTEN_VERSION"),
                "scons_version": lock_values.get("SCONS_VERSION"),
                "android_ndk_version": lock_values.get("ANDROID_NDK_VERSION"),
                "android_api_level": lock_values.get("ANDROID_API_LEVEL"),
                "ios_min_version": lock_values.get("IOS_MIN_VERSION"),
            },
            "binaries": included_binaries,
            "web_export_templates": web_templates,
        }
        manifest_path = staging / "BUILD_MANIFEST.json"
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

        archive_root = staging.parent if args.mode == "bundle" else staging
        zip_tree(archive_root, args.output.resolve())

    print(f"Created {args.output.resolve()}")
    if missing:
        print("Package is intentionally partial; missing files:")
        for item in missing:
            print(f"  - {item}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
