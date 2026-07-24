#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$repo_root/build}"
godot_bin="${GODOT_BIN:-godot}"
test_addon_bin="$repo_root/test_project/addons/godot-box3d/bin"
godot_metadata_dir="$repo_root/test_project/.godot"
jobs="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || printf '4')}"

export CMAKE_BUILD_PARALLEL_LEVEL="$jobs"
if [[ -z "${MAKEFLAGS:-}" ]]; then
	export MAKEFLAGS="-j$jobs"
fi

case "$(uname -s)" in
	Darwin)
		extension_filename="libgodot-box3d.dylib"
		;;
	*)
		extension_filename="libgodot-box3d.so"
		;;
esac
extension_library="$repo_root/bin/$extension_filename"

if [[ -f "$repo_root/.gitmodules" ]]; then
	git -C "$repo_root" submodule update --init --recursive
fi

cmake -S "$repo_root" -B "$build_dir"
cmake --build "$build_dir" --target godot-box3d --parallel "$jobs"

if [[ ! -f "$extension_library" ]]; then
	printf 'ERROR: Expected extension library was not built: %s\n' "$extension_library" >&2
	exit 1
fi

mkdir -p "$test_addon_bin" "$godot_metadata_dir"
ln -sf "$extension_library" "$test_addon_bin/$extension_filename"

# Running a script directly does not scan the project for GDExtensions. Register
# the extension explicitly so tests cannot silently use GodotPhysics3D.
printf '%s\n' 'res://addons/godot-box3d/godot-box3d.gdextension' > "$godot_metadata_dir/extension_list.cfg"

backend_test="backend_activation_test.gd"

printf '\n== %s ==\n' "$backend_test"
"$godot_bin" --headless --path "$repo_root/test_project" --script "res://$backend_test"

for test_path in "$repo_root"/test_project/*_test.gd; do
	test_script="${test_path##*/}"
	if [[ "$test_script" == "$backend_test" ]]; then
		continue
	fi
	printf '\n== %s ==\n' "$test_script"
	"$godot_bin" --headless --path "$repo_root/test_project" --script "res://$test_script"
done
