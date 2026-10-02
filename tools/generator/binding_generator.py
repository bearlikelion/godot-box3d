"""Generate C++ bindings for Box3D API functions.

Emit the static Box3DAPI method implementations, the generated declaration
header, and the ClassDB registration for every function classified as
generatable. Array-aware functions are delegated to array_emit.py.
"""

from pathlib import Path

from array_emit import (
    generate_array_stub,
    get_array_return_type,
    get_godot_params,
    get_id_param,
    get_skip_param_names,
)
from classification import blob_structs
from config import load_config
from naming import (
    godot_param_type,
    to_godot_name,
    to_godot_param_name,
    to_snake_case,
)
from parser import (
    Function,
    is_string_type,
    is_struct_type,
    parse_headers,
)
from type_conversions import (
    convert_to_b3,
    emit_return_lines,
    godot_default_value,
    to_godot_type,
)


def generate_stub(func: Function, type_map: dict, blobs: set, data: dict) -> str:
    """Generate a C++ method body for a Box3D function."""
    array_stub = generate_array_stub(func, type_map, blobs, data)
    if array_stub:
        return array_stub

    godot_name = to_godot_name(func.name)
    ret_godot = to_godot_type(func.return_type, type_map, qualified=False, is_pointer="*" in func.return_type)

    # Blob pointer returns are wrapped in owning RefCounted classes. A non-const
    # return transfers ownership (b3CreateHull, b3CloneHull); a const return is
    # a view into shape internals and must be deep-cloned first. Anything else
    # pointer-shaped is not generatable.
    if "*" in func.return_type and "char" not in func.return_type:
        ret_clean = func.return_type.replace("const ", "").replace("*", "").strip()
        if ret_clean not in blobs:
            return ""

    id_info = get_id_param(func, type_map)

    param_parts = []
    for i, p in enumerate(func.params):
        if p.pointer and not is_string_type(p):
            t_clean = p.type.replace("const ", "").replace("*", "").strip()
            if not is_struct_type(t_clean):
                continue
        ptype = to_godot_type(p.type, type_map, qualified=False, is_pointer=p.pointer)
        pname = to_godot_param_name(p.name, i)
        param_parts.append(f"{godot_param_type(ptype)} {pname}")

    lines = []
    call_args = []

    if id_info:
        _emit_rid_setup(id_info, func, lines, type_map)

    _build_call_args(func, id_info, lines, call_args, type_map)

    emit_return_lines(lines, func, type_map, call_args, blobs, data)

    params_str = ", ".join(param_parts) if param_parts else ""
    sig = f"{ret_godot} Box3DAPI::{godot_name}({params_str})"
    result = f"{sig} {{\n" + "\n".join(f"{l}" for l in lines) + "\n}"
    return result


def _emit_rid_setup(id_info, func, lines, type_map):
    """Emit RID lookup and null-check for the primary ID parameter."""
    param_name, id_type_name, id_data = id_info
    if "impl" not in id_data:
        return

    impl_type = id_data["impl"]
    owner = id_data["owner"]
    rid_param = to_godot_param_name(param_name, 0)
    helper = f"api_get_{owner.replace('_owner', '')}"
    lines.append(f"    {impl_type}* obj = {helper}({rid_param});")

    is_void = func.return_type == "void"
    if is_void:
        lines.append(f"    ERR_FAIL_NULL(obj);")
    else:
        default = godot_default_value(func.return_type, type_map, is_pointer="*" in func.return_type)
        lines.append(f"    ERR_FAIL_NULL_V(obj, {default});")


