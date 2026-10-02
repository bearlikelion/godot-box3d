#include "box3d_api_callback.hpp"

#include "../misc/type_conversions.hpp"
#include "../spaces/box3d_space_3d.hpp"

#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <box3d/id.h>

extern "C" {

bool box3d_api_custom_filter_trampoline(b3ShapeId shapeIdA, b3ShapeId shapeIdB, void* context) {
	Box3DSpace3D* space = static_cast<Box3DSpace3D*>(context);
	godot::Callable cb = space->get_custom_filter_callback();
	if (!cb.is_valid()) {
		return true;
	}
	godot::Variant result = cb.callv(godot::Array::make(
		(int)b3StoreShapeId(shapeIdA),
		(int)b3StoreShapeId(shapeIdB)));
	return result.operator bool();
}

bool box3d_api_pre_solve_trampoline(b3ShapeId shapeIdA, b3ShapeId shapeIdB, b3Pos point, b3Vec3 normal, void* context) {
	Box3DSpace3D* space = static_cast<Box3DSpace3D*>(context);
	godot::Callable cb = space->get_pre_solve_callback();
	if (!cb.is_valid()) {
		return true;
	}
	godot::Variant result = cb.callv(godot::Array::make(
		(int)b3StoreShapeId(shapeIdA),
		(int)b3StoreShapeId(shapeIdB),
		b3_to_godot(point),
		b3_to_godot(normal)));
	return result.operator bool();
}

} // extern "C"
