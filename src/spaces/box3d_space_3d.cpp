#include "box3d_space_3d.hpp"

#include "../misc/type_conversions.hpp"
#include "../objects/box3d_area_impl_3d.hpp"
#include "../objects/box3d_body_impl_3d.hpp"
#include "../objects/box3d_physics_direct_body_state_3d.hpp"
#include "../objects/box3d_shaped_object_impl_3d.hpp"
#include "box3d_physics_direct_space_state_3d.hpp"

#include <box3d/box3d.h>

namespace {
constexpr int SUB_STEP_COUNT = 4;

// Godot solver-pair semantics: two bodies generate contacts iff EITHER scans the other
// (A.mask & B.layer) || (B.mask & A.layer). Engine-level shape filters are deliberately
// permissive (maskBits=~0, see Box3DShapedObjectImpl3D::build_shape), so this callback
// is the sole authority for body-body pair filtering. Sensor pairs pass through
// unconditionally: the engine consults this callback for sensors too, but their pairing
// order is unspecified and Godot's Area3D rule is asymmetric (the AREA's mask vs the
// body's layer), so area semantics are enforced where the detector is known -- in
// Box3DSpace3D::_pull_sensor_events.
bool godot_pair_filter(b3ShapeId p_shape_a, b3ShapeId p_shape_b, void* p_context) {
	(void)p_context;
	if (b3Shape_IsSensor(p_shape_a) || b3Shape_IsSensor(p_shape_b)) {
		return true;
	}
	auto* a = static_cast<Box3DObjectImpl3D*>(b3Body_GetUserData(b3Shape_GetBody(p_shape_a)));
	auto* b = static_cast<Box3DObjectImpl3D*>(b3Body_GetUserData(b3Shape_GetBody(p_shape_b)));
	if (a == nullptr || b == nullptr) {
		return true;
	}
	return (a->get_collision_mask() & b->get_collision_layer()) != 0 ||
			(b->get_collision_mask() & a->get_collision_layer()) != 0;
}

} // namespace

Box3DSpace3D::Box3DSpace3D() {
	b3WorldDef def = b3DefaultWorldDef();
	def.workerCount = 1;
	world_id = b3CreateWorld(&def);
	b3World_SetCustomFilterCallback(world_id, godot_pair_filter, nullptr);

	direct_state = memnew(Box3DPhysicsDirectSpaceState3D);
	direct_state->set_space(this);
}

Box3DSpace3D::~Box3DSpace3D() {
	if (direct_state != nullptr) {
		memdelete(direct_state);
		direct_state = nullptr;
	}
	if (B3_IS_NON_NULL(world_id)) {
		b3DestroyWorld(world_id);
		world_id = b3_nullWorldId;
	}
}

double Box3DSpace3D::get_param(PhysicsServer3D::SpaceParameter p_param) const {
	switch (p_param) {
		case PhysicsServer3D::SPACE_PARAM_CONTACT_RECYCLE_RADIUS:
			return b3World_GetContactRecycleDistance(world_id);
		case PhysicsServer3D::SPACE_PARAM_CONTACT_MAX_SEPARATION:
			return 0.0;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION:
			return 0.0;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_DEFAULT_BIAS:
			return 0.0;
		case PhysicsServer3D::SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD:
			return 0.05;
		case PhysicsServer3D::SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_THRESHOLD:
			return 0.05;
		case PhysicsServer3D::SPACE_PARAM_BODY_TIME_TO_SLEEP:
			return 0.5;
		case PhysicsServer3D::SPACE_PARAM_SOLVER_ITERATIONS:
			return SUB_STEP_COUNT;
		default:
			return 0.0;
	}
}

void Box3DSpace3D::set_param(PhysicsServer3D::SpaceParameter p_param, double p_value) {
	switch (p_param) {
		case PhysicsServer3D::SPACE_PARAM_CONTACT_RECYCLE_RADIUS:
			b3World_SetContactRecycleDistance(world_id, (float)p_value);
			break;
		default:
			// No direct Box3D equivalent for the remaining space parameters.
			break;
	}
}

void Box3DSpace3D::set_default_area(Box3DAreaImpl3D* p_area) {
	default_area = p_area;
}

void Box3DSpace3D::step(float p_step) {
	last_step = p_step;

	_apply_area_overrides();

	for (Box3DBodyImpl3D* body : bodies) {
		body->pre_step();
	}

	if (default_area != nullptr) {
		b3World_SetGravity(world_id, godot_to_b3(default_area->compute_gravity(Vector3())));
	}

	b3World_Step(world_id, p_step, SUB_STEP_COUNT);

	_pull_body_events();
	_pull_sensor_events();

	// Contact and joint events are intentionally not drained into any callback pipeline
	// for v1: RigidBody3D contact monitoring is served on-demand via
	// b3Body_GetContactData, and joint events are ignored, per the plan. We still fetch
	// them here so any internal Box3D per-step bookkeeping tied to fetching is exercised
	// consistently, though this is not strictly required by the API.
	b3World_GetContactEvents(world_id);
	b3World_GetJointEvents(world_id);
}

void Box3DSpace3D::_apply_area_overrides() {
	for (Box3DAreaImpl3D* area : areas) {
		if (area->get_gravity_mode() == PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED &&
				area->get_linear_damp_mode() == PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED &&
				area->get_angular_damp_mode() == PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED) {
			continue;
		}

		for (const KeyValue<Box3DShapedObjectImpl3D*, int32_t>& overlap : area->get_overlaps()) {
			auto* body = dynamic_cast<Box3DBodyImpl3D*>(overlap.key);
			if (body == nullptr || !body->has_body_id()) {
				continue;
			}
			if (area->get_gravity_mode() != PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED) {
				const Vector3 gravity = area->compute_gravity(body->get_transform().origin);
				b3Body_ApplyForceToCenter(body->get_body_id(), godot_to_b3(gravity * (float)body->get_mass()), false);
			}
		}
	}
}

