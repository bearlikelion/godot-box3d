"""Audit Box3D API coverage — what's bound, what's skipped, what's missing.

Beyond the bound/skipped/missing tally the audit now also verifies:

- **Hand-written implementations**: functions listed in ``handwritten_functions``
  (config.yaml) must have a matching declaration in ``src/api/*.hpp``. Missing
  ones are reported as ``handwritten_missing`` and count toward the failure exit
  code — this is what surfaces the missing creation/callback/event layer.
- **Config drift**: every entry in the config lists is checked against the parsed
  headers, so an upstream rename (e.g. ``b3CreateHeightfieldShape`` →
  ``b3CreateHeightFieldShape``) can never silently stop matching again.
- **Unused types**: generated data classes and bound enums that no bound or
  hand-written function (transitively) references — the checklist for scope
  trimming.
"""

import argparse
import re
import sys
from pathlib import Path

from classification import SKIP, blob_structs, classify_function, prepare_type_map
from config import load_config
from parser import ArrayDirection, parse_headers
from struct_model import collect_convertible_fields


def load_existing_bindings(root: str) -> set:
    """Find existing binding stubs by scanning src/ for method declarations."""
    bound = set()
    src = Path(root) / "src"

    for f in src.rglob("*.hpp"):
        text = f.read_text()
        # Match: static ReturnType methodName(
        for m in re.finditer(r'static\s+\w+.*?\b(\w+)\s*\(', text):
            name = m.group(1)
            if name in ("_bind_methods", "Box3DAPI"):
                continue
            bound.add(name)

    return bound


def load_handwritten_bindings(root: str) -> set:
    """Find hand-written API method declarations in src/api/ (excludes .gen files)."""
    bound = set()
    api = Path(root) / "src" / "api"
    if not api.is_dir():
        return bound

    for f in api.glob("*.hpp"):
        if ".gen." in f.name:
            continue
        for m in re.finditer(r'static\s+\w+.*?\b(\w+)\s*\(', f.read_text()):
            name = m.group(1)
            if name in ("_bind_methods", "Box3DAPI"):
                continue
            bound.add(name)

    return bound


def _clean(t: str) -> str:
    """Strip const/pointer decoration from a C type string."""
    return t.replace("const ", "").replace("*", "").strip()


def _referenced_types(func) -> set:
    """Base types referenced by a function (params + return)."""
    types = {_clean(func.return_type)}
    for p in func.params:
        types.add(_clean(p.type))
    return types


def audit(root: str) -> dict:
    """Run the audit and return results.

    Classification is delegated to :func:`classification.classify_function`
    so the audit can never disagree with the binding generator about what is
    generatable and what is skipped.
    """
    data = parse_headers(root)
    type_map = load_config(root)
    prepare_type_map(type_map, data)
    bound = load_existing_bindings(root)
    handwritten_declared = load_handwritten_bindings(root)

    function_names = {f.name for f in data["functions"]}
    struct_names = {s.name for s in data["structs"]}
    enum_names = {e.name for e in data["enums"]}

    domains = {}
    missing = []
    skipped = []  # (name, reason, configured, handwritten)
    implicit = []  # (name, reason) — skipped for structural reasons w/o config
    array_functions = []
    handwritten_missing = []  # (b3_name, godot_name)
    verdicts = {}

    for func in data["functions"]:
        verdict = classify_function(func, data, type_map)
        verdicts[func.name] = verdict
        domain = verdict.domain
        domains.setdefault(domain, {"total": 0, "bound": 0, "skipped": 0, "missing": 0})
        domains[domain]["total"] += 1

        if verdict.decision == SKIP:
            domains[domain]["skipped"] += 1
            skipped.append((func.name, verdict.reason, verdict.configured, verdict.handwritten))
            if verdict.handwritten:
                godot_name = verdict.godot_name
                if godot_name not in handwritten_declared and func.name not in handwritten_declared:
                    handwritten_missing.append((func.name, godot_name))
            elif not verdict.configured:
                implicit.append((func.name, verdict.reason))
        else:
            godot_name = verdict.godot_name
            if godot_name in bound or func.name in bound:
                domains[domain]["bound"] += 1
            else:
                domains[domain]["missing"] += 1
                missing.append((func.name, godot_name, domain))

        if verdict.array_params:
            array_functions.append((func, verdict.array_params))

    stale_config = _detect_stale_config(
        type_map, data, function_names, struct_names, enum_names
    )

    unused_structs, unused_enums, unused_skip_refs = _detect_unused_types(
        type_map, data, verdicts
    )

    return {
        "domains": domains,
        "missing": missing,
        "skipped": skipped,
        "implicit": implicit,
        "array_functions": array_functions,
        "handwritten_missing": handwritten_missing,
        "stale_config": stale_config,
        "unused_structs": unused_structs,
        "unused_enums": unused_enums,
        "unused_skip_refs": unused_skip_refs,
        "total_functions": len(data["functions"]),
        "total_structs": len(data["structs"]),
        "total_enums": len(data["enums"]),
    }


