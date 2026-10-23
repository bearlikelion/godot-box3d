#pragma once

#include "box3d_shaped_object_impl_3d.hpp"

#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/callable.hpp>

#include <box3d/types.h>

using namespace godot;

class Box3DFilterJointImpl3D;
class Box3DPhysicsDirectBodyState3D;

// One Godot contact point, flattened from a Box3D manifold point.
struct Box3DContactPoint3D {
	Vector3 local_position;
	Vector3 local_normal;
	Vector3 impulse;
	Vector3 local_velocity;
	Vector3 collider_position;
	Vector3 collider_velocity;
	RID collider_rid;
	uint64_t collider_instance_id = 0;
	int32_t local_shape = -1;
	int32_t collider_shape = -1;
};

// RigidBody-facing wrapper: static/kinematic/dynamic bodies. Box3D requires a valid world
// before a body can be created, so construction of the b3BodyId is deferred until
// set_space() (see Box3DShapedObjectImpl3D::set_space); until then all state is cached on
// this object and applied to a freshly created b3BodyDef the moment a space is attached.
class Box3DBodyImpl3D final : public Box3DShapedObjectImpl3D {
public:
	using BodyMode = PhysicsServer3D::BodyMode;

	~Box3DBodyImpl3D() override;

	// Lazily creates (on first call) the PhysicsDirectBodyState3DExtension wrapper handed
	// to scripts and to Godot core's move_and_slide(); reused for the object's lifetime.
	Box3DPhysicsDirectBodyState3D* get_direct_state_or_null();

	BodyMode get_mode() const { return mode; }

	void set_mode(BodyMode p_mode);

	// Godot semantics: mass is always the explicitly set value (default 1.0), never
	// derived from shape volume like native Box3D.
	real_t get_mass() const { return mass; }

	void set_mass(real_t p_mass);

	Vector3 get_inertia() const;

	void set_inertia(const Vector3& p_inertia);

	bool get_center_of_mass_custom_enabled() const { return use_custom_center_of_mass; }

	Vector3 get_center_of_mass() const;

	void set_center_of_mass(const Vector3& p_center);

	void apply_mass_from_shapes();

	real_t get_linear_damping() const { return linear_damping; }

	void set_linear_damping(real_t p_damping);

	real_t get_bounce() const { return bounce; }

	void set_bounce(real_t p_bounce) { bounce = p_bounce; }

	real_t get_friction() const { return friction; }

	void set_friction(real_t p_friction) { friction = p_friction; }

	real_t get_angular_damping() const { return angular_damping; }

	void set_angular_damping(real_t p_damping);

	real_t get_gravity_scale() const { return gravity_scale; }

	void set_gravity_scale(real_t p_scale);

	bool is_omitting_force_integration() const { return omit_force_integration; }

	void set_omit_force_integration(bool p_enabled);

	Vector3 get_linear_velocity() const;

	void set_linear_velocity(const Vector3& p_velocity);

	Vector3 get_angular_velocity() const;

	void set_angular_velocity(const Vector3& p_velocity);

	bool is_sleeping() const;

	void set_sleeping(bool p_sleeping);

	bool is_sleep_enabled() const { return sleep_enabled; }

	void set_sleep_enabled(bool p_enabled);

	real_t get_sleep_threshold() const { return sleep_threshold; }

	void set_sleep_threshold(real_t p_threshold);

	bool is_ccd_enabled() const { return ccd_enabled; }

	void set_ccd_enabled(bool p_enabled);

