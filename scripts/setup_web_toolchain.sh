#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

require_command git
require_command python3

emsdk_dir="${EMSDK_DIR:-$repo_root/.deps/emsdk}"
mkdir -p "$(dirname "$emsdk_dir")"
ensure_git_checkout \
    "Emscripten SDK manager" \
    "$EMSDK_REPOSITORY" \
    "$EMSDK_REF" \
    "$EMSDK_LABEL" \
    "$emsdk_dir"

note "Installing and activating Emscripten $EMSCRIPTEN_VERSION"
"$emsdk_dir/emsdk" install "$EMSCRIPTEN_VERSION"
"$emsdk_dir/emsdk" activate "$EMSCRIPTEN_VERSION"

note "Web toolchain is ready. The build scripts automatically source: $emsdk_dir/emsdk_env.sh"
