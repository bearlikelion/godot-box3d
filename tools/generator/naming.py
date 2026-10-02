"""Naming conventions — Box3D C names to Godot names and back.

Holds every rule for deriving Godot identifiers (method names, class names,
parameter names, enum constants, data class filenames) from their Box3D
counterparts.
"""

import re

# Canonical C scalar type → Godot Packed*Array mapping. Used for C-style array
# parameters, pointer-array fields, and fixed-size scalar struct arrays. Kept in
# naming.py because it is dependency-free (struct_model and type_conversions both
# import it without creating a cycle).
SCALAR_PACKED_ARRAYS = {
    "int8_t": "PackedByteArray",
    "uint8_t": "PackedByteArray",
    "int16_t": "PackedInt32Array",
    "uint16_t": "PackedInt32Array",
    "int": "PackedInt32Array",
    "int32_t": "PackedInt32Array",
    "uint32_t": "PackedInt32Array",
    "int64_t": "PackedInt64Array",
    "uint64_t": "PackedInt64Array",
    "size_t": "PackedInt64Array",
    "float": "PackedFloat32Array",
    "double": "PackedFloat64Array",
}

# Variant::TYPE constant for each Godot Packed*Array type.
PACKED_ARRAY_VARIANTS = {
    "PackedByteArray": "Variant::PACKED_BYTE_ARRAY",
    "PackedInt32Array": "Variant::PACKED_INT32_ARRAY",
    "PackedInt64Array": "Variant::PACKED_INT64_ARRAY",
    "PackedFloat32Array": "Variant::PACKED_FLOAT32_ARRAY",
    "PackedFloat64Array": "Variant::PACKED_FLOAT64_ARRAY",
    "PackedVector2Array": "Variant::PACKED_VECTOR2_ARRAY",
    "PackedVector3Array": "Variant::PACKED_VECTOR3_ARRAY",
    "PackedVector4Array": "Variant::PACKED_VECTOR4_ARRAY",
    "PackedColorArray": "Variant::PACKED_COLOR_ARRAY",
}


# Godot types that are reference-counted wrappers and should be passed as
# `const T &` in generated parameter lists (matching godot-cpp conventions).
# POD value types (int, float, bool, Vector3, Transform3D, enums, ...) are
# passed by value and never listed here.
_CONST_REF_TYPES = {
    "Array",
    "Callable",
    "Dictionary",
    "NodePath",
    "RID",
    "Signal",
    "String",
    "StringName",
    "Variant",
}


def is_const_ref_type(godot_type: str) -> bool:
    """Whether a Godot type should be passed as `const T &` in a parameter list.

    Handles godot:: qualification, Ref<T> / TypedArray<T> templates, and
    Packed*Array containers. Value types return False.
    """
    t = godot_type
    if t.startswith("godot::"):
        t = t[len("godot::"):]
    if t.startswith("Ref<") or t.startswith("TypedArray<") or t.startswith("Packed"):
        return True
    return t in _CONST_REF_TYPES


def godot_param_type(godot_type: str) -> str:
    """Format a Godot type for a parameter declaration.

    Reference-counted types become `const T &`; value types stay bare.
    """
    if is_const_ref_type(godot_type):
        return f"const {godot_type} &"
    return godot_type


def packed_array_type(scalar_type: str) -> str | None:
    """Return the Godot Packed*Array type for a C scalar element type, or None.

    Accepts decorated type strings ("const uint8_t*") and strips decoration.
    """
    t = scalar_type.replace("const ", "").replace("*", "").strip()
    return SCALAR_PACKED_ARRAYS.get(t)


def packed_array_variant(packed_type: str) -> str:
    """Return the Variant::TYPE constant for a Godot Packed*Array type."""
    return PACKED_ARRAY_VARIANTS.get(packed_type, "Variant::NIL")


def to_snake_case(name: str) -> str:
    """Convert camelCase or PascalCase to snake_case."""
    result = re.sub(r"([a-z])([A-Z])", r"\1_\2", name)
    result = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", result)
    return result.lower()


def to_godot_name(b3_name: str) -> str:
    """Convert b3Name_Action to snake_case (strips b3 prefix)."""
    name = b3_name
    if name.startswith("b3"):
        name = name[2:]
    return to_snake_case(name)


def to_godot_class_name(b3_name: str) -> str:
    """Convert b3StructName to Box3DStructName."""
    name = b3_name
    if name.startswith("b3"):
        name = name[2:]
    return f"Box3D{name}"


def to_godot_param_name(b3_name: str, idx: int) -> str:
    """Convert a b3 parameter name to a Godot parameter name."""
    return f"p_{to_snake_case(b3_name)}"


def data_class_filename(struct_name: str) -> str:
    """Convert a struct name to the filename stem used by both docs and code gen.

    Strips b3 prefix then converts to snake_case.
    e.g. "b3Transformation" -> "transformation"
    """
    name = struct_name[2:] if struct_name.startswith("b3") else struct_name
    return to_snake_case(name)


def enum_value_to_godot_name(val_name: str) -> str:
    """Convert b3_enumValue to SCREAMING_SNAKE_CASE (strip b3_ prefix)."""
    name = val_name
    if name.startswith("b3_"):
        name = name[3:]
    return to_snake_case(name).upper()


def enum_value_to_hint_name(val_name: str) -> str:
    """Convert b3_enumValue to snake_case (strip b3_ prefix) for PROPERTY_HINT_ENUM."""
    name = val_name
    if name.startswith("b3_"):
        name = name[3:]
    return to_snake_case(name)


def build_enum_info(parsed_enums, skip_enum_names: set) -> tuple[list, dict]:
    """Build enum name list and constant mapping from parsed enums.

    Returns (enum_names, enum_constants) where:
    - enum_names: list of str (enum type names)
    - enum_constants: {enum_name: [(godot_const_name, b3_val_name), ...]}
      e.g. {b3BodyType: [("STATIC_BODY", "b3_staticBody"), ...]}

    Skips sentinel values ending in 'Count'.
    """
    enum_names = []
    enum_constants = {}

    for e in parsed_enums:
        if e.name in skip_enum_names:
            continue
        enum_names.append(e.name)
        constants = []
        for v in e.values:
            # Skip sentinel values (e.g., b3_bodyTypeCount)
            if v.name.endswith("Count"):
                continue
            godot_name = enum_value_to_godot_name(v.name)
            constants.append((godot_name, v.name))
        enum_constants[e.name] = constants

    return enum_names, enum_constants