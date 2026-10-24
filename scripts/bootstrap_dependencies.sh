#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

check_only=0
if [[ "${1:-}" == "--check" ]]; then
    check_only=1
elif [[ $# -gt 0 ]]; then
    fail "Unknown argument: $1. Supported argument: --check"
fi

check_checkout() {
    local name="$1"
    local destination="$2"
    local expected_ref="$3"

    git -C "$destination" rev-parse --is-inside-work-tree >/dev/null 2>&1 || \
        fail "$name is not initialized at $destination"
    local actual
    actual="$(git -C "$destination" rev-parse HEAD)"
    [[ "$actual" == "$expected_ref" ]] || \
        fail "$name is at $actual; expected $expected_ref"
    note "$name revision is correct: $actual"
}

if [[ "$check_only" == "1" ]]; then
    require_command git
    check_checkout "Box3D" "$repo_root/box3d" "$BOX3D_REF"
    check_checkout "godot-cpp" "$repo_root/godot-cpp" "$GODOT_CPP_REF"
    exit 0
fi

ensure_git_checkout "Box3D" "$BOX3D_REPOSITORY" "$BOX3D_REF" "$BOX3D_LABEL" "$repo_root/box3d"
ensure_git_checkout "godot-cpp" "$GODOT_CPP_REPOSITORY" "$GODOT_CPP_REF" "$GODOT_CPP_LABEL" "$repo_root/godot-cpp"

note "All source dependencies are ready."
