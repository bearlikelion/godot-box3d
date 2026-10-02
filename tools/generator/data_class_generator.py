"""Generate C++ GDExtension data classes for Box3D structs.

For each struct marked as 'data_class' in config.yaml, generates a RefCounted
subclass with typed properties, to_b3()/from_b3() conversion methods, and
ClassDB registration via _bind_methods().
"""

import shutil
from pathlib import Path

from classification import blob_structs, infer_clone_fn, infer_destroy_fn, prepare_type_map
from config import load_config
from naming import data_class_filename, godot_param_type, packed_array_type, to_godot_class_name, to_snake_case
from parser import (
    Struct,
    parse_headers,
)
from struct_model import (
    FieldClassification,
    classify_field,
    collect_convertible_fields,
    compute_struct_dependencies,
    is_pair_struct_name,
    pair_count_field,
    pair_elem_b3_type,
    pair_is_math,
    pair_packed_type,
    pair_paths,
    pair_storage_name,
    resolve_field_type,
    topological_sort,
)
from type_conversions import (
    convert_to_b3,
    convert_to_godot,
    to_godot_type,
)


def generate_helper_functions() -> str:
    """Generate shared helper functions for data class conversions."""
    return """\
// Helper: create a Ref<T> from a b3 struct using from_b3()
template <typename B3T, typename GodotClass>
static Ref<GodotClass> _from_b3_to_ref(const B3T& p_b3) {
    Ref<GodotClass> ref;
    ref.instantiate();
    ref->from_b3(p_b3);
    return ref;
}

// Helper: extract b3 struct from a Ref<T> using to_b3()
template <typename B3T, typename GodotClass>
static B3T _ref_to_b3(const Ref<GodotClass>& p_ref) {
    B3T result;
    if (p_ref.is_valid()) {
        result = p_ref->to_b3();
    }
    return result;
}

// Helper: get raw b3 struct pointer from a Ref<T> using ptr()
template <typename B3T, typename GodotClass>
static const B3T* _ref_ptr(const Ref<GodotClass>& p_ref) {
    return p_ref.is_valid() ? p_ref->ptr() : nullptr;
}

// Helper: build PackedFloat32Array or PackedInt32Array from C array
template <typename PackedArr, typename ElemType, size_t N>
static PackedArr _make_packed_array(const ElemType (&p_src)[N], PackedArr p_dst, size_t p_size) {
    p_dst.resize(p_size);
    for (size_t i = 0; i < p_size; i++) {
        p_dst.set(i, p_src[i]);
    }
    return p_dst;
}

// Helper: unpack Packed*Array into C array
template <typename PackedArr, typename ElemType, size_t N>
static void _unpack_array(const PackedArr& p_src, ElemType (&p_dst)[N], size_t p_size) {
    for (size_t i = 0; i < p_size && i < (size_t)p_src.size(); i++) {
        p_dst[i] = p_src[i];
    }
}
"""


def _is_child_struct(struct: Struct, type_map: dict, all_struct_names: set,
                     skip_structs: set) -> bool:
    """Check if this struct is used as a NESTED_STRUCT field in any other struct."""
    # A struct is a "child" if it could be classified as NESTED_STRUCT.
    # This means it's in all_struct_names, not skipped, not an enum, not a math/id type.
    if struct.name in skip_structs:
        return False
    if struct.name in type_map.get("enums", []):
        return False
    if struct.name in type_map.get("math_types", {}):
        return False
    if struct.name in type_map.get("id_types", {}):
        return False
    return True


PACKED_ARRAY_HEADERS = {
    "PackedByteArray": "packed_byte_array",
    "PackedInt32Array": "packed_int32_array",
    "PackedInt64Array": "packed_int64_array",
    "PackedFloat32Array": "packed_float32_array",
    "PackedFloat64Array": "packed_float64_array",
    "PackedVector2Array": "packed_vector2_array",
    "PackedVector3Array": "packed_vector3_array",
    "PackedVector4Array": "packed_vector4_array",
    "PackedColorArray": "packed_color_array",
}


def _packed_include(packed: str) -> str:
    """godot-cpp include line for a Packed*Array type."""
    header = PACKED_ARRAY_HEADERS.get(packed)
    if header is None:
        return ""
    return f"#include <godot_cpp/variant/{header}.hpp>"


def _pair_redirect_name(path: str) -> str:
    """Redirect helper name for a pair path (relative to the owning class)."""
    return "set_" + "_".join(to_snake_case(seg) for seg in path.split(".")) + "_storage"


def _count_path(path: str, count_field: str) -> str:
    """Replace the leaf pair field in a dot path with its count field name."""
    parts = path.split(".")
    parts[-1] = count_field
    return ".".join(parts)


