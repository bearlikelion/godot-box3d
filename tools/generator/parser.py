"""Parse Box3D C headers to extract API functions, structs, and enums."""

import re
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import Optional
from typing import Literal


class ArrayDirection(Enum):
    """Direction of data flow for an array parameter."""
    INPUT = auto()       # Godot → C (e.g., b3CreateHull points)
    OUTPUT = auto()      # C → Godot (e.g., b3Body_GetShapes shapeArray)


@dataclass
class ArrayParamInfo:
    """Metadata for a C-style array parameter (Type* arr, int count)."""
    direction: ArrayDirection
    count_param: Optional[str] = None     # None = fixed single element
    count_function: Optional[str] = None  # optional: pre-count function name
    config_source: Literal["auto", "override"] = "auto"
    fixed_count: Optional[int] = None     # fixed array size (exposed as separate params)
    output_struct: Optional[dict] = None  # output struct with internal array config


@dataclass
class Param:
    type: str
    name: str
    const: bool = False
    pointer: bool = False
    array_info: Optional[ArrayParamInfo] = None


@dataclass
class FuncDoc:
    """Doxygen documentation extracted from a function's preceding comment block."""
    summary: str = ""            # main description text
    params: dict[str, str] = field(default_factory=dict)  # param name -> description
    returns: str = ""            # @return description
    notes: list[str] = field(default_factory=list)        # @note text
    warnings: list[str] = field(default_factory=list)     # @warning text
    sees: list[str] = field(default_factory=list)         # @see references


@dataclass
class Function:
    name: str
    return_type: str
    params: list[Param] = field(default_factory=list)
    header: str = ""  # source header file
    doc: FuncDoc = field(default_factory=FuncDoc)


@dataclass
class StructField:
    type: str
    name: str
    array_size: Optional[str] = None  # "24" or "B3_CONTACT_MANIFOLD_COUNT_BUCKETS"
    pointer: bool = False
    doc: str = ""


@dataclass
class Struct:
    name: str
    fields: list[StructField] = field(default_factory=list)
    header: str = ""
    doc: str = ""


@dataclass
class EnumValue:
    name: str
    value: Optional[int] = None  # None = auto-increment


@dataclass
class Enum:
    name: str
    values: list[EnumValue] = field(default_factory=list)
    header: str = ""


# --- Type classification helpers ---

def is_pointer_type(type_str: str) -> bool:
    """Check if a type string represents a pointer (including function pointers)."""
    return "*" in type_str


def is_scalar_type(type_str: str) -> bool:
    """Check if a type is a simple scalar (int, float, bool, etc.)."""
    scalars = {"int", "float", "bool", "void", "size_t", "int32_t", "int64_t",
                "uint32_t", "uint64_t", "uint8_t", "uint16_t", "int16_t"}
    return type_str.strip() in scalars


def is_struct_type(type_str: str) -> bool:
    """Check if a type is a b3 struct type."""
    return type_str.startswith("b3") and not is_pointer_type(type_str)


def is_string_type(param) -> bool:
    """Check if a parameter is a C string (const char*)."""
    return param.type == "char" and param.pointer


# --- Parameter parsing ---

_PARAM_RE = re.compile(
    r'(?:const\s+)?(?P<type>\w+(?:\s*\*)*)(?:\s+)(?P<name>\w+)'
)


def parse_params(param_str: str) -> list[Param]:
    """Parse a parameter string into Param objects.
    
    Handles: "b3WorldId worldId, float timeStep, int subStepCount"
    Handles: "const b3WorldDef* def"
    Handles: "b3CastResultFcn* fcn, void* context"
    """
    params = []
    # Split by comma, but be careful with function pointer types
    # Simple approach: split on comma, trim, parse each
    for part in param_str.split(","):
        part = part.strip()
        if not part or part == "void":
            continue
        m = _PARAM_RE.search(part)
        if m:
            ptype = m.group("type").strip()
            params.append(Param(
                type=ptype.replace("*", ""),
                name=m.group("name"),
                const="const" in part,
                pointer="*" in ptype,
            ))
    return params


# --- Doxygen comment extraction ---

