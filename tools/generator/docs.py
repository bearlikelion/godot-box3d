"""Generate Godot class reference XML from Box3D Doxygen documentation.

Produces doc_classes/box3d_api.xml and per-struct data class XML files
in Godot's class reference format.
"""

import re
import shutil
from pathlib import Path

from classification import collect_generatable
from config import load_config
from naming import (
    data_class_filename,
    to_godot_class_name,
    to_godot_name,
    to_snake_case,
)
from parser import (
    Function,
    Struct,
    classify_domain,
    is_string_type,
    parse_headers,
)
from struct_model import (
    collect_convertible_fields,
    resolve_field_type,
)
from type_conversions import (
    to_godot_type,
)


def to_godot_xml_type(b3_type: str, type_map: dict, is_pointer: bool = False) -> str:
    """Convert a Box3D type to the Godot XML type string.

    Returns the bare type name suitable for XML <type> attributes.
    """
    return to_godot_type(b3_type, type_map, qualified=False, is_pointer=is_pointer)


def doxygen_to_bbcode(text: str) -> str:
    """Convert Doxygen markup to Godot BBCode.

    Handles:
    - `backticks` → [code]...[/code]
    - @see b3FunctionName → [code]b3FunctionName[/code]
    - Multiple spaces/newlines → single space (Godot XML paragraphs are separated by blank lines)
    """
    if not text:
        return ""

    # Convert `backtick` code spans to [code]...[/code]
    text = re.sub(r'`([^`]+)`', r'[code]\1[/code]', text)

    # Convert @see references
    text = re.sub(r'@see\s+(\S+)', r'[code]\1[/code]', text)

    # Collapse whitespace
    text = re.sub(r'\s+', ' ', text).strip()

    return text


def build_method_xml(func: Function, type_map: dict) -> str:
    """Build the <method> XML element for a function."""
    godot_name = to_godot_name(func.name)
    ret_type = to_godot_xml_type(func.return_type, type_map, is_pointer="*" in func.return_type)

    lines = []
    lines.append(f'\t\t<method name="{godot_name}">')
    lines.append(f'\t\t\t<return type="{ret_type}" />')

    # Arguments — match Doxygen @param names to Godot param names
    # Doxygen uses original C names (e.g. "worldId"), Godot uses lowercase (e.g. "worldid")
    for i, p in enumerate(func.params):
        # Skip pointer params that are filtered out by the generator (non-string pointers)
        if p.pointer and not is_string_type(p):
            continue

        xml_name = to_snake_case(p.name)  # matches D_METHOD param name
        xml_type = to_godot_xml_type(p.type, type_map, is_pointer=p.pointer)

        # Find the Doxygen description for this param
        doc_desc = func.doc.params.get(p.name, "")
        bbcode_desc = doxygen_to_bbcode(doc_desc) if doc_desc else ""

        lines.append(f'\t\t\t<argument index="{i}" name="{xml_name}" type="{xml_type}">')
        if bbcode_desc:
            lines.append(f'\t\t\t\t{bbcode_desc}')
        lines.append(f'\t\t\t</argument>')

    # Method description
    desc_parts = []
    if func.doc.summary:
        desc_parts.append(func.doc.summary)

    # Add @return info if the function returns a value and has a return description
    if func.return_type != "void" and func.doc.returns:
        desc_parts.append(f"Returns: {func.doc.returns}")

    # Add @note as [b]Note:[/b]
    for note in func.doc.notes:
        desc_parts.append(f"[b]Note:[/b] {note}")

    # Add @warning as [b]Warning:[/b]
    for warn in func.doc.warnings:
        desc_parts.append(f"[b]Warning:[/b] {warn}")

    full_desc = " ".join(desc_parts)
    bbcode_desc = doxygen_to_bbcode(full_desc)

    if bbcode_desc:
        lines.append(f'\t\t\t<description>')
        lines.append(f'\t\t\t\t{bbcode_desc}')
        lines.append(f'\t\t\t</description>')
    else:
        lines.append(f'\t\t\t<description></description>')

    lines.append(f'\t\t</method>')
    return "\n".join(lines)


def generate_class_xml(functions: list[Function], type_map: dict) -> str:
    """Generate the complete Godot class reference XML for Box3DAPI."""
    lines = []
    lines.append('<?xml version="1.0" encoding="UTF-8" ?>')
    lines.append('<class name="Box3DAPI" inherits="Object" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="../class.xsd">')
    lines.append('\t<brief_description>')
    lines.append('\t\tStatic API class for direct access to the Box3D physics engine.')
    lines.append('\t</brief_description>')
    lines.append('\t<description>')
    lines.append('\t\tStatic API class that exposes the full Box3D C API directly to Godot.')
    lines.append('\t\tFor complete documentation, see [the Box3D documentation](https://box2d.org/documentation3d/).')
    lines.append('\t\tShares the same RID space as Box3DPhysicsServer3D so objects created by either layer are accessible from both.')
    lines.append('\t</description>')
    lines.append('\t<methods>')

    # Group methods by domain for organized output
    domains = {}
    for func in functions:
        domain = classify_domain(func.name, rules=type_map.get("domain_rules"))
        domains.setdefault(domain, []).append(func)

    for domain in sorted(domains.keys()):
        lines.append(f'\t\t<!-- {domain} -->')
        for func in sorted(domains[domain], key=lambda f: to_godot_name(f.name)):
            lines.append(build_method_xml(func, type_map))

    lines.append('\t</methods>')
    lines.append('</class>')
    return "\n".join(lines)


