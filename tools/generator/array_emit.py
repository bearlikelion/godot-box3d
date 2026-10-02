"""Array-parameter method emission for Box3D API bindings.

Handles C-style array parameters (Type* arr, int count) by generating
Godot-compatible wrappers that allocate buffers, call the C API, and
convert results to Godot array types. Array *detection* lives in
array_detection.py; this module turns the detected metadata into code.
"""

from naming import godot_param_type, to_godot_class_name, to_godot_name, to_godot_param_name
from parser import ArrayDirection, Function, is_struct_type
from type_conversions import (
    array_element_godot_type,
    convert_to_b3,
    emit_return_lines,
    godot_default_value,
    to_godot_type,
)


def _is_output_struct_param(param) -> bool:
    """Check if this param is an output struct with internal array."""
    return param.array_info is not None and param.array_info.output_struct is not None


def get_array_params(func: Function) -> list:
    """Get all array parameters for a function."""
    return [p for p in func.params if p.array_info is not None]


def get_skip_param_names(func: Function) -> set:
    """Get param names to exclude from Godot signature.

    OUTPUT array params and their count params are hidden (internal allocation).
    INPUT array params stay in the signature (caller provides them).
    Count params for INPUT are hidden (count comes from .size()).
    Output struct params are hidden (internal allocation, capacity added as param).
    Fixed-count arrays are hidden (exposed as separate params).
    """
    skip = set()
    for p in func.params:
        if p.array_info:
            if p.array_info.direction == ArrayDirection.OUTPUT:
                skip.add(p.name)
            if _is_output_struct_param(p):
                skip.add(p.name)
            if p.array_info.fixed_count is not None:
                skip.add(p.name)  # fixed-count arrays exposed as separate params
            if p.array_info.count_param:
                skip.add(p.array_info.count_param)
    return skip


def get_godot_params(func: Function, type_map: dict, qualified: bool = True) -> list:
    """Get (type, name) pairs for Godot signature, excluding OUTPUT array params and count params."""
    skip_names = get_skip_param_names(func)
    result = []
    has_output_struct = any(_is_output_struct_param(p) for p in func.params)
    for p in func.params:
        # Fixed-count arrays: expose as separate Vector3 params (check before skip_names)
        if p.array_info and p.array_info.fixed_count is not None:
            base_name = to_godot_param_name(p.name, 0).replace("_b", "")
            prefix = "godot::" if qualified else ""
            for i in range(p.array_info.fixed_count):
                result.append((f"{prefix}Vector3", f"{base_name}v{i+1}"))
            continue
        if p.name in skip_names:
            continue
        if p.pointer and not p.type == "char" and not p.array_info:
            t_clean = p.type.replace("const ", "").replace("*", "").strip()
            if not is_struct_type(t_clean):
                continue
        if p.array_info:
            ptype = array_element_godot_type(p.type, type_map, qualified=qualified)
        else:
            ptype = to_godot_type(p.type, type_map, qualified=qualified, is_pointer=p.pointer)
        pname = to_godot_param_name(p.name, 0)
        result.append((ptype, pname))
    # Output struct functions get capacity as the last param
    if has_output_struct:
        result.append(("int", "p_capacity"))
    return result


def get_array_return_type(func: Function, type_map: dict, qualified: bool = True) -> str | None:
    """Get the Godot return type for a function with OUTPUT array params."""
    array_params = get_array_params(func)
    if not array_params:
        return None

    # Output struct params return TypedArray<return_element>
    for p in array_params:
        if _is_output_struct_param(p):
            prefix = "godot::" if qualified else ""
            elem = p.array_info.output_struct["return_element"]
            return f"{prefix}TypedArray<{elem}>"

    output_params = [p for p in array_params if p.array_info.direction == ArrayDirection.OUTPUT]
    if output_params:
        elem_type = output_params[0].type
        return array_element_godot_type(elem_type, type_map, qualified=qualified)
    return None


def get_id_param(func: Function, type_map: dict) -> tuple | None:
    """Find the first ID-type parameter in a function's params.

    Returns (param_name, type_name, id_info) or None.
    """
    id_types = set(type_map.get("id_types", {}).keys())
    for p in func.params:
        t = p.type.replace("const ", "").strip()
        if t in id_types:
            return (p.name, t, type_map["id_types"][t])
    return None


