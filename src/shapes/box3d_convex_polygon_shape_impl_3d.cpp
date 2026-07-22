#include "box3d_convex_polygon_shape_impl_3d.hpp"

#include "../misc/type_conversions.hpp"

#include <box3d/collision.h>
#include <godot_cpp/templates/local_vector.hpp>

Box3DConvexPolygonShapeImpl3D::~Box3DConvexPolygonShapeImpl3D() {
	if (hull != nullptr) {
		b3DestroyHull(hull);
		hull = nullptr;
	}
}

Variant Box3DConvexPolygonShapeImpl3D::get_data() const {
	return points;
}

void Box3DConvexPolygonShapeImpl3D::set_data(const Variant& p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::PACKED_VECTOR3_ARRAY);
	points = p_data;
	_rebuild_hull();
}

void Box3DConvexPolygonShapeImpl3D::_rebuild_hull() {
	if (hull != nullptr) {
		b3DestroyHull(hull);
		hull = nullptr;
	}

	const int count = points.size();
	if (count < 4) {
		return;
	}

	LocalVector<b3Vec3> b3_points;
	b3_points.resize(count);

	Vector3 min_point = points[0];
	Vector3 max_point = points[0];

	for (int i = 0; i < count; i++) {
		b3_points[i] = godot_to_b3(points[i]);
		min_point = min_point.min(points[i]);
		max_point = max_point.max(points[i]);
	}

	aabb = AABB(min_point, max_point - min_point);

	const int max_vertex_count = count < 255 ? count : 255;
	hull = b3CreateHull(b3_points.ptr(), count, max_vertex_count);
}
