#include "box3d_cone_twist_joint_impl_3d.hpp"

#include <box3d/box3d.h>

namespace {

// Box3D documents its twist limits as valid within [-0.99*pi, 0.99*pi] and the cone
// limit within [0, pi]; clamp so out-of-range Godot spans degrade instead of asserting.
constexpr float MAX_TWIST = 0.99f * (float)Math_PI;

} // namespace

Box3DConeTwistJointImpl3D::Box3DConeTwistJointImpl3D(
		Box3DBodyImpl3D* p_body_a,
		Box3DBodyImpl3D* p_body_b,
		const Transform3D& p_local_frame_a,
		const Transform3D& p_local_frame_b) :
		Box3DJointImpl3D(p_body_a, p_body_b, p_local_frame_a, p_local_frame_b) {
}

b3JointId Box3DConeTwistJointImpl3D::_create_joint_id(b3WorldId p_world_id, b3BodyId p_body_a, b3BodyId p_body_b, b3Transform p_local_frame_a, b3Transform p_local_frame_b) {
	b3SphericalJointDef def = b3DefaultSphericalJointDef();
	def.base.bodyIdA = p_body_a;
	def.base.bodyIdB = p_body_b;
	def.base.localFrameA = p_local_frame_a;
	def.base.localFrameB = p_local_frame_b;

	def.enableConeLimit = swing_span >= 0.0;
	def.coneAngle = CLAMP((float)swing_span, 0.0f, (float)Math_PI);

	def.enableTwistLimit = twist_span >= 0.0;
	const float twist = CLAMP((float)twist_span, 0.0f, MAX_TWIST);
	def.lowerTwistAngle = -twist;
	def.upperTwistAngle = twist;

	return b3CreateSphericalJoint(p_world_id, &def);
}

real_t Box3DConeTwistJointImpl3D::get_param(Param p_param) const {
	switch (p_param) {
		case PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN:
			return swing_span;
		case PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN:
			return twist_span;
		case PhysicsServer3D::CONE_TWIST_JOINT_BIAS:
			return bias;
		case PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS:
			return softness;
		case PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION:
			return relaxation;
		default:
			return 0.0;
	}
}

void Box3DConeTwistJointImpl3D::set_param(Param p_param, real_t p_value) {
	switch (p_param) {
		case PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN:
			swing_span = p_value;
			_apply_swing_limit();
			break;
		case PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN:
			twist_span = p_value;
			_apply_twist_limit();
			break;
		case PhysicsServer3D::CONE_TWIST_JOINT_BIAS:
			bias = p_value;
			WARN_PRINT_ONCE("Box3D: ConeTwistJoint3D's BIAS parameter has no Box3D equivalent and is ignored.");
			break;
		case PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS:
			softness = p_value;
			WARN_PRINT_ONCE("Box3D: ConeTwistJoint3D's SOFTNESS parameter has no Box3D equivalent and is ignored.");
			break;
		case PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION:
			relaxation = p_value;
			WARN_PRINT_ONCE("Box3D: ConeTwistJoint3D's RELAXATION parameter has no Box3D equivalent and is ignored.");
			break;
		default:
			break;
	}
}

void Box3DConeTwistJointImpl3D::_apply_swing_limit() {
	if (has_joint_id()) {
		b3SphericalJoint_EnableConeLimit(get_joint_id(), swing_span >= 0.0);
		b3SphericalJoint_SetConeLimit(get_joint_id(), CLAMP((float)swing_span, 0.0f, (float)Math_PI));
	}
}

void Box3DConeTwistJointImpl3D::_apply_twist_limit() {
	if (has_joint_id()) {
		const float twist = CLAMP((float)twist_span, 0.0f, MAX_TWIST);
		b3SphericalJoint_EnableTwistLimit(get_joint_id(), twist_span >= 0.0);
		b3SphericalJoint_SetTwistLimits(get_joint_id(), -twist, twist);
	}
}