def _b3_to_godot_array_convert(elem_b3_type: str, buf_var: str, index_var: str, type_map: dict) -> str:
    """Generate expression to convert a single buffer element to Godot type."""
    access = f"{buf_var}[{index_var}]"

    for id_type, info in type_map.get("id_types", {}).items():
        if elem_b3_type == id_type:
            if info["godot"] == "RID":
                return f"{info['pack']}({access})"
            return f"(int64_t){info['pack']}({access})"

    for m_type, info in type_map.get("math_types", {}).items():
        if elem_b3_type == m_type:
            fn = info.get("to_godot", "b3_to_godot")
            return f"{fn}({access})"

    if is_struct_type(elem_b3_type):
        cls_name = to_godot_class_name(elem_b3_type)
        return f"_from_b3_to_ref<{elem_b3_type}, {cls_name}>({access})"

    return access


def _generate_output_array_stub(func: Function, type_map: dict) -> str:
    """Generate stub for OUTPUT array function."""
    godot_name = to_godot_name(func.name)
    array_params = get_array_params(func)
    output_param = [p for p in array_params if p.array_info.direction == ArrayDirection.OUTPUT][0]
    elem_type = output_param.type
    array_godot_type = array_element_godot_type(elem_type, type_map, qualified=False)

    godot_params = get_godot_params(func, type_map, qualified=False)
    params_str = ", ".join(f"{godot_param_type(pt)} {pn}" for pt, pn in godot_params) if godot_params else ""

    id_info = get_id_param(func, type_map)

    lines = []

    if id_info:
        param_name, id_type_name, id_data = id_info
        is_rid = "impl" in id_data
        if is_rid:
            impl_type = id_data["impl"]
            owner = id_data["owner"]
            id_field = id_data["id_field"]
            rid_param = to_godot_param_name(param_name, 0)
            helper = f"api_get_{owner.replace('_owner', '')}"
            lines.append(f"    {impl_type}* obj = {helper}({rid_param});")
            lines.append(f"    ERR_FAIL_NULL_V(obj, {array_godot_type}());")
            rid_id_expr = f"obj->{id_field}"
        else:
            rid_id_expr = None
    else:
        rid_id_expr = None
        is_rid = False

    count_fn = output_param.array_info.count_function
    if count_fn:
        if rid_id_expr:
            count_args = rid_id_expr
        else:
            non_array_params = [p for p in func.params if p.array_info is None]
            if non_array_params:
                pname = to_godot_param_name(non_array_params[0].name, 0)
                ptype_clean = non_array_params[0].type.replace("const ", "").replace("*", "").strip()
                if ptype_clean in type_map.get("id_types", {}):
                    info = type_map["id_types"][ptype_clean]
                    if "unpack" in info:
                        count_args = f"{info['unpack']}({pname})"
                    else:
                        count_args = pname
                else:
                    count_args = pname
            else:
                count_args = ""
        lines.append(f"    int _count = {count_fn}({count_args});")
    else:
        lines.append(f"    int _count = 0;")

    buf_var = f"_{output_param.name.lower()}_buf"
    lines.append(f"    Vector<{elem_type}> {buf_var};")
    lines.append(f"    {buf_var}.resize(_count);")

    call_args = []
    for p in func.params:
        if p.name == output_param.name:
            call_args.append(f"{buf_var}.ptrw()")
        elif p.name == output_param.array_info.count_param:
            call_args.append("_count")
        elif p.array_info:
            continue
        elif p.name in get_skip_param_names(func):
            continue
        else:
            pname = to_godot_param_name(p.name, 0)
            ptype_clean = p.type.replace("const ", "").replace("*", "").strip()
            if is_rid and id_info and ptype_clean == id_info[1]:
                call_args.append(rid_id_expr)
            elif ptype_clean in type_map.get("id_types", {}):
                info = type_map["id_types"][ptype_clean]
                if "unpack" in info:
                    call_args.append(f"{info['unpack']}({pname})")
                else:
                    call_args.append(pname)
            else:
                call_args.append(convert_to_b3(pname, p.type, type_map, is_pointer=p.pointer))

    args_str = ", ".join(call_args)
    lines.append(f"    int _filled = {func.name}({args_str});")

    result_var = "result"
    lines.append(f"    {array_godot_type} {result_var};")
    lines.append(f"    {result_var}.resize(_filled);")

    is_packed = array_godot_type.startswith("Packed")

    convert_expr = _b3_to_godot_array_convert(elem_type, buf_var, "i", type_map)
    if is_packed:
        lines.append(f"    for (int i = 0; i < _filled; i++) {{")
        lines.append(f"        {result_var}.set(i, {convert_expr});")
        lines.append(f"    }}")
    else:
        lines.append(f"    for (int i = 0; i < _filled; i++) {{")
        lines.append(f"        {result_var}[i] = {convert_expr};")
        lines.append(f"    }}")

    lines.append(f"    return {result_var};")

    sig = f"{array_godot_type} Box3DAPI::{godot_name}({params_str})"
    return f"{sig} {{\n" + "\n".join(lines) + "\n}"


