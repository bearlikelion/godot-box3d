"""Unified type conversion functions for Box3D ↔ Godot bindings.

Centralizes all b3 type → Godot type mapping and C++ expression generation,
plus return-statement emission. Used by the binding generator, data class
generator, and docs generator.
"""

from classification import infer_clone_fn
from naming import packed_array_type, to_godot_class_name
from parser import Function, is_scalar_type, is_struct_type


def to_godot_type(b3_type: str, type_map: dict, qualified: bool = True, is_pointer: bool = False) -> str:
    """Convert a Box3D type to a Godot C++ type name.

    When qualified=True, prefixes godot:: for class-scope declarations.
    When qualified=False, uses bare names for implementation files.
    """
    t = b3_type.replace("const ", "").replace("*", "").strip()
    q = "godot::" if qualified else ""

    if t == "char" and is_pointer:
        return f"{q}String"

    for id_type, info in type_map.get("id_types", {}).items():
        if t == id_type:
            gt = info["godot"]
            if gt == "RID":
                return f"{q}RID"
            if gt.startswith("Packed"):
                return f"{q}{gt}"
            return gt

    for m_type, info in type_map.get("math_types", {}).items():
        if t == m_type:
            gt = info["godot"]
            if gt in ("Vector3", "Quaternion", "Transform3D", "AABB", "Basis", "Plane"):
                return f"{q}{gt}"
            return gt

    # Scalar pointer (e.g. uint8_t* bytes, int* indices) → Packed*Array.
    # char* is handled above as String. Non-array scalar pointers are rejected
    # by classification, so this only fires for array-shaped params/returns.
    if is_pointer:
        packed = packed_array_type(t)
        if packed:
            return f"{q}{packed}"

    if is_scalar_type(t):
        return t

    if t in type_map.get("enums", []):
        return t

    if is_struct_type(t):
        cls_name = to_godot_class_name(t)
        return f"{q}Ref<{cls_name}>"

    return t


def convert_to_godot(expr: str, b3_type: str, type_map: dict, is_pointer: bool = False) -> str:
    """Generate the C++ expression to convert a b3 value to Godot type."""
    t = b3_type.replace("const ", "").replace("*", "").strip()

    if t == "char" and is_pointer:
        return f"{expr} ? String({expr}) : String()"

    for m_type, info in type_map.get("math_types", {}).items():
        if t == m_type:
            fn = info.get("to_godot", "b3_to_godot")
            return f"{fn}({expr})"

    if t in type_map.get("enums", []):
        return expr

    for id_type, info in type_map.get("id_types", {}).items():
        if t == id_type:
            if "pack" in info:
                return f"(int){info['pack']}({expr})"
            return expr

    if is_struct_type(t):
        cls_name = to_godot_class_name(t)
        return f"_from_b3_to_ref<{t}, {cls_name}>({expr})"

    return expr


def convert_to_b3(expr: str, b3_type: str, type_map: dict, is_pointer: bool = False) -> str:
    """Generate the C++ expression to convert a Godot value to b3 type."""
    t = b3_type.replace("const ", "").replace("*", "").strip()

    if t == "char" and is_pointer:
        return f"{expr}.utf8().ptr()"

    for m_type, info in type_map.get("math_types", {}).items():
        if t == m_type:
            fn = info.get("to_b3", "godot_to_b3")
            return f"{fn}({expr})"

    if t in type_map.get("enums", []):
        return expr

    for id_type, info in type_map.get("id_types", {}).items():
        if t == id_type:
            if info["godot"] == "RID":
                if "unpack" in info:
                    return f"{info['unpack']}((uint64_t){expr})"
                return f"(b3Id){expr}"
            if "unpack" in info:
                return f"{info['unpack']}((uint64_t){expr})"
            return expr

    if is_struct_type(t):
        cls_name = to_godot_class_name(t)
        if is_pointer:
            # Check if it's a const pointer
            if "const" in b3_type:
                return f"_ref_ptr<{t}, {cls_name}>({expr})"
            else:
                # Non-const pointer: use const_cast for INOUT params
                return f"const_cast<{t}*>(_ref_ptr<{t}, {cls_name}>({expr}))"
        return f"_ref_to_b3<{t}, {cls_name}>({expr})"

    return expr