# Matches /// single-line or multi-line comments
_SINGLE_LINE_DOC_RE = re.compile(r'^///\s?(.*)', re.MULTILINE)

# Matches /** */ block comments
_BLOCK_DOC_RE = re.compile(r'/\*\*(.*?)\*/', re.DOTALL)

# Matches @tag {name} description
_PARAM_TAG_RE = re.compile(r'@param\s+(\w+)\s+(.*)')
_RETURN_TAG_RE = re.compile(r'@return\s+(.*)')
_NOTE_TAG_RE = re.compile(r'@note\s+(.*)')
_WARNING_TAG_RE = re.compile(r'@warning\s+(.*)')
_SEE_TAG_RE = re.compile(r'@see\s+(.*)')
_GROUP_TAG_RE = re.compile(r'@ingroup\s+\w+')


def _extract_doc_block_before(text: str, start_pos: int) -> str:
    """Extract the Doxygen comment block immediately preceding position start_pos.

    Looks backward from start_pos for either:
    - A /** */ block comment (only if nothing but whitespace/#define between it and the decl)
    - A series of /// single-line comments (contiguous, ending just before the decl)
    Returns the raw comment text (without markers).
    """
    preceding = text[:start_pos]

    # First try /// single-line comments — scan lines backward
    lines_before = preceding.split("\n")
    result_lines = []
    for line in reversed(lines_before):
        stripped_line = line.strip()
        if stripped_line.startswith("///"):
            content = stripped_line[3:]
            # Strip leading tab or space after ///
            if content and content[0] in (' ', '\t'):
                content = content.lstrip()
            result_lines.insert(0, content)
        elif stripped_line == "":
            # Blank line — if we haven't started collecting, skip it
            # If we have started, stop (blank line breaks the block)
            if result_lines:
                break
        else:
            break

    if result_lines:
        return "\n".join(result_lines)

    # Try /** */ block comment — must be immediately before the declaration
    # Find ALL block comments, take the last one that's immediately before start_pos
    for block_match in reversed(list(_BLOCK_DOC_RE.finditer(preceding))):
        # Check nothing but whitespace, preprocessor, or other comments between block end and decl
        between = text[block_match.end():start_pos].strip()
        if not between:
            raw = block_match.group(1)
            # Clean up: remove leading/trailing *, join lines
            lines = []
            for line in raw.split("\n"):
                line = line.strip()
                # Remove trailing *
                if line.endswith("*"):
                    line = line[:-1].strip()
                # Remove leading *
                if line.startswith("*"):
                    line = line[1:].strip()
                if line:
                    lines.append(line)
            return "\n".join(lines)

    return ""


def _parse_doc_block(raw: str) -> FuncDoc:
    """Parse a raw Doxygen comment block into structured FuncDoc."""
    doc = FuncDoc()
    if not raw:
        return doc

    lines = raw.split("\n")
    summary_lines = []
    current_param = None
    current_param_desc = []
    current_note_desc = []
    current_warn_desc = []

    def _flush_param():
        nonlocal current_param, current_param_desc
        if current_param:
            doc.params[current_param] = " ".join(current_param_desc).strip()
            current_param = None
            current_param_desc = []

    def _flush_note():
        nonlocal current_note_desc
        if current_note_desc:
            doc.notes.append(" ".join(current_note_desc).strip())
            current_note_desc = []

    def _flush_warn():
        nonlocal current_warn_desc
        if current_warn_desc:
            doc.warnings.append(" ".join(current_warn_desc).strip())
            current_warn_desc = []

    _IS_TAG = re.compile(r'@(param|return|note|warning|see|ingroup)\s')

    for line in lines:
        line = line.strip()
        if not line:
            continue

        # Check if this line starts a new tag
        tag_m = _IS_TAG.match(line)

        if tag_m:
            # Flush any in-progress content
            _flush_param()
            _flush_note()
            _flush_warn()

            tag = tag_m.group(1)

            if tag == "param":
                pm = _PARAM_TAG_RE.match(line)
                if pm:
                    current_param = pm.group(1)
                    current_param_desc = [pm.group(2).strip()]
            elif tag == "return":
                rm = _RETURN_TAG_RE.match(line)
                if rm:
                    doc.returns = rm.group(1).strip()
            elif tag == "note":
                nm = _NOTE_TAG_RE.match(line)
                if nm:
                    current_note_desc = [nm.group(1).strip()]
            elif tag == "warning":
                wm = _WARNING_TAG_RE.match(line)
                if wm:
                    current_warn_desc = [wm.group(1).strip()]
            elif tag == "see":
                sm = _SEE_TAG_RE.match(line)
                if sm:
                    doc.sees.append(sm.group(1).strip())
            # @ingroup — ignore
        else:
            # Continuation line
            if current_param:
                current_param_desc.append(line)
            elif current_note_desc:
                current_note_desc.append(line)
            elif current_warn_desc:
                current_warn_desc.append(line)
            elif not (doc.params or doc.returns or doc.notes or doc.warnings):
                # Still in summary section
                summary_lines.append(line)

    # Flush remaining
    _flush_param()
    _flush_note()
    _flush_warn()

    doc.summary = re.sub(r'\s+', ' ', " ".join(summary_lines)).strip()
    return doc