def _detect_stale_config(
    type_map: dict,
    data: dict,
    function_names: set,
    struct_names: set,
    enum_names: set,
) -> list:
    """Return [(section, entry, detail)] for config entries that match nothing.

    Catches upstream renames: a ``skip_functions``/``handwritten_functions``/
    ``array_params`` entry whose function no longer exists, a ``skip_structs``/
    ``skip_enums`` entry whose type no longer exists, or a ``skip_types`` entry
    that appears neither as a parsed type nor anywhere in the C headers.

    Opaque pointer typedefs (e.g. ``b3Recording``, ``b3TaskCallback``) are not
    parsed as structs but DO appear in the headers, so they are not flagged.
    """
    stale = []

    for section, entries in (
        ("skip_functions", set(type_map.get("skip_functions", []))),
        ("handwritten_functions", set(type_map.get("handwritten_functions", []))),
        ("array_params", set(type_map.get("array_params", {}).keys())),
    ):
        for entry in sorted(entries):
            if entry not in function_names:
                stale.append((section, entry, "no matching b3 function in headers"))

    # Type sections: fall back to raw header text so opaque typedefs are allowed.
    raw_headers = _raw_header_text(data)

    for section, entries, names in (
        ("skip_structs", set(type_map.get("skip_structs", [])), struct_names),
        ("skip_enums", set(type_map.get("skip_enums", [])), enum_names),
        ("skip_types", set(type_map.get("skip_types", [])), struct_names | enum_names),
    ):
        for entry in sorted(entries):
            if entry in names:
                continue
            if re.search(rf"\b{re.escape(entry)}\b", raw_headers):
                continue
            stale.append((section, entry, "no matching type in headers"))

    return stale


def _raw_header_text(data: dict) -> str:
    """Concatenated C header source as parsed by the parser (for word search)."""
    return data.get("header_source", "")


def _detect_unused_types(type_map: dict, data: dict, verdicts: dict):
    """Find generated data classes / bound enums with no live API reference.

    "Live" = referenced by a bound (GENERATE) function or by a hand-written
    function (``handwritten_functions``), plus everything reachable transitively
    through generated struct fields. Structs/enums only referenced by policy
    skips are reported as unused, along with the skipped functions that
    reference them (context for deciding whether to model or blacklist).
    """
    blobs = blob_structs(data)
    skip_structs = set(type_map.get("skip_structs", []))
    math_types = set(type_map.get("math_types", {}).keys())
    id_types = set(type_map.get("id_types", {}).keys())
    enums = set(type_map.get("enums", []))
    struct_names = {s.name for s in data["structs"]}
    eff_skip = skip_structs | blobs

    def is_generated_struct(name: str) -> bool:
        if name in eff_skip or name in math_types or name in id_types or name in enums:
            return False
        for s in data["structs"]:
            if s.name == name:
                return bool(collect_convertible_fields(s, type_map, struct_names, skip_structs))
        return False

    generated_structs = {s.name for s in data["structs"] if is_generated_struct(s.name)}
    generated_structs |= blobs

    # Types referenced by bound or hand-written functions.
    referenced = set()
    for func in data["functions"]:
        verdict = verdicts[func.name]
        if verdict.decision == SKIP and not verdict.handwritten:
            continue
        referenced |= _referenced_types(func)

    # Transitive closure through generated struct fields (child views).
    changed = True
    while changed:
        changed = False
        for s in data["structs"]:
            if s.name in generated_structs and s.name in referenced:
                for f in s.fields:
                    t = _clean(f.type)
                    if t in generated_structs and t not in referenced:
                        referenced.add(t)
                        changed = True

    # Enums referenced by bound/hand-written functions or by referenced structs.
    referenced_enums = set()
    for s in data["structs"]:
        if s.name in generated_structs and s.name in referenced:
            for f in s.fields:
                t = _clean(f.type)
                if t in enums:
                    referenced_enums.add(t)
    for func in data["functions"]:
        verdict = verdicts[func.name]
        if verdict.decision == SKIP and not verdict.handwritten:
            continue
        for t in _referenced_types(func):
            if t in enums:
                referenced_enums.add(t)

    unused_structs = sorted(generated_structs - referenced)
    unused_enums = sorted(enums - referenced_enums)

    # Context: which skipped functions reference each unused struct.
    skip_refs = {}
    for func in data["functions"]:
        verdict = verdicts[func.name]
        if verdict.decision != SKIP:
            continue
        for t in _referenced_types(func):
            if t in unused_structs:
                skip_refs.setdefault(t, []).append(func.name)

    return unused_structs, unused_enums, skip_refs


