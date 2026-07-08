#pragma once

#include "box3d_joint_impl_3d.hpp"

// ConeTwistJoint3D -> b3SphericalJointDef/b3CreateSphericalJoint. Axis convention:
// Godot's cone-twist joint twists about the local X axis of its joint frames (Bullet
// heritage), while Box3D's spherical joint centers its cone on frame A's local Z axis and
// measures twist about frame B's local Z axis. The joint frames passed to this class must
// therefore already carry a rotation mapping local Z to the Godot twist axis (local X);
// the server's _joint_make_cone_twist applies that remap (see there). This class only
// forwards params to the resulting b3JointId.
//
// Parameter mapping:
//   SWING_SPAN -> cone limit half-angle (negative span disables the cone limit, matching
//                 Godot's "below 0.05 locks / negative frees" convention approximately:
//                 negative disables, otherwise the span is clamped to Box3D's [0, pi]).
//   TWIST_SPAN -> symmetric twist limits [-span, +span], clamped to Box3D's
//                 [-0.99*pi, 0.99*pi] (negative span disables the twist limit).
//   BIAS / SOFTNESS / RELAXATION -> no Box3D equivalent (cached + WARN_PRINT_ONCE),
//                 consistent with how the hinge bridge treats its Bullet-era params.
class Box3DConeTwistJointImpl3D final : public Box3DJointImpl3D {
public:
	using Param = PhysicsServer3D::ConeTwistJointParam;

	Box3DConeTwistJointImpl3D(Box3DBodyImpl3D* p_body_a, Box3DBodyImpl3D* p_body_b, const Transform3D& p_local_frame_a, const Transform3D& p_local_frame_b);

	PhysicsServer3D::JointType get_type() const override { return PhysicsServer3D::JOINT_TYPE_CONE_TWIST; }

	real_t get_param(Param p_param) const;

	void set_param(Param p_param, real_t p_value);

protected:
	b3JointId _create_joint_id(b3WorldId p_world_id, b3BodyId p_body_a, b3BodyId p_body_b, b3Transform p_local_frame_a, b3Transform p_local_frame_b) override;

private:
	void _apply_swing_limit();

	void _apply_twist_limit();

	// Godot ConeTwistJoint3D defaults: swing span 45 deg, twist span 180 deg.
	real_t swing_span = Math_PI * 0.25;
	real_t twist_span = Math_PI;
	real_t bias = 0.3;
	real_t softness = 0.8;
	real_t relaxation = 1.0;
};
