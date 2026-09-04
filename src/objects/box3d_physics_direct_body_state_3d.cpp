#include "box3d_physics_direct_body_state_3d.hpp"

#include "../misc/type_conversions.hpp"
#include "../spaces/box3d_physics_direct_space_state_3d.hpp"
#include "../spaces/box3d_space_3d.hpp"
#include "box3d_body_impl_3d.hpp"

#include <box3d/box3d.h>

Vector3 Box3DPhysicsDirectBodyState3D::_get_total_gravity() const {
	Box3DSpace3D* space = body->get_space();
	if (space == nullptr) {
		return Vector3();
	}
	const Box3DSpace3D::AreaOverrides overrides = space->compute_area_overrides(body);
	Vector3 gravity = overrides.gravity;
	if (!overrides.replaces_world_gravity) {
		gravity += b3_to_godot(b3World_GetGravity(space->get_world_id()));
	}
	return gravity * (float)body->get_gravity_scale();
}

double Box3DPhysicsDirectBodyState3D::_get_total_angular_damp() const {
	Box3DSpace3D* space = body->get_space();
	if (space == nullptr) {
		return body->get_angular_damping();
	}
	return space->compute_area_overrides(body).angular_damp;
}

double Box3DPhysicsDirectBodyState3D::_get_total_linear_damp() const {
	Box3DSpace3D* space = body->get_space();
	if (space == nullptr) {
		return body->get_linear_damping();
	}
	return space->compute_area_overrides(body).linear_damp;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_center_of_mass() const {
	return body->get_transform().xform(body->get_center_of_mass());
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_center_of_mass_local() const {
	return body->get_center_of_mass();
}

Basis Box3DPhysicsDirectBodyState3D::_get_principal_inertia_axes() const {
	// Box3D's inertia is always diagonal in body space, so the axes are the body rotation.
	return body->get_transform().basis.orthonormalized();
}

double Box3DPhysicsDirectBodyState3D::_get_inverse_mass() const {
	const real_t mass = body->get_mass();
	return mass > 0.0 ? 1.0 / mass : 0.0;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_inverse_inertia() const {
	const Vector3 inertia = body->get_inertia();
	return Vector3(
			inertia.x > 0.0 ? 1.0 / inertia.x : 0.0,
			inertia.y > 0.0 ? 1.0 / inertia.y : 0.0,
			inertia.z > 0.0 ? 1.0 / inertia.z : 0.0);
}

Basis Box3DPhysicsDirectBodyState3D::_get_inverse_inertia_tensor() const {
	// Callers multiply this against world-space vectors, so rotate the local diagonal into
	// world space the way GodotBody3D does (tb * diag * tb transposed).
	const Basis axes = _get_principal_inertia_axes();
	Basis diagonal;
	diagonal.scale(_get_inverse_inertia());
	return axes * diagonal * axes.transposed();
}

void Box3DPhysicsDirectBodyState3D::_set_linear_velocity(const Vector3& p_velocity) {
	body->set_linear_velocity(p_velocity);
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_linear_velocity() const {
	return body->get_linear_velocity();
}

void Box3DPhysicsDirectBodyState3D::_set_angular_velocity(const Vector3& p_velocity) {
	body->set_angular_velocity(p_velocity);
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_angular_velocity() const {
	return body->get_angular_velocity();
}

void Box3DPhysicsDirectBodyState3D::_set_transform(const Transform3D& p_transform) {
	body->set_transform(p_transform);
}

Transform3D Box3DPhysicsDirectBodyState3D::_get_transform() const {
	return body->get_transform();
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_velocity_at_local_position(const Vector3& p_local_position) const {
	if (!body->has_body_id()) {
		return Vector3();
	}
	const Vector3 world_point = body->get_transform().xform(p_local_position);
	return b3_to_godot(b3Body_GetWorldPointVelocity(body->get_body_id(), godot_to_b3(world_point)));
}

void Box3DPhysicsDirectBodyState3D::_apply_central_impulse(const Vector3& p_impulse) {
	body->apply_central_impulse(p_impulse);
}

void Box3DPhysicsDirectBodyState3D::_apply_impulse(const Vector3& p_impulse, const Vector3& p_position) {
	body->apply_impulse(p_impulse, p_position);
}

void Box3DPhysicsDirectBodyState3D::_apply_torque_impulse(const Vector3& p_impulse) {
	body->apply_torque_impulse(p_impulse);
}

void Box3DPhysicsDirectBodyState3D::_apply_central_force(const Vector3& p_force) {
	body->apply_central_force(p_force);
}

void Box3DPhysicsDirectBodyState3D::_apply_force(const Vector3& p_force, const Vector3& p_position) {
	body->apply_force(p_force, p_position);
}

void Box3DPhysicsDirectBodyState3D::_apply_torque(const Vector3& p_torque) {
	body->apply_torque(p_torque);
}

void Box3DPhysicsDirectBodyState3D::_add_constant_central_force(const Vector3& p_force) {
	body->add_constant_central_force(p_force);
}

void Box3DPhysicsDirectBodyState3D::_add_constant_force(const Vector3& p_force, const Vector3& p_position) {
	body->add_constant_force(p_force, p_position);
}

void Box3DPhysicsDirectBodyState3D::_add_constant_torque(const Vector3& p_torque) {
	body->add_constant_torque(p_torque);
}

void Box3DPhysicsDirectBodyState3D::_set_constant_force(const Vector3& p_force) {
	body->set_constant_force(p_force);
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_constant_force() const {
	return body->get_constant_force();
}

void Box3DPhysicsDirectBodyState3D::_set_constant_torque(const Vector3& p_torque) {
	body->set_constant_torque(p_torque);
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_constant_torque() const {
	return body->get_constant_torque();
}

void Box3DPhysicsDirectBodyState3D::_set_sleep_state(bool p_enabled) {
	body->set_sleeping(p_enabled);
}

bool Box3DPhysicsDirectBodyState3D::_is_sleeping() const {
	return body->is_sleeping();
}

int32_t Box3DPhysicsDirectBodyState3D::_get_contact_count() const {
	return (int32_t)body->get_contacts().size();
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_local_position(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), Vector3());
	return contacts[p_index].local_position;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_local_normal(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), Vector3());
	return contacts[p_index].local_normal;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_impulse(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), Vector3());
	return contacts[p_index].impulse;
}

int32_t Box3DPhysicsDirectBodyState3D::_get_contact_local_shape(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), -1);
	return contacts[p_index].local_shape;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_local_velocity_at_position(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), Vector3());
	return contacts[p_index].local_velocity;
}

RID Box3DPhysicsDirectBodyState3D::_get_contact_collider(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), RID());
	return contacts[p_index].collider_rid;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_collider_position(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), Vector3());
	return contacts[p_index].collider_position;
}