def print_report(results: dict):
    domains = results["domains"]
    total_covered = sum(d["bound"] for d in domains.values())
    total_skipped = sum(d["skipped"] for d in domains.values())
    total_missing = sum(d["missing"] for d in domains.values())
    handwritten_missing = results["handwritten_missing"]

    print(f"\nBox3D API Audit")
    print(f"{'=' * 50}")
    print(f"Functions: {results['total_functions']} total, {results['total_structs']} structs, {results['total_enums']} enums")
    print(f"Bound: {total_covered} | Skipped: {total_skipped} | Missing: {total_missing}")
    print()

    # Domain breakdown
    print("Domain Coverage")
    print("-" * 40)
    for domain in sorted(domains.keys()):
        d = domains[domain]
        if d["total"] == 0:
            continue
        bar = "█" * d["bound"] + "░" * (d["total"] - d["bound"])
        print(f"  {domain:12s} {d['bound']:3d}/{d['total']:3d}  {bar}")

    # Hand-written implementations
    if results["handwritten_missing"]:
        print(f"\nHand-written missing ({len(results['handwritten_missing'])} configured in "
              f"config.yaml but no src/api/ declaration):")
        for b3_name, godot_name in sorted(results["handwritten_missing"]):
            print(f"    {b3_name:45s} → {godot_name}")
    else:
        print(f"\nHand-written: all {len([s for s in results['skipped'] if s[3]])} configured "
              f"implementations present in src/api/")

    # Stale config
    if results["stale_config"]:
        print(f"\nStale config ({len(results['stale_config'])} entries match nothing in headers):")
        for section, entry, detail in sorted(results["stale_config"]):
            print(f"    [{section}] {entry:40s} {detail}")

    # Unused types (scope-trim candidates)
    if results["unused_structs"] or results["unused_enums"]:
        print(f"\nUnused types (not referenced by any bound/hand-written function):")
        for t in results["unused_structs"]:
            refs = results["unused_skip_refs"].get(t, [])
            ref_note = f"  (referenced only by skipped: {', '.join(sorted(refs))})" if refs else ""
            print(f"    struct {t:40s}{ref_note}")
        for e in results["unused_enums"]:
            print(f"    enum   {e}")
        print("    → candidates for skip_structs/skip_enums or for modeling; see REVIEW_HANDOFF.md §2")

    # Array params
    if results["array_functions"]:
        print(f"\nArray Params ({len(results['array_functions'])} functions):")
        for func, detected in results["array_functions"]:
            for param_name, info in detected.items():
                dir_str = info.direction.name.lower()
                count_fn = info.count_function if info.count_function else "MISSING"
                # Warn only for plain OUTPUT arrays with no way to size the buffer:
                # no count function, no count param, and not an output-struct.
                needs_attention = (
                    info.direction == ArrayDirection.OUTPUT
                    and not info.count_function
                    and not info.count_param
                    and not info.output_struct
                )
                marker = " ⚠" if needs_attention else ""
                print(f"  {func.name}: {param_name} → {dir_str} array[{info.config_source}], count_fn={count_fn}{marker}")

    # Missing functions
    if results["missing"]:
        print(f"\nMissing ({len(results['missing'])} functions not bound, not skipped):")
        by_domain = {}
        for b3_name, godot_name, domain in results["missing"]:
            by_domain.setdefault(domain, []).append((b3_name, godot_name))

        for domain in sorted(by_domain.keys()):
            funcs = by_domain[domain]
            print(f"  [{domain}]")
            for b3_name, godot_name in sorted(funcs, key=lambda x: x[1]):
                print(f"    {b3_name:45s} → {godot_name}")

    # Skipped
    if results["skipped"]:
        print(f"\nSkipped ({len(results['skipped'])} functions):")
        for name, reason, configured, handwritten in sorted(results["skipped"]):
            marker = " [IMPLICIT]" if not configured else (" [HANDWRITTEN]" if handwritten else "")
            print(f"    {name:45s} {reason}{marker}")

    # Implicit skips — structural skips without an explicit config entry.
    # These are surfaced so a human can decide whether to support or configure.
    if results["implicit"]:
        print(f"\nImplicit Skips ({len(results['implicit'])} — no config entry, needs attention):")
        for name, reason in sorted(results["implicit"]):
            print(f"    {name:45s} {reason}")

    print()


def main():
    parser = argparse.ArgumentParser(description="Audit Box3D API coverage")
    parser.add_argument("root", nargs="?", default=".", help="Project root directory")
    args = parser.parse_args()

    results = audit(args.root)
    print_report(results)

    # Exit with error if there are missing functions or hand-written functions
    # that are configured but have no implementation.
    if results["missing"] or results["handwritten_missing"]:
        sys.exit(1)


if __name__ == "__main__":
    main()