void Box3DSpace3D::_pull_body_events() {
	// event.userData is set to the raw C++ object pointer at body creation time (see
	// Box3DBodyImpl3D::_create_body_id / Box3DAreaImpl3D::_create_body_id), but both
	// regular bodies AND area-backing kinematic bodies generate move events (areas move
	// too). The two are sibling classes under Box3DShapedObjectImpl3D, not related to
	// each other, so a blind static_cast<Box3DBodyImpl3D*> on an area's userData produces
	// a garbage pointer and crashes. dynamic_cast safely yields nullptr for area bodies.
	const b3BodyEvents events = b3World_GetBodyEvents(world_id);
	for (int i = 0; i < events.moveCount; i++) {
		const b3BodyMoveEvent& event = events.moveEvents[i];
		auto* body = dynamic_cast<Box3DBodyImpl3D*>(static_cast<Box3DShapedObjectImpl3D*>(event.userData));
		if (body == nullptr || !body->get_state_sync_callback().is_valid()) {
			continue;
		}
		PendingStateSync sync;
		sync.body = body;
		pending_state_syncs.push_back(sync);
	}
}

void Box3DSpace3D::_pull_sensor_events() {
	const b3SensorEvents events = b3World_GetSensorEvents(world_id);

	for (int i = 0; i < events.beginCount; i++) {
		const b3SensorBeginTouchEvent& event = events.beginEvents[i];
		if (!b3Shape_IsValid(event.sensorShapeId) || !b3Shape_IsValid(event.visitorShapeId)) {
			continue;
		}
		const b3BodyId sensor_body_id = b3Shape_GetBody(event.sensorShapeId);
		const b3BodyId visitor_body_id = b3Shape_GetBody(event.visitorShapeId);
		auto* area = static_cast<Box3DAreaImpl3D*>(b3Body_GetUserData(sensor_body_id));
		auto* other = static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(visitor_body_id));
		if (area == nullptr || other == nullptr || other == area) {
			continue;
		}
		// Godot Area3D detection rule: the AREA's collision_mask against the visitor's
		// collision_layer (the visitor's mask is irrelevant). Engine-level sensor
		// pairing is permissive (see godot_pair_filter in box3d_space_3d.cpp), so this
		// is where the semantics are enforced. End events need no mirror check:
		// remove_overlap() only fires for overlaps admitted here.
		if ((area->get_collision_mask() & other->get_collision_layer()) == 0) {
			continue;
		}

		if (area->add_overlap(other)) {
			_queue_area_event(area, other, PhysicsServer3D::AREA_BODY_ADDED);
		}
	}

	for (int i = 0; i < events.endCount; i++) {
		const b3SensorEndTouchEvent& event = events.endEvents[i];
		if (!b3Shape_IsValid(event.sensorShapeId)) {
			continue;
		}
		const b3BodyId sensor_body_id = b3Shape_GetBody(event.sensorShapeId);
		auto* area = static_cast<Box3DAreaImpl3D*>(b3Body_GetUserData(sensor_body_id));
		if (area == nullptr) {
			continue;
		}

		Box3DShapedObjectImpl3D* other = nullptr;
		if (b3Shape_IsValid(event.visitorShapeId)) {
			const b3BodyId visitor_body_id = b3Shape_GetBody(event.visitorShapeId);
			other = static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(visitor_body_id));
		}
		if (other == nullptr || other == area) {
			continue;
		}

		if (area->remove_overlap(other)) {
			_queue_area_event(area, other, PhysicsServer3D::AREA_BODY_REMOVED);
		}
	}
}

void Box3DSpace3D::_queue_area_event(
		Box3DAreaImpl3D* p_area,
		Box3DShapedObjectImpl3D* p_other,
		PhysicsServer3D::AreaBodyStatus p_status) {
	auto* other_body = dynamic_cast<Box3DBodyImpl3D*>(p_other);
	auto* other_area = dynamic_cast<Box3DAreaImpl3D*>(p_other);

	PendingAreaEvent event;
	event.status = p_status;
	event.other_rid = p_other->get_rid();
	event.other_instance_id = p_other->get_instance_id();

	if (other_body != nullptr && p_area->has_body_monitor_callback()) {
		event.callback = p_area->get_body_monitor_callback();
		pending_area_events.push_back(event);
	} else if (other_area != nullptr && p_area->has_area_monitor_callback()) {
		event.callback = p_area->get_area_monitor_callback();
		pending_area_events.push_back(event);
	}
}

void Box3DSpace3D::flush_queries() {
	flushing_queries = true;

	for (const PendingStateSync& sync : pending_state_syncs) {
		Box3DPhysicsDirectBodyState3D* state = sync.body->get_direct_state_or_null();
		if (state == nullptr) {
			continue;
		}
		Array arguments;
		arguments.resize(1);
		arguments[0] = state;
		sync.body->get_state_sync_callback().callv(arguments);
	}
	pending_state_syncs.clear();

	for (const PendingAreaEvent& event : pending_area_events) {
		Array arguments;
		arguments.resize(5);
		arguments[0] = event.status;
		arguments[1] = event.other_rid;
		arguments[2] = event.other_instance_id;
		arguments[3] = 0;
		arguments[4] = 0;
		event.callback.callv(arguments);
	}
	pending_area_events.clear();

	flushing_queries = false;
}
