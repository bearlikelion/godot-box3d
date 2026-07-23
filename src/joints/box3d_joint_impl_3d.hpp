#pragma once

#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <box3d/id.h>
#include <box3d/math_functions.h>

using namespace godot;

class Box3DBodyImpl3D;

// Base for pin/hinge/slider joint wrappers. Box3D joints are created eagerly against two
// live b3BodyIds (unlike Godot's PhysicsServer3D API, which allows _joint_make_* to be
// called with bodies that may not yet be attached to a space) so, similar to
// Box3DShapedObjectImpl3D, the b3JointId is only created once both bodies have a valid
// b3BodyId; until then all construction state is cached here.
class Box3DJointImpl3D {
public:
	virtual ~Box3DJointImpl3D();

	virtual PhysicsServer3D::JointType get_type() const { return PhysicsServer3D::JOINT_TYPE_MAX; }

	RID get_rid() const { return rid; }

	void set_rid(const RID& p_rid) { rid = p_rid; }

	Box3DBodyImpl3D* get_body_a() const { return body_a; }

	Box3DBodyImpl3D* get_body_b() const { return body_b; }

	b3JointId get_joint_id() const { return joint_id; }

	bool has_joint_id() const { return B3_IS_NON_NULL(joint_id); }

	bool is_collision_disabled() const { return collision_disabled; }

	void set_collision_disabled(bool p_disabled);

	// Called by the server once both bodies (or either body's space attachment) may have
	// changed, to (re)build the live b3JointId if both bodies now have a b3BodyId.
	void rebuild();

protected:
	Box3DJointImpl3D(Box3DBodyImpl3D* p_body_a, Box3DBodyImpl3D* p_body_b, const Transform3D& p_local_frame_a, const Transform3D& p_local_frame_b);

	virtual b3JointId _create_joint_id(b3WorldId p_world_id, b3BodyId p_body_a, b3BodyId p_body_b, b3Transform p_local_frame_a, b3Transform p_local_frame_b) = 0;

	void _destroy_joint_id();

	Transform3D local_frame_a;
	Transform3D local_frame_b;

private:
	RID rid;
	Box3DBodyImpl3D* body_a = nullptr;
	Box3DBodyImpl3D* body_b = nullptr;
	b3JointId joint_id = b3_nullJointId;
	bool collision_disabled = false;
};