def extract_doc_for_function(text: str, match_start: int) -> FuncDoc:
    """Extract and parse Doxygen documentation for a function at match_start position."""
    raw = _extract_doc_block_before(text, match_start)
    return _parse_doc_block(raw)


# --- Function parsing ---

# Match B3_API declarations, possibly multi-line
_FUNCTION_RE = re.compile(
    r'B3_API\s+'
    r'(?P<return>(?:const\s+)?\w+(?:\s*\*)?)\s+'
    r'(?P<name>b3\w+)'
    r'\s*\((?P<params>[^)]*)\)\s*;',
    re.DOTALL,
)


def parse_functions(text: str, header: str = "") -> list[Function]:
    """Extract all B3_API function declarations from header text."""
    functions = []
    for m in _FUNCTION_RE.finditer(text):
        param_str = m.group("params").strip()
        # Normalize whitespace in params (remove newlines, collapse spaces)
        param_str = re.sub(r'\s+', ' ', param_str).strip()
        if param_str == "void" or param_str == "":
            params = []
        else:
            params = parse_params(param_str)

        ret = m.group("return").strip().replace("\n", " ").strip()
        func_doc = extract_doc_for_function(text, m.start())
        functions.append(Function(
            name=m.group("name"),
            return_type=ret,
            params=params,
            header=header,
            doc=func_doc,
        ))
    return functions


# --- Struct parsing ---

_STRUCT_RE = re.compile(
    r'typedef\s+struct\s+(?P<name>b3\w+)\s*\{(?P<body>.*?)\}\s*\1\s*;',
    re.DOTALL,
)

# Anonymous typedef struct: `typedef struct { ... } b3Name;` (e.g. b3SATCache).
_ANON_STRUCT_RE = re.compile(
    r'typedef\s+struct\s*\{(?P<body>.*?)\}\s*(?P<name>b3\w+)\s*;',
    re.DOTALL,
)

_FIELD_RE = re.compile(
    r'(?P<type>(?:const\s+)?\w+(?:\s*\*)?)(?:\s+)(?P<name>\w+)(?:\[(?P<array>[\w]+)\])?\s*;'
)


def _extract_field_doc(body: str, field_start: int) -> str:
    """Extract the /// comment block immediately preceding a field within a struct body.

    Scans lines backward from field_start within the body text for contiguous /// comments.
    Returns the joined comment text (without markers).
    """
    preceding = body[:field_start]
    lines_before = preceding.split("\n")
    result_lines = []
    for line in reversed(lines_before):
        stripped_line = line.strip()
        if stripped_line.startswith("///"):
            content = stripped_line[3:]
            if content and content[0] in (' ', '\t'):
                content = content.lstrip()
            result_lines.insert(0, content)
        elif stripped_line == "":
            if result_lines:
                break
        else:
            break
    return re.sub(r'\s+', ' ', " ".join(result_lines)).strip()


