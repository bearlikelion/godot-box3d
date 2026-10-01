#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=_common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_common.sh"

require_command python3

venv_dir="${VENV_DIR:-$repo_root/.venv}"
if [[ ! -x "$venv_dir/bin/python" ]] && [[ ! -x "$venv_dir/Scripts/python.exe" ]]; then
    note "Creating Python virtual environment at $venv_dir"
    python3 -m venv "$venv_dir"
fi

if [[ -x "$venv_dir/bin/python" ]]; then
    python_bin="$venv_dir/bin/python"
    scons_bin="$venv_dir/bin/scons"
else
    python_bin="$venv_dir/Scripts/python.exe"
    scons_bin="$venv_dir/Scripts/scons.exe"
fi

"$python_bin" -m pip install --upgrade pip
"$python_bin" -m pip install "scons==$SCONS_VERSION"
"$scons_bin" --version
note "Pinned SCons environment is ready. Build scripts will discover it automatically."
