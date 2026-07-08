#include "box3d_cylinder_shape_impl_3d.hpp"

#include "../misc/type_conversions.hpp"

#include <box3d/collision.h>
#include <box3d/math_functions.h>

Box3DCylinderShapeImpl3D::~Box3DCylinderShapeImpl3D() {
	if (hull != nullptr) {
		b3DestroyHull(hull);
		hull = nullptr;
	}
}

Variant Box3DCylinderShapeImpl3D::get_data() const {
	Dictionary data;
	data["radius"] = radius;
	data["height"] = height;
	return data;
}

void Box3DCylinderShapeImpl3D::set_data(const Variant& p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);
	const Dictionary data = p_data;
	ERR_FAIL_COND(!data.has("radius"));
	ERR_FAIL_COND(!data.has("height"));
	radius = data["radius"];
	height = data["height"];
	_rebuild_hull();
}

AABB Box3DCylinderShapeImpl3D::get_aabb() const {
	const Vector3 half(radius, height * 0.5, radius);
	return AABB(-half, half * 2.0);
}

void Box3DCylinderShapeImpl3D::_rebuild_hull() {
	if (hull != nullptr) {
		b3DestroyHull(hull);
		hull = nullptr;
	}

	if (radius <= 0.0 || height <= 0.0) {
		return;
	}

	// Two rings of SLICES points at y = +/- height/2, centered on the origin. The
	// engine's b3CreateCone helper builds an equivalent ring pair but with its base at
	// y = 0 rather than centered, so the point cloud is generated here directly to match
	// Godot's origin-centered CylinderShape3D without a post-offset.
	constexpr int point_count = 2 * SLICES;
	b3Vec3 points[point_count];

	const float r = (float)radius;
	const float half_height = (float)height * 0.5f;
	const float delta_alpha = 2.0f * (float)Math_PI / (float)SLICES;

	float alpha = 0.0f;
	for (int i = 0; i < SLICES; i++) {
		const float x = r * Math::cos(alpha);
		const float z = r * Math::sin(alpha);
		points[2 * i + 0] = b3Vec3{x, -half_height, z};
		points[2 * i + 1] = b3Vec3{x, half_height, z};
		alpha += delta_alpha;
	}

	hull = b3CreateHull(points, point_count, point_count);

	if (hull == nullptr) {
		WARN_PRINT(vformat("Box3D: failed to build a convex-hull approximation for a CylinderShape3D (radius=%f height=%f).", radius, height));
	}
}
