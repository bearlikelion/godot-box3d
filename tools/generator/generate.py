#!/usr/bin/env python3
"""Box3D API binding generator.

Usage:
    python generate.py audit         # Check API coverage
    python generate.py bindings      # Generate C++ bindings
    python generate.py data_classes  # Generate data class types
    python generate.py docs          # Generate Godot class reference XML
    python generate.py               # Run all
"""

import argparse
import sys
from pathlib import Path

# Resolve paths relative to this script's location, not CWD.
# The generator is always at <project>/tools/generator/generate.py.
_SCRIPT_DIR = Path(__file__).resolve().parent
_PROJECT_ROOT = _SCRIPT_DIR.parent.parent


def main():
    parser = argparse.ArgumentParser(description="Box3D API binding generator")
    parser.add_argument(
        "action",
        nargs="?",
        default="all",
        choices=["all", "audit", "bindings", "data_classes", "docs"],
        help="Action to perform (default: all)",
    )
    parser.add_argument(
        "root", nargs="?", default=None, help="Project root directory (default: auto-detected)"
    )
    args = parser.parse_args()

    root = Path(args.root).resolve() if args.root else _PROJECT_ROOT
    sys.path.insert(0, str(root / "tools" / "generator"))

    if args.action in ("all", "data_classes"):
        from data_class_generator import generate_data_classes

        count, warnings = generate_data_classes(str(root))
        print(f"Generated {count} data classes in src/api/bindings/data_classes/")
        if warnings:
            print(f"  warnings: {len(warnings)}")

    if args.action in ("all", "audit"):
        from audit import audit, print_report

        results = audit(str(root))
        print_report(results)
        if (results["missing"] or results["handwritten_missing"]) and args.action == "audit":
            sys.exit(1)

    if args.action in ("all", "bindings"):
        from binding_generator import write_output
        from classification import collect_generatable
        from config import load_config

        type_map = load_config(str(root))
        domains = collect_generatable(str(root), type_map)
        total = sum(len(f) for f in domains.values())
        print(f"\nGenerating bindings for {total} functions across {len(domains)} domains:")
        for domain, funcs in sorted(domains.items()):
            print(f"  {domain}: {len(funcs)} functions")
        write_output(str(root), domains, type_map)
        print(f"Output written to src/api/bindings/")

    if args.action in ("all", "docs"):
        from docs import generate_docs

        generate_docs(str(root))


if __name__ == "__main__":
    main()
