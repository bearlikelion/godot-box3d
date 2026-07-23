#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

godot_bin="${GODOT_BIN:-godot}"
godot_metadata_dir="$repo_root/test_project/.godot"
jobs="$(cpu_jobs)"

require_command "$godot_bin"
require_dependency_trees
scons_bin="$(scons_command)"

case "$(uname -s)" in
    Darwin)
        platform=macos
        arch=universal
        ;;
    Linux)
        platform=linux
        case "$(uname -m)" in
            x86_64|amd64) arch=x86_64 ;;
            aarch64|arm64) arch=arm64 ;;
            *) fail "Unsupported Linux test architecture: $(uname -m)" ;;
        esac
        ;;
    *)
        fail "scripts/run_headless_tests.sh currently supports Linux and macOS hosts."
        ;;
esac

note "Building host template_debug extension for $platform / $arch"
"$scons_bin" -C "$repo_root" -j "$jobs" \
    platform="$platform" \
    target=template_debug \
    arch="$arch"

mkdir -p "$godot_metadata_dir"
printf '%s\n' 'res://addons/godot-box3d/godot-box3d.gdextension' > "$godot_metadata_dir/extension_list.cfg"

tests=(
    backend_activation_test.gd
    review_regression_test.gd
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