def parse_structs(text: str, header: str = "") -> list[Struct]:
    """Extract all typedef struct definitions from header text.

    Handles both forms:
      typedef struct b3Name { ... } b3Name;
      typedef struct { ... } b3Name;   (anonymous, e.g. b3SATCache)
    """
    structs = []
    for m in _STRUCT_RE.finditer(text):
        structs.append(_parse_struct_match(m, text, header))
    for m in _ANON_STRUCT_RE.finditer(text):
        # Skip matches already captured by the named regex (it also requires a
        # name, so anonymous forms never overlap).
        structs.append(_parse_struct_match(m, text, header))
    return structs


def _parse_struct_match(m, text: str, header: str) -> Struct:
    struct_doc = _parse_doc_block(_extract_doc_block_before(text, m.start()))
    body = m.group("body")
    fields = []
    for fm in _FIELD_RE.finditer(body):
        ftype = fm.group("type").strip()
        field_doc = _extract_field_doc(body, fm.start())
        fields.append(StructField(
            type=ftype.replace("*", "").strip(),
            name=fm.group("name"),
            array_size=fm.group("array"),  # str: "24" or "B3_SOME_MACRO"
            pointer="*" in ftype,
            doc=field_doc,
        ))
    return Struct(
        name=m.group("name"),
        fields=fields,
        header=header,
        doc=struct_doc.summary,
    )


# --- Enum parsing ---

_ENUM_RE = re.compile(
    r'typedef\s+enum\s+(?:(?P<name>b3\w+)\s+)?\{(?P<body>.*?)\}\s+(?P<name2>b3\w+)\s*;',
    re.DOTALL,
)

_VALUE_RE = re.compile(
    r'(?m)^\s*(?P<name>b3_\w+)(?:\s*=\s*(?P<value>\d+))?'
)


def parse_enums(text: str, header: str = "") -> list[Enum]:
    """Extract all typedef enum definitions from header text.

    Handles both forms:
      typedef enum b3Name { ... } b3Name;
      typedef enum { ... } b3Name;
    """
    enums = []
    for m in _ENUM_RE.finditer(text):
        values = []
        for vm in _VALUE_RE.finditer(m.group("body")):
            values.append(EnumValue(
                name=vm.group("name"),
                value=int(vm.group("value")) if vm.group("value") else None,
            ))
        # name2 is always present; name is only present for the named form
        enum_name = m.group("name2") if m.group("name2") else m.group("name")
        enums.append(Enum(
            name=enum_name,
            values=values,
            header=header,
        ))
    return enums


# --- Header file parser ---

DEFAULT_HEADERS = [
    "box3d/include/box3d/base.h",
    "box3d/include/box3d/box3d.h",
    "box3d/include/box3d/collision.h",
    "box3d/include/box3d/types.h",
    "box3d/include/box3d/math_functions.h",
    "box3d/include/box3d/id.h",
    "box3d/include/box3d/constants.h",
]


def parse_headers(root: str, headers: list[str] | None = None) -> dict:
    """Parse Box3D headers and return all extracted data.
    
    Returns dict with keys: functions, structs, enums, header_source
    """
    if headers is None:
        headers = DEFAULT_HEADERS

    all_functions = []
    all_structs = []
    all_enums = []
    header_source = ""

    for header_path in headers:
        path = Path(root) / header_path
        if not path.exists():
            continue
        text = path.read_text()
        hdr_name = path.name
        all_functions.extend(parse_functions(text, header=hdr_name))
        all_structs.extend(parse_structs(text, header=hdr_name))
        all_enums.extend(parse_enums(text, header=hdr_name))
        header_source += f"\n/* {hdr_name} */\n" + text

    return {
        "functions": all_functions,
        "structs": all_structs,
        "enums": all_enums,
        "header_source": header_source,
    }


# --- Domain classification ---
#
# All domain rules live in config.yaml `domain_rules` (an ordered list of
# [domain, pattern] pairs; first match wins). Nothing is hardcoded here.


def classify_domain(name: str, rules: list | None = None) -> str:
    """Classify a function name into a domain based on config rules.

    ``rules`` (config.yaml ``domain_rules``) is an ordered list of
    ``[domain, pattern]`` pairs matched top-to-bottom; the first match wins.
    Returns ``"other"`` when nothing matches.
    """
    for domain, pattern in rules or []:
        if re.match(pattern, name):
            return domain
    return "other"
