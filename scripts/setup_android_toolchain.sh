#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

sdk_root="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
[[ -n "$sdk_root" ]] || fail "Set ANDROID_HOME or ANDROID_SDK_ROOT to your Android SDK directory."

sdkmanager=""
for candidate in \
    "$sdk_root/cmdline-tools/latest/bin/sdkmanager" \
    "$sdk_root/cmdline-tools/bin/sdkmanager" \
    "$sdk_root/tools/bin/sdkmanager"; do
    if [[ -x "$candidate" ]]; then
        sdkmanager="$candidate"
        break
    fi
done

if [[ -z "$sdkmanager" ]]; then
    sdkmanager="$(command -v sdkmanager || true)"
fi
[[ -n "$sdkmanager" ]] || fail "sdkmanager was not found. Install Android SDK Command-line Tools first."

note "Installing Android NDK $ANDROID_NDK_VERSION and platform android-$ANDROID_API_LEVEL"
yes | "$sdkmanager" --licenses >/dev/null || true
"$sdkmanager" \
    "platform-tools" \
    "platforms;android-${ANDROID_API_LEVEL}" \
    "ndk;${ANDROID_NDK_VERSION}"

ndk_dir="$sdk_root/ndk/$ANDROID_NDK_VERSION"
[[ -d "$ndk_dir" ]] || fail "Expected NDK directory was not created: $ndk_dir"
note "Android toolchain is ready at $ndk_dir"
