#include "box3d_body_impl_3d.hpp"

#include "../misc/type_conversions.hpp"
#include "box3d_physics_direct_body_state_3d.hpp"

#include <box3d/box3d.h>

Box3DBodyImpl3D::~Box3DBodyImpl3D() {
	if (direct_state != nullptr) {
		memdelete(direct_state);
		direct_state = nullptr;
	}
}

Box3DPhysicsDirectBodyState3D* Box3DBodyImpl3D::get_direct_state_or_null() {
	if (direct_state == nullptr) {
		direct_state = memnew(Box3DPhysicsDirectBodyState3D);
		direct_state->set_body(this);
	}
	return direct_state;
}

namespace {
b3BodyType to_box3d_body_type(PhysicsServer3D::BodyMode p_mode) {
	switch (p_mode) {
		case PhysicsServer3D::BODY_MODE_STATIC:
			return b3_staticBody;
		case PhysicsServer3D::BODY_MODE_KINEMATIC:
			return b3_kinematicBody;
		case PhysicsServer3D::BODY_MODE_RIGID:
		case PhysicsServer3D::BODY_MODE_RIGID_LINEAR:
		default:
			return b3_dynamicBody;
	}
}
} // namespace

b3BodyId Box3DBodyImpl3D::_create_body_id(b3WorldId p_world_id) {
	b3BodyDef def = b3DefaultBodyDef();
	const b3Transform t = godot_to_b3_transform(get_transform());
	def.type = to_box3d_body_type(mode);
	def.position = t.p;
	def.rotation = t.q;
	def.linearVelocity = godot_to_b3(initial_linear_velocity);
	def.angularVelocity = godot_to_b3(initial_angular_velocity);
	def.linearDamping = (float)linear_damping;
	def.angularDamping = (float)angular_damping;
	def.gravityScale = (float)gravity_scale;
	def.sleepThreshold = (float)sleep_threshold;
	def.userData = this;
	def.enableSleep = sleep_enabled;
	def.isAwake = true;
	def.isBullet = ccd_enabled;
	def.isEnabled = true;
	def.motionLocks.linearX = axis_lock_linear_x;
	def.motionLocks.linearY = axis_lock_linear_y;
	def.motionLocks.linearZ = axis_lock_linear_z;
	def.motionLocks.angularX = axis_lock_angular_x || mode == PhysicsServer3D::BODY_MODE_RIGID_LINEAR;
	def.motionLocks.angularY = axis_lock_angular_y || mode == PhysicsServer3D::BODY_MODE_RIGID_LINEAR;
	def.motionLocks.angularZ = axis_lock_angular_z || mode == PhysicsServer3D::BODY_MODE_RIGID_LINEAR;

	const b3BodyId new_body_id = b3CreateBody(p_world_id, &def);

	if (use_custom_mass || use_custom_inertia) {
		b3MassData mass_data = b3Body_GetMassData(new_body_id);
		if (use_custom_mass) {
			mass_data.mass = (float)mass;
		}
		if (use_custom_inertia) {
			mass_data.inertia = b3Mat3_zero;
			mass_data.inertia.cx.x = (float)inertia.x;
			mass_data.inertia.cy.y = (float)inertia.y;
			mass_data.inertia.cz.z = (float)inertia.z;
		}
		if (use_custom_center_of_mass) {
			mass_data.center = godot_to_b3(center_of_mass_custom);
		}
		b3Body_SetMassData(new_body_id, mass_data);
	}

	return new_body_id;
}

void Box3DBodyImpl3D::set_mode(BodyMode p_mode) {
	if (mode == p_mode) {
		return;
	}
	mode = p_mode;
	if (has_body_id()) {
		b3Body_SetType(body_id, to_box3d_body_type(mode));
		_update_motion_locks();
	}
}

real_t Box3DBodyImpl3D::get_mass() const {
	if (has_body_id()) {
		return (real_t)b3Body_GetMass(body_id);
	}
	return mass;
}

void Box3DBodyImpl3D::set_mass(real_t p_mass) {
	mass = p_mass;
	use_custom_mass = true;
	if (has_body_id()) {
		b3MassData mass_data = b3Body_GetMassData(body_id);
		mass_data.mass = (float)p_mass;
		b3Body_SetMassData(body_id, mass_data);
	}
}

Vector3 Box3DBodyImpl3D::get_inertia() const {
	if (has_body_id()) {
		const b3Matrix3 tensor = b3Body_GetLocalRotationalInertia(body_id);
		return Vector3(tensor.cx.x, tensor.cy.y, tensor.cz.z);
	}
	return inertia;
}

