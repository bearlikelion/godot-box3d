#pragma once

#include "box3d_shape_impl_3d.hpp"

#include <box3d/types.h>

// CylinderShape3D -> a tessellated convex-hull approximation (Box3D has no analytic
// cylinder primitive). The hull is a b3HullData* built once from a 2*SLICES ring point
// cloud centered on the local origin with the axis along local Y (matching Godot's
// CylinderShape3D convention), shared by every attaching Box3DShapeInstance3D, and
// recreated whenever radius/height change. Behavior therefore deviates slightly from a
// true cylinder: the lateral surface is an N-gon prism rather than perfectly round.
class Box3DCylinderShapeImpl3D final : public Box3DShapeImpl3D {
public:
	// 16 slices keeps hull faces well under Box3D's limits while rolling acceptably;
	// b3CreateHull itself caps input vertex counts at 255 and the engine's own
	// b3CreateCone helper accepts at most 32 slices, so stay comfortably below both.
	static constexpr int SLICES = 16;

	~Box3DCylinderShapeImpl3D() override;

	ShapeType get_type() const override { return PhysicsServer3D::SHAPE_CYLINDER; }

	Variant get_data() const override;

	void set_data(const Variant& p_data) override;

	AABB get_aabb() const override;

	real_t get_radius() const { return radius; }

	real_t get_height() const { return height; }

	// Returns the shared hull, building it lazily if needed. Null while radius/height are
	// degenerate (<= 0), mirroring Box3DConvexPolygonShapeImpl3D::get_hull()'s contract.
	const b3HullData* get_hull() const { return hull; }

private:
	void _rebuild_hull();

	real_t radius = 0.5;
	real_t height = 2.0;
	b3HullData* hull = nullptr;
};
