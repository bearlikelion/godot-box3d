#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$repo_root/build}"
godot_bin="${GODOT_BIN:-godot}"
extension_library="$repo_root/bin/libgodot-box3d.so"
test_addon_bin="$repo_root/test_project/addons/godot-box3d/bin"
jobs="${JOBS:-$(nproc 2>/dev/null || printf '4')}"

export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$jobs}"
if [[ -z "${MAKEFLAGS:-}" ]]; then
	export MAKEFLAGS="-j$jobs"
fi

if [[ -f "$repo_root/.gitmodules" ]]; then
	git -C "$repo_root" submodule update --init --recursive
fi

cmake -S "$repo_root" -B "$build_dir"
cmake --build "$build_dir" --target godot-box3d --parallel "$jobs"

mkdir -p "$test_addon_bin"
ln -sf "$extension_library" "$test_addon_bin/libgodot-box3d.so"

tests=(
	physics_contract_test.gd
	ray_pickability_test.gd
	fall_test.gd
	settle_test.gd
	area_test.gd
	joint_test.gd
)

for test_script in "${tests[@]}"; do
	printf '\n== %s ==\n' "$test_script"
	"$godot_bin" --headless --path "$repo_root/test_project" --script "res://$test_script"
done