def generate_header(struct: Struct, type_map: dict, all_struct_names: set,
                    skip_structs: set, structs: list) -> tuple[str, list[str]]:
    """Generate header file for a data class.

    Returns (header_content, warnings).
    """
    cls_name = to_godot_class_name(struct.name)
    warnings = []
    convertible_fields = collect_convertible_fields(struct, type_map, all_struct_names, skip_structs)

    for field in struct.fields:
        cls = classify_field(field, type_map, all_struct_names, skip_structs)
        if cls in (FieldClassification.POINTER, FieldClassification.SKIP_ARRAY, FieldClassification.UNKNOWN):
            warnings.append(f"  {struct.name}.{field.name}: skipping ({field.type})")

    if not convertible_fields:
        return "", warnings

    is_child = _is_child_struct(struct, type_map, all_struct_names, skip_structs)
    pairs = pair_paths(struct, type_map, structs) if structs else []

    lines = [
        "#pragma once",
        "",
        f"// Auto-generated by tools/generator/data_class_generator.py — do not edit.",
        "",
        '#include <godot_cpp/classes/ref_counted.hpp>',
        '#include <godot_cpp/variant/dictionary.hpp>',
        '#include <godot_cpp/variant/packed_int32_array.hpp>',
        '#include <godot_cpp/variant/packed_float32_array.hpp>',
        "",
        '#include "../../../misc/type_conversions.hpp"',
        "",
    ]

    # Forward declarations for nested structs
    nested_includes = set()
    for field, cls in convertible_fields:
        if cls == FieldClassification.NESTED_STRUCT:
            nested_includes.add(field.type)
        if cls == FieldClassification.NESTED_PAIR_STRUCT:
            nested_includes.add(field.type)

    # Include nested data class headers
    for nested_type in sorted(nested_includes):
        nested_cls = to_godot_class_name(nested_type)
        nested_file = to_snake_case(nested_type)
        if nested_type.startswith("b3"):
            nested_file = nested_file[2:]  # strip b3 prefix
        lines.append(f'#include "box3d_{nested_file}.gen.hpp"')

    # Pointer-array pair fields need the godot::Vector template and the Packed*
    # array header for their element type.
    if pairs:
        lines.append("#include <godot_cpp/templates/vector.hpp>")
        packed_set = set()
        for _path, leaf_field, _count in pairs:
            packed_set.add(pair_packed_type(leaf_field, type_map))
        for packed in sorted(packed_set):
            inc = _packed_include(packed)
            if inc:
                lines.append(inc)

    lines.append("")
    lines.append("using namespace godot;")
    lines.append("")

    T = "\t"
    # Class declaration
    lines.append(f"class {cls_name} : public godot::RefCounted {{")
    lines.append(f"{T}GDCLASS({cls_name}, RefCounted)")
    lines.append("")
    lines.append("public:")

    # Constructor + view setup for child structs (used as nested in parents)
    if is_child:
        lines.append(f"{T}{cls_name}();")
        lines.append(f"{T}~{cls_name}();")
        lines.append(
            f"{T}void set_as_view({struct.name}* p_data, RefCounted* p_parent);"
        )

    # Internal storage redirect for pointer-array pairs (not bound to ClassDB).
    # Parents redirect a nested pair struct's storage so the deepest owner's
    # b3_ pointers always stay valid while this instance is alive.
    for _path, leaf_field, _count in pairs:
        elem = pair_elem_b3_type(leaf_field)
        redirect = _pair_redirect_name(_path)
        lines.append(f"{T}void {redirect}(godot::Vector<{elem}>* p_storage);")

    # Getter/setter declarations
    for field, cls in convertible_fields:
        godot_type = resolve_field_type(field, cls, type_map)["cpp"]
        prop_name = to_snake_case(field.name)

        lines.append(f"{T}{godot_type} get_{prop_name}() const;")
        lines.append(f"{T}void set_{prop_name}({godot_param_type(godot_type)} p_value);")

    # to_b3 / from_b3 / ptr / copy
    lines.append(f"{T}{struct.name} to_b3() const;")
    lines.append(f"{T}void from_b3(const {struct.name}& p_data);")
    lines.append(f"{T}{struct.name}* ptr();")
    lines.append(f"{T}const {struct.name}* ptr() const;")
    lines.append(f"{T}Ref<{cls_name}> copy() const;")
    lines.append("")

    lines.append(f"{T}static void _bind_methods();")
    lines.append("")
    lines.append("private:")

    # Single embedded b3 struct member
    lines.append(f"{T}{struct.name} b3_;")

    # View infrastructure for child structs
    if is_child:
        lines.append(f"{T}mutable {struct.name}* _data_ptr;")
        lines.append(f"{T}Ref<RefCounted> _parent_ref;")

    # Deep storage for pointer-array pairs. The pointer member defaults to the
    # owned storage and may be redirected when this class is used as a nested
    # view, so pair data written through the view lands in the parent's storage.
    for _path, leaf_field, _count in pairs:
        elem = pair_elem_b3_type(leaf_field)
        storage = pair_storage_name(_path)
        lines.append(f"{T}godot::Vector<{elem}> {storage};")
        lines.append(f"{T}godot::Vector<{elem}>* {storage}_ptr;")

    lines.append("};")

    return "\n".join(lines) + "\n", warnings


