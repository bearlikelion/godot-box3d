#!/usr/bin/env bash
# Shared helpers for the portable build scripts. This file is meant to be sourced.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
lock_file="$repo_root/dependencies.lock"

if [[ ! -f "$lock_file" ]]; then
    printf 'ERROR: Missing dependency lock file: %s\n' "$lock_file" >&2
    exit 1
fi

# shellcheck source=../dependencies.lock
source "$lock_file"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

note() {
    printf '==> %s\n' "$*"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Required command '$1' was not found in PATH."
}

cpu_jobs() {
    local detected
    local maximum="${MAX_JOBS:-8}"

    if [[ -n "${JOBS:-}" ]]; then
        detected="$JOBS"
    elif command -v nproc >/dev/null 2>&1; then
        detected="$(nproc)"
    elif command -v sysctl >/dev/null 2>&1; then
        detected="$(sysctl -n hw.logicalcpu 2>/dev/null || printf '4\n')"
    else
        detected=4
    fi

    [[ "$detected" =~ ^[1-9][0-9]*$ ]] || fail "JOBS must be a positive integer; got '$detected'."
    [[ "$maximum" =~ ^[1-9][0-9]*$ ]] || fail "MAX_JOBS must be a positive integer; got '$maximum'."

    if (( detected > maximum )); then
        detected="$maximum"
    fi
    printf '%s\n' "$detected"
}

require_dependency_trees() {
    local dependency
    for dependency in box3d godot-cpp; do
        if [[ ! -d "$repo_root/$dependency" ]] || [[ -z "$(find "$repo_root/$dependency" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
            fail "Dependency '$dependency' is missing. Run scripts/bootstrap_dependencies.sh first."
        fi
    done
}

scons_command() {
    local requested="${SCONS_BIN:-}"
    local candidate

    if [[ -n "$requested" ]]; then
        command -v "$requested" >/dev/null 2>&1 || fail "SCONS_BIN does not resolve to an executable: $requested"
        printf '%s\n' "$requested"
        return
    fi

    for candidate in \
        "$repo_root/.venv/bin/scons" \
        "$repo_root/.venv/Scripts/scons.exe" \
        "$(command -v scons 2>/dev/null || true)"; do
        if [[ -n "$candidate" && -x "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return
        fi
    done

    fail "SCons was not found. Run scripts/setup_python_build_env.sh."
}

emcc_version() {
    emcc --version 2>/dev/null | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1
}

require_pinned_emscripten() {
    require_command emcc
    require_command em++
    require_command emar
    local actual
    actual="$(emcc_version)"
    [[ -n "$actual" ]] || fail "Could not determine the active Emscripten version."
    if [[ "$actual" != "$EMSCRIPTEN_VERSION" ]] && [[ "${ALLOW_EMSCRIPTEN_VERSION_MISMATCH:-0}" != "1" ]]; then
        fail "Emscripten $EMSCRIPTEN_VERSION is required, but $actual is active. Run scripts/setup_web_toolchain.sh or set ALLOW_EMSCRIPTEN_VERSION_MISMATCH=1 for an intentional experiment."
    fi
    note "Using Emscripten $actual"
}

is_git_checkout_root() {
    local destination="$1"

    [[ -d "$destination" ]] &&
        git -C "$destination" rev-parse --is-inside-work-tree >/dev/null 2>&1 &&
        [[ -z "$(git -C "$destination" rev-parse --show-prefix 2>/dev/null)" ]]
}

ensure_git_checkout() {
    local name="$1"
    local repository="$2"
    local ref="$3"
    local label="$4"
    local destination="$5"

    require_command git

    if [[ -e "$destination" ]] && ! is_git_checkout_root "$destination"; then
        if [[ -n "$(find "$destination" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
            fail "$name destination exists and is not a Git checkout: $destination"
        fi
        rm -rf "$destination"
    fi

    if ! is_git_checkout_root "$destination"; then
        note "Cloning $name ($label)"
        git clone --filter=blob:none --no-checkout "$repository" "$destination"
    fi

    if ! git -C "$destination" cat-file -e "${ref}^{commit}" 2>/dev/null; then
        note "Fetching pinned $name revision $ref"
        if ! git -C "$destination" fetch --depth 1 origin "$ref"; then
            git -C "$destination" fetch --depth 1 origin "$label"
        fi
    fi

    local resolved
    resolved="$(git -C "$destination" rev-parse "${ref}^{commit}" 2>/dev/null || true)"
    [[ "$resolved" == "$ref" ]] || fail "$name did not resolve to the pinned revision $ref. Resolved: ${resolved:-<none>}"

    git -C "$destination" checkout --detach --force "$ref"
    git -C "$destination" submodule update --init --recursive

    local actual
    actual="$(git -C "$destination" rev-parse HEAD)"
    [[ "$actual" == "$ref" ]] || fail "$name checkout mismatch: expected $ref, got $actual"
    note "$name is pinned at $actual"
}