uint64_t Box3DPhysicsDirectBodyState3D::_get_contact_collider_id(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V_MSG(p_index, (int32_t)contacts.size(), 0, "Contact index out of range.");
	return contacts[p_index].collider_instance_id;
}

Object* Box3DPhysicsDirectBodyState3D::_get_contact_collider_object(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), nullptr);
	const uint64_t instance_id = contacts[p_index].collider_instance_id;
	if (instance_id == 0) {
		return nullptr;
	}
	return ObjectDB::get_instance(instance_id);
}

int32_t Box3DPhysicsDirectBodyState3D::_get_contact_collider_shape(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), -1);
	return contacts[p_index].collider_shape;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_collider_velocity_at_position(int32_t p_index) const {
	const LocalVector<Box3DContactPoint3D>& contacts = body->get_contacts();
	ERR_FAIL_INDEX_V(p_index, (int32_t)contacts.size(), Vector3());
	return contacts[p_index].collider_velocity;
}

double Box3DPhysicsDirectBodyState3D::_get_step() const {
	if (body->get_space() != nullptr) {
		return body->get_space()->get_last_step();
	}
	return 0.0;
}

void Box3DPhysicsDirectBodyState3D::_integrate_forces() {
	const real_t step = _get_step();
	Vector3 linear_velocity = _get_linear_velocity() + _get_total_gravity() * step;
	Vector3 angular_velocity = _get_angular_velocity();

	const real_t linear_damp = MAX(0.0, 1.0 - step * _get_total_linear_damp());
	const real_t angular_damp = MAX(0.0, 1.0 - step * _get_total_angular_damp());
	linear_velocity *= linear_damp;
	angular_velocity *= angular_damp;

	_set_linear_velocity(linear_velocity);
	_set_angular_velocity(angular_velocity);
}

PhysicsDirectSpaceState3D* Box3DPhysicsDirectBodyState3D::_get_space_state() {
	if (body->get_space() != nullptr) {
		return body->get_space()->get_direct_state();
	}
	return nullptr;
}