def generate_implementation(struct: Struct, type_map: dict, all_struct_names: set,
                            skip_structs: set, snake: str = "", structs: list = None) -> str:
    """Generate implementation file for a data class."""
    cls_name = to_godot_class_name(struct.name)
    T = "\t"

    convertible_fields = collect_convertible_fields(struct, type_map, all_struct_names, skip_structs)
    if not convertible_fields:
        return ""

    is_child = _is_child_struct(struct, type_map, all_struct_names, skip_structs)
    pairs = pair_paths(struct, type_map, structs) if structs else []

    lines = [
        f'#include "box3d_{snake}.gen.hpp"',
        f'#include "data_class_helpers.gen.hpp"',
        '',
        "using namespace godot;",
        "",
    ]

    # --- Constructor + destructor + set_as_view for child structs ---
    if is_child:
        init_parts = ["_data_ptr(&b3_)"]
        init_parts += [f"{pair_storage_name(path)}_ptr(&{pair_storage_name(path)})" for path, _lf, _c in pairs]
        init_expr = ", ".join(init_parts)
        lines.append(f"{cls_name}::{cls_name}() : {init_expr} {{")
        for path, _leaf_field, count_field in pairs:
            lines.append(f"{T}_data_ptr->{path} = nullptr;")
            lines.append(f"{T}_data_ptr->{_count_path(path, count_field)} = 0;")
        lines.append("}")
        lines.append("")
        lines.append(f"{cls_name}::~{cls_name}() {{")
        lines.append("}")
        lines.append("")
        lines.append(f"void {cls_name}::set_as_view({struct.name}* p_data, RefCounted* p_parent) {{")
        lines.append(f"{T}_data_ptr = p_data;")
        lines.append(f"{T}_parent_ref = Ref<RefCounted>(p_parent);")
        lines.append("}")
        lines.append("")

    # --- Pair storage redirect helpers ---
    for _path, leaf_field, _count in pairs:
        elem = pair_elem_b3_type(leaf_field)
        storage = pair_storage_name(_path)
        redirect = _pair_redirect_name(_path)
        lines.append(f"void {cls_name}::{redirect}(godot::Vector<{elem}>* p_storage) {{")
        lines.append(f"{T}{storage}_ptr = p_storage;")
        lines.append("}")
        lines.append("")

    # --- Getters ---
    for field, cls in convertible_fields:
        prop_name = to_snake_case(field.name)

        if cls == FieldClassification.ARRAY:
            size = field.array_size
            packed = packed_array_type(field.type) or "PackedInt32Array"
            lines.append(f"{packed} {cls_name}::get_{prop_name}() const {{")
            lines.append(f"{T}return _make_packed_array(_data_ptr->{field.name}, {packed}(), (size_t){size});")
            lines.append("}")
            lines.append("")
        elif cls == FieldClassification.POINTER_ARRAY_PAIR:
            count_field = pair_count_field(struct.name, field.name, type_map) or "count"
            packed = pair_packed_type(field, type_map)
            conv = "b3_to_godot" if pair_is_math(field, type_map) else ""
            lines.append(f"{packed} {cls_name}::get_{prop_name}() const {{")
            lines.append(f"{T}{packed} result;")
            lines.append(f"{T}if (_data_ptr->{field.name} && _data_ptr->{count_field} > 0) {{")
            lines.append(f"{T}{T}result.resize(_data_ptr->{count_field});")
            lines.append(f"{T}{T}for (int i = 0; i < _data_ptr->{count_field}; i++) {{")
            if conv:
                lines.append(f"{T}{T}{T}result.set(i, {conv}(_data_ptr->{field.name}[i]));")
            else:
                lines.append(f"{T}{T}{T}result.set(i, _data_ptr->{field.name}[i]);")
            lines.append(f"{T}{T}}}")
            lines.append(f"{T}}}")
            lines.append(f"{T}return result;")
            lines.append("}")
            lines.append("")
        elif cls == FieldClassification.NESTED_PAIR_STRUCT:
            nested_cls = to_godot_class_name(field.type)
            godot_type = resolve_field_type(field, cls, type_map)["cpp"]
            lines.append(f"{godot_type} {cls_name}::get_{prop_name}() const {{")
            lines.append(f"{T}Ref<{nested_cls}> ref;")
            lines.append(f"{T}ref.instantiate();")
            lines.append(
                f"{T}ref->set_as_view("
                f"const_cast<{field.type}*>(&_data_ptr->{field.name}),"
                f" const_cast<{cls_name}*>(this));"
            )
            for path, _leaf_field, _count in pairs:
                if not path.startswith(field.name + "."):
                    continue
                child_path = path[len(field.name) + 1:]
                child_redirect = _pair_redirect_name(child_path)
                parent_ptr = f"{pair_storage_name(path)}_ptr"
                lines.append(f"{T}ref->{child_redirect}({parent_ptr});")
            lines.append(f"{T}return ref;")
            lines.append("}")
            lines.append("")
        elif cls == FieldClassification.NESTED_STRUCT:
            nested_cls = to_godot_class_name(field.type)
            godot_type = resolve_field_type(field, cls, type_map)["cpp"]
            lines.append(f"{godot_type} {cls_name}::get_{prop_name}() const {{")
            lines.append(f"{T}Ref<{nested_cls}> ref;")
            lines.append(f"{T}ref.instantiate();")
            lines.append(
                f"{T}ref->set_as_view("
                f"const_cast<{field.type}*>(&_data_ptr->{field.name}),"
                f" const_cast<{cls_name}*>(this));"
            )
            lines.append(f"{T}return ref;")
            lines.append("}")
            lines.append("")
        else:
            field_access = f"_data_ptr->{field.name}"
            conv = convert_to_godot(field_access, field.type, type_map)
            lines.append(f"{resolve_field_type(field, cls, type_map)['cpp']} {cls_name}::get_{prop_name}() const {{")
            lines.append(f"{T}return {conv};")
            lines.append("}")
            lines.append("")

    # --- Setters ---
    for field, cls in convertible_fields:
        prop_name = to_snake_case(field.name)
        godot_type = resolve_field_type(field, cls, type_map)["cpp"]
        param = f"p_{prop_name}"

        if cls == FieldClassification.ARRAY:
            size = field.array_size
            packed = packed_array_type(field.type) or "PackedInt32Array"
            lines.append(f"void {cls_name}::set_{prop_name}({godot_param_type(packed)} {param}) {{")
            lines.append(f"{T}_unpack_array({param}, _data_ptr->{field.name}, {size});")
            lines.append("}")
            lines.append("")
        elif cls == FieldClassification.POINTER_ARRAY_PAIR:
            count_field = pair_count_field(struct.name, field.name, type_map) or "count"
            packed = pair_packed_type(field, type_map)
            elem = pair_elem_b3_type(field)
            storage = pair_storage_name(field.name)
            conv = "godot_to_b3" if pair_is_math(field, type_map) else ""
            lines.append(f"void {cls_name}::set_{prop_name}({godot_param_type(packed)} {param}) {{")
            lines.append(f"{T}int n = {param}.size();")
            lines.append(f"{T}godot::Vector<{elem}> tmp;")
            lines.append(f"{T}tmp.resize(n);")
            lines.append(f"{T}for (int i = 0; i < n; i++) {{")
            if conv:
                lines.append(f"{T}{T}tmp.ptrw()[i] = {conv}({param}[i]);")
            else:
                lines.append(f"{T}{T}tmp.ptrw()[i] = {param}[i];")
            lines.append(f"{T}}}")
            lines.append(f"{T}*{storage}_ptr = tmp;")
            lines.append(f"{T}_data_ptr->{field.name} = {storage}_ptr->size() > 0 ? {storage}_ptr->ptrw() : nullptr;")
            lines.append(f"{T}_data_ptr->{count_field} = n;")
            lines.append("}")
            lines.append("")
        elif cls == FieldClassification.NESTED_PAIR_STRUCT:
            nested_cls = to_godot_class_name(field.type)
            lines.append(f"void {cls_name}::set_{prop_name}({godot_param_type(godot_type)} {param}) {{")
            lines.append(f"{T}if ({param}.is_valid()) {{")
            lines.append(f"{T}{T}const {field.type}* src = {param}->ptr();")
            field_pairs = [(path, leaf_field, count_field) for path, leaf_field, count_field in pairs if path.startswith(field.name + ".")]
            for _path, leaf_field, _count in field_pairs:
                child_leaf = _path.split(".")[-1]
                child_count = _count
                elem = pair_elem_b3_type(leaf_field)
                storage = f"{pair_storage_name(_path)}_ptr"
                lines.append(f"{T}{T}godot::Vector<{elem}> tmp;")
                lines.append(f"{T}{T}if (src->{child_leaf} && src->{child_count} > 0) {{")
                lines.append(f"{T}{T}{T}tmp.resize(src->{child_count});")
                lines.append(f"{T}{T}{T}for (int i = 0; i < src->{child_count}; i++) {{")
                lines.append(f"{T}{T}{T}{T}tmp.ptrw()[i] = src->{child_leaf}[i];")
                lines.append(f"{T}{T}{T}}}")
                lines.append(f"{T}{T}}}")
                lines.append(f"{T}{T}*{storage} = tmp;")
            lines.append(f"{T}{T}_data_ptr->{field.name} = *src;")
            for _path, leaf_field, _count in field_pairs:
                child_leaf = _path.split(".")[-1]
                child_count = _count
                storage = f"{pair_storage_name(_path)}_ptr"
                lines.append(f"{T}{T}_data_ptr->{field.name}.{child_leaf} = {storage}->size() > 0 ? {storage}->ptrw() : nullptr;")
                lines.append(f"{T}{T}_data_ptr->{field.name}.{child_count} = (int){storage}->size();")
            lines.append(f"{T}}}")
            lines.append("}")
            lines.append("")
        elif cls == FieldClassification.NESTED_STRUCT:
            nested_cls = to_godot_class_name(field.type)
            lines.append(f"void {cls_name}::set_{prop_name}({godot_param_type(godot_type)} {param}) {{")
            lines.append(f"{T}if ({param}.is_valid()) {{")
            lines.append(f"{T}{T}_data_ptr->{field.name} = {param}->to_b3();")
            lines.append(f"{T}}}")
            lines.append("}")
            lines.append("")
        else:
            conv = convert_to_b3(param, field.type, type_map)
            lines.append(f"void {cls_name}::set_{prop_name}({godot_param_type(godot_type)} {param}) {{")
            lines.append(f"{T}_data_ptr->{field.name} = {conv};")
            lines.append("}")
            lines.append("")

    # --- to_b3() ---
    lines.append(f"{struct.name} {cls_name}::to_b3() const {{")
    lines.append(f"{T}return *_data_ptr;")
    lines.append("}")
    lines.append("")

    # --- from_b3() ---
    lines.append(f"void {cls_name}::from_b3(const {struct.name}& p_data) {{")
    lines.append(f"{T}*_data_ptr = p_data;")
    for _path, _leaf_field, count_field in pairs:
        elem = pair_elem_b3_type(_leaf_field)
        storage = f"{pair_storage_name(_path)}_ptr"
        count_path = _count_path(_path, count_field)
        lines.append(f"{T}if (p_data.{_path} && p_data.{count_path} > 0) {{")
        lines.append(f"{T}{T}godot::Vector<{elem}> tmp;")
        lines.append(f"{T}{T}tmp.resize(p_data.{count_path});")
        lines.append(f"{T}{T}for (int i = 0; i < p_data.{count_path}; i++) {{")
        lines.append(f"{T}{T}{T}tmp.ptrw()[i] = p_data.{_path}[i];")
        lines.append(f"{T}{T}}}")
        lines.append(f"{T}{T}*{storage} = tmp;")
        lines.append(f"{T}}} else {{")
        lines.append(f"{T}{T}*{storage} = godot::Vector<{elem}>();")
        lines.append(f"{T}}}")
        lines.append(f"{T}_data_ptr->{_path} = {storage}->size() > 0 ? {storage}->ptrw() : nullptr;")
        lines.append(f"{T}_data_ptr->{count_path} = (int){storage}->size();")
    lines.append("}")
    lines.append("")

    # --- ptr() ---
    lines.append(f"{struct.name}* {cls_name}::ptr() {{")
    lines.append(f"{T}return _data_ptr;")
    lines.append("}")
    lines.append("")

    lines.append(f"const {struct.name}* {cls_name}::ptr() const {{")
    lines.append(f"{T}return _data_ptr;")
    lines.append("}")
    lines.append("")

    # --- copy() ---
    lines.append(f"Ref<{cls_name}> {cls_name}::copy() const {{")
    lines.append(f"{T}Ref<{cls_name}> c;")
    lines.append(f"{T}c.instantiate();")
    lines.append(f"{T}c->from_b3(to_b3());")
    lines.append(f"{T}return c;")
    lines.append("}")
    lines.append("")

    # --- _bind_methods() ---
    lines.append(f"void {cls_name}::_bind_methods() {{")

    # Bind copy method
    lines.append(f'{T}ClassDB::bind_method(D_METHOD("copy"), &{cls_name}::copy);')

    # Bind methods first (required before ADD_PROPERTY can reference them)
    for field, cls in convertible_fields:
        prop_name = to_snake_case(field.name)
        lines.append(
            f'{T}ClassDB::bind_method(D_METHOD("get_{prop_name}"), '
            f'&{cls_name}::get_{prop_name});'
        )
        lines.append(
            f'{T}ClassDB::bind_method(D_METHOD("set_{prop_name}", "value"), '
            f'&{cls_name}::set_{prop_name});'
        )

    lines.append("")

    # Add properties
    for field, cls in convertible_fields:
        prop_name = to_snake_case(field.name)
        getter = f"get_{prop_name}"
        setter = f"set_{prop_name}"
        variant_type = resolve_field_type(field, cls, type_map)["variant"]

        # For Ref<T> types, we need the class name string
        # godot-cpp ADD_PROPERTY signature: (property_info, setter, getter)
        if cls in (FieldClassification.NESTED_STRUCT, FieldClassification.NESTED_PAIR_STRUCT):
            nested_cls = to_godot_class_name(field.type)
            lines.append(
                f'{T}ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "{prop_name}", PROPERTY_HINT_NONE, '
                f'"{nested_cls}"), "{setter}", "{getter}");'
            )
        else:
            lines.append(
                f'{T}ADD_PROPERTY(PropertyInfo({variant_type}, "{prop_name}"), '
                f'"{setter}", "{getter}");'
            )

    lines.append("}")
    lines.append("")

    return "\n".join(lines) + "\n"


