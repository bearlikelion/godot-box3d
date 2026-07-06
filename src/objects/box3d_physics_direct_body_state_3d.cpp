#include "box3d_physics_direct_body_state_3d.hpp"

#include "../misc/type_conversions.hpp"
#include "../spaces/box3d_physics_direct_space_state_3d.hpp"
#include "../spaces/box3d_space_3d.hpp"
#include "box3d_body_impl_3d.hpp"

#include <box3d/box3d.h>

namespace {

const Box3DBodyContact3D* get_contact_or_null(const Box3DBodyImpl3D* p_body, int32_t p_index) {
	if (p_body == nullptr || p_index < 0 || p_index >= (int32_t)p_body->get_contacts().size()) {
		return nullptr;
	}
	return &p_body->get_contacts()[p_index];
}

} // namespace

Vector3 Box3DPhysicsDirectBodyState3D::_get_total_gravity() const {
	if (body->get_space() != nullptr) {
		return b3_to_godot(b3World_GetGravity(body->get_space()->get_world_id())) * (float)body->get_gravity_scale();
	}
	return Vector3();
}

double Box3DPhysicsDirectBodyState3D::_get_total_angular_damp() const {
	return body->get_angular_damping();
}

double Box3DPhysicsDirectBodyState3D::_get_total_linear_damp() const {
	return body->get_linear_damping();
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_center_of_mass() const {
	return body->get_transform().xform(body->get_center_of_mass());
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_center_of_mass_local() const {
	return body->get_center_of_mass();
}

Basis Box3DPhysicsDirectBodyState3D::_get_principal_inertia_axes() const {
	return Basis();
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
	const Vector3 inv_inertia = _get_inverse_inertia();
	Basis basis;
	basis.set_column(0, Vector3(inv_inertia.x, 0, 0));
	basis.set_column(1, Vector3(0, inv_inertia.y, 0));
	basis.set_column(2, Vector3(0, 0, inv_inertia.z));
	return basis;
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
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? body->get_transform().affine_inverse().xform(contact->position) : Vector3();
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_local_normal(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? body->get_transform().basis.inverse().xform(contact->normal).normalized() : Vector3();
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_impulse(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? contact->impulse : Vector3();
}

int32_t Box3DPhysicsDirectBodyState3D::_get_contact_local_shape(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? contact->local_shape : -1;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_local_velocity_at_position(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	if (contact == nullptr || !body->has_body_id()) {
		return Vector3();
	}
	return b3_to_godot(b3Body_GetWorldPointVelocity(body->get_body_id(), godot_to_b3(contact->position)));
}

RID Box3DPhysicsDirectBodyState3D::_get_contact_collider(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? contact->collider : RID();
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_collider_position(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? contact->collider_position : Vector3();
}

uint64_t Box3DPhysicsDirectBodyState3D::_get_contact_collider_id(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? contact->collider_id : 0;
}

Object* Box3DPhysicsDirectBodyState3D::_get_contact_collider_object(int32_t p_index) const {
	return nullptr;
}

int32_t Box3DPhysicsDirectBodyState3D::_get_contact_collider_shape(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? contact->collider_shape : -1;
}

Vector3 Box3DPhysicsDirectBodyState3D::_get_contact_collider_velocity_at_position(int32_t p_index) const {
	const Box3DBodyContact3D* contact = get_contact_or_null(body, p_index);
	return contact != nullptr ? contact->collider_velocity : Vector3();
}

double Box3DPhysicsDirectBodyState3D::_get_step() const {
	if (body->get_space() != nullptr) {
		return body->get_space()->get_last_step();
	}
	return 0.0;
}

void Box3DPhysicsDirectBodyState3D::_integrate_forces() {
	// Default implementation: PhysicsServer3DExtension calls the script-side
	// _integrate_forces() separately; nothing additional needed here for v1.
}

PhysicsDirectSpaceState3D* Box3DPhysicsDirectBodyState3D::_get_space_state() {
	if (body->get_space() != nullptr) {
		return body->get_space()->get_direct_state();
	}
	return nullptr;
}
