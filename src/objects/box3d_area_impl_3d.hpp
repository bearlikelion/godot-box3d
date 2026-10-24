#pragma once

#include "box3d_shaped_object_impl_3d.hpp"

#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/callable.hpp>

using namespace godot;

// Area-facing wrapper. Box3D has no Area/sensor *body* concept, only sensor *shapes* on any
// body, so this is backed by a b3_kinematicBody whose every shape is created with
// isSensor=true/enableSensorEvents=true (see Box3DShapedObjectImpl3D::_is_sensor_body).
// All area-only state (gravity/damp override mode+vector, priority, monitor callbacks,
// overlap refcounting) lives here; Box3D has zero concept of any of it. The server applies
// gravity/damp overrides manually each step (see Box3DSpace3D::pre_step) using the overlap
// bookkeeping populated from b3SensorBeginTouchEvent/EndTouchEvent.
class Box3DAreaImpl3D final : public Box3DShapedObjectImpl3D {
public:
	using OverrideMode = PhysicsServer3D::AreaSpaceOverrideMode;

	bool is_default_area() const { return default_area; }

	void set_default_area(bool p_value) { default_area = p_value; }

	Variant get_param(PhysicsServer3D::AreaParameter p_param) const;

	void set_param(PhysicsServer3D::AreaParameter p_param, const Variant& p_value);

	bool has_body_monitor_callback() const { return body_monitor_callback.is_valid(); }

	void set_body_monitor_callback(const Callable& p_callback) { body_monitor_callback = p_callback; }

	const Callable& get_body_monitor_callback() const { return body_monitor_callback; }

	bool has_area_monitor_callback() const { return area_monitor_callback.is_valid(); }

	void set_area_monitor_callback(const Callable& p_callback) { area_monitor_callback = p_callback; }

	const Callable& get_area_monitor_callback() const { return area_monitor_callback; }

	bool is_monitorable() const { return monitorable; }

	void set_monitorable(bool p_monitorable) { monitorable = p_monitorable; }

	float get_priority() const { return priority; }

	void set_priority(float p_priority) { priority = p_priority; }

	OverrideMode get_gravity_mode() const { return gravity_mode; }

	void set_gravity_mode(OverrideMode p_mode) { gravity_mode = p_mode; }

	OverrideMode get_linear_damp_mode() const { return linear_damp_mode; }

	void set_linear_damp_mode(OverrideMode p_mode) { linear_damp_mode = p_mode; }

	OverrideMode get_angular_damp_mode() const { return angular_damp_mode; }

	void set_angular_damp_mode(OverrideMode p_mode) { angular_damp_mode = p_mode; }

	float get_gravity() const { return gravity; }

	void set_gravity(float p_gravity) { gravity = p_gravity; }

	bool is_point_gravity() const { return point_gravity; }

	void set_point_gravity(bool p_enabled) { point_gravity = p_enabled; }

	float get_point_gravity_distance() const { return point_gravity_distance; }

	void set_point_gravity_distance(float p_distance) { point_gravity_distance = p_distance; }

	Vector3 get_gravity_vector() const { return gravity_vector; }

	void set_gravity_vector(const Vector3& p_vector) { gravity_vector = p_vector; }

	float get_linear_damp() const { return linear_damp; }

	void set_linear_damp(float p_damp) { linear_damp = p_damp; }

	float get_angular_damp() const { return angular_damp; }

	void set_angular_damp(float p_damp) { angular_damp = p_damp; }

	Vector3 compute_gravity(const Vector3& p_position) const;

	// Refcounted per overlapping object RID (a body/area may touch this area
	// via multiple shape pairs; Godot signals entered/exited once per body, not per pair).
	// Returns true the first time this object starts overlapping (i.e. entered).
	bool add_overlap(const RID& p_other);

	// Returns true the last time this object stops overlapping (i.e. exited).
	bool remove_overlap(const RID& p_other);

	const HashMap<RID, int32_t>& get_overlaps() const { return overlaps; }

protected:
	b3BodyId _create_body_id(b3WorldId p_world_id) override;

	bool _is_sensor_body() const override { return true; }

private:
	Callable body_monitor_callback;
	Callable area_monitor_callback;

	HashMap<RID, int32_t> overlaps;

	Vector3 gravity_vector = Vector3(0, -1, 0);
	float gravity = 9.8f;
	float point_gravity_distance = 0.0f;
	float linear_damp = 0.1f;
	float angular_damp = 0.1f;
	float priority = 0.0f;

	OverrideMode gravity_mode = PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED;
	OverrideMode linear_damp_mode = PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED;
	OverrideMode angular_damp_mode = PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED;

	bool monitorable = false;
	bool point_gravity = false;
	bool default_area = false;
};
