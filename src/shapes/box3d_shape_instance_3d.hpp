#pragma once

#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <box3d/id.h>

typedef struct b3MeshData b3MeshData;

class Box3DShapeImpl3D;
class Box3DShapedObjectImpl3D;

// Bridges one body/area attachment (an index inside the owning object's LocalVector) to
// one live b3ShapeId. Needs no separate RID -- it is addressed purely by index inside its
// owning Box3DShapedObjectImpl3D, mirroring godot-jolt's identical design.
//
// Box3D bakes local geometry (sphere center, capsule endpoints, hull vertices/scale) at
// shape-creation time; there is no generic transform setter. So whenever the local
// transform changes, the shape must be destroyed and its geometry rebuilt & recreated
// (or, for sphere/capsule/hull, updated in place via the matching b3Shape_Set* call).
class Box3DShapeInstance3D {
public:
	Box3DShapeInstance3D() = default;

	Box3DShapeInstance3D(Box3DShapeImpl3D* p_shape, const Transform3D& p_transform) :
			shape(p_shape), transform(p_transform) {}

	Box3DShapeImpl3D* get_shape() const { return shape; }

	void set_shape(Box3DShapeImpl3D* p_shape) { shape = p_shape; }

	const Transform3D& get_transform() const { return transform; }

	void set_transform(const Transform3D& p_transform) { transform = p_transform; }

	bool is_disabled() const { return disabled; }

	void set_disabled(bool p_disabled) { disabled = p_disabled; }

	b3ShapeId get_shape_id() const { return shape_id; }

	void set_shape_id(b3ShapeId p_id) { shape_id = p_id; }

	bool has_shape_id() const { return B3_IS_NON_NULL(shape_id); }

	uint32_t get_index() const { return index; }

	void set_index(uint32_t p_index) { index = p_index; }

	void set_user_data(const Variant& p_data) { user_data = p_data; }

	Variant get_user_data() const { return user_data; }
	
	// Set when a trimesh instance needs its local transform baked into its own mesh copy.
	b3MeshData* get_owned_mesh() const { return owned_mesh; }

	void set_owned_mesh(b3MeshData* p_mesh) { owned_mesh = p_mesh; }

private:
	Box3DShapeImpl3D* shape = nullptr;
	Transform3D transform;
	b3ShapeId shape_id = b3_nullShapeId;
	b3MeshData* owned_mesh = nullptr;
	uint32_t index = 0;
	bool disabled = false;
	Variant user_data;
};