def _build_call_args(func, id_info, lines, call_args, type_map):
    """Convert Godot params to C API call arguments."""
    id_types = set(type_map.get("id_types", {}).keys())
    primary_id = None
    primary_id_field = None
    if id_info and "impl" in id_info[2]:
        primary_id = id_info[1]
        primary_id_field = id_info[2]["id_field"]

    for i, p in enumerate(func.params):
        pname = to_godot_param_name(p.name, i)
        ptype_clean = p.type.replace("const ", "").replace("*", "").strip()

        # Primary RID param → obj->id_field
        if ptype_clean == primary_id:
            call_args.append(f"obj->{primary_id_field}")
            continue

        # Secondary RID param → separate lookup
        if ptype_clean in id_types:
            id_data = type_map["id_types"][ptype_clean]
            if "impl" in id_data:
                _emit_secondary_rid_lookup(p, pname, id_data, lines, call_args)
                continue
            elif "unpack" in id_data:
                call_args.append(f"{id_data['unpack']}({pname})")
                continue
            else:
                call_args.append(pname)
                continue

        # Math type pointer → convert to temp b3 value, pass its address.
        # Math types are Godot value types (Vector3, Transform3D, ...), not
        # data classes, so they have no ptr(); the value is converted and the
        # call receives a pointer to a local.
        if p.pointer and not is_string_type(p):
            if ptype_clean in type_map.get("math_types", {}):
                b3_t = ptype_clean
                fn = type_map["math_types"][b3_t].get("to_b3", "godot_to_b3")
                temp = f"_{p.name}_b3"
                lines.append(f"    {b3_t} {temp} = {fn}({pname});")
                call_args.append(f"&{temp}")
                continue

        # Struct pointer → null check + ptr()
        if p.pointer and not is_string_type(p):
            if is_struct_type(ptype_clean):
                is_void = func.return_type == "void"
                if is_void:
                    lines.append(f"    ERR_FAIL_NULL({pname});")
                else:
                    default = godot_default_value(func.return_type, type_map, is_pointer="*" in func.return_type)
                    lines.append(f"    ERR_FAIL_NULL_V({pname}, {default});")
                call_args.append(f"{pname}->ptr()")
            continue

        # Regular param → type conversion
        call_args.append(convert_to_b3(pname, p.type, type_map, is_pointer=p.pointer))


def _emit_secondary_rid_lookup(p, pname, id_data, lines, call_args):
    """Emit RID lookup for a secondary ID parameter (inserted at position 2)."""
    impl_type = id_data["impl"]
    owner = id_data["owner"]
    id_field = id_data["id_field"]
    helper = f"api_get_{owner.replace('_owner', '')}"
    var_name = f"_{p.name.lower()}"
    lines.insert(2, f"    {{")
    lines.insert(3, f"    {impl_type}* {var_name} = {helper}({pname});")
    lines.insert(4, f"    ERR_FAIL_NULL({var_name});")
    call_args.append(f"{var_name}->{id_field}")
    lines.insert(5, f"    }}")


def generate_registration(func: Function, type_map: dict) -> str:
    """Generate ClassDB::bind_static_method call for a function."""
    godot_name = to_godot_name(func.name)

    skip_names = get_skip_param_names(func)
    method_args = [f'"{godot_name}"']
    for p in func.params:
        if p.name in skip_names:
            continue
        if p.pointer and not is_string_type(p):
            t_clean = p.type.replace("const ", "").replace("*", "").strip()
            if not is_struct_type(t_clean):
                continue
        pname = to_snake_case(p.name)
        method_args.append(f'"{pname}"')

    d_method = f"D_METHOD({', '.join(method_args)})"
    return f'    ClassDB::bind_static_method("Box3DAPI", {d_method}, &Box3DAPI::{godot_name});'


def generate(funcs: list[Function], domain: str, type_map: dict, blobs: set, data: dict) -> tuple[str, str, str]:
    """Generate header declarations and implementation for a domain's functions.

    Returns (header_text, impl_text, registration_text).
    """
    decls = []
    impls = []
    registrations = []

    for func in funcs:
        godot_name = to_godot_name(func.name)

        godot_params = get_godot_params(func, type_map, qualified=True)
        param_parts = [f"{godot_param_type(pt)} {pn}" for pt, pn in godot_params]
        params_str = ", ".join(param_parts) if param_parts else ""

        array_return = get_array_return_type(func, type_map, qualified=True)
        if array_return:
            ret_godot = array_return
        else:
            ret_godot = to_godot_type(func.return_type, type_map, is_pointer="*" in func.return_type)

        decls.append(f"static {ret_godot} {godot_name}({params_str});")

        stub = generate_stub(func, type_map, blobs, data)
        if stub:
            impls.append(stub)

        reg = generate_registration(func, type_map)
        registrations.append(reg)

    header = "\n".join(decls)
    impl = "\n".join(impls)
    reg_text = "\n".join(registrations)

    return header, impl, reg_text


