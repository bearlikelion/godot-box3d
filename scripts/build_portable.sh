#!/usr/bin/env bash
set -euo pipefail

# This orchestrator builds every target available on the current host. Android
# and Web work on Linux/macOS; iOS is included automatically on macOS.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"$script_dir/bootstrap_dependencies.sh"
"$script_dir/build_android.sh"
"$script_dir/build_web.sh"
"$script_dir/build_web_templates.sh"

if [[ "$(uname -s)" == "Darwin" ]]; then
    "$script_dir/build_ios.sh"
else
    printf '==> iOS was skipped because this host is not macOS. Use CI or run scripts/build_ios.sh on a Mac.\n'
fi
