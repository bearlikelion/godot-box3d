#pragma once

// Common standard-library includes used throughout the extension.
#include <cstdint>
#include <cstring>

// godot-cpp
#include <godot_cpp/classes/wrapped.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// Box3D (headers already provide their own extern "C" guards for C++ consumers)
#include <box3d/box3d.h>
#include <box3d/collision.h>
#include <box3d/id.h>
#include <box3d/math_functions.h>
#include <box3d/types.h>