void Box3DBodyImpl3D::set_inertia(const Vector3& p_inertia) {
	inertia = p_inertia;
	use_custom_inertia = p_inertia != Vector3();
	if (has_body_id()) {
		b3MassData mass_data = b3Body_GetMassData(body_id);
		if (use_custom_inertia) {
			mass_data.inertia = b3Mat3_zero;
			mass_data.inertia.cx.x = (float)p_inertia.x;
			mass_data.inertia.cy.y = (float)p_inertia.y;
			mass_data.inertia.cz.z = (float)p_inertia.z;
		}
		b3Body_SetMassData(body_id, mass_data);
	}
}

Vector3 Box3DBodyImpl3D::get_center_of_mass() const {
	if (has_body_id()) {
		return b3_to_godot(b3Body_GetLocalCenterOfMass(body_id));
	}
	return center_of_mass_custom;
}

void Box3DBodyImpl3D::set_center_of_mass(const Vector3& p_center) {
	center_of_mass_custom = p_center;
	use_custom_center_of_mass = true;
	if (has_body_id()) {
		b3MassData mass_data = b3Body_GetMassData(body_id);
		mass_data.center = godot_to_b3(p_center);
		b3Body_SetMassData(body_id, mass_data);
	}
}

void Box3DBodyImpl3D::apply_mass_from_shapes() {
	use_custom_mass = false;
	use_custom_inertia = false;
	use_custom_center_of_mass = false;
	if (has_body_id()) {
		b3Body_ApplyMassFromShapes(body_id);
	}
}

void Box3DBodyImpl3D::set_linear_damping(real_t p_damping) {
	linear_damping = p_damping;
	if (has_body_id()) {
		b3Body_SetLinearDamping(body_id, (float)p_damping);
	}
}

void Box3DBodyImpl3D::set_angular_damping(real_t p_damping) {
	angular_damping = p_damping;
	if (has_body_id()) {
		b3Body_SetAngularDamping(body_id, (float)p_damping);
	}
}

void Box3DBodyImpl3D::set_gravity_scale(real_t p_scale) {
	gravity_scale = p_scale;
	if (has_body_id()) {
		b3Body_SetGravityScale(body_id, (float)p_scale);
	}
}

Vector3 Box3DBodyImpl3D::get_linear_velocity() const {
	if (has_body_id()) {
		return b3_to_godot(b3Body_GetLinearVelocity(body_id));
	}
	return initial_linear_velocity;
}

void Box3DBodyImpl3D::set_linear_velocity(const Vector3& p_velocity) {
	initial_linear_velocity = p_velocity;
	if (has_body_id()) {
		b3Body_SetLinearVelocity(body_id, godot_to_b3(p_velocity));
	}
}

Vector3 Box3DBodyImpl3D::get_angular_velocity() const {
	if (has_body_id()) {
		return b3_to_godot(b3Body_GetAngularVelocity(body_id));
	}
	return initial_angular_velocity;
}

void Box3DBodyImpl3D::set_angular_velocity(const Vector3& p_velocity) {
	initial_angular_velocity = p_velocity;
	if (has_body_id()) {
		b3Body_SetAngularVelocity(body_id, godot_to_b3(p_velocity));
	}
}

bool Box3DBodyImpl3D::is_sleeping() const {
	if (has_body_id()) {
		return !b3Body_IsAwake(body_id);
	}
	return false;
}

void Box3DBodyImpl3D::set_sleeping(bool p_sleeping) {
	if (has_body_id()) {
		b3Body_SetAwake(body_id, !p_sleeping);
	}
}

void Box3DBodyImpl3D::set_sleep_enabled(bool p_enabled) {
	sleep_enabled = p_enabled;
	if (has_body_id()) {
		b3Body_EnableSleep(body_id, p_enabled);
	}
}

void Box3DBodyImpl3D::set_sleep_threshold(real_t p_threshold) {
	sleep_threshold = p_threshold;
	if (has_body_id()) {
		b3Body_SetSleepThreshold(body_id, (float)p_threshold);
	}
}

void Box3DBodyImpl3D::set_ccd_enabled(bool p_enabled) {
	ccd_enabled = p_enabled;
	if (has_body_id()) {
		b3Body_SetBullet(body_id, p_enabled);
	}
}

void Box3DBodyImpl3D::set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock) {
	switch (p_axis) {
		case PhysicsServer3D::BODY_AXIS_LINEAR_X:
			axis_lock_linear_x = p_lock;
			break;
		case PhysicsServer3D::BODY_AXIS_LINEAR_Y:
			axis_lock_linear_y = p_lock;
			break;
		case PhysicsServer3D::BODY_AXIS_LINEAR_Z:
			axis_lock_linear_z = p_lock;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_X:
			axis_lock_angular_x = p_lock;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_Y:
			axis_lock_angular_y = p_lock;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_Z:
			axis_lock_angular_z = p_lock;
			break;
		default:
			break;
	}
	apply_axis_locks();
}

