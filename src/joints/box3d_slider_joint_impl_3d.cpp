#include "box3d_slider_joint_impl_3d.hpp"

#include <box3d/box3d.h>

Box3DSliderJointImpl3D::Box3DSliderJointImpl3D(
		Box3DBodyImpl3D* p_body_a,
		Box3DBodyImpl3D* p_body_b,
		const Transform3D& p_local_frame_a,
		const Transform3D& p_local_frame_b) :
		Box3DJointImpl3D(p_body_a, p_body_b, p_local_frame_a, p_local_frame_b) {
}

b3JointId Box3DSliderJointImpl3D::_create_joint_id(b3WorldId p_world_id, b3BodyId p_body_a, b3BodyId p_body_b, b3Transform p_local_frame_a, b3Transform p_local_frame_b) {
	b3PrismaticJointDef def = b3DefaultPrismaticJointDef();
	def.base.bodyIdA = p_body_a;
	def.base.bodyIdB = p_body_b;
	def.base.localFrameA = p_local_frame_a;
	def.base.localFrameB = p_local_frame_b;
	def.enableLimit = true;
	def.lowerTranslation = (float)limit_lower;
	def.upperTranslation = (float)limit_upper;
	return b3CreatePrismaticJoint(p_world_id, &def);
}

real_t Box3DSliderJointImpl3D::get_param(Param p_param) const {
	switch (p_param) {
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER:
			return limit_upper;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER:
			return limit_lower;
		default: {
			const real_t* value = unsupported_params.getptr((int)p_param);
			return value != nullptr ? *value : 0.0;
		}
	}
}

void Box3DSliderJointImpl3D::set_param(Param p_param, real_t p_value) {
	if (get_param(p_param) == p_value) {
		return;
	}
	switch (p_param) {
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER:
			limit_upper = p_value;
			_apply_limit();
			break;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER:
			limit_lower = p_value;
			_apply_limit();
			break;
		default:
			unsupported_params[(int)p_param] = p_value;
			WARN_PRINT_ONCE("Box3D: this SliderJoint3D parameter has no Box3D equivalent (Box3D's prismatic joint is a pure 1-DOF slider) and is ignored.");
			break;
	}
}

void Box3DSliderJointImpl3D::_apply_limit() {
	if (has_joint_id()) {
		b3PrismaticJoint_SetLimits(get_joint_id(), (float)limit_lower, (float)limit_upper);
		_wake_bodies();
	}
}
