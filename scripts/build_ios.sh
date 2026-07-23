#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

[[ "$(uname -s)" == "Darwin" ]] || fail "iOS builds require macOS with Xcode installed."
require_command xcrun
xcrun --sdk iphoneos --show-sdk-path >/dev/null
require_dependency_trees
scons_bin="$(scons_command)"

jobs="$(cpu_jobs)"

for target in template_debug template_release; do
    note "Building iOS $target / arm64"
    "$scons_bin" -C "$repo_root" -j "$jobs" \
        platform=ios \
        target="$target" \
        arch=arm64 \
        ios_min_version="$IOS_MIN_VERSION" \
        "$@"
done

python3 "$repo_root/scripts/verify_port.py" --platform ios --require-binaries
note "iOS binaries are under $repo_root/bin/ios"
