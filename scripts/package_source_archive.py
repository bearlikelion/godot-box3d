#!/usr/bin/env python3
"""Create a deterministic source archive for godot-box3d."""

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
ARCHIVE_ROOT_NAME = "godot-box3d-source"

EXCLUDED_DIRECTORY_NAMES = {
    ".deps",
    ".git",
    ".godot",
    ".idea",
    ".venv",
    ".vscode",
    "__pycache__",
    "bin",
    "box3d",
    "build",
    "dist",
    "godot-cpp",
}
EXCLUDED_FILE_NAMES = {
    ".DS_Store",
    ".sconsign.dblite",
}
REQUIRED_SOURCE_FILES = (
    "WEB_QUICKSTART.md",
    "MOBILE_WEB_INTEGRATION.md",
    "PORT_STATUS.md",
    "VALIDATION_REPORT.md",
    "SConstruct",
    "CMakeLists.txt",
    "godot-box3d.gdextension",
    "godot_cpp_build_profile.json",
    "dependencies.lock",
    ".github/workflows/portable-release.yml",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=REPO_ROOT / "dist/godot-box3d-source.zip",
        help="Destination ZIP path.",
    )
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_lock(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value.strip().strip('"')
    return values


def should_include(path: Path) -> bool:
    relative = path.relative_to(REPO_ROOT)
    if any(part in EXCLUDED_DIRECTORY_NAMES for part in relative.parts[:-1]):
        return False
    if path.name in EXCLUDED_FILE_NAMES:
        return False
    if path.suffix in {".pyc", ".pyo"}:
        return False
    if path.name.endswith("~"):
        return False
    return path.is_file()


def copy_source_tree(staging_root: Path) -> None:
    for source in sorted(REPO_ROOT.rglob("*")):
        if not should_include(source):
            continue
        relative = source.relative_to(REPO_ROOT)
        destination = staging_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def build_manifest(staging_root: Path) -> dict[str, object]:
    lock = parse_lock(staging_root / "dependencies.lock")
    files: list[dict[str, object]] = []
    for path in sorted(item for item in staging_root.rglob("*") if item.is_file()):
        if path.name == "SOURCE_MANIFEST.json":
            continue
        relative = path.relative_to(staging_root).as_posix()
        files.append(
            {
                "path": relative,
                "size": path.stat().st_size,
                "sha256": sha256(path),
                "executable": bool(path.stat().st_mode & 0o111),
            }
        )

    return {
        "package": "godot-box3d source archive",
        "archive_root": ARCHIVE_ROOT_NAME,
        "source_only": True,
        "source_date_epoch": int(os.environ.get("SOURCE_DATE_EPOCH", "315532800")),
        "dependency_and_toolchain_pins": {
            key: lock.get(key)
            for key in (
                "GODOT_CPP_REF",
                "BOX3D_REF",
                "GODOT_ENGINE_REF",
                "EMSDK_REF",
                "SCONS_VERSION",
                "ANDROID_NDK_VERSION",
                "ANDROID_API_LEVEL",
                "IOS_MIN_VERSION",
                "EMSCRIPTEN_VERSION",
            )
        },
        "excluded_generated_content": [
            "native/Web build outputs",
            "downloaded dependency trees",
            "platform SDKs and signing material",
            "local build caches and virtual environments",
        ],
        "files": files,
    }


def zip_tree(staging_parent: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    source_date_epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "315532800"))
    timestamp = datetime.fromtimestamp(max(source_date_epoch, 315532800), tz=timezone.utc)
    zip_date = (
        timestamp.year,
        timestamp.month,
        timestamp.day,
        timestamp.hour,
        timestamp.minute,
        timestamp.second,
    )

    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(item for item in staging_parent.rglob("*") if item.is_file()):
            relative = path.relative_to(staging_parent).as_posix()
            mode = path.stat().st_mode & 0o777
            info = zipfile.ZipInfo(relative, date_time=zip_date)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (mode & 0xFFFF) << 16
            archive.writestr(
                info,
                path.read_bytes(),
                compress_type=zipfile.ZIP_DEFLATED,
                compresslevel=9,
            )


def main() -> int:
    args = parse_args()

    missing = [item for item in REQUIRED_SOURCE_FILES if not (REPO_ROOT / item).is_file()]
    if missing:
        formatted = "\n".join(f"  - {item}" for item in missing)
        raise FileNotFoundError(f"Source archive cannot be packaged; required files are missing:\n{formatted}")

    with tempfile.TemporaryDirectory(prefix="godot-box3d-source-archive-") as temp_dir:
        staging_parent = Path(temp_dir)
        staging_root = staging_parent / ARCHIVE_ROOT_NAME
        staging_root.mkdir(parents=True)
        copy_source_tree(staging_root)

        manifest = build_manifest(staging_root)
        (staging_root / "SOURCE_MANIFEST.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        zip_tree(staging_parent, args.output.resolve())

    print(f"Created {args.output.resolve()}")
    print(f"Included {len(manifest['files']) + 1} files under {ARCHIVE_ROOT_NAME}/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
