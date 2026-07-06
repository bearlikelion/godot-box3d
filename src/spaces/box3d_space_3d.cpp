#include "box3d_space_3d.hpp"

#include "../misc/type_conversions.hpp"
#include "../objects/box3d_area_impl_3d.hpp"
#include "../objects/box3d_body_impl_3d.hpp"
#include "../objects/box3d_physics_direct_body_state_3d.hpp"
#include "../objects/box3d_shaped_object_impl_3d.hpp"
#include "box3d_physics_direct_space_state_3d.hpp"

#include <box3d/box3d.h>

#include <godot_cpp/variant/array.hpp>

#include <algorithm>
#include <vector>

namespace {
constexpr int SUB_STEP_COUNT = 4;

bool collision_layers_match(const Box3DShapedObjectImpl3D* p_a, const Box3DShapedObjectImpl3D* p_b) {
	return p_a != nullptr && p_b != nullptr &&
			(p_a->get_collision_mask() & p_b->get_collision_layer()) != 0 &&
			(p_b->get_collision_mask() & p_a->get_collision_layer()) != 0;
}

Box3DShapedObjectImpl3D* object_from_shape(b3ShapeId p_shape_id) {
	if (B3_IS_NULL(p_shape_id) || !b3Shape_IsValid(p_shape_id)) {
		return nullptr;
	}
	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	return static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(body_id));
}

bool custom_filter_callback(b3ShapeId p_shape_a, b3ShapeId p_shape_b, void* p_context) {
	(void)p_context;

	Box3DShapedObjectImpl3D* object_a = object_from_shape(p_shape_a);
	Box3DShapedObjectImpl3D* object_b = object_from_shape(p_shape_b);
	if (object_a == nullptr || object_b == nullptr) {
		return true;
	}

	auto* body_a = dynamic_cast<Box3DBodyImpl3D*>(object_a);
	auto* body_b = dynamic_cast<Box3DBodyImpl3D*>(object_b);
	if (body_a != nullptr && body_b != nullptr &&
			(body_a->has_collision_exception(body_b->get_rid()) || body_b->has_collision_exception(body_a->get_rid()))) {
		return false;
	}

	return collision_layers_match(object_a, object_b);
}

Vector3 contact_point_from_anchor(b3BodyId p_body_id, const b3Vec3& p_anchor) {
	return b3_to_godot(b3Body_GetWorldCenterOfMass(p_body_id)) + b3_to_godot(p_anchor);
}
} // namespace

Box3DSpace3D::Box3DSpace3D() {
	b3WorldDef def = b3DefaultWorldDef();
	def.workerCount = 1;
	world_id = b3CreateWorld(&def);
	b3World_SetCustomFilterCallback(world_id, custom_filter_callback, this);

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

void Box3DSpace3D::set_debug_contact_capacity(int32_t p_capacity) {
	debug_contact_capacity = MAX(0, p_capacity);
	debug_contacts.clear();
}

int32_t Box3DSpace3D::get_process_info(PhysicsServer3D::ProcessInfo p_process_info) const {
	if (B3_IS_NULL(world_id) || !b3World_IsValid(world_id)) {
		return 0;
	}
	const b3Counters counters = b3World_GetCounters(world_id);
	switch (p_process_info) {
		case PhysicsServer3D::INFO_ACTIVE_OBJECTS:
			return b3World_GetAwakeBodyCount(world_id);
		case PhysicsServer3D::INFO_COLLISION_PAIRS:
			return counters.contactCount;
		case PhysicsServer3D::INFO_ISLAND_COUNT:
			return counters.islandCount;
		default:
			return 0;
	}
}

void Box3DSpace3D::set_default_area(Box3DAreaImpl3D* p_area) {
	default_area = p_area;
}

void Box3DSpace3D::step(float p_step) {
	last_step = p_step;

	_apply_area_overrides();

	for (Box3DBodyImpl3D* body : bodies) {
		if (body->get_force_integration_callback().is_valid()) {
			Box3DPhysicsDirectBodyState3D* state = body->get_direct_state_or_null();
			Array arguments;
			if (body->get_force_integration_userdata().get_type() == Variant::NIL) {
				arguments.resize(1);
				arguments[0] = state;
			} else {
				arguments.resize(2);
				arguments[0] = state;
				arguments[1] = body->get_force_integration_userdata();
			}
			body->get_force_integration_callback().callv(arguments);
		}
		body->pre_step();
	}

	if (default_area != nullptr) {
		b3World_SetGravity(world_id, godot_to_b3(default_area->compute_gravity(Vector3())));
	}

	b3World_Step(world_id, p_step, SUB_STEP_COUNT);

	_pull_body_events();
	_pull_sensor_events();
	_pull_contact_events();

	// Joint events are not surfaced through Godot's PhysicsServer3D API. Fetch them so
	// any transient event buffers are consistently drained after every step.
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
		auto* area = dynamic_cast<Box3DAreaImpl3D*>(object_from_shape(event.sensorShapeId));
		auto* other = object_from_shape(event.visitorShapeId);
		if (area == nullptr || other == nullptr || other == area) {
			continue;
		}

		if (area->add_overlap(other)) {
			_queue_area_event(
					area,
					other,
					PhysicsServer3D::AREA_BODY_ADDED,
					other->find_shape_index(event.visitorShapeId),
					area->find_shape_index(event.sensorShapeId));
		}
	}

	for (int i = 0; i < events.endCount; i++) {
		const b3SensorEndTouchEvent& event = events.endEvents[i];
		if (!b3Shape_IsValid(event.sensorShapeId)) {
			continue;
		}
		auto* area = dynamic_cast<Box3DAreaImpl3D*>(object_from_shape(event.sensorShapeId));
		if (area == nullptr) {
			continue;
		}

		Box3DShapedObjectImpl3D* other = nullptr;
		if (b3Shape_IsValid(event.visitorShapeId)) {
			other = object_from_shape(event.visitorShapeId);
		}
		if (other == nullptr || other == area) {
			continue;
		}

		if (area->remove_overlap(other)) {
			_queue_area_event(
					area,
					other,
					PhysicsServer3D::AREA_BODY_REMOVED,
					other->find_shape_index(event.visitorShapeId),
					area->find_shape_index(event.sensorShapeId));
		}
	}
}