def _generate_input_array_stub(func: Function, type_map: dict, blobs: set = None, data: dict = None) -> str:
    """Generate stub for INPUT array function."""
    blobs = blobs or set()
    data = data or {}
    godot_name = to_godot_name(func.name)
    array_params = get_array_params(func)
    input_param = [p for p in array_params if p.array_info.direction == ArrayDirection.INPUT][0]
    elem_type = input_param.type

    ret_godot = to_godot_type(func.return_type, type_map, qualified=False, is_pointer="*" in func.return_type)
    is_void = func.return_type == "void"

    godot_params = get_godot_params(func, type_map, qualified=False)
    params_str = ", ".join(f"{godot_param_type(pt)} {pn}" for pt, pn in godot_params) if godot_params else ""

    id_info = get_id_param(func, type_map)

    lines = []

    rid_id_expr = None
    if id_info:
        param_name, id_type_name, id_data = id_info
        is_rid = "impl" in id_data
        if is_rid:
            impl_type = id_data["impl"]
            owner = id_data["owner"]
            id_field = id_data["id_field"]
            rid_param = to_godot_param_name(param_name, 0)
            helper = f"api_get_{owner.replace('_owner', '')}"
            lines.append(f"    {impl_type}* obj = {helper}({rid_param});")
            if is_void:
                lines.append(f"    ERR_FAIL_NULL(obj);")
            else:
                default = godot_default_value(func.return_type, type_map, is_pointer="*" in func.return_type)
                lines.append(f"    ERR_FAIL_NULL_V(obj, {default});")
            rid_id_expr = f"obj->{id_field}"

    arr_param = to_godot_param_name(input_param.name, 0)
    ptr_var = f"_{input_param.name.lower()}_ptr"
    count_var = f"_{input_param.name.lower()}_count"

    elem_is_math = elem_type in type_map.get("math_types", {})

    if elem_is_math:
        lines.append(f"    const {elem_type}* {ptr_var} = {arr_param}.size() > 0 ? (const {elem_type}*){arr_param}.ptr() : nullptr;")
        lines.append(f"    int {count_var} = (int){arr_param}.size();")
    else:
        buf_var = f"_{input_param.name.lower()}_buf"
        lines.append(f"    Vector<{elem_type}> {buf_var};")
        lines.append(f"    {buf_var}.resize({arr_param}.size());")
        if is_struct_type(elem_type):
            cls_name = to_godot_class_name(elem_type)
            lines.append(f"    for (int i = 0; i < {arr_param}.size(); i++) {{")
            lines.append(f"        {buf_var}.ptrw()[i] = _ref_to_b3<{elem_type}, {cls_name}>({arr_param}[i]);")
            lines.append(f"    }}")
        else:
            lines.append(f"    for (int i = 0; i < {arr_param}.size(); i++) {{")
            lines.append(f"        {buf_var}.ptrw()[i] = {arr_param}[i];")
            lines.append(f"    }}")
        lines.append(f"    const {elem_type}* {ptr_var} = {arr_param}.size() > 0 ? {buf_var}.ptr() : nullptr;")
        lines.append(f"    int {count_var} = (int){arr_param}.size();")

    call_args = []
    for p in func.params:
        if p.name == input_param.name:
            call_args.append(ptr_var)
        elif p.name == input_param.array_info.count_param:
            call_args.append(count_var)
        elif p.array_info:
            continue
        elif p.name in get_skip_param_names(func):
            continue
        else:
            pname = to_godot_param_name(p.name, 0)
            ptype_clean = p.type.replace("const ", "").replace("*", "").strip()
            if id_info and ptype_clean == id_info[1] and rid_id_expr:
                call_args.append(rid_id_expr)
            elif ptype_clean in type_map.get("id_types", {}):
                info = type_map["id_types"][ptype_clean]
                if "unpack" in info:
                    call_args.append(f"{info['unpack']}({pname})")
                else:
                    call_args.append(pname)
            else:
                call_args.append(convert_to_b3(pname, p.type, type_map, is_pointer=p.pointer))

    args_str = ", ".join(call_args)

    emit_return_lines(lines, func, type_map, call_args, blobs, data)

    sig = f"{ret_godot} Box3DAPI::{godot_name}({params_str})"
    return f"{sig} {{\n" + "\n".join(lines) + "\n}"


