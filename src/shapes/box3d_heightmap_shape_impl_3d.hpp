#pragma once

#include "box3d_shape_impl_3d.hpp"

#include <godot_cpp/variant/packed_float32_array.hpp>

#include <box3d/types.h>

// Godot centers the grid on its shape origin. Box3D's height-field API only creates
// corner-origin fixtures without a local transform, so use a centered static mesh.
// Identity attachments share it; transformed attachments own a baked mesh copy.
class Box3DHeightMapShapeImpl3D final : public Box3DShapeImpl3D {
public:
	~Box3DHeightMapShapeImpl3D() override;

	ShapeType get_type() const override { return PhysicsServer3D::SHAPE_HEIGHTMAP; }

	Variant get_data() const override;

	void set_data(const Variant& p_data) override;

	AABB get_aabb() const override { return aabb; }

	const b3MeshData* get_mesh() const { return mesh; }

	const PackedVector3Array& get_faces() const { return faces; }

	int get_width() const { return width; }

	int get_depth() const { return depth; }

private:
	void _rebuild();

	PackedFloat32Array heights;
	int width = 0;
	int depth = 0;
	PackedVector3Array faces;
	b3MeshData* mesh = nullptr;
	AABB aabb;
};
