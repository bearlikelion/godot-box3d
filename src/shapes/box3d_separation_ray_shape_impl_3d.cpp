#include "box3d_separation_ray_shape_impl_3d.hpp"

#include <godot_cpp/variant/dictionary.hpp>

Variant Box3DSeparationRayShapeImpl3D::get_data() const {
	Dictionary data;
	data["length"] = length;
	data["slide_on_slope"] = slide_on_slope;
	return data;
}

void Box3DSeparationRayShapeImpl3D::set_data(const Variant& p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);
	const Dictionary data = p_data;
	const Variant maybe_length = data.get("length", Variant());
	const Variant maybe_slide_on_slope = data.get("slide_on_slope", Variant());
	ERR_FAIL_COND(maybe_length.get_type() != Variant::FLOAT && maybe_length.get_type() != Variant::INT);
	ERR_FAIL_COND(maybe_slide_on_slope.get_type() != Variant::BOOL);
	length = maybe_length;
	slide_on_slope = maybe_slide_on_slope;
}

AABB Box3DSeparationRayShapeImpl3D::get_aabb() const {
	return AABB(Vector3(), Vector3(0.1, 0.1, length));
}
