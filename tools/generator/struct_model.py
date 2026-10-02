"""Data class field modeling for Box3D structs.

Decides how each struct field maps to Godot (scalar/enum/math/id/nested/array/
pointer/unknown) and resolves a classified field to concrete Godot types for the
C++ declarations, Variant bindings, and XML docs.
"""

from enum import Enum, auto

from naming import packed_array_type, to_snake_case


class FieldClassification(Enum):
    """Classification of a struct field for code generation purposes."""
    SCALAR = auto()
    ENUM = auto()
    MATH = auto()
    ID = auto()
    NESTED_STRUCT = auto()
    NESTED_PAIR_STRUCT = auto()
    ARRAY = auto()
    POINTER_ARRAY_PAIR = auto()
    SKIP_ARRAY = auto()
    POINTER = auto()
    UNKNOWN = auto()


def pointer_array_fields(type_map: dict) -> dict:
    """Struct name -> {field_name: {count_field: ...}} for C pointer+count pairs.

    From config.yaml ``pointer_array_fields``. A struct that owns one or more
    pointer fields with a sibling count field becomes a POINTER_ARRAY_PAIR data
    class holding its own deep storage for the pointed-to elements.
    """
    return type_map.get("pointer_array_fields", {})


def pair_count_field(struct_name: str, field_name: str, type_map: dict) -> str | None:
    """Count field name for a configured pointer-array pair field, or None."""
    cfg = pointer_array_fields(type_map).get(struct_name, {}).get(field_name, {})
    return cfg.get("count_field")


def pair_elem_b3_type(field) -> str:
    """Element type of a pointer-array pair field (strips const/pointer)."""
    return field.type.replace("const ", "").replace("*", "").strip()


def pair_is_math(field, type_map: dict) -> bool:
    """True when the pair's elements are a math type (b3Vec3 etc.)."""
    return pair_elem_b3_type(field) in type_map.get("math_types", {})


def pair_packed_type(field, type_map: dict) -> str:
    """Godot Packed*Array type for a pointer-array pair field."""
    elem = pair_elem_b3_type(field)
    if elem in type_map.get("math_types", {}):
        gt = type_map["math_types"][elem]["godot"]
        mapping = {"Vector3": "PackedVector3Array", "Quaternion": "PackedQuaternionArray"}
        return mapping.get(gt, "PackedVector3Array")
    return packed_array_type(elem) or "PackedInt32Array"


def pair_storage_name(path: str) -> str:
    """Storage member name for a pair path like "proxyA.points" -> "_proxy_a_points_storage".

    ``path`` is the dot-joined C field names from the owning struct down to the
    pair field. Also used to derive the redirect helper and pointer member.
    """
    return "_" + "_".join(to_snake_case(seg) for seg in path.split(".")) + "_storage"


def pair_paths(struct, type_map: dict, structs: list) -> list:
    """All pointer-array pair paths reachable from ``struct``.

    Returns ``(path, leaf_field, count_field)`` tuples, e.g.
    ``("proxyA.points", <points field>, "count")``. ``path`` is the dot-joined
    C field chain from ``struct`` down to the pair field, covering both direct
    pair fields and pairs nested inside NESTED_PAIR_STRUCT fields. A parent owns
    the deep storage for every pair it exposes, so ``structs`` (the parsed
    struct list) is needed to walk nested structs.
    """
    structs_by_name = {s.name: s for s in structs}
    cfg = pointer_array_fields(type_map).get(struct.name, {})
    result = []
    for field in struct.fields:
        if field.name in cfg:
            result.append((field.name, field, cfg[field.name].get("count_field")))
    for field in struct.fields:
        sub = structs_by_name.get(field.type)
        if sub is not None and is_pair_struct_name(field.type, type_map):
            for sub_path, sub_field, sub_count in pair_paths(sub, type_map, structs):
                result.append((f"{field.name}.{sub_path}", sub_field, sub_count))
    return result


def is_pair_struct_name(struct_name: str, type_map: dict) -> bool:
    """True when the struct owns at least one pointer-array pair field."""
    return bool(pointer_array_fields(type_map).get(struct_name))


def classify_field(
    field,
    type_map: dict,
    all_struct_names: set,
    skip_structs: set,
    pair_cfg: dict | None = None,
) -> FieldClassification:
    """Classify a struct field into a conversion category."""
    from parser import is_scalar_type

    t = field.type

    if field.pointer or "*" in t:
        pair_cfg = pair_cfg if pair_cfg is not None else {}
        if field.name in pair_cfg:
            return FieldClassification.POINTER_ARRAY_PAIR
        return FieldClassification.POINTER

    if field.array_size is not None:
        if is_scalar_type(t):
            return FieldClassification.ARRAY
        return FieldClassification.SKIP_ARRAY

    if is_scalar_type(t):
        return FieldClassification.SCALAR

    if t in type_map.get("enums", []):
        return FieldClassification.ENUM

    if t in type_map.get("math_types", {}):
        return FieldClassification.MATH

    if t in type_map.get("id_types", {}):
        return FieldClassification.ID

    if t in all_struct_names:
        if t in skip_structs:
            return FieldClassification.POINTER
        if is_pair_struct_name(t, type_map):
            return FieldClassification.NESTED_PAIR_STRUCT
        return FieldClassification.NESTED_STRUCT

    return FieldClassification.UNKNOWN