def _blob_field_type(field, cls, type_map: dict) -> str:
    """C++ type for a blob class field accessor."""
    if cls == FieldClassification.ARRAY:
        return packed_array_type(field.type) or "PackedInt32Array"
    if cls == FieldClassification.ENUM:
        return field.type
    return to_godot_type(field.type, type_map, qualified=False)


def generate_blob_header(struct: Struct, type_map: dict, all_struct_names: set,
                         blobs: set, data: dict) -> str:
    """Generate header for a heap-blob data class (owning RefCounted).

    Unlike embedded data classes, a blob cannot be copied by value, so there is
    no ``to_b3``/``from_b3`` and no embedded ``b3_`` member. Ownership is held
    by ``_data_ptr`` + ``_owns`` and released via the destroy function in the
    destructor. ``take_ownership`` is an internal bridge used by generated
    Box3DAPI stubs and is intentionally NOT bound to ClassDB.
    """
    T = "\t"
    cls_name = to_godot_class_name(struct.name)
    fields = collect_convertible_fields(struct, type_map, all_struct_names, blobs)

    lines = [
        "#pragma once",
        "",
        "// Auto-generated by tools/generator/data_class_generator.py — do not edit.",
        "",
        '#include <godot_cpp/classes/ref_counted.hpp>',
        '#include <godot_cpp/variant/dictionary.hpp>',
        '#include <godot_cpp/variant/packed_int32_array.hpp>',
        '#include <godot_cpp/variant/packed_float32_array.hpp>',
        "",
        '#include "../../../misc/type_conversions.hpp"',
        "",
        "using namespace godot;",
        "",
        f"class {cls_name} : public godot::RefCounted {{",
        f"{T}GDCLASS({cls_name}, RefCounted)",
        "",
        "public:",
        f"{T}{cls_name}();",
        f"{T}~{cls_name}();",
        f"{T}void take_ownership({struct.name}* p_data);",
        f"{T}void destroy();",
        f"{T}{struct.name}* ptr();",
        f"{T}const {struct.name}* ptr() const;",
    ]

    for field, cls in fields:
        cpp = _blob_field_type(field, cls, type_map)
        prop_name = to_snake_case(field.name)
        lines.append(f"{T}{cpp} get_{prop_name}() const;")
        lines.append(f"{T}void set_{prop_name}({godot_param_type(cpp)} p_value);")

    if infer_clone_fn(struct.name, data):
        lines.append(f"{T}Ref<{cls_name}> copy() const;")

    lines.append(f"{T}static void _bind_methods();")
    lines.append("")
    lines.append("private:")
    lines.append(f"{T}{struct.name}* _data_ptr;")
    lines.append(f"{T}bool _owns;")
    lines.append("};")

    return "\n".join(lines) + "\n"


