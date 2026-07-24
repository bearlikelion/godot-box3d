#pragma once

#include "box3d_shape_impl_3d.hpp"

class Box3DCylinderShapeImpl3D final : public Box3DShapeImpl3D {
public:
	static constexpr int HULL_SIDES = 24;

	ShapeType get_type() const override { return PhysicsServer3D::SHAPE_CYLINDER; }

	Variant get_data() const override;

	void set_data(const Variant& p_data) override;

	AABB get_aabb() const override;

	real_t get_radius() const { return radius; }

	real_t get_height() const { return height; }

private:
	real_t radius = 0.5;
	real_t height = 2.0;
};