def _generate_output_struct_stub(func: Function, type_map: dict) -> str:
    """Generate stub for a function with an output struct that has an internal array.

    The config specifies the struct type, array field, count field, and return element.
    The wrapper allocates the struct with an internal buffer, calls the C function,
    and returns TypedArray<return_element>.
    """
    godot_name = to_godot_name(func.name)
    array_params = get_array_params(func)
    output_param = None
    for p in array_params:
        if _is_output_struct_param(p):
            output_param = p
            break

    if not output_param:
        return ""

    # Read config values
    cfg = output_param.array_info.output_struct
    struct_type = cfg["type"]
    array_field = cfg["array_field"]
    count_field = cfg["count_field"]
    return_element = cfg["return_element"]
    default_capacity = cfg.get("default_capacity", 4)

    # Find the capacity param
    cap_param_name = output_param.array_info.count_param

    # Use get_godot_params for consistent header/impl signatures
    godot_params = get_godot_params(func, type_map, qualified=False)
    params_str = ", ".join(f"{godot_param_type(pt)} {pn}" for pt, pn in godot_params)

    # Collect fixed-count array params for call arg generation
    fixed_arrays = [p for p in func.params if p.array_info and p.array_info.fixed_count is not None]

    ret_type = f"TypedArray<{return_element}>"

    # Buffer element type: from config, or derived from struct+array field naming
    buffer_elem_type = cfg.get(
        "buffer_element_type",
        f"b3{struct_type.replace('b3', '')}{array_field.rstrip('s').capitalize()}"
    )

    struct_var = f"_{output_param.name.lower()}"
    buf_var = f"_{output_param.name.lower()}_buf"

    lines = []
    lines.append(f"    int _capacity = p_capacity > 0 ? p_capacity : {default_capacity};")
    lines.append(f"    Vector<{buffer_elem_type}> {buf_var};")
    lines.append(f"    {buf_var}.resize(_capacity);")
    lines.append(f"    {struct_type} {struct_var};")
    lines.append(f"    {struct_var}.{array_field} = {buf_var}.ptrw();")
    lines.append(f"    {struct_var}.{count_field} = 0;")

    # Create temp arrays for fixed-count array params
    for p in fixed_arrays:
        base_name = to_godot_param_name(p.name, 0).replace("_b", "")
        arr_var = f"_{p.name}_arr"
        lines.append(f"    b3Vec3 {arr_var}[{p.array_info.fixed_count}] = {{")
        vec_conversions = []
        for i in range(p.array_info.fixed_count):
            vec_conversions.append(f"godot_to_b3({base_name}v{i+1})")
        lines.append(f"        {', '.join(vec_conversions)}")
        lines.append(f"    }};")

    # Build call args
    call_args = []
    for p in func.params:
        if p.name == output_param.name:
            call_args.append(f"&{struct_var}")
        elif p.name == cap_param_name:
            call_args.append("_capacity")
        elif p.array_info and p.array_info.fixed_count is not None:
            arr_var = f"_{p.name}_arr"
            call_args.append(arr_var)
        elif p.array_info:
            continue
        elif p.name in get_skip_param_names(func):
            continue
        else:
            pname = to_godot_param_name(p.name, 0)
            ptype_clean = p.type.replace("const ", "").replace("*", "").strip()
            if ptype_clean in type_map.get("id_types", {}):
                info = type_map["id_types"][ptype_clean]
                if "unpack" in info:
                    call_args.append(f"{info['unpack']}({pname})")
                else:
                    call_args.append(pname)
            else:
                call_args.append(convert_to_b3(pname, p.type, type_map, is_pointer=p.pointer))

    args_str = ", ".join(call_args)
    lines.append(f"    {func.name}({args_str});")

    lines.append(f"    TypedArray<{return_element}> result;")
    lines.append(f"    int _count = {struct_var}.{count_field} > 0 ? {struct_var}.{count_field} : 0;")
    lines.append(f"    for (int i = 0; i < _count; i++) {{")
    lines.append(f"        Ref<{return_element}> ref;")
    lines.append(f"        ref.instantiate();")
    lines.append(f"        ref->from_b3({buf_var}[i]);")
    lines.append(f"        result.push_back(ref);")
    lines.append(f"    }}")
    lines.append(f"    return result;")

    sig = f"{ret_type} Box3DAPI::{godot_name}({params_str})"
    return f"{sig} {{\n" + "\n".join(lines) + "\n}"


def generate_array_stub(func: Function, type_map: dict, blobs: set = None, data: dict = None) -> str:
    """Generate a C++ method stub for a Box3D function with array parameters.

    Returns the stub string, or empty string if no array params.
    """
    blobs = blobs or set()
    data = data or {}
    array_params = get_array_params(func)
    if not array_params:
        return ""

    # Check for output struct params first
    for p in array_params:
        if _is_output_struct_param(p):
            return _generate_output_struct_stub(func, type_map)

    directions = {p.array_info.direction for p in array_params}
    if ArrayDirection.OUTPUT in directions:
        return _generate_output_array_stub(func, type_map)
    if ArrayDirection.INPUT in directions:
        return _generate_input_array_stub(func, type_map, blobs, data)
    return ""
