"""Single source of truth for whether a Box3D function can be auto-generated.

Both the binding generator and the audit use :func:`classify_function` so they
can never disagree about what is generatable, what is deliberately skipped,
and why.

Skipping is only allowed when backed by an explicit entry in ``config.yaml``
(``skip_functions``, ``skip_types``, ``skip_structs``). Anything skipped for a
structural reason that has no config entry is an *implicit* skip and is
reported by the audit so a human can decide whether to support or configure it.
"""

from dataclasses import dataclass, field

from array_detection import apply_array_overrides, detect_array_params
from naming import build_enum_info, to_godot_name
from parser import classify_domain, is_string_type, is_struct_type, parse_headers
from struct_model import collect_convertible_fields

GENERATE = "generate"
SKIP = "skip"


def prepare_type_map(type_map: dict, data: dict) -> dict:
    """Populate derived type_map entries needed by classification.

    Enum info (``enums``, ``enum_constants``) is derived from the parsed
    headers. Both the binding generator and the audit must call this so
    enum-typed functions classify identically.
    """
    if "enums" not in type_map or not type_map["enums"]:
        skip_enum_names = set(type_map.get("skip_enums", []))
        type_map["enums"], type_map["enum_constants"] = build_enum_info(
            data.get("enums", []), skip_enum_names
        )
    return type_map


def blob_structs(data: dict) -> set:
    """Infer heap-blob structs (data hangs off the end) from parsed headers.

    Heuristic: a struct with a ``byteCount`` field and at least one ``*Offset``
    field is stored as a single allocation with arrays indexed by byte offset
    (b3HullData, b3MeshData, b3HeightFieldData, b3CompoundData). Such structs
    cannot be copied by value and are owned by a RefCounted wrapper instead.
    """
    blobs = set()
    for s in data.get("structs", []):
        names = {f.name for f in s.fields}
        if "byteCount" in names and any(n.endswith("Offset") for n in names):
            blobs.add(s.name)
    return blobs


def infer_clone_fn(blob_type: str, data: dict) -> str | None:
    """Find a deep-clone function for a blob type by naming inference.

    b3HullData → b3CloneHull (strip ``b3``/``Data``, look for ``b3Clone*``).
    Returns None when no clone function exists in the API.
    """
    base = blob_type[2:]  # strip "b3"
    if base.endswith("Data"):
        base = base[: -len("Data")]
    candidate = f"b3Clone{base}"
    func_names = {f.name for f in data["functions"]}
    return candidate if candidate in func_names else None


def infer_destroy_fn(blob_type: str, data: dict) -> str | None:
    """Find the free function for a blob type by naming inference.

    b3HullData → b3DestroyHull. Returns None when no destroy function exists.
    """
    base = blob_type[2:]  # strip "b3"
    if base.endswith("Data"):
        base = base[: -len("Data")]
    candidate = f"b3Destroy{base}"
    func_names = {f.name for f in data["functions"]}
    return candidate if candidate in func_names else None


@dataclass
class FuncVerdict:
    func: object
    domain: str
    godot_name: str
    decision: str
    reason: str = ""
    configured: bool = True
    handwritten: bool = False
    array_params: dict = field(default_factory=dict)


def _referenced_struct_types(func) -> set:
    """Base struct types referenced by a function (params and return)."""
    types = set()
    for p in func.params:
        types.add(p.type.replace("const ", "").replace("*", "").strip())
    types.add(func.return_type.replace("const ", "").replace("*", "").strip())
    return types


def _data_class_generated(t: str, data: dict, type_map: dict, skip_structs: set) -> bool:
    """Check whether the data class generator will emit a class for struct t."""
    if t in type_map.get("math_types", {}):
        return True
    if t in type_map.get("id_types", {}):
        return True
    if t in type_map.get("enums", []):
        return True
    for s in data.get("structs", []):
        if s.name == t:
            all_names = {x.name for x in data.get("structs", [])}
            return bool(collect_convertible_fields(s, type_map, all_names, skip_structs))
    return False


