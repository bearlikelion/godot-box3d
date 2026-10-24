#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

emsdk_dir="${EMSDK_DIR:-$repo_root/.deps/emsdk}"
if ! command -v emcc >/dev/null 2>&1 && [[ -f "$emsdk_dir/emsdk_env.sh" ]]; then
    # shellcheck disable=SC1090
    source "$emsdk_dir/emsdk_env.sh" >/dev/null
fi

require_pinned_emscripten
scons_bin="$(scons_command)"

godot_source_dir="${GODOT_SOURCE_DIR:-$repo_root/.deps/godot-${GODOT_ENGINE_LABEL}}"
ensure_git_checkout \
    "Godot Engine" \
    "$GODOT_ENGINE_REPOSITORY" \
    "$GODOT_ENGINE_REF" \
    "$GODOT_ENGINE_LABEL" \
    "$godot_source_dir"

jobs="$(cpu_jobs)"
output_dir="${WEB_TEMPLATE_OUTPUT_DIR:-$repo_root/dist/web-export-templates}"
mkdir -p "$output_dir"

for target in template_debug template_release; do
    note "Building Godot Web export template: $target, dynamic linking, no threads"
    "$scons_bin" -C "$godot_source_dir" -j "$jobs" \
        platform=web \
        target="$target" \
        arch=wasm32 \
        threads=no \
        dlink_enabled=yes

    expected="$godot_source_dir/bin/godot.web.${target}.wasm32.nothreads.dlink.zip"
    if [[ ! -f "$expected" ]]; then
        expected="$(find "$godot_source_dir/bin" -maxdepth 1 -type f \
            -name "godot.web.${target}*.nothreads*.dlink*.zip" -print -quit)"
    fi
    [[ -n "$expected" && -f "$expected" ]] || fail "Could not find the generated $target Web template ZIP."

    case "$target" in
        template_debug) destination="$output_dir/godot-box3d-web-debug.zip" ;;
        template_release) destination="$output_dir/godot-box3d-web-release.zip" ;;
        *) fail "Unexpected target: $target" ;;
    esac
    cp -f "$expected" "$destination"
    note "Copied $(basename "$destination")"
done

python3 - "$output_dir/manifest.json" "$GODOT_ENGINE_REF" "$EMSCRIPTEN_VERSION" <<'PY'
import json
import pathlib
import sys

manifest_path = pathlib.Path(sys.argv[1])
manifest = {
    "godot_engine_ref": sys.argv[2],
    "emscripten_version": sys.argv[3],
    "threads": False,
    "dlink_enabled": True,
    "debug_template": "godot-box3d-web-debug.zip",
    "release_template": "godot-box3d-web-release.zip",
}
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
PY

note "Custom Web export templates are under $output_dir"
