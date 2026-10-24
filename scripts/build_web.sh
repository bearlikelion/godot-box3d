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
require_dependency_trees
scons_bin="$(scons_command)"

jobs="$(cpu_jobs)"

for target in template_debug template_release; do
    note "Building Web $target / wasm32 / no threads"
    "$scons_bin" -C "$repo_root" -j "$jobs" \
        platform=web \
        target="$target" \
        arch=wasm32 \
        threads=no \
        "$@"
done

python3 "$repo_root/scripts/verify_port.py" --platform web --require-binaries
note "Web GDExtension binaries are under $repo_root/bin/web"
