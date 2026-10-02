"""Auto-detect C-style array parameters in Box3D API functions.

Detects pointer parameters that represent arrays (Type* arr, int count)
and classifies them as input, output, or inout.
"""

import re
from typing import Optional

from parser import (
    Function,
    Param,
    ArrayDirection,
    ArrayParamInfo,
    is_struct_type,
)


def _is_array_candidate(param: Param, skip_types: set) -> bool:
    """Check if a pointer parameter is a candidate for array treatment."""
    if not param.pointer:
        return False
    # Skip char* (strings)
    if param.type == "char":
        return False
    # Skip void* (opaque data)
    if param.type == "void":
        return False
    # Skip types listed in config.yaml skip_types
    if param.type in skip_types:
        return False
    return True


def _detect_direction(param: Param, func: Function) -> Optional[ArrayDirection]:
    """Auto-detect array direction from parameter and function signature.

    Heuristics:
    1. const Type* → INPUT
    2. Type* + return type is int → OUTPUT (int is the filled count)
    3. Type* + return type is void → OUTPUT (single element)
    """
    # Rule 1: const pointer → input
    if param.const:
        return ArrayDirection.INPUT

    ret = func.return_type.strip()

    # Rule 2: int return → output (return value is filled count)
    if ret == "int":
        return ArrayDirection.OUTPUT

    # Rule 3: void return → output (single element)
    if ret == "void":
        return ArrayDirection.OUTPUT

    # Ambiguous — can't determine
    return None


def _find_count_param(param: Param, func: Function) -> Optional[str]:
    """Auto-detect the count/capacity parameter for an array parameter.

    Naming convention detection (in order of preference):
    1. {paramName}Count — e.g., points → pointCount
    2. {paramName}Capacity — e.g., shapeArray → shapeArrayCapacity
    3. 'count' or 'capacity' — fallback if this is the only pointer param
    """
    name = param.name

    # Strip common suffixes for count detection
    base = name
    for suffix in ("Array", "s"):
        if base.endswith(suffix):
            base = base[:-len(suffix)]

    # Rule 1: {base}Count — e.g., pointCount, shapeCount
    camel_count = base[0].upper() + base[1:] + "Count"
    snake_count = base + "Count"

    # Rule 2: {base}Capacity
    camel_cap = base[0].upper() + base[1:] + "Capacity"
    snake_cap = base + "Capacity"

    param_names = {p.name for p in func.params if p.name != param.name}

    # Check specific naming patterns first
    for candidate in (camel_count, snake_count, camel_cap, snake_cap):
        if candidate in param_names:
            return candidate

    # Fallback 'count'/'capacity' — only if this is the only pointer param
    # (avoids false positives when multiple struct pointers share one count)
    pointer_params = [p for p in func.params
                      if p.pointer and p.name != param.name and p.type != "char"]
    if not pointer_params:
        for candidate in ("count", "capacity"):
            if candidate in param_names:
                return candidate

    return None


def _discover_count_function(func: Function, all_functions: list) -> Optional[str]:
    """Auto-discover a corresponding count function.

    For b3Body_GetShapes → look for b3Body_GetShapeCount
    For b3Shape_GetSensorData → look for b3Shape_GetSensorCapacity
    """
    name = func.name

    # Pattern: b3{Subject}_Get{Plural} → b3{Subject}_Get{Singular}Count
    # e.g., b3Body_GetShapes → b3Body_GetShapeCount
    m = re.match(r'^(b3\w+)_Get(\w+)(s?)$', name)
    if m:
        subject = m.group(1)
        noun = m.group(2)
        # Try singular (drop trailing 's')
        singular = noun[:-1] if noun.endswith('s') else noun
        count_fn = f"{subject}_Get{singular}Count"
        func_names = {f.name for f in all_functions}
        if count_fn in func_names:
            return count_fn
        # Try capacity variant
        cap_fn = f"{subject}_Get{singular}Capacity"
        if cap_fn in func_names:
            return cap_fn

    return None


def detect_array_params(
    func: Function,
    all_functions: list,
    skip_types: set | None = None,
) -> dict[str, ArrayParamInfo]:
    """Auto-detect array parameters for a function.

    Returns mapping of param name → ArrayParamInfo for each detected array.
    """
    if skip_types is None:
        skip_types = set()

    result = {}

    for param in func.params:
        if not _is_array_candidate(param, skip_types):
            continue

        count_param = _find_count_param(param, func)

        # Struct pointers without a count param are single struct pointers, not arrays
        if is_struct_type(param.type) and count_param is None:
            continue

        direction = _detect_direction(param, func)
        if direction is None:
            continue  # ambiguous — skip

        count_function = _discover_count_function(func, all_functions)

        result[param.name] = ArrayParamInfo(
            direction=direction,
            count_param=count_param,
            count_function=count_function,
            config_source="auto",
        )

    return result


def apply_array_overrides(
    detected: dict[str, ArrayParamInfo],
    overrides: dict,
) -> dict[str, ArrayParamInfo]:
    """Apply config overrides on top of auto-detected array params.

    Overrides is a dict from config.yaml array_params section:
    {
        "paramName": {
            "direction": "output",  # optional
            "count_param": "capacity",  # optional
            "count_function": "b3Body_GetShapeCount",  # optional
            "fixed_count": 3,  # optional: fixed-size array
            "output_struct": {  # optional: struct with internal array
                "type": "b3StructType",
                "array_field": "items",
                "count_field": "itemCount",
                "return_element": "Box3DItem",
                "default_capacity": 4,
            }
        }
    }
    """
    direction_map = {
        "input": ArrayDirection.INPUT,
        "output": ArrayDirection.OUTPUT,
    }

    for param_name, override in overrides.items():
        if param_name in detected:
            info = detected[param_name]
        else:
            # New param not auto-detected
            info = ArrayParamInfo(
                direction=ArrayDirection.OUTPUT,  # default
                config_source="override",
            )

        if "direction" in override:
            info.direction = direction_map.get(
                override["direction"], info.direction
            )
            info.config_source = "override"

        if "count_param" in override:
            info.count_param = override["count_param"]
            info.config_source = "override"

        if "count_function" in override:
            info.count_function = override["count_function"]
            info.config_source = "override"

        # Output struct: struct with internal array that needs custom handling
        if "output_struct" in override:
            info.output_struct = override["output_struct"]
            info.config_source = "override"

        # Fixed count: array with known size (exposed as separate params)
        if "fixed_count" in override:
            info.count_param = None  # no count param needed
            info.fixed_count = override["fixed_count"]
            info.config_source = "override"

        detected[param_name] = info

    return detected