def collect_convertible_fields(
    struct,
    type_map: dict,
    all_struct_names: set,
    skip_structs: set,
) -> list:
    """Return list of (field, classification) tuples for fields that can be exposed.

    Filters out POINTER, SKIP_ARRAY, and UNKNOWN classifications. Count fields
    consumed by a pointer-array pair are dropped (their length is derived from
    the paired array).
    """
    pair_cfg = pointer_array_fields(type_map).get(struct.name, {})
    count_fields = {
        v.get("count_field") for v in pair_cfg.values() if v.get("count_field")
    }
    convertible = []
    for field in struct.fields:
        if field.name in count_fields:
            continue
        cls = classify_field(field, type_map, all_struct_names, skip_structs, pair_cfg)
        if cls not in (FieldClassification.POINTER, FieldClassification.SKIP_ARRAY, FieldClassification.UNKNOWN):
            convertible.append((field, cls))
    return convertible


def compute_struct_dependencies(structs: list) -> dict[str, set[str]]:
    """Compute dependency graph: struct -> set of structs it depends on."""
    struct_names = {s.name for s in structs}
    deps = {}
    for s in structs:
        deps[s.name] = set()
        for f in s.fields:
            if f.type in struct_names:
                deps[s.name].add(f.type)
    return deps


def topological_sort(structs: list, deps: dict[str, set[str]]) -> list:
    """Sort structs so dependencies come first."""
    struct_map = {s.name: s for s in structs}
    visited = set()
    result = []

    def visit(name: str):
        if name in visited:
            return
        visited.add(name)
        for dep in deps.get(name, set()):
            if dep in struct_map:
                visit(dep)
        if name in struct_map:
            result.append(struct_map[name])

    for s in structs:
        visit(s.name)

    return result


def resolve_field_type(field, classification: str, type_map: dict) -> dict:
    """Resolve a classified field to its type representations for all output formats.

    Returns a dict with keys:
      - xml:    Godot class reference XML type string (e.g. "RID", "PackedFloat32Array")
      - variant: Variant::TYPE constant for _bind_methods (e.g. "Variant::INT")
      - cpp:    C++ type for method declarations (e.g. "int64_t", "PackedFloat32Array")
    """
    from naming import packed_array_type, packed_array_variant, to_godot_class_name

    if classification == FieldClassification.SCALAR:
        t = field.type
        if t == "bool":
            return {"xml": "bool", "variant": "Variant::BOOL", "cpp": "bool"}
        if t == "float":
            return {"xml": "float", "variant": "Variant::FLOAT", "cpp": "float"}
        return {"xml": "int", "variant": "Variant::INT", "cpp": "int"}

    if classification == FieldClassification.ENUM:
        return {"xml": "int", "variant": "Variant::INT", "cpp": field.type}

    if classification == FieldClassification.MATH:
        info = type_map["math_types"][field.type]
        gt = info["godot"]
        variant_map = {
            "Vector3": "Variant::VECTOR3",
            "Quaternion": "Variant::QUATERNION",
            "Transform3D": "Variant::TRANSFORM3D",
            "AABB": "Variant::AABB",
            "Basis": "Variant::BASIS",
            "Plane": "Variant::PLANE",
        }
        return {
            "xml": gt,
            "variant": variant_map.get(gt, "Variant::NIL"),
            "cpp": gt,
        }

    if classification == FieldClassification.ID:
        info = type_map["id_types"][field.type]
        gt = info["godot"]
        if gt == "RID":
            return {"xml": "RID", "variant": "Variant::INT", "cpp": "int64_t"}
        if gt.startswith("Packed"):
            return {"xml": gt, "variant": "Variant::PACKED_INT32_ARRAY", "cpp": gt}
        return {"xml": "int", "variant": "Variant::INT", "cpp": "int"}

    if classification == FieldClassification.NESTED_STRUCT:
        cls_name = to_godot_class_name(field.type)
        return {
            "xml": cls_name,
            "variant": f'Variant::OBJECT, "{cls_name}"',
            "cpp": f"Ref<{cls_name}>",
        }

    if classification == FieldClassification.NESTED_PAIR_STRUCT:
        return resolve_field_type(field, FieldClassification.NESTED_STRUCT, type_map)

    if classification == FieldClassification.POINTER_ARRAY_PAIR:
        packed = pair_packed_type(field, type_map)
        return {
            "xml": packed,
            "variant": packed_array_variant(packed),
            "cpp": packed,
        }

    if classification == FieldClassification.ARRAY:
        packed = packed_array_type(field.type) or "PackedInt32Array"
        return {
            "xml": packed,
            "variant": packed_array_variant(packed),
            "cpp": packed,
        }

    return {"xml": "Variant", "variant": "Variant::NIL", "cpp": "Variant"}