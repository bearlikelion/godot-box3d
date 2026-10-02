#pragma once

// Minimal helpers for generated binding code.
// Provides RID lookup functions without pulling in the full server header.
// Note: relies on precompiled.hpp being included first (via CMake PCH).

#include <godot_cpp/variant/rid.hpp>

using namespace godot;

// Forward declarations (avoid including full headers)
class Box3DPhysicsServer3D;
class Box3DSpace3D;
class Box3DBodyImpl3D;
class Box3DJointImpl3D;

// RID lookup helpers — implemented in box3d_api_helpers.cpp
Box3DSpace3D* api_get_space(RID p_rid);
Box3DBodyImpl3D* api_get_body(RID p_rid);
Box3DJointImpl3D* api_get_joint(RID p_rid);