void Box3DSpace3D::_pull_contact_events() {
	debug_contacts.clear();

	for (Box3DBodyImpl3D* body : bodies) {
		body->clear_contacts();
		if (!body->has_body_id()) {
			continue;
		}
		const bool needs_body_contacts = body->get_max_contacts_reported() > 0;
		const bool needs_debug_contacts = debug_contact_capacity > 0;
		if (!needs_body_contacts && !needs_debug_contacts) {
			continue;
		}

		const int capacity = b3Body_GetContactCapacity(body->get_body_id());
		if (capacity <= 0) {
			continue;
		}

		std::vector<b3ContactData> contact_data((size_t)capacity);
		const int count = b3Body_GetContactData(body->get_body_id(), contact_data.data(), capacity);
		for (int i = 0; i < count; i++) {
			const b3ContactData& data = contact_data[(size_t)i];
			Box3DShapedObjectImpl3D* object_a = object_from_shape(data.shapeIdA);
			Box3DShapedObjectImpl3D* object_b = object_from_shape(data.shapeIdB);
			auto* body_a = dynamic_cast<Box3DBodyImpl3D*>(object_a);
			auto* body_b = dynamic_cast<Box3DBodyImpl3D*>(object_b);
			if (body_a == nullptr || body_b == nullptr || data.manifolds == nullptr || data.manifoldCount <= 0) {
				continue;
			}

			const bool body_is_a = body == body_a;
			if (!body_is_a && body != body_b) {
				continue;
			}

			Box3DBodyImpl3D* collider = body_is_a ? body_b : body_a;
			const int32_t local_shape = body->find_shape_index(body_is_a ? data.shapeIdA : data.shapeIdB);
			const int32_t collider_shape = collider->find_shape_index(body_is_a ? data.shapeIdB : data.shapeIdA);
			const b3BodyId local_body_id = body_is_a ? b3Shape_GetBody(data.shapeIdA) : b3Shape_GetBody(data.shapeIdB);
			for (int manifold_index = 0; manifold_index < data.manifoldCount; manifold_index++) {
				const b3Manifold& manifold = data.manifolds[manifold_index];
				const Vector3 normal = body_is_a ? b3_to_godot(manifold.normal) : -b3_to_godot(manifold.normal);
				for (int point_index = 0; point_index < manifold.pointCount; point_index++) {
					const b3ManifoldPoint& point = manifold.points[point_index];
					const Vector3 position = body_is_a
							? contact_point_from_anchor(local_body_id, point.anchorA)
							: contact_point_from_anchor(local_body_id, point.anchorB);

					if (needs_debug_contacts && debug_contacts.size() < debug_contact_capacity && body == body_a) {
						debug_contacts.push_back(position);
					}

					if (needs_body_contacts) {
						Box3DBodyContact3D contact;
						contact.position = position;
						contact.normal = normal;
						contact.impulse = normal * point.totalNormalImpulse;
						contact.collider = collider->get_rid();
						contact.collider_id = collider->get_instance_id();
						contact.local_shape = local_shape;
						contact.collider_shape = collider_shape;
						contact.collider_position = collider->get_transform().origin;
						contact.collider_velocity = collider->get_linear_velocity();
						contact.collider_angular_velocity = collider->get_angular_velocity();
						body->add_contact(contact);
					}

					if (needs_body_contacts && body->get_contacts().size() >= (uint32_t)body->get_max_contacts_reported()) {
						break;
					}
				}
				if (needs_body_contacts && body->get_contacts().size() >= (uint32_t)body->get_max_contacts_reported()) {
					break;
				}
			}
		}
	}

	const b3ContactEvents events = b3World_GetContactEvents(world_id);
	for (int i = 0; i < events.hitCount && debug_contacts.size() < debug_contact_capacity; i++) {
		debug_contacts.push_back(b3_to_godot(events.hitEvents[i].point));
	}
}

void Box3DSpace3D::_queue_area_event(
		Box3DAreaImpl3D* p_area,
		Box3DShapedObjectImpl3D* p_other,
		PhysicsServer3D::AreaBodyStatus p_status,
		int32_t p_other_shape,
		int32_t p_area_shape) {
	auto* other_body = dynamic_cast<Box3DBodyImpl3D*>(p_other);
	auto* other_area = dynamic_cast<Box3DAreaImpl3D*>(p_other);

	PendingAreaEvent event;
	event.status = p_status;
	event.other_rid = p_other->get_rid();
	event.other_instance_id = p_other->get_instance_id();
	event.other_shape = p_other_shape;
	event.area_shape = p_area_shape;

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
		arguments[3] = event.other_shape;
		arguments[4] = event.area_shape;
		event.callback.callv(arguments);
	}
	pending_area_events.clear();

	flushing_queries = false;
}
