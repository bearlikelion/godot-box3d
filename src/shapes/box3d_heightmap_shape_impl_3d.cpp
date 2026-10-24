#include "box3d_heightmap_shape_impl_3d.hpp"

#include "box3d_concave_polygon_shape_impl_3d.hpp"

#include <box3d/collision.h>

Box3DHeightMapShapeImpl3D::~Box3DHeightMapShapeImpl3D() {
	if (mesh != nullptr) {
		b3DestroyMesh(mesh);
		mesh = nullptr;
	}
}

Variant Box3DHeightMapShapeImpl3D::get_data() const {
	Dictionary data;
	data["width"] = width;
	data["depth"] = depth;
	data["heights"] = heights;
	return data;
}

void Box3DHeightMapShapeImpl3D::set_data(const Variant& p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_heights = data.get("heights", Variant());
	ERR_FAIL_COND(maybe_heights.get_type() != Variant::PACKED_FLOAT32_ARRAY && maybe_heights.get_type() != Variant::PACKED_FLOAT64_ARRAY);

	const Variant maybe_width = data.get("width", Variant());
	ERR_FAIL_COND(maybe_width.get_type() != Variant::INT);

	const Variant maybe_depth = data.get("depth", Variant());
	ERR_FAIL_COND(maybe_depth.get_type() != Variant::INT);

	if (maybe_heights.get_type() == Variant::PACKED_FLOAT64_ARRAY) {
		const PackedFloat64Array heights64 = maybe_heights;
		heights.resize(heights64.size());
		for (int i = 0; i < heights64.size(); i++) {
			heights.set(i, (float)heights64[i]);
		}
	} else {
		heights = maybe_heights;
	}

	width = maybe_width;
	depth = maybe_depth;

	_rebuild();
}

void Box3DHeightMapShapeImpl3D::_rebuild() {
	if (mesh != nullptr) {
		b3DestroyMesh(mesh);
		mesh = nullptr;
	}
	faces.clear();
	aabb = AABB();

	if (width < 2 || depth < 2 || heights.size() != width * depth) {
		return;
	}

	const real_t half_x = (real_t)(width - 1) * 0.5;
	const real_t half_z = (real_t)(depth - 1) * 0.5;
	faces.resize((width - 1) * (depth - 1) * 6);
	int next = 0;
	for (int z = 0; z < depth - 1; z++) {
		for (int x = 0; x < width - 1; x++) {
			const Vector3 a(x - half_x, heights[z * width + x], z - half_z);
			const Vector3 b(x + 1 - half_x, heights[z * width + x + 1], z - half_z);
			const Vector3 c(x - half_x, heights[(z + 1) * width + x], z + 1 - half_z);
			const Vector3 d(x + 1 - half_x, heights[(z + 1) * width + x + 1], z + 1 - half_z);
			// The same diagonal and winding as Godot's heightmap debug geometry.
			// NaN vertices are holes, never inputs to the native mesh builder.
			if (a.is_finite() && b.is_finite() && c.is_finite()) {
				faces.set(next++, a);
				faces.set(next++, b);
				faces.set(next++, c);
			}
			if (b.is_finite() && d.is_finite() && c.is_finite()) {
				faces.set(next++, b);
				faces.set(next++, d);
				faces.set(next++, c);
			}
		}
	}
	faces.resize(next);
	if (faces.is_empty()) {
		return;
	}
	Vector3 min_point = faces[0];
	Vector3 max_point = faces[0];
	for (int i = 1; i < faces.size(); i++) {
		min_point = min_point.min(faces[i]);
		max_point = max_point.max(faces[i]);
	}
	aabb = AABB(min_point, max_point - min_point);
	mesh = Box3DConcavePolygonShapeImpl3D::build_mesh(faces, Transform3D());
}