bool Box3DBodyImpl3D::get_axis_lock(PhysicsServer3D::BodyAxis p_axis) const {
	switch (p_axis) {
		case PhysicsServer3D::BODY_AXIS_LINEAR_X:
			return axis_lock_linear_x;
		case PhysicsServer3D::BODY_AXIS_LINEAR_Y:
			return axis_lock_linear_y;
		case PhysicsServer3D::BODY_AXIS_LINEAR_Z:
			return axis_lock_linear_z;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_X:
			return axis_lock_angular_x;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_Y:
			return axis_lock_angular_y;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_Z:
			return axis_lock_angular_z;
		default:
			return false;
	}
}

void Box3DBodyImpl3D::apply_axis_locks() {
	_update_motion_locks();
}

void Box3DBodyImpl3D::_update_motion_locks() {
	if (!has_body_id()) {
		return;
	}
	b3MotionLocks locks;
	locks.linearX = axis_lock_linear_x;
	locks.linearY = axis_lock_linear_y;
	locks.linearZ = axis_lock_linear_z;
	locks.angularX = axis_lock_angular_x || mode == PhysicsServer3D::BODY_MODE_RIGID_LINEAR;
	locks.angularY = axis_lock_angular_y || mode == PhysicsServer3D::BODY_MODE_RIGID_LINEAR;
	locks.angularZ = axis_lock_angular_z || mode == PhysicsServer3D::BODY_MODE_RIGID_LINEAR;
	b3Body_SetMotionLocks(body_id, locks);
}

void Box3DBodyImpl3D::apply_central_impulse(const Vector3& p_impulse) {
	if (has_body_id()) {
		b3Body_ApplyLinearImpulseToCenter(body_id, godot_to_b3(p_impulse), true);
	}
}

void Box3DBodyImpl3D::apply_impulse(const Vector3& p_impulse, const Vector3& p_position) {
	if (has_body_id()) {
		const Vector3 world_point = get_transform().xform(p_position);
		b3Body_ApplyLinearImpulse(body_id, godot_to_b3(p_impulse), godot_to_b3(world_point), true);
	}
}

void Box3DBodyImpl3D::apply_torque_impulse(const Vector3& p_impulse) {
	if (has_body_id()) {
		b3Body_ApplyAngularImpulse(body_id, godot_to_b3(p_impulse), true);
	}
}

void Box3DBodyImpl3D::apply_central_force(const Vector3& p_force) {
	if (has_body_id()) {
		b3Body_ApplyForceToCenter(body_id, godot_to_b3(p_force), true);
	}
}

void Box3DBodyImpl3D::apply_force(const Vector3& p_force, const Vector3& p_position) {
	if (has_body_id()) {
		const Vector3 world_point = get_transform().xform(p_position);
		b3Body_ApplyForce(body_id, godot_to_b3(p_force), godot_to_b3(world_point), true);
	}
}

void Box3DBodyImpl3D::apply_torque(const Vector3& p_torque) {
	if (has_body_id()) {
		b3Body_ApplyTorque(body_id, godot_to_b3(p_torque), true);
	}
}

void Box3DBodyImpl3D::add_constant_central_force(const Vector3& p_force) {
	constant_force += p_force;
}

void Box3DBodyImpl3D::add_constant_force(const Vector3& p_force, const Vector3& p_position) {
	// Godot's constant force API allows an off-center application point but stores a
	// single accumulated force+torque pair; approximate by converting to an equivalent
	// force+torque about the center of mass, reapplied every step in pre_step().
	constant_force += p_force;
	const Vector3 local_point = p_position - get_center_of_mass();
	constant_torque += local_point.cross(p_force);
}

void Box3DBodyImpl3D::add_constant_torque(const Vector3& p_torque) {
	constant_torque += p_torque;
}

void Box3DBodyImpl3D::set_constant_force(const Vector3& p_force) {
	constant_force = p_force;
}

void Box3DBodyImpl3D::set_constant_torque(const Vector3& p_torque) {
	constant_torque = p_torque;
}

void Box3DBodyImpl3D::pre_step() {
	if (!has_body_id()) {
		return;
	}
	if (constant_force != Vector3()) {
		b3Body_ApplyForceToCenter(body_id, godot_to_b3(constant_force), false);
	}
	if (constant_torque != Vector3()) {
		b3Body_ApplyTorque(body_id, godot_to_b3(constant_torque), false);
	}
}

void Box3DBodyImpl3D::add_collision_exception(const RID& p_excepted_body) {
	collision_exceptions.insert(p_excepted_body);
}

void Box3DBodyImpl3D::remove_collision_exception(const RID& p_excepted_body) {
	collision_exceptions.erase(p_excepted_body);
}

bool Box3DBodyImpl3D::has_collision_exception(const RID& p_excepted_body) const {
	return collision_exceptions.has(p_excepted_body);
}

void Box3DBodyImpl3D::add_contact(const Box3DBodyContact3D& p_contact) {
	if (max_contacts_reported <= 0 || contacts.size() >= (uint32_t)max_contacts_reported) {
		return;
	}
	contacts.push_back(p_contact);
}