def godot_default_value(b3_type: str, type_map: dict, is_pointer: bool = False) -> str:
    """Generate a default value for a Godot type (for ERR_FAIL_NULL_V)."""
    t = b3_type.replace("const ", "").replace("*", "").strip()

    if t == "char" and is_pointer:
        return "String()"

    for id_type, info in type_map.get("id_types", {}).items():
        if t == id_type:
            gt = info["godot"]
            if gt == "RID":
                return "RID()"
            return "0"

    for m_type, info in type_map.get("math_types", {}).items():
        if t == m_type:
            return f"{info['godot']}()"

    if t in type_map.get("enums", []):
        return f"({t})0"

    # Pointer-returned scalar arrays → default empty Packed*Array.
    if is_pointer:
        packed = packed_array_type(t)
        if packed:
            return f"{packed}()"

    if is_struct_type(t):
        cls_name = to_godot_class_name(t)
        return f"Ref<{cls_name}>()"

    if is_scalar_type(t):
        if t == "bool":
            return "false"
        if t == "float":
            return "0.0f"
        return "0"

    return "0"


def array_element_godot_type(b3_type: str, type_map: dict, qualified: bool = True) -> str:
    """Convert array element type to Godot array return type."""
    q = "godot::" if qualified else ""
    t = b3_type.replace("const ", "").replace("*", "").strip()

    for m_type, info in type_map.get("math_types", {}).items():
        if t == m_type:
            gt = info["godot"]
            if gt == "Vector3":
                return f"{q}PackedVector3Array"
            return f"{q}TypedArray<{q}{gt}>"

    for id_type, info in type_map.get("id_types", {}).items():
        if t == id_type:
            if info["godot"] == "RID":
                return f"{q}TypedArray<{q}RID>"
            gt = info["godot"]
            if gt.startswith("Packed"):
                return f"{q}{gt}"
            return f"{q}PackedInt64Array"

    packed = packed_array_type(b3_type)
    if packed:
        return f"{q}{packed}"

    if is_struct_type(t):
        return f"{q}Array"

    return t


def emit_return_lines(lines: list, func: Function, type_map: dict, call_args: list,
                      blobs: set, data: dict, indent: str = "    ") -> None:
    """Append the C call and return conversion to ``lines``.

    Handles void, blob pointer returns (owning RefCounted wrap; const views are
    deep-cloned first), and ordinary value returns. Shared by the plain and
    array-aware stub generators so blob handling never diverges.
    """
    args_str = ", ".join(call_args)
    is_void = func.return_type == "void"
    is_ptr_return = "*" in func.return_type and "char" not in func.return_type

    if is_void:
        lines.append(f"{indent}{func.name}({args_str});")
        return

    if is_ptr_return:
        ret_clean = func.return_type.replace("const ", "").replace("*", "").strip()
        if ret_clean in blobs:
            ret_const = "const" in func.return_type
            cls_name = to_godot_class_name(ret_clean)
            if ret_const:
                clone_fn = infer_clone_fn(ret_clean, data)
                if clone_fn is None:
                    return
                raw_decl = f"const {ret_clean}* raw = {func.name}({args_str});"
                raw_expr = f"{clone_fn}(raw)"
            else:
                raw_decl = f"{ret_clean}* raw = {func.name}({args_str});"
                raw_expr = "raw"
            lines.append(f"{indent}Ref<{cls_name}> result;")
            lines.append(f"{indent}{raw_decl}")
            lines.append(f"{indent}if (raw) {{")
            lines.append(f"{indent}    result.instantiate();")
            lines.append(f"{indent}    result->take_ownership({raw_expr});")
            lines.append(f"{indent}}}")
            lines.append(f"{indent}return result;")
            return

    convert_expr = convert_to_godot(
        f"{func.name}({args_str})", func.return_type, type_map,
        is_pointer="*" in func.return_type,
    )
    lines.append(f"{indent}return {convert_expr};")



