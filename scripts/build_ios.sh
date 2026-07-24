#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

[[ "$(uname -s)" == "Darwin" ]] || fail "iOS builds require macOS with Xcode installed."
require_command xcrun
require_command xcodebuild
xcrun --sdk iphoneos --show-sdk-path >/dev/null
xcrun --sdk iphonesimulator --show-sdk-path >/dev/null
require_dependency_trees
scons_bin="$(scons_command)"

jobs="$(cpu_jobs)"
ios_bin_dir="$repo_root/bin/ios"
test_ios_bin_dir="$repo_root/test_project/addons/godot-box3d/bin/ios"
mkdir -p "$ios_bin_dir" "$test_ios_bin_dir"

for target in template_debug template_release; do
    note "Building iOS $target / arm64 device"
    "$scons_bin" -C "$repo_root" -j "$jobs" \
        platform=ios \
        target="$target" \
        arch=arm64 \
        ios_simulator=no \
        ios_min_version="$IOS_MIN_VERSION" \
        "$@"

    note "Building iOS $target / universal simulator"
    "$scons_bin" -C "$repo_root" -j "$jobs" \
        platform=ios \
        target="$target" \
        arch=universal \
        ios_simulator=yes \
        ios_min_version="$IOS_MIN_VERSION" \
        "$@"

    device_library="$ios_bin_dir/libgodot-box3d.ios.${target}.arm64.dylib"
    simulator_library="$ios_bin_dir/libgodot-box3d.ios.${target}.simulator.dylib"
    xcframework="$ios_bin_dir/libgodot-box3d.ios.${target}.xcframework"
    test_xcframework="$test_ios_bin_dir/libgodot-box3d.ios.${target}.xcframework"

    [[ -s "$device_library" ]] || fail "Missing iOS device library: $device_library"
    [[ -s "$simulator_library" ]] || fail "Missing iOS simulator library: $simulator_library"

    rm -rf "$xcframework" "$test_xcframework"
    note "Packaging iOS $target XCFramework"
    xcodebuild -create-xcframework \
        -library "$device_library" \
        -library "$simulator_library" \
        -output "$xcframework"
    cp -R "$xcframework" "$test_xcframework"
done

python3 "$repo_root/scripts/verify_port.py" --platform ios --require-binaries
note "iOS XCFrameworks are under $ios_bin_dir"
