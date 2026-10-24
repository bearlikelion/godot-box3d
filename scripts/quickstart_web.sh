#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

"$script_dir/setup_python_build_env.sh"
"$script_dir/bootstrap_dependencies.sh"
"$script_dir/setup_web_toolchain.sh"
"$script_dir/build_web.sh"
"$script_dir/build_web_templates.sh"

python3 "$script_dir/package_addon.py" \
    --mode bundle \
    --platform web \
    --web-templates-dir "$repo_root/dist/web-export-templates" \
    --output "$repo_root/dist/godot-box3d-web-release.zip"

printf '\nWeb support is ready.\n'
printf 'Bundle: %s\n' "$repo_root/dist/godot-box3d-web-release.zip"
printf 'Next: follow WEB_QUICKSTART.md to copy the addon and configure the custom templates.\n'
