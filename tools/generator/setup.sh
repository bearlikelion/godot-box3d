#!/bin/bash
# Bootstrap the generator virtual environment.
# Run from the repo root:  tools/generator/setup.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"

if [ ! -d "$VENV_DIR" ]; then
    echo "Creating venv at $VENV_DIR ..."
    python3 -m venv "$VENV_DIR"
fi

echo "Installing dependencies ..."
"$VENV_DIR/bin/pip" install -q -r "$SCRIPT_DIR/requirements.txt"

echo ""
echo "Done. Activate with:"
echo "  source $VENV_DIR/bin/activate"
