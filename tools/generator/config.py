"""Generator configuration loading.

Reads the generator's single config file (config.yaml) from the project root.
The returned dict is referred to as the "type map" throughout the generator —
it maps Box3D types to Godot types and carries the policy lists (skip/
handwritten/array overrides).
"""

from pathlib import Path

import yaml


def load_config(root: str) -> dict:
    """Load config.yaml from the project root."""
    path = Path(root) / "tools" / "generator" / "config.yaml"
    if not path.exists():
        return {}
    return yaml.safe_load(path.read_text()) or {}