def generate_blob_implementation(struct: Struct, type_map: dict, all_struct_names: set,
                                blobs: set, snake: str, data: dict) -> str:
    """Generate implementation for a heap-blob data class."""
    T = "\t"
    cls_name = to_godot_class_name(struct.name)
    destroy_fn = infer_destroy_fn(struct.name, data)
    clone_fn = infer_clone_fn(struct.name, data)
    fields = collect_convertible_fields(struct, type_map, all_struct_names, blobs)

    lines = [
        f'#include "box3d_{snake}.gen.hpp"',
        f'#include "data_class_helpers.gen.hpp"',
        "",
        "using namespace godot;",
        "",
        f"{cls_name}::{cls_name}() : _data_ptr(nullptr), _owns(false) {{",
        "",
        "}",
        "",
        f"{cls_name}::~{cls_name}() {{",
    ]
    if destroy_fn:
        lines.append(f"{T}if (_owns && _data_ptr) {{ {destroy_fn}(_data_ptr); }}")
    lines.append("}")
    lines.append("")
    lines.append(f"void {cls_name}::take_ownership({struct.name}* p_data) {{")
    lines.append(f"{T}_data_ptr = p_data;")
    lines.append(f"{T}_owns = true;")
    lines.append("}")
    lines.append("")
    if destroy_fn:
        lines.append(f"void {cls_name}::destroy() {{")
        lines.append(f"{T}if (_owns && _data_ptr) {{ {destroy_fn}(_data_ptr); }}")
        lines.append(f"{T}_owns = false;")
        lines.append(f"{T}_data_ptr = nullptr;")
        lines.append("}")
        lines.append("")
    lines.append(f"{struct.name}* {cls_name}::ptr() {{")
    lines.append(f"{T}return _data_ptr;")
    lines.append("}")
    lines.append("")
    lines.append(f"const {struct.name}* {cls_name}::ptr() const {{")
    lines.append(f"{T}return _data_ptr;")
    lines.append("}")
    lines.append("")

    for field, cls in fields:
        prop_name = to_snake_case(field.name)
        param = f"p_{prop_name}"

        if cls == FieldClassification.ARRAY:
            size = field.array_size
            packed = packed_array_type(field.type) or "PackedInt32Array"
            lines.append(f"{packed} {cls_name}::get_{prop_name}() const {{")
            lines.append(f"{T}ERR_FAIL_COND_V_MSG(_data_ptr == nullptr, {packed}(), \"Blob data is null. Blob data classes cannot be instantiated.\");")
            lines.append(f"{T}return _make_packed_array(_data_ptr->{field.name}, {packed}(), (size_t){size});")
            lines.append("}")
            lines.append("")
            lines.append(f"void {cls_name}::set_{prop_name}({godot_param_type(packed)} {param}) {{")
            lines.append(f"{T}ERR_FAIL_COND_MSG(_data_ptr == nullptr, \"Blob data is null. Blob data classes cannot be instantiated.\");")
            lines.append(f"{T}_unpack_array({param}, _data_ptr->{field.name}, {size});")
            lines.append("}")
            lines.append("")
        else:
            cpp = _blob_field_type(field, cls, type_map)
            conv = convert_to_godot(f"_data_ptr->{field.name}", field.type, type_map)
            lines.append(f"{cpp} {cls_name}::get_{prop_name}() const {{")
            lines.append(f"{T}ERR_FAIL_COND_V_MSG(_data_ptr == nullptr, {cpp}{{}}, \"Blob data is null. Blob data classes cannot be instantiated.\");")
            lines.append(f"{T}return {conv};")
            lines.append("}")
            lines.append("")
            cb = convert_to_b3(param, field.type, type_map)
            lines.append(f"void {cls_name}::set_{prop_name}({godot_param_type(cpp)} {param}) {{")
            lines.append(f"{T}ERR_FAIL_COND_MSG(_data_ptr == nullptr, \"Blob data is null. Blob data classes cannot be instantiated.\");")
            lines.append(f"{T}_data_ptr->{field.name} = {cb};")
            lines.append("}")
            lines.append("")

    if clone_fn:
        lines.append(f"Ref<{cls_name}> {cls_name}::copy() const {{")
        lines.append(f"{T}Ref<{cls_name}> c;")
        lines.append(f"{T}c.instantiate();")
        lines.append(f"{T}c->take_ownership({clone_fn}(_data_ptr));")
        lines.append(f"{T}return c;")
        lines.append("}")
        lines.append("")

    lines.append(f"void {cls_name}::_bind_methods() {{")
    if clone_fn:
        lines.append(f'{T}ClassDB::bind_method(D_METHOD("copy"), &{cls_name}::copy);')
    lines.append(f'{T}ClassDB::bind_method(D_METHOD("destroy"), &{cls_name}::destroy);')

    for field, cls in fields:
        prop_name = to_snake_case(field.name)
        lines.append(
            f'{T}ClassDB::bind_method(D_METHOD("get_{prop_name}"), '
            f'&{cls_name}::get_{prop_name});'
        )
        lines.append(
            f'{T}ClassDB::bind_method(D_METHOD("set_{prop_name}", "value"), '
            f'&{cls_name}::set_{prop_name});'
        )
    lines.append("")

    for field, cls in fields:
        prop_name = to_snake_case(field.name)
        variant_type = resolve_field_type(field, cls, type_map)["variant"]
        lines.append(
            f'{T}ADD_PROPERTY(PropertyInfo({variant_type}, "{prop_name}"), '
            f'"set_{prop_name}", "get_{prop_name}");'
        )
    
    # Set default values manually to avoid a flood of warnings/errors on extension initialization
    # TODO blocked by https://github.com/godotengine/godot-cpp/issues/1706
    # for field, cls in fields:
    #     prop_name = to_snake_case(field.name)
    #     lines.append(f"{T}ClassDB::set_property_default_value(\"{cls_name}\", \"{prop_name}\", {{}});")

    lines.append("}")
    lines.append("")
    return "\n".join(lines) + "\n"


