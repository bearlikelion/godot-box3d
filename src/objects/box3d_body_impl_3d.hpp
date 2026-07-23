#pragma once

#include "box3d_shaped_object_impl_3d.hpp"

#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/callable.hpp>

using namespace godot;

class Box3DPhysicsDirectBodyState3D;

struct Box3DBodyContact3D {
	Vector3 position;
	Vector3 normal;
	Vector3 impulse;
	RID collider;
	uint64_t collider_id = 0;
	int32_t local_shape = -1;
	int32_t collider_shape = -1;
	Vector3 collider_position;
	Vector3 collider_velocity;
	Vector3 collider_angular_velocity;
};

// RigidBody-facing wrapper: static/kinematic/dynamic bodies. Box3D requires a valid world
// before a body can be created, so construction of the b3BodyId is deferred until
// set_space() (see Box3DShapedObjectImpl3D::set_space); until then all state is cached on
// this object and applied to a freshly created b3BodyDef the moment a space is attached.
class Box3DBodyImpl3D final : public Box3DShapedObjectImpl3D {
public:
	using BodyMode = PhysicsServer3D::BodyMode;
	using BodyDampMode = PhysicsServer3D::BodyDampMode;

	~Box3DBodyImpl3D() override;

	void set_space(Box3DSpace3D* p_space) override;

	// Lazily creates (on first call) the PhysicsDirectBodyState3DExtension wrapper handed
	// to scripts and to Godot core's move_and_slide(); reused for the object's lifetime.
	Box3DPhysicsDirectBodyState3D* get_direct_state_or_null();

	BodyMode get_mode() const { return mode; }

	void set_mode(BodyMode p_mode);

	real_t get_mass() const;

	void set_mass(real_t p_mass);

	Vector3 get_inertia() const;

	void set_inertia(const Vector3& p_inertia);

	bool get_center_of_mass_custom_enabled() const { return use_custom_center_of_mass; }

	Vector3 get_center_of_mass() const;

	void set_center_of_mass(const Vector3& p_center);

	void apply_mass_from_shapes();

	real_t get_linear_damping() const { return linear_damping; }

	void set_linear_damping(real_t p_damping);

	BodyDampMode get_linear_damp_mode() const { return linear_damp_mode; }

	void set_linear_damp_mode(BodyDampMode p_mode) { linear_damp_mode = p_mode; }

	real_t get_effective_linear_damping() const { return runtime_area_state_valid ? effective_linear_damping : linear_damping; }

	real_t get_bounce() const { return bounce; }

	void set_bounce(real_t p_bounce) { bounce = p_bounce; }

	real_t get_friction() const { return friction; }

	void set_friction(real_t p_friction) { friction = p_friction; }

	real_t get_angular_damping() const { return angular_damping; }

	void set_angular_damping(real_t p_damping);

	BodyDampMode get_angular_damp_mode() const { return angular_damp_mode; }

	void set_angular_damp_mode(BodyDampMode p_mode) { angular_damp_mode = p_mode; }

	real_t get_effective_angular_damping() const { return runtime_area_state_valid ? effective_angular_damping : angular_damping; }

	real_t get_gravity_scale() const { return gravity_scale; }

	void set_gravity_scale(real_t p_scale);

	bool has_runtime_area_state() const { return runtime_area_state_valid; }

	Vector3 get_effective_total_gravity() const { return effective_total_gravity; }

	void apply_runtime_area_state(const Vector3& p_total_gravity, real_t p_linear_damping, real_t p_angular_damping);

	void clear_runtime_area_state() { runtime_area_state_valid = false; }

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

	// Called by Box3DSpace3D::pre_step() before b3World_Step: Box3D has no persistent
	// "constant force" concept, so it must be reapplied every step.
	void pre_step();

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

	bool is_contact_monitor_enabled() const { return contact_monitor_enabled; }

	void set_contact_monitor_enabled(bool p_enabled) { contact_monitor_enabled = p_enabled; }

	void add_collision_exception(const RID& p_excepted_body);

	void remove_collision_exception(const RID& p_excepted_body);

	bool has_collision_exception(const RID& p_excepted_body) const;

	const HashSet<RID>& get_collision_exceptions() const { return collision_exceptions; }

	void clear_contacts() { contacts.clear(); }

	void add_contact(const Box3DBodyContact3D& p_contact);

	const LocalVector<Box3DBodyContact3D>& get_contacts() const { return contacts; }

protected:
	b3BodyId _create_body_id(b3WorldId p_world_id) override;

	float _get_shape_friction() const override { return (float)friction; }

	float _get_shape_restitution() const override { return (float)bounce; }

private:
	void _update_motion_locks();

	BodyMode mode = PhysicsServer3D::BODY_MODE_RIGID;

	real_t mass = 1.0;
	Vector3 inertia;
	bool use_custom_mass = false;
	bool use_custom_inertia = false;
	bool use_custom_center_of_mass = false;
	Vector3 center_of_mass_custom;

	real_t linear_damping = 0.0;
	real_t angular_damping = 0.0;
	BodyDampMode linear_damp_mode = PhysicsServer3D::BODY_DAMP_MODE_COMBINE;
	BodyDampMode angular_damp_mode = PhysicsServer3D::BODY_DAMP_MODE_COMBINE;
	real_t effective_linear_damping = 0.0;
	real_t effective_angular_damping = 0.0;
	Vector3 effective_total_gravity;
	bool runtime_area_state_valid = false;
	real_t bounce = 0.0;
	real_t friction = 1.0;
	real_t gravity_scale = 1.0;
	real_t sleep_threshold = 0.05f;
	bool sleep_enabled = true;
	bool ccd_enabled = false;

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

	Callable state_sync_callback;
	Callable force_integration_callback;
	Variant force_integration_userdata;

	int32_t max_contacts_reported = 0;
	bool contact_monitor_enabled = false;
	HashSet<RID> collision_exceptions;
	LocalVector<Box3DBodyContact3D> contacts;

	Box3DPhysicsDirectBodyState3D* direct_state = nullptr;
};