def generate_data_class_xml(struct: Struct, type_map: dict, all_struct_names: set,
                            skip_structs: set) -> str | None:
    """Generate Godot class reference XML for a single data class.

    Returns XML string or None if the struct has no convertible fields.
    """
    cls_name = to_godot_class_name(struct.name)

    convertible_fields = collect_convertible_fields(struct, type_map, all_struct_names, skip_structs)
    if not convertible_fields:
        return None

    lines = []
    lines.append('<?xml version="1.0" encoding="UTF-8" ?>')
    lines.append(f'<class name="{cls_name}" inherits="RefCounted" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="../class.xsd">')

    # Brief description — short bindings-layer summary (distinct from the full doc)
    brief = f"Box3D bindings data class for the [code]{struct.name}[/code] struct. Use with [code]Box3DAPI[/code]."
    lines.append('\t<brief_description>')
    lines.append(f'\t\t{brief}')
    lines.append('\t</brief_description>')

    # Full description
    desc = doxygen_to_bbcode(struct.doc) if struct.doc else f"Data class wrapping the {struct.name} struct from the Box3D physics engine."
    lines.append('\t<description>')
    lines.append(f'\t\t{desc}')
    lines.append('\t</description>')

    # Members (properties)
    lines.append('\t<members>')
    for field, cls in convertible_fields:
        prop_name = to_snake_case(field.name)
        xml_type = resolve_field_type(field, cls, type_map)["xml"]
        setter = f"set_{prop_name}"
        getter = f"get_{prop_name}"
        field_doc = doxygen_to_bbcode(field.doc)

        # Escape XML special characters in the description
        if field_doc:
            # XML member element has description as text content
            lines.append(f'\t\t<member name="{prop_name}" type="{xml_type}" setter="{setter}" getter="{getter}">')
            lines.append(f'\t\t\t{field_doc}')
            lines.append(f'\t\t</member>')
        else:
            lines.append(f'\t\t<member name="{prop_name}" type="{xml_type}" setter="{setter}" getter="{getter}"></member>')
    lines.append('\t</members>')

    lines.append('</class>')
    return "\n".join(lines)


def generate_data_class_docs(root: str, type_map: dict, structs: list[Struct]) -> int:
    """Generate Godot class reference XML for all data classes.

    Returns the number of data class XML files generated.
    """
    skip_structs = set(type_map.get("skip_structs", []))
    math_types = set(type_map.get("math_types", {}).keys())
    id_types = set(type_map.get("id_types", {}).keys())

    # Filter: all structs except skip_structs, math_types, id_types
    candidates = [
        s
        for s in structs
        if s.name not in skip_structs
        and s.name not in math_types
        and s.name not in id_types
    ]

    all_struct_names = {s.name for s in structs}

    doc_dir = Path(root) / "doc_classes"
    doc_dir.mkdir(parents=True, exist_ok=True)

    generated = 0
    documented = 0

    for struct in candidates:
        xml_content = generate_data_class_xml(struct, type_map, all_struct_names, skip_structs)
        if xml_content is None:
            continue

        snake = data_class_filename(struct.name)
        out_path = doc_dir / f"box3d_{snake}.xml"
        out_path.write_text(xml_content + "\n")
        generated += 1

        if struct.doc:
            documented += 1

    print(f"Generated {generated} data class XML files in doc_classes/")
    print(f"  documented: {documented} with struct-level documentation")
    print(f"  undocumented: {generated - documented}")

    return generated


def generate_docs(root: str):
    """Generate Godot class reference XML from Box3D headers.

    Returns path to generated XML file.
    """
    type_map = load_config(root)
    domains = collect_generatable(root, type_map)
    all_functions = []
    for funcs in domains.values():
        all_functions.extend(funcs)

    # Sort by godot name for consistent output
    all_functions.sort(key=lambda f: to_godot_name(f.name))

    xml_content = generate_class_xml(all_functions, type_map)

    # Write to doc_classes/ (swept first so output always matches the generator)
    doc_dir = Path(root) / "doc_classes"
    if doc_dir.exists():
        shutil.rmtree(doc_dir)
    doc_dir.mkdir(parents=True, exist_ok=True)
    out_path = doc_dir / "box3d_api.xml"
    out_path.write_text(xml_content + "\n")

    # Count documented methods
    documented = sum(1 for f in all_functions if f.doc.summary)
    total = len(all_functions)

    print(f"Generated {out_path}")
    print(f"  methods: {total} total, {documented} with documentation")
    print(f"  undocumented: {total - documented}")

    # Generate data class documentation
    data = parse_headers(root)
    generate_data_class_docs(root, type_map, data["structs"])

    return out_path


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate Godot class reference XML for Box3DAPI and data classes")
    parser.add_argument("root", nargs="?", default=".", help="Project root directory")
    args = parser.parse_args()

    generate_docs(args.root)


if __name__ == "__main__":
    main()
