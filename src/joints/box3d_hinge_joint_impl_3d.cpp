#include "box3d_hinge_joint_impl_3d.hpp"

#include <box3d/box3d.h>

Box3DHingeJointImpl3D::Box3DHingeJointImpl3D(
		Box3DBodyImpl3D* p_body_a,
		Box3DBodyImpl3D* p_body_b,
		const Transform3D& p_local_frame_a,
		const Transform3D& p_local_frame_b) :
		Box3DJointImpl3D(p_body_a, p_body_b, p_local_frame_a, p_local_frame_b) {
}

b3JointId Box3DHingeJointImpl3D::_create_joint_id(b3WorldId p_world_id, b3BodyId p_body_a, b3BodyId p_body_b, b3Transform p_local_frame_a, b3Transform p_local_frame_b) {
	b3RevoluteJointDef def = b3DefaultRevoluteJointDef();
	def.base.bodyIdA = p_body_a;
	def.base.bodyIdB = p_body_b;
	def.base.localFrameA = p_local_frame_a;
	def.base.localFrameB = p_local_frame_b;
	def.enableLimit = use_limit;
	def.lowerAngle = (float)limit_lower;
	def.upperAngle = (float)limit_upper;
	def.enableMotor = enable_motor;
	def.motorSpeed = (float)motor_target_velocity;
	def.maxMotorTorque = (float)motor_max_impulse;

	const b3JointId new_joint_id = b3CreateRevoluteJoint(p_world_id, &def);
	return new_joint_id;
}

real_t Box3DHingeJointImpl3D::get_param(Param p_param) const {
	switch (p_param) {
		case PhysicsServer3D::HINGE_JOINT_BIAS:
			return bias;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER:
			return limit_upper;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER:
			return limit_lower;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS:
			return limit_bias;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS:
			return limit_softness;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION:
			return limit_relaxation;
		case PhysicsServer3D::HINGE_JOINT_MOTOR_TARGET_VELOCITY:
			return motor_target_velocity;
		case PhysicsServer3D::HINGE_JOINT_MOTOR_MAX_IMPULSE:
			return motor_max_impulse;
		default:
			return 0.0;
	}
}

void Box3DHingeJointImpl3D::set_param(Param p_param, real_t p_value) {
	if (get_param(p_param) == p_value) {
		return;
	}
	switch (p_param) {
		case PhysicsServer3D::HINGE_JOINT_BIAS:
			bias = p_value;
			WARN_PRINT_ONCE("Box3D: HingeJoint3D's BIAS parameter has no Box3D equivalent and is ignored.");
			break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER:
			limit_upper = p_value;
			_apply_limit();
			break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER:
			limit_lower = p_value;
			_apply_limit();
			break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS:
			limit_bias = p_value;
			WARN_PRINT_ONCE("Box3D: HingeJoint3D's LIMIT_BIAS parameter has no Box3D equivalent and is ignored.");
			break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS:
			limit_softness = p_value;
			WARN_PRINT_ONCE("Box3D: HingeJoint3D's LIMIT_SOFTNESS parameter has no Box3D equivalent and is ignored.");
			break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION:
			limit_relaxation = p_value;
			WARN_PRINT_ONCE("Box3D: HingeJoint3D's LIMIT_RELAXATION parameter has no Box3D equivalent and is ignored.");
			break;
		case PhysicsServer3D::HINGE_JOINT_MOTOR_TARGET_VELOCITY:
			motor_target_velocity = p_value;
			_apply_motor();
			break;
		case PhysicsServer3D::HINGE_JOINT_MOTOR_MAX_IMPULSE:
			motor_max_impulse = p_value;
			_apply_motor();
			break;
		default:
			break;
	}
}

bool Box3DHingeJointImpl3D::get_flag(Flag p_flag) const {
	switch (p_flag) {
		case PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT:
			return use_limit;
		case PhysicsServer3D::HINGE_JOINT_FLAG_ENABLE_MOTOR:
			return enable_motor;
		default:
			return false;
	}
}

void Box3DHingeJointImpl3D::set_flag(Flag p_flag, bool p_enabled) {
	if (get_flag(p_flag) == p_enabled) {
		return;
	}
	switch (p_flag) {
		case PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT:
			use_limit = p_enabled;
			if (has_joint_id()) {
				b3RevoluteJoint_EnableLimit(get_joint_id(), use_limit);
				_wake_bodies();
			}
			break;
		case PhysicsServer3D::HINGE_JOINT_FLAG_ENABLE_MOTOR:
			enable_motor = p_enabled;
			if (has_joint_id()) {
				b3RevoluteJoint_EnableMotor(get_joint_id(), enable_motor);
				_wake_bodies();
			}
			break;
		default:
			break;
	}
}

void Box3DHingeJointImpl3D::_apply_limit() {
	if (has_joint_id()) {
		b3RevoluteJoint_SetLimits(get_joint_id(), (float)limit_lower, (float)limit_upper);
		_wake_bodies();
	}
}

void Box3DHingeJointImpl3D::_apply_motor() {
	if (has_joint_id()) {
		b3RevoluteJoint_SetMotorSpeed(get_joint_id(), (float)motor_target_velocity);
		b3RevoluteJoint_SetMaxMotorTorque(get_joint_id(), (float)motor_max_impulse);
		_wake_bodies();
	}
}
