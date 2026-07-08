#pragma once

#include "box3d_object_impl_3d.hpp"

#include "../shapes/box3d_shape_instance_3d.hpp"

#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <box3d/id.h>

class Box3DShapeImpl3D;

// Base for anything backed by one b3BodyId with a list of shape attachments: bodies and
// areas (areas are backed by a kinematic body whose shapes are all sensors, see
// Box3DAreaImpl3D). Owns the b3BodyId lifecycle relative to shape (re)creation: Box3D
// requires a valid world before a body can be created, so the b3BodyId only exists while
// this object has a space (see set_space()).
class Box3DShapedObjectImpl3D : public Box3DObjectImpl3D {
public:
	~Box3DShapedObjectImpl3D() override;

	b3BodyId get_body_id() const { return body_id; }

	bool has_body_id() const { return B3_IS_NON_NULL(body_id); }

	Transform3D get_transform() const;

	void set_transform(const Transform3D& p_transform);

	void add_shape(Box3DShapeImpl3D* p_shape, const Transform3D& p_transform, bool p_disabled);

	void remove_shape(const Box3DShapeImpl3D* p_shape);

	void remove_shape(int32_t p_index);

	void set_shape(int32_t p_index, Box3DShapeImpl3D* p_shape);

	void clear_shapes();

	int32_t get_shape_count() const { return (int32_t)shapes.size(); }

	Box3DShapeImpl3D* get_shape(int32_t p_index) const;

	Transform3D get_shape_transform(int32_t p_index) const;

	void set_shape_transform(int32_t p_index, const Transform3D& p_transform);

	bool is_shape_disabled(int32_t p_index) const;

	void set_shape_disabled(int32_t p_index, bool p_disabled);

	void set_space(Box3DSpace3D* p_space) override;

	// The Godot layer must reach the live engine shape filters (categoryBits): queries
	// and pair filtering read it from there. The Godot mask is deliberately NOT encoded
	// in engine filters (see build_shape in the .cpp) so only the layer setter needs to
	// touch live shapes; the base-class stored value is what queries and the space's
	// custom pair filter consult.
	void set_collision_layer(uint32_t p_layer) override;

	// Rebuilds every live b3ShapeId for the current body (used after (re)attaching to a
	// space, and after body type transitions that need shapes recreated).
	void rebuild_shapes();

	// Destroys and recreates the live b3ShapeIds of every instance referencing p_shape,
	// so geometry changes made through shape_set_data reach the engine. Distinct from
	// rebuild_shapes(), which only creates MISSING engine shapes: a data change must
	// replace an engine shape that already exists (and refresh body mass). This is the
	// path CSG level geometry depends on -- CSG attaches an empty ConcavePolygonShape3D
	// first and pushes faces into it afterwards, on every brush rebuild.
	void refresh_shape(const Box3DShapeImpl3D* p_shape);

	const Vector3& get_body_scale() const { return body_scale; }

protected:
	// Creates the underlying b3BodyId in the given world using the cached construction
	// state (transform, velocities, ...). Subclasses fill in body-type-specific fields.
	virtual b3BodyId _create_body_id(b3WorldId p_world_id) = 0;

	// Areas back themselves with a kinematic body whose every shape is a Box3D sensor
	// shape (see Box3DAreaImpl3D); regular bodies return false.
	virtual bool _is_sensor_body() const { return false; }

	// Bodies override these with their BODY_PARAM_FRICTION/BOUNCE values; areas keep the
	// Box3D defaults since sensors never generate a collision response.
	virtual float _get_shape_friction() const { return 0.6f; }

	virtual float _get_shape_restitution() const { return 0.0f; }

	void _destroy_body_id();

	b3BodyId body_id = b3_nullBodyId;

private:
	void _create_shape_instance(Box3DShapeInstance3D& p_instance);

	void _destroy_shape_instance(Box3DShapeInstance3D& p_instance);

	LocalVector<Box3DShapeInstance3D> shapes;

	// Scale component of the Godot body transform, baked into engine shape geometry at
	// build time (Box3D body transforms are rigid). Tracked so scale changes trigger a
	// geometry rebuild in set_transform.
	Vector3 body_scale = Vector3(1, 1, 1);
	// Squared-column-length cache for the cheap scale-change test in set_transform.
	Vector3 body_scale_sq = Vector3(1, 1, 1);

	// Cached so the transform is available even while detached from a space (no body_id).
	Transform3D cached_transform;
};