def write_output(root: str, domains: dict, type_map: dict):
    """Write generated files to src/api/bindings/."""
    out = Path(root) / "src" / "api" / "bindings"
    out.mkdir(parents=True, exist_ok=True)
    # Sweep previously generated stub files so the output always matches the
    # current generator (data_classes/ is owned by data_class_generator.py).
    for f in out.glob("box3d_api_*.gen.cpp"):
        f.unlink()
    for name in ("box3d_api_generated.gen.hpp", "box3d_api_register.gen.cpp"):
        (out / name).unlink(missing_ok=True)

    data = parse_headers(root)
    blobs = blob_structs(data)

    all_decls = []
    all_regs = []

    for domain, funcs in sorted(domains.items()):
        header, impl, regs = generate(funcs, domain, type_map, blobs, data)
        all_decls.append(f"// --- {domain} ---\n{header}")
        all_regs.append(f"    // --- {domain} ---\n{regs}")

        impl_path = out / f"box3d_api_{domain}.gen.cpp"
        headers = [
            f'#include "../box3d_api.hpp"\n',
            f'#include "../box3d_api_helpers.hpp"\n',
            f'#include "../../misc/type_conversions.hpp"\n',
            f'#include "data_classes/data_classes.gen.hpp"\n',
        ]
        domain_headers = type_map.get("domain_headers", {})
        headers.extend(f'#include "{h}"\n' for h in domain_headers.get(domain, []))
        impl_path.write_text(
            "".join(headers) + "\n\n"
            f"{impl}\n"
        )

    header_path = out / "box3d_api_generated.gen.hpp"
    header_path.write_text(
        "#pragma once\n\n"
        "// Auto-generated by tools/generator/binding_generator.py — do not edit.\n"
        "// Type declarations use godot:: qualification for class-scope inclusion.\n\n"
        "#include <godot_cpp/variant/typed_array.hpp>\n"
        "#include <godot_cpp/variant/packed_int64_array.hpp>\n"
        "#include <godot_cpp/variant/packed_vector3_array.hpp>\n\n"
        + "\n\n".join(all_decls) + "\n"
    )

    reg_path = out / "box3d_api_register.gen.cpp"
    reg_lines = [
        "#include \"../box3d_api.hpp\"",
        "",
        "using namespace godot;",
        "",
        "void bind_generated_methods() {",
    ]

    enum_constants = type_map.get("enum_constants", {})
    if enum_constants:
        reg_lines.append("    // Enum constants")
        for enum_name, constants in sorted(enum_constants.items()):
            group_name = enum_name[2:] if enum_name.startswith("b3") else enum_name
            for godot_const, b3_val in constants:
                reg_lines.append(
                    f'    ClassDB::bind_integer_constant("Box3DAPI", "{group_name}", '
                    f'"{godot_const}", {b3_val});'
                )
        reg_lines.append("")

    reg_lines.extend("\n\n".join(all_regs).split("\n"))
    reg_lines.append("}")
    reg_lines.append("")

    reg_path.write_text("\n".join(reg_lines) + "\n")


def main():
    import argparse

    from classification import collect_generatable

    parser = argparse.ArgumentParser(description="Generate Box3D API binding methods")
    parser.add_argument("root", nargs="?", default=".", help="Project root directory")
    args = parser.parse_args()

    type_map = load_config(args.root)
    domains = collect_generatable(args.root, type_map)

    total = sum(len(f) for f in domains.values())
    print(f"Generating bindings for {total} functions across {len(domains)} domains:")
    for domain, funcs in sorted(domains.items()):
        print(f"  {domain}: {len(funcs)} functions")

    write_output(args.root, domains, type_map)
    print(f"\nOutput written to src/api/bindings/")


if __name__ == "__main__":
    main()