def generate_data_classes(root: str) -> tuple[int, list[str]]:
    """Generate all data class files.

    Returns (class_count, warnings).
    """
    type_map = load_config(root)
    data = parse_headers(root)
    prepare_type_map(type_map, data)

    skip_structs = set(type_map.get("skip_structs", []))
    math_types = set(type_map.get("math_types", {}).keys())
    id_types = set(type_map.get("id_types", {}).keys())

    # Blob structs inferred by parser (byteCount + Offset fields).
    # These get separate RefCounted wrappers instead of embedded mode.
    blobs = set(blob_structs(data))
    eff_skip = skip_structs | blobs

    # Filter: all structs except skip_structs, blobs, math_types, id_types, enums
    candidates = [
        s
        for s in data["structs"]
        if s.name not in eff_skip
        and s.name not in math_types
        and s.name not in id_types
        and s.name not in type_map["enums"]
    ]

    all_struct_names = {s.name for s in data["structs"]}

    # Sort by dependency
    deps = compute_struct_dependencies(candidates)
    sorted_structs = topological_sort(candidates, deps)

    out_dir = Path(root) / "src" / "api" / "bindings" / "data_classes"
    # Sweep the whole directory so output always matches the current generator.
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    all_warnings = []
    generated = 0
    generated_files = []

    for struct in sorted_structs:
        snake = data_class_filename(struct.name)

        header_content, warns = generate_header(
            struct, type_map, all_struct_names, eff_skip, data["structs"]
        )
        all_warnings.extend(warns)

        if not header_content:
            continue

        impl_content = generate_implementation(
            struct, type_map, all_struct_names, eff_skip, snake, data["structs"]
        )

        # Write header
        header_path = out_dir / f"box3d_{snake}.gen.hpp"
        header_path.write_text(header_content)

        # Write implementation
        impl_path = out_dir / f"box3d_{snake}.gen.cpp"
        impl_path.write_text(impl_content)

        generated_files.append(snake)
        generated += 1

    # Generate blob classes (owning RefCounted wrappers for heap blobs)
    for struct in sorted(data["structs"], key=lambda s: s.name):
        if struct.name not in blobs:
            continue

        snake = data_class_filename(struct.name)
        header_content = generate_blob_header(
            struct, type_map, all_struct_names, eff_skip, data
        )
        impl_content = generate_blob_implementation(
            struct, type_map, all_struct_names, eff_skip, snake, data
        )

        out_dir.joinpath(f"box3d_{snake}.gen.hpp").write_text(header_content)
        out_dir.joinpath(f"box3d_{snake}.gen.cpp").write_text(impl_content)

        generated_files.append(snake)
        generated += 1

    # Generate helpers header
    helpers_path = out_dir / "data_class_helpers.gen.hpp"
    helpers_lines = [
        "#pragma once",
        "",
        "// Auto-generated shared helper functions for Box3D data class conversions.",
        "",
        "#include <godot_cpp/classes/ref_counted.hpp>",
        "#include <godot_cpp/core/class_db.hpp>",
        "#include <godot_cpp/variant/packed_int32_array.hpp>",
        "#include <godot_cpp/variant/packed_float32_array.hpp>",
        "",
        "using namespace godot;",
        "",
    ]

    # VARIANT_ENUM_CAST for each enum type (must be in helpers so all .cpp files see them)
    for enum_name in type_map.get("enums", []):
        helpers_lines.append(f"VARIANT_ENUM_CAST({enum_name});")

    helpers_lines.append("")
    helpers_lines.append(generate_helper_functions())
    helpers_lines.append("")

    helpers_path.write_text("\n".join(helpers_lines) + "\n")

    # Generate umbrella header
    umbrella = [
        "#pragma once",
        "",
        "// Auto-generated umbrella header for all Box3D data classes.",
        "// Includes all generated data class headers and shared helpers.",
        "",
        "#include <godot_cpp/classes/ref_counted.hpp>",
        "#include <godot_cpp/variant/packed_int32_array.hpp>",
        "#include <godot_cpp/variant/packed_float32_array.hpp>",
        "",
        '#include "../../../misc/type_conversions.hpp"',
        '',
        '#include "data_class_helpers.gen.hpp"',
        "",
    ]

    for snake in sorted(generated_files):
        umbrella.append(f'#include "box3d_{snake}.gen.hpp"')

    umbrella.append("")
    umbrella.append("using namespace godot;")

    # Add registration function declaration
    umbrella.append("")
    umbrella.append("// Register all data classes with ClassDB")
    umbrella.append("void register_data_classes();")

    umbrella_path = out_dir / "data_classes.gen.hpp"
    umbrella_path.write_text("\n".join(umbrella) + "\n")

    # Generate registration implementation
    T = "\t"
    reg_lines = [
        "#include \"data_classes.gen.hpp\"",
        "",
        "void register_data_classes() {",
    ]
    for struct in sorted_structs:
        cls_name = to_godot_class_name(struct.name)
        reg_lines.append(f'{T}ClassDB::register_class<{cls_name}>();')
    for struct in sorted(data["structs"], key=lambda s: s.name):
        if struct.name not in blobs:
            continue
        cls_name = to_godot_class_name(struct.name)
        reg_lines.append(f'{T}ClassDB::register_class<{cls_name}>();')

    reg_lines.append("}")
    reg_lines.append("")

    reg_path = out_dir / "data_classes_register.gen.cpp"
    reg_path.write_text("\n".join(reg_lines) + "\n")

    return generated, all_warnings


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Generate Box3D data classes")
    parser.add_argument("root", nargs="?", default=".", help="Project root directory")
    args = parser.parse_args()

    count, warnings = generate_data_classes(args.root)
    print(f"Generated {count} data classes in src/api/bindings/data_classes/")

    if warnings:
        print(f"\nWarnings ({len(warnings)}):")
        for w in warnings:
            print(w)


if __name__ == "__main__":
    main()