	void set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock);

	bool get_axis_lock(PhysicsServer3D::BodyAxis p_axis) const;

	void apply_axis_locks();

	void apply_central_impulse(const Vector3& p_impulse);

	void apply_impulse(const Vector3& p_impulse, const Vector3& p_position);

	void apply_torque_impulse(const Vector3& p_impulse);

	void apply_central_force(const Vector3& p_force);

	void apply_force(const Vector3& p_force, const Vector3& p_position);

	void apply_torque(const Vector3& p_torque);

	void add_constant_central_force(const Vector3& p_force);

	void add_constant_force(const Vector3& p_force, const Vector3& p_position);

	void add_constant_torque(const Vector3& p_torque);

	Vector3 get_constant_force() const { return constant_force; }

	void set_constant_force(const Vector3& p_force);

	Vector3 get_constant_torque() const { return constant_torque; }

	void set_constant_torque(const Vector3& p_torque);

	// Called before b3World_Step to apply transient and constant force accumulators when
	// standard force integration is enabled, then clear the transient accumulators.
	void pre_step();

	bool needs_state_sync() const { return state_sync_pending; }

	void set_needs_state_sync(bool p_needed) { state_sync_pending = p_needed; }

	void set_state_sync_callback(const Callable& p_callable) { state_sync_callback = p_callable; }

	const Callable& get_state_sync_callback() const { return state_sync_callback; }

	void set_force_integration_callback(const Callable& p_callable, const Variant& p_userdata) {
		force_integration_callback = p_callable;
		force_integration_userdata = p_userdata;
	}

	const Callable& get_force_integration_callback() const { return force_integration_callback; }

	const Variant& get_force_integration_userdata() const { return force_integration_userdata; }

	int32_t get_max_contacts_reported() const { return max_contacts_reported; }

	void set_max_contacts_reported(int32_t p_count) { max_contacts_reported = p_count; }

	// Bodies this one is excepted from colliding with, keyed by RID.
	HashMap<RID, Box3DFilterJointImpl3D*>& get_collision_exceptions() { return collision_exceptions; }

	const HashMap<RID, Box3DFilterJointImpl3D*>& get_collision_exceptions() const { return collision_exceptions; }

	// Rebuilds the contact cache; manifold pointers are only valid until the next step.
	void refresh_contacts();

	const LocalVector<Box3DContactPoint3D>& get_contacts() const { return contacts; }

protected:
	b3BodyId _create_body_id(b3WorldId p_world_id) override;

	float _get_shape_friction() const override { return (float)friction; }

	float _get_shape_restitution() const override { return (float)bounce; }

	void _shapes_changed() override { _refresh_mass_data(); }

private:
	void _update_motion_locks();

	void _sync_force_integration_settings();

	void _refresh_mass_data();

	BodyMode mode = PhysicsServer3D::BODY_MODE_RIGID;

	real_t mass = 1.0;
	Vector3 inertia;
	bool use_custom_inertia = false;
	bool use_custom_center_of_mass = false;
	Vector3 center_of_mass_custom;

	real_t linear_damping = 0.0;
	real_t angular_damping = 0.0;
	real_t bounce = 0.0;
	real_t friction = 1.0;
	real_t gravity_scale = 1.0;
	real_t sleep_threshold = 0.05f;
	bool sleep_enabled = true;
	bool ccd_enabled = false;
	bool omit_force_integration = false;

	Vector3 initial_linear_velocity;
	Vector3 initial_angular_velocity;

	bool axis_lock_linear_x = false;
	bool axis_lock_linear_y = false;
	bool axis_lock_linear_z = false;
	bool axis_lock_angular_x = false;
	bool axis_lock_angular_y = false;
	bool axis_lock_angular_z = false;

	Vector3 constant_force;
	Vector3 constant_torque;
	Vector3 applied_force;
	Vector3 applied_torque;

	Callable state_sync_callback;
	bool state_sync_pending = false;
	Callable force_integration_callback;
	Variant force_integration_userdata;

	int32_t max_contacts_reported = 0;

	LocalVector<Box3DContactPoint3D> contacts;
	// Reused every step so contact polling does not allocate in the physics loop.
	LocalVector<b3ContactData> contact_pairs;

	// Both bodies in a pair hold the same joint pointer; the server owns and frees it once.
	HashMap<RID, Box3DFilterJointImpl3D*> collision_exceptions;

	Box3DPhysicsDirectBodyState3D* direct_state = nullptr;
};
