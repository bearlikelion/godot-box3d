#pragma once

#include <box3d/types.h>

// Static C trampolines that bridge Box3D callback function pointers to Godot Callables
// stored on Box3DSpace3D. The void* context parameter is always a Box3DSpace3D*.
extern "C" {
	bool box3d_api_custom_filter_trampoline(b3ShapeId shapeIdA, b3ShapeId shapeIdB, void* context);
	bool box3d_api_pre_solve_trampoline(b3ShapeId shapeIdA, b3ShapeId shapeIdB, b3Pos point, b3Vec3 normal, void* context);
}
