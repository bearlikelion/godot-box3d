#pragma once

#include "box3d_shape_impl_3d.hpp"

#include <godot_cpp/templates/local_vector.hpp>

#include <box3d/types.h>

// ConcavePolygonShape3D -> a b3MeshData* built via b3CreateMesh() and shared by every
// attaching Box3DShapeInstance3D. Static bodies only, matching both Box3D's own
// restriction (mesh shapes only generate contacts on static bodies) and Godot's existing
// documented restriction for this shape type.
class Box3DConcavePolygonShapeImpl3D final : public Box3DShapeImpl3D {
public:
	~Box3DConcavePolygonShapeImpl3D() override;

	ShapeType get_type() const override { return PhysicsServer3D::SHAPE_CONCAVE_POLYGON; }

	Variant get_data() const override;

	void set_data(const Variant& p_data) override;

	AABB get_aabb() const override { return aabb; }

	// Returns the shared mesh; the shape holds a *reference* to this per Box3D's docs, so
	// callers must keep this Box3DConcavePolygonShapeImpl3D alive for as long as any
	// b3ShapeId built from it exists.
	const b3MeshData* get_mesh() const { return mesh; }

	const PackedVector3Array& get_faces() const { return faces; }
	bool is_backface_collision_enabled() const { return backface_collision; }

	// Box3D meshes carry no transform, so an offset instance needs its own baked copy.
	static b3MeshData* build_mesh(const PackedVector3Array& p_faces, const Transform3D& p_transform);

private:
	void _rebuild_mesh();

	PackedVector3Array faces;
	b3MeshData* mesh = nullptr;
	AABB aabb;
	bool backface_collision = false;
};
