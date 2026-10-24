#pragma once

#include "box3d_shape_impl_3d.hpp"

class Box3DSeparationRayShapeImpl3D final : public Box3DShapeImpl3D {
public:
	ShapeType get_type() const override { return PhysicsServer3D::SHAPE_SEPARATION_RAY; }

	Variant get_data() const override;

	void set_data(const Variant& p_data) override;

	AABB get_aabb() const override;

	real_t get_length() const { return length; }

	bool get_slide_on_slope() const { return slide_on_slope; }

private:
	real_t length = 1.0;
	bool slide_on_slope = false;
};
