#pragma once

#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <box3d/id.h>

using namespace godot;

class Box3DAreaImpl3D;
class Box3DBodyImpl3D;
class Box3DPhysicsDirectSpaceState3D;
class Box3DShapedObjectImpl3D;

// Wraps one b3WorldId 1:1. Owns per-step event draining: b3World_Step's event arrays are
// transient (Box3D documents them as becoming invalid once bodies/shapes are destroyed),
// and _step()/_flush_queries() are separate engine-driven calls, so every event is
// converted into a queued descriptor immediately after b3World_Step returns rather than
// held onto across the _step/_flush_queries boundary (see step()).
class Box3DSpace3D {
public:
	explicit Box3DSpace3D();

	~Box3DSpace3D();

	b3WorldId get_world_id() const { return world_id; }

	RID get_rid() const { return rid; }

	void set_rid(const RID& p_rid) { rid = p_rid; }

	bool is_active() const { return active; }

	void set_active(bool p_active) { active = p_active; }

	double get_param(PhysicsServer3D::SpaceParameter p_param) const;

	void set_param(PhysicsServer3D::SpaceParameter p_param, double p_value);

	Box3DAreaImpl3D* get_default_area() const { return default_area; }

	void set_default_area(Box3DAreaImpl3D* p_area);

	Box3DPhysicsDirectSpaceState3D* get_direct_state() const { return direct_state; }

	float get_last_step() const { return last_step; }

	bool is_flushing_queries() const { return flushing_queries; }

	void set_debug_contact_capacity(int32_t p_capacity);

	PackedVector3Array get_debug_contacts() const { return debug_contacts; }

	int32_t get_debug_contact_count() const { return debug_contacts.size(); }

	int32_t get_process_info(PhysicsServer3D::ProcessInfo p_process_info) const;

	void register_body(Box3DBodyImpl3D* p_body) { bodies.insert(p_body); }

	void unregister_body(Box3DBodyImpl3D* p_body) { bodies.erase(p_body); }

	void register_area(Box3DAreaImpl3D* p_area) { areas.insert(p_area); }

	void unregister_area(Box3DAreaImpl3D* p_area) { areas.erase(p_area); }

	// Applies area gravity/damp overrides + constant-force accumulators, steps the world,
	// then immediately drains every Box3D event array into the pending queues below.
	void step(float p_step);

	// Drains the pending queues, invoking every queued callable directly (this call *is*
	// the deferred point, so callables run synchronously here rather than via
	// call_deferred).
	void flush_queries();

private:
	struct PendingStateSync {
		Box3DBodyImpl3D* body = nullptr;
	};

	struct PendingAreaEvent {
		Callable callback;
		PhysicsServer3D::AreaBodyStatus status = PhysicsServer3D::AREA_BODY_ADDED;
		RID other_rid;
		uint64_t other_instance_id = 0;
		int32_t other_shape = -1;
		int32_t area_shape = -1;
	};

	void _apply_area_overrides(const Vector3& p_default_gravity);

	void _queue_state_sync(Box3DBodyImpl3D* p_body);

	void _pull_body_events();

	void _pull_sensor_events();

	void _pull_contact_events();

	void _queue_area_event(
			Box3DAreaImpl3D* p_area,
			Box3DShapedObjectImpl3D* p_other,
			PhysicsServer3D::AreaBodyStatus p_status,
			int32_t p_other_shape,
			int32_t p_area_shape);

	RID rid;
	b3WorldId world_id = b3_nullWorldId;
	Box3DPhysicsDirectSpaceState3D* direct_state = nullptr;
	Box3DAreaImpl3D* default_area = nullptr;

	HashSet<Box3DBodyImpl3D*> bodies;
	HashSet<Box3DAreaImpl3D*> areas;

	LocalVector<PendingStateSync> pending_state_syncs;
	LocalVector<PendingAreaEvent> pending_area_events;
	PackedVector3Array debug_contacts;
	int32_t debug_contact_capacity = 0;

	float last_step = 0.0f;
	bool active = false;
	bool flushing_queries = false;
};