def classify_function(func, data: dict, type_map: dict) -> FuncVerdict:
    """Classify a single function into generate/skip.

    Also attaches detected ``array_info`` to the function's params so the
    array-aware stub generators can read it directly.
    """
    domain = classify_domain(func.name, rules=type_map.get("domain_rules"))
    godot_name = to_godot_name(func.name)
    skip_functions = set(type_map.get("skip_functions", []))
    handwritten_functions = set(type_map.get("handwritten_functions", []))
    skip_types = set(type_map.get("skip_types", []))
    skip_structs = set(type_map.get("skip_structs", []))
    blobs = blob_structs(data)
    array_overrides = type_map.get("array_params", {})

    detected = detect_array_params(func, data["functions"], skip_types)
    if func.name in array_overrides:
        detected = apply_array_overrides(detected, array_overrides[func.name])
    for p in func.params:
        p.array_info = detected.get(p.name)

    def _skip(reason: str, configured: bool = True, handwritten: bool = False) -> FuncVerdict:
        return FuncVerdict(func, domain, godot_name, SKIP, reason=reason,
                           configured=configured, handwritten=handwritten,
                           array_params=detected)

    # Explicit function skip — the only place "never want in API" lives.
    if func.name in skip_functions:
        return _skip(f"explicit skip_functions entry")

    # Hand-written implementation — deliberately not auto-generated, but a
    # human implementation in src/api/ is expected (audit verifies it exists).
    if func.name in handwritten_functions:
        return _skip(f"handwritten implementation (configured)", handwritten=True)

    # Parameters.
    for p in func.params:
        if p.type in skip_types:
            return _skip(f"param {p.name} type {p.type} in skip_types")
        if p.pointer and not is_string_type(p):
            t_clean = p.type.replace("const ", "").replace("*", "").strip()
            if t_clean in skip_structs:
                return _skip(f"param {p.name} {p.type} (struct {t_clean} in skip_structs)")
            if p.name not in detected and not is_struct_type(t_clean):
                return _skip(f"param {p.name} {p.type} is an unbindable pointer",
                             configured=False)

    # Pointer returns: blob types are generated (owning or clone); others skip.
    is_ptr_return = "*" in func.return_type and "char" not in func.return_type
    ret_clean = func.return_type.replace("const ", "").replace("*", "").strip()
    if is_ptr_return:
        if ret_clean in blobs:
            is_const = "const" in func.return_type
            if is_const and infer_clone_fn(ret_clean, data) is None:
                return _skip(
                    f"const blob pointer return {func.return_type} has no clone function",
                    configured=False,
                )
        elif ret_clean in skip_structs:
            return _skip(f"pointer return {func.return_type} (struct in skip_structs)")
        else:
            return _skip(f"unconfigured pointer return {func.return_type}",
                         configured=False)

    # Struct returns.
    if ret_clean in skip_structs:
        return _skip(f"return type {ret_clean} in skip_structs")

    # Every referenced struct must have a Godot representation the generators
    # can actually emit, otherwise the stub would reference a class that
    # doesn't exist.
    for t in _referenced_struct_types(func):
        if t in blobs:
            continue
        if not is_struct_type(t):
            continue
        if t in skip_structs:
            continue  # already rejected above if it appeared
        if t in type_map.get("math_types", {}):
            continue
        if t in type_map.get("id_types", {}):
            continue
        if t in type_map.get("enums", []):
            continue
        if not _data_class_generated(t, data, type_map, skip_structs):
            return _skip(f"struct {t} has no generatable Godot representation",
                         configured=False)

    return FuncVerdict(func, domain, godot_name, GENERATE, array_params=detected)


def collect_generatable(root: str, type_map: dict) -> dict:
    """Parse headers and collect all generatable functions, grouped by domain.

    Uses :func:`classify_function` as the single source of truth. Domain is
    purely a grouping key for output files — no function is ever dropped for
    landing in the "other" bucket.
    """
    data = parse_headers(root)
    prepare_type_map(type_map, data)

    domains = {}
    for func in data["functions"]:
        verdict = classify_function(func, data, type_map)
        if verdict.decision != GENERATE:
            continue
        domains.setdefault(verdict.domain, []).append(func)

    return domains
