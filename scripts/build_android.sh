#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

require_dependency_trees
scons_bin="$(scons_command)"

sdk_root="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
[[ -n "$sdk_root" ]] || fail "Set ANDROID_HOME or ANDROID_SDK_ROOT before building Android."
export ANDROID_HOME="$sdk_root"

ndk_root="$sdk_root/ndk/$ANDROID_NDK_VERSION"
[[ -d "$ndk_root" ]] || fail "Android NDK $ANDROID_NDK_VERSION is not installed under $sdk_root/ndk. Run scripts/setup_android_toolchain.sh."
export ANDROID_NDK_ROOT="$ndk_root"

jobs="$(cpu_jobs)"

for target in template_debug template_release; do
    for arch in arm64 x86_64; do
        note "Building Android $target / $arch"
        "$scons_bin" -C "$repo_root" -j "$jobs" \
            platform=android \
            target="$target" \
            arch="$arch" \
            android_api_level="$ANDROID_API_LEVEL" \
            "$@"
    done
done

python3 "$repo_root/scripts/verify_port.py" --platform android --require-binaries
note "Android binaries are under $repo_root/bin/android"
