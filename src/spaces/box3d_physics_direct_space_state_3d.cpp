#include "box3d_physics_direct_space_state_3d.hpp"

#include "../misc/box3d_shape_proxy.hpp"
#include "../misc/type_conversions.hpp"
#include "../objects/box3d_area_impl_3d.hpp"
#include "../objects/box3d_body_impl_3d.hpp"
#include "../objects/box3d_shaped_object_impl_3d.hpp"
#include "../servers/box3d_physics_server_3d.hpp"
#include "../shapes/box3d_shape_impl_3d.hpp"
#include "box3d_query_filter_3d.hpp"
#include "box3d_space_3d.hpp"

#include <box3d/box3d.h>

#include <godot_cpp/templates/local_vector.hpp>

namespace {

struct OverlapContext {
	const Box3DQueryFilter3D* filter = nullptr;
	PhysicsServer3DExtensionShapeResult* results = nullptr;
	int32_t max_results = 0;
	int32_t count = 0;
};

bool should_report(void* p_user_data, const Box3DQueryFilter3D& p_filter, Box3DShapedObjectImpl3D*& r_object) {
	auto* object = static_cast<Box3DShapedObjectImpl3D*>(p_user_data);
	if (object == nullptr) {
		return false;
	}
	const bool is_area = dynamic_cast<Box3DAreaImpl3D*>(object) != nullptr;
	if (is_area && !p_filter.collide_with_areas) {
		return false;
	}
	if (!is_area && !p_filter.collide_with_bodies) {
		return false;
	}
	if (p_filter.should_exclude(object->get_rid())) {
		return false;
	}
	r_object = object;
	return true;
}

bool overlap_result_fcn(b3ShapeId p_shape_id, void* p_context) {
	auto* ctx = static_cast<OverlapContext*>(p_context);
	if (ctx->count >= ctx->max_results) {
		return false;
	}

	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	Box3DShapedObjectImpl3D* object = nullptr;
	if (!should_report(b3Body_GetUserData(body_id), *ctx->filter, object)) {
		return true;
	}

	PhysicsServer3DExtensionShapeResult& result = ctx->results[ctx->count];
	result.rid = object->get_rid();
	result.collider_id = object->get_instance_id();
	result.shape = 0;
	ctx->count++;
	return true;
}

struct CollideShapeContext {
	const Box3DQueryFilter3D* filter = nullptr;
	const b3ShapeProxy* query_proxy = nullptr;
	Vector3* results = nullptr;
	int32_t max_results = 0;
	int32_t count = 0;
};

// Reports the closest points between the query shape and one overlapping shape. Godot wants
// world-space pairs, and b3ShapeDistance runs in frame A, which is world space here because
// Box3DShapeProxy3D already bakes the transform into its points.
bool collide_shape_result_fcn(b3ShapeId p_shape_id, void* p_context) {
	auto* ctx = static_cast<CollideShapeContext*>(p_context);
	if (ctx->count >= ctx->max_results) {
		return false;
	}

	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	Box3DShapedObjectImpl3D* object = nullptr;
	if (!should_report(b3Body_GetUserData(body_id), *ctx->filter, object)) {
		return true;
	}

	const Transform3D object_transform = object->get_transform();
	for (int32_t i = 0; i < object->get_shape_count(); i++) {
		if (!object->has_shape_id(i) || !B3_ID_EQUALS(object->get_shape_id(i), p_shape_id)) {
			continue;
		}

		const Box3DShapeProxy3D other_proxy(object->get_shape(i), object_transform * object->get_shape_transform(i));
		if (!other_proxy.is_supported()) {
			return true;
		}

		b3DistanceInput input{};
		input.proxyA = *ctx->query_proxy;
		input.proxyB = other_proxy.get_proxy();
		input.transform = b3Transform_identity;
		input.useRadii = true;

		b3SimplexCache cache{};
		const b3DistanceOutput output = b3ShapeDistance(&input, &cache, nullptr, 0);

		// GJK cannot recover penetration depth, so an overlapping pair reports its witness
		// point for both sides rather than a fabricated depth.
		ctx->results[ctx->count * 2 + 0] = b3_to_godot(output.pointA);
		ctx->results[ctx->count * 2 + 1] = b3_to_godot(output.pointB);
		ctx->count++;
		return true;
	}
	return true;
}

struct RayContext {
	const Box3DQueryFilter3D* filter = nullptr;
	bool hit_from_inside = false;
	bool has_hit = false;
	b3ShapeId shape_id = b3_nullShapeId;
	b3Pos point{};
	b3Vec3 normal{};
	float fraction = 1.0f;
};

float cast_result_fcn(b3ShapeId p_shape_id, b3Pos p_point, b3Vec3 p_normal, float p_fraction, uint64_t, int, int, void* p_context) {
	auto* ctx = static_cast<RayContext*>(p_context);

	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	Box3DShapedObjectImpl3D* object = nullptr;
	if (!should_report(b3Body_GetUserData(body_id), *ctx->filter, object)) {
		return -1.0f;
	}

	ctx->has_hit = true;
	ctx->shape_id = p_shape_id;
	ctx->point = p_point;
	ctx->normal = p_normal;
	ctx->fraction = p_fraction;
	return p_fraction;
}

int32_t find_shape_index(const Box3DShapedObjectImpl3D& p_object, b3ShapeId p_shape_id) {
	for (int32_t i = 0; i < p_object.get_shape_count(); i++) {
		if (p_object.has_shape_id(i) && B3_ID_EQUALS(p_object.get_shape_id(i), p_shape_id)) {
			return i;
		}
	}
	return -1;
}

struct MotionCollisionData {
	Box3DShapedObjectImpl3D* object = nullptr;
	Vector3 position;
	Vector3 normal;
	real_t depth = 0.0;
	float fraction = 1.0f;
	int32_t local_shape = -1;
	int32_t collider_shape = -1;
};

void append_motion_collision(LocalVector<MotionCollisionData>& r_collisions, const MotionCollisionData& p_collision) {
	if (p_collision.object == nullptr || p_collision.normal.length_squared() < CMP_EPSILON) {
		return;
	}
	for (MotionCollisionData& existing : r_collisions) {
		if (existing.object == p_collision.object && existing.local_shape == p_collision.local_shape &&
				existing.collider_shape == p_collision.collider_shape) {
			if (p_collision.depth > existing.depth || p_collision.fraction < existing.fraction) {
				existing = p_collision;
			}
			return;
		}
	}
	r_collisions.push_back(p_collision);
}

struct MotionCastContext {
	const Box3DQueryFilter3D* filter = nullptr;
	LocalVector<MotionCollisionData>* collisions = nullptr;
	bool has_hit = false;
	Box3DShapedObjectImpl3D* object = nullptr;
	b3Pos point{};
	b3Vec3 normal{};
	float fraction = 1.0f;
	int32_t local_shape = -1;
	int32_t collider_shape = -1;
};

float motion_cast_result_fcn(
		b3ShapeId p_shape_id,
		b3Pos p_point,
		b3Vec3 p_normal,
		float p_fraction,
		uint64_t,
		int,
		int,
		void* p_context) {
	auto* ctx = static_cast<MotionCastContext*>(p_context);

	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	Box3DShapedObjectImpl3D* object = nullptr;
	if (!should_report(b3Body_GetUserData(body_id), *ctx->filter, object)) {
		return -1.0f;
	}

	// Box3D reports a zero normal when the cast starts overlapped; recovery handles that case.
	if (p_fraction <= CMP_EPSILON && b3LengthSquared(p_normal) < 0.25f) {
		return -1.0f;
	}

	const int32_t collider_shape = find_shape_index(*object, p_shape_id);
	MotionCollisionData collision;
	collision.object = object;
	collision.position = b3_to_godot(p_point);
	collision.normal = b3_to_godot(p_normal);
	collision.fraction = p_fraction;
	collision.local_shape = ctx->local_shape;
	collision.collider_shape = collider_shape;
	append_motion_collision(*ctx->collisions, collision);

	if (ctx->has_hit && p_fraction >= ctx->fraction) {
		return ctx->fraction;
	}
	ctx->has_hit = true;
	ctx->object = object;
	ctx->point = p_point;
	ctx->normal = p_normal;
	ctx->fraction = p_fraction;
	ctx->collider_shape = collider_shape;
	return p_fraction;
}

b3AABB proxy_aabb(const b3ShapeProxy& p_proxy) {
	if (p_proxy.points == nullptr || p_proxy.count <= 0) {
		return b3AABB{b3Vec3_zero, b3Vec3_zero};
	}
	b3Vec3 lower = p_proxy.points[0];
	b3Vec3 upper = p_proxy.points[0];
	for (int32_t i = 1; i < p_proxy.count; i++) {
		lower = b3Min(lower, p_proxy.points[i]);
		upper = b3Max(upper, p_proxy.points[i]);
	}
	const b3Vec3 radius{p_proxy.radius, p_proxy.radius, p_proxy.radius};
	return b3AABB{b3Sub(lower, radius), b3Add(upper, radius)};
}

// Strip Box3D's speculative AABB inflation so recovery does not make characters hover.
b3AABB exact_shape_aabb(b3ShapeId p_shape_id) {
	b3AABB aabb = b3Shape_GetAABB(p_shape_id);
	const b3Vec3 inflation{B3_SPECULATIVE_DISTANCE, B3_SPECULATIVE_DISTANCE, B3_SPECULATIVE_DISTANCE};
	aabb.lowerBound = b3Add(aabb.lowerBound, inflation);
	aabb.upperBound = b3Sub(aabb.upperBound, inflation);
	return aabb;
}

struct RecoveryContext {
	const Box3DQueryFilter3D* filter = nullptr;
	b3AABB query_aabb{};
	Vector3 preferred_motion;
	Vector3* accumulated_push = nullptr;
	int32_t* push_count = nullptr;
	int32_t local_shape = -1;
	LocalVector<MotionCollisionData>* collisions = nullptr;
};

bool recovery_result_fcn(b3ShapeId p_shape_id, void* p_context) {
	auto* ctx = static_cast<RecoveryContext*>(p_context);
	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	Box3DShapedObjectImpl3D* object = nullptr;
	if (!should_report(b3Body_GetUserData(body_id), *ctx->filter, object)) {
		return true;
	}

	const b3ShapeType target_type = b3Shape_GetType(p_shape_id);
	if (target_type == b3_meshShape || target_type == b3_heightShape) {
		return true;
	}
	const b3AABB target_aabb = exact_shape_aabb(p_shape_id);
	const float depths[3] = {
		MIN(ctx->query_aabb.upperBound.x, target_aabb.upperBound.x) -
				MAX(ctx->query_aabb.lowerBound.x, target_aabb.lowerBound.x),
		MIN(ctx->query_aabb.upperBound.y, target_aabb.upperBound.y) -
				MAX(ctx->query_aabb.lowerBound.y, target_aabb.lowerBound.y),
		MIN(ctx->query_aabb.upperBound.z, target_aabb.upperBound.z) -
				MAX(ctx->query_aabb.lowerBound.z, target_aabb.lowerBound.z),
	};
	if (depths[0] <= CMP_EPSILON || depths[1] <= CMP_EPSILON || depths[2] <= CMP_EPSILON) {
		return true;
	}

	int32_t axis = 0;
	if (depths[1] < depths[axis]) {
		axis = 1;
	}
	if (depths[2] < depths[axis]) {
		axis = 2;
	}

	const b3Vec3 query_center = b3MulSV(0.5f, b3Add(ctx->query_aabb.lowerBound, ctx->query_aabb.upperBound));
	const b3Vec3 target_center = b3MulSV(0.5f, b3Add(target_aabb.lowerBound, target_aabb.upperBound));
	const float query_axis = axis == 0 ? query_center.x : axis == 1 ? query_center.y : query_center.z;
	const float target_axis = axis == 0 ? target_center.x : axis == 1 ? target_center.y : target_center.z;
	const real_t motion_axis = ctx->preferred_motion[axis];
	const float direction = Math::is_equal_approx(query_axis, target_axis) ?
			(motion_axis > 0.0 ? -1.0f : 1.0f) :
			(query_axis > target_axis ? 1.0f : -1.0f);

	Vector3 normal;
	normal[axis] = direction;
	const real_t depth = depths[axis];
	const Vector3 push = normal * (depth + B3_LINEAR_SLOP);
	const real_t existing_push = (*ctx->accumulated_push)[axis];
	const bool candidate_is_deeper = Math::abs(push[axis]) > Math::abs(existing_push);
	const bool candidate_better_opposes_motion =
			Math::is_equal_approx(Math::abs(push[axis]), Math::abs(existing_push)) &&
			push[axis] * motion_axis < existing_push * motion_axis;
	if (candidate_is_deeper || candidate_better_opposes_motion) {
		(*ctx->accumulated_push)[axis] = push[axis];
	}
	(*ctx->push_count)++;

	MotionCollisionData collision;
	collision.object = object;
	collision.position = b3_to_godot(b3Shape_GetClosestPoint(p_shape_id, query_center));
	collision.normal = normal;
	collision.depth = depth;
	collision.local_shape = ctx->local_shape;
	collision.collider_shape = find_shape_index(*object, p_shape_id);
	append_motion_collision(*ctx->collisions, collision);
	return true;
}

void fill_motion_collision(const MotionCollisionData& p_source, PhysicsServer3DExtensionMotionCollision& r_target) {
	r_target.position = p_source.position;
	r_target.normal = p_source.normal.normalized();
	r_target.depth = p_source.depth;
	r_target.local_shape = p_source.local_shape;
	r_target.collider = p_source.object->get_rid();
	r_target.collider_id = p_source.object->get_instance_id();
	r_target.collider_shape = p_source.collider_shape;

	auto* body = dynamic_cast<Box3DBodyImpl3D*>(p_source.object);
	if (body != nullptr) {
		r_target.collider_angular_velocity = body->get_angular_velocity();
		const Vector3 center_of_mass = body->get_transform().xform(body->get_center_of_mass());
		r_target.collider_velocity =
				body->get_linear_velocity() + body->get_angular_velocity().cross(p_source.position - center_of_mass);
	}
}

} // namespace

bool Box3DPhysicsDirectSpaceState3D::_intersect_ray(
		const Vector3& p_from,
		const Vector3& p_to,
		uint32_t p_collision_mask,
		bool p_collide_with_bodies,
		bool p_collide_with_areas,
		bool p_hit_from_inside,
		bool p_hit_back_faces,
		bool p_pick_ray,
		PhysicsServer3DExtensionRayResult* p_result) {
	ERR_FAIL_NULL_V(space, false);

	Box3DQueryFilter3D filter(p_collision_mask, p_collide_with_bodies, p_collide_with_areas);
	filter.direct_state = this;

	RayContext context;
	context.filter = &filter;
	context.hit_from_inside = p_hit_from_inside;

	const b3Vec3 origin = godot_to_b3(p_from);
	const b3Vec3 translation = godot_to_b3(p_to - p_from);

	b3World_CastRay(space->get_world_id(), origin, translation, filter.filter, cast_result_fcn, &context);

	if (!context.has_hit) {
		return false;
	}

	const b3BodyId body_id = b3Shape_GetBody(context.shape_id);
	auto* object = static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(body_id));
	if (object == nullptr) {
		return false;
	}

	p_result->position = b3_to_godot(context.point);
	p_result->normal = b3_to_godot(context.normal);
	p_result->rid = object->get_rid();
	p_result->collider_id = object->get_instance_id();
	p_result->shape = 0;
	return true;
}

int32_t Box3DPhysicsDirectSpaceState3D::_intersect_point(
		const Vector3& p_position,
		uint32_t p_collision_mask,
		bool p_collide_with_bodies,
		bool p_collide_with_areas,
		PhysicsServer3DExtensionShapeResult* p_results,
		int32_t p_max_results) {
	ERR_FAIL_NULL_V(space, 0);

	Box3DQueryFilter3D filter(p_collision_mask, p_collide_with_bodies, p_collide_with_areas);
	filter.direct_state = this;

	const b3Vec3 point = godot_to_b3(p_position);
	b3ShapeProxy proxy;
	proxy.points = &point;
	proxy.count = 1;
	proxy.radius = 0.0f;

	OverlapContext context;
	context.filter = &filter;
	context.results = p_results;
	context.max_results = p_max_results;

	b3World_OverlapShape(space->get_world_id(), b3Vec3_zero, &proxy, filter.filter, overlap_result_fcn, &context);

	return context.count;
}

int32_t Box3DPhysicsDirectSpaceState3D::_intersect_shape(
		const RID& p_shape_rid,
		const Transform3D& p_transform,
		const Vector3& p_motion,
		double p_margin,
		uint32_t p_collision_mask,
		bool p_collide_with_bodies,
		bool p_collide_with_areas,
		PhysicsServer3DExtensionShapeResult* p_results,
		int32_t p_max_results) {
	ERR_FAIL_NULL_V(space, 0);

	Box3DShapeImpl3D* shape = Box3DPhysicsServer3D::get_singleton()->get_shape(p_shape_rid);
	ERR_FAIL_NULL_V(shape, 0);

	const Box3DShapeProxy3D shape_proxy(shape, p_transform);
	if (!shape_proxy.is_supported()) {
		return 0;
	}

	Box3DQueryFilter3D filter(p_collision_mask, p_collide_with_bodies, p_collide_with_areas);
	filter.direct_state = this;

	OverlapContext context;
	context.filter = &filter;
	context.results = p_results;
	context.max_results = p_max_results;

	b3World_OverlapShape(space->get_world_id(), b3Vec3_zero, &shape_proxy.get_proxy(), filter.filter, overlap_result_fcn, &context);

	return context.count;
}

bool Box3DPhysicsDirectSpaceState3D::_cast_motion(
		const RID& p_shape_rid,
		const Transform3D& p_transform,
		const Vector3& p_motion,
		double p_margin,
		uint32_t p_collision_mask,
		bool p_collide_with_bodies,
		bool p_collide_with_areas,
		float* p_closest_safe,
		float* p_closest_unsafe,
		PhysicsServer3DExtensionShapeRestInfo* p_info) {
	ERR_FAIL_NULL_V(space, false);

	Box3DShapeImpl3D* shape = Box3DPhysicsServer3D::get_singleton()->get_shape(p_shape_rid);
	ERR_FAIL_NULL_V(shape, false);

	const Box3DShapeProxy3D shape_proxy(shape, p_transform);
	if (!shape_proxy.is_supported()) {
		*p_closest_safe = 1.0;
		*p_closest_unsafe = 1.0;
		return false;
	}

	Box3DQueryFilter3D filter(p_collision_mask, p_collide_with_bodies, p_collide_with_areas);
	filter.direct_state = this;

	RayContext context;
	context.filter = &filter;

	b3World_CastShape(space->get_world_id(), b3Vec3_zero, &shape_proxy.get_proxy(), godot_to_b3(p_motion), filter.filter, cast_result_fcn, &context);

	if (!context.has_hit) {
		*p_closest_safe = 1.0;
		*p_closest_unsafe = 1.0;
		return false;
	}

	*p_closest_safe = context.fraction;
	*p_closest_unsafe = context.fraction;
	return true;
}

bool Box3DPhysicsDirectSpaceState3D::_collide_shape(
		const RID& p_shape_rid,
		const Transform3D& p_transform,
		const Vector3& p_motion,
		double p_margin,
		uint32_t p_collision_mask,
		bool p_collide_with_bodies,
		bool p_collide_with_areas,
		void* p_results,
		int32_t p_max_results,
		int32_t* p_result_count) {
	*p_result_count = 0;
	ERR_FAIL_NULL_V(space, false);
	if (p_max_results <= 0) {
		return false;
	}

	Box3DShapeImpl3D* shape = Box3DPhysicsServer3D::get_singleton()->get_shape(p_shape_rid);
	ERR_FAIL_NULL_V(shape, false);

	const Box3DShapeProxy3D shape_proxy(shape, p_transform);
	if (!shape_proxy.is_supported()) {
		return false;
	}

	Box3DQueryFilter3D filter(p_collision_mask, p_collide_with_bodies, p_collide_with_areas);
	filter.direct_state = this;

	CollideShapeContext context;
	context.filter = &filter;
	context.query_proxy = &shape_proxy.get_proxy();
	context.results = static_cast<Vector3*>(p_results);
	context.max_results = p_max_results;

	b3World_OverlapShape(
			space->get_world_id(), b3Vec3_zero, &shape_proxy.get_proxy(), filter.filter, collide_shape_result_fcn, &context);

	*p_result_count = context.count;
	return context.count > 0;
}

bool Box3DPhysicsDirectSpaceState3D::_rest_info(
		const RID& p_shape_rid,
		const Transform3D& p_transform,
		const Vector3& p_motion,
		double p_margin,
		uint32_t p_collision_mask,
		bool p_collide_with_bodies,
		bool p_collide_with_areas,
		PhysicsServer3DExtensionShapeRestInfo* p_info) {
	ERR_FAIL_NULL_V(space, false);

	Box3DShapeImpl3D* shape = Box3DPhysicsServer3D::get_singleton()->get_shape(p_shape_rid);
	ERR_FAIL_NULL_V(shape, false);

	const Box3DShapeProxy3D shape_proxy(shape, p_transform);
	if (!shape_proxy.is_supported()) {
		return false;
	}

	Box3DQueryFilter3D filter(p_collision_mask, p_collide_with_bodies, p_collide_with_areas);
	filter.direct_state = this;

	RayContext context;
	context.filter = &filter;

	b3World_CastShape(space->get_world_id(), b3Vec3_zero, &shape_proxy.get_proxy(), godot_to_b3(p_motion), filter.filter, cast_result_fcn, &context);

	if (!context.has_hit) {
		return false;
	}

	const b3BodyId body_id = b3Shape_GetBody(context.shape_id);
	auto* object = static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(body_id));
	if (object == nullptr) {
		return false;
	}

	p_info->point = b3_to_godot(context.point);
	p_info->normal = b3_to_godot(context.normal);
	p_info->rid = object->get_rid();
	p_info->collider_id = object->get_instance_id();
	p_info->shape = 0;

	auto* body = dynamic_cast<Box3DBodyImpl3D*>(object);
	if (body != nullptr) {
		p_info->linear_velocity = body->get_linear_velocity();
	}

	return true;
}

Vector3 Box3DPhysicsDirectSpaceState3D::_get_closest_point_to_object_volume(const RID& p_object, const Vector3& p_point) const {
	Box3DShapedObjectImpl3D* object = Box3DPhysicsServer3D::get_singleton()->get_body(p_object);
	if (object == nullptr) {
		object = Box3DPhysicsServer3D::get_singleton()->get_area(p_object);
	}
	if (object == nullptr || !object->has_body_id()) {
		return p_point;
	}

	b3Vec3 result_point{};
	b3Body_GetClosestPoint(object->get_body_id(), &result_point, godot_to_b3(p_point));
	return b3_to_godot(result_point);
}

bool Box3DPhysicsDirectSpaceState3D::test_body_motion(
		Box3DShapedObjectImpl3D& p_body,
		const Transform3D& p_transform,
		const Vector3& p_motion,
		double p_margin,
		int32_t p_max_collisions,
		bool p_recovery_as_collision,
		PhysicsServer3DExtensionMotionResult* p_result) const {
	ERR_FAIL_NULL_V(space, false);
	ERR_FAIL_NULL_V(p_result, false);

	p_result->travel = Vector3();
	p_result->remainder = p_motion;
	p_result->collision_depth = 0.0f;
	p_result->collision_safe_fraction = 1.0f;
	p_result->collision_unsafe_fraction = 1.0f;
	p_result->collision_count = 0;

	if (p_body.get_shape_count() == 0) {
		p_result->travel = p_motion;
		p_result->remainder = Vector3();
		return false;
	}

	Box3DQueryFilter3D filter;
	filter.set_collision_mask(p_body.get_collision_mask());
	filter.exclude.insert(p_body.get_rid());
	if (auto* body = dynamic_cast<Box3DBodyImpl3D*>(&p_body)) {
		for (const KeyValue<RID, Box3DFilterJointImpl3D*>& entry : body->get_collision_exceptions()) {
			filter.exclude.insert(entry.key);
		}
	}

	const double margin = MAX(p_margin, 0.001);
	Transform3D recovered_transform = p_transform;
	LocalVector<MotionCollisionData> recovery_collisions;
	bool found_supported_shape = false;
	bool recovered = false;

	// CharacterBody3D starts each frame slightly embedded in the floor, so recover before casting.
	for (int32_t attempt = 0; attempt < 4; attempt++) {
		Vector3 accumulated_push;
		int32_t push_count = 0;

		for (int32_t i = 0; i < p_body.get_shape_count(); i++) {
			if (p_body.is_shape_disabled(i)) {
				continue;
			}
			Box3DShapeImpl3D* shape = p_body.get_shape(i);
			if (shape == nullptr) {
				continue;
			}
			const Box3DShapeProxy3D shape_proxy(shape, recovered_transform * p_body.get_shape_transform(i), margin);
			if (!shape_proxy.is_supported()) {
				continue;
			}
			found_supported_shape = true;

			RecoveryContext context;
			context.filter = &filter;
			context.query_aabb = proxy_aabb(shape_proxy.get_proxy());
			context.preferred_motion = p_motion;
			context.accumulated_push = &accumulated_push;
			context.push_count = &push_count;
			context.local_shape = i;
			context.collisions = &recovery_collisions;
			b3World_OverlapShape(
					space->get_world_id(),
					b3Vec3_zero,
					&shape_proxy.get_proxy(),
					filter.filter,
					recovery_result_fcn,
					&context);
		}

		if (push_count == 0 || accumulated_push.length_squared() <= CMP_EPSILON) {
			break;
		}
		recovered_transform.origin += accumulated_push;
		recovered = true;
	}

	bool found_collision = false;
	MotionCastContext best_context;
	best_context.filter = &filter;
	LocalVector<MotionCollisionData> cast_collisions;

	for (int32_t i = 0; i < p_body.get_shape_count(); i++) {
		if (p_body.is_shape_disabled(i)) {
			continue;
		}
		Box3DShapeImpl3D* shape = p_body.get_shape(i);
		if (shape == nullptr) {
			continue;
		}
		const Box3DShapeProxy3D shape_proxy(shape, recovered_transform * p_body.get_shape_transform(i), margin);
		if (!shape_proxy.is_supported()) {
			continue;
		}
		found_supported_shape = true;
		if (p_motion.is_zero_approx()) {
			continue;
		}

		MotionCastContext context;
		context.filter = &filter;
		context.collisions = &cast_collisions;
		context.local_shape = i;
		b3World_CastShape(
				space->get_world_id(),
				b3Vec3_zero,
				&shape_proxy.get_proxy(),
				godot_to_b3(p_motion),
				filter.filter,
				motion_cast_result_fcn,
				&context);
		if (context.has_hit && (!found_collision || context.fraction < best_context.fraction)) {
			best_context = context;
			found_collision = true;
		}
	}

	if (!found_supported_shape) {
		p_result->travel = p_motion;
		p_result->remainder = Vector3();
		return false;
	}

	const Vector3 recovery = recovered_transform.origin - p_transform.origin;
	const float unsafe_fraction = found_collision ? best_context.fraction : 1.0f;
	const float motion_length = (float)p_motion.length();
	const float safe_backoff_fraction = found_collision && motion_length > CMP_EPSILON ?
			MIN(1.0f, MAX((float)margin, B3_LINEAR_SLOP) / motion_length) :
			0.0f;
	const float safe_fraction = found_collision ? MAX(0.0f, unsafe_fraction - safe_backoff_fraction) : 1.0f;
	p_result->travel = recovery + p_motion * safe_fraction;
	p_result->remainder = p_motion * (1.0f - safe_fraction);
	p_result->collision_safe_fraction = safe_fraction;
	p_result->collision_unsafe_fraction = unsafe_fraction;

	LocalVector<MotionCollisionData> reported_collisions;
	if (found_collision && best_context.object != nullptr) {
		MotionCollisionData collision;
		collision.object = best_context.object;
		collision.position = b3_to_godot(best_context.point);
		collision.normal = b3_to_godot(best_context.normal);
		collision.fraction = best_context.fraction;
		collision.local_shape = best_context.local_shape;
		collision.collider_shape = best_context.collider_shape;
		append_motion_collision(reported_collisions, collision);
	}
	if (found_collision) {
		for (const MotionCollisionData& collision : cast_collisions) {
			if (collision.fraction <= unsafe_fraction + 0.001f) {
				append_motion_collision(reported_collisions, collision);
			}
		}
	}
	if (recovered && p_recovery_as_collision) {
		for (const MotionCollisionData& collision : recovery_collisions) {
			append_motion_collision(reported_collisions, collision);
		}
	}

	for (uint32_t i = 0; i < reported_collisions.size(); i++) {
		uint32_t best = i;
		for (uint32_t j = i + 1; j < reported_collisions.size(); j++) {
			if (reported_collisions[j].depth > reported_collisions[best].depth ||
					(Math::is_equal_approx(reported_collisions[j].depth, reported_collisions[best].depth) &&
							reported_collisions[j].fraction < reported_collisions[best].fraction)) {
				best = j;
			}
		}
		if (best != i) {
			SWAP(reported_collisions[i], reported_collisions[best]);
		}
	}

	const int32_t collision_limit = MAX(0, MIN(MIN(p_max_collisions, 32), (int32_t)reported_collisions.size()));
	for (int32_t i = 0; i < collision_limit; i++) {
		fill_motion_collision(reported_collisions[i], p_result->collisions[i]);
		p_result->collision_depth = MAX(p_result->collision_depth, reported_collisions[i].depth);
	}
	p_result->collision_count = collision_limit;

	return found_collision || (recovered && p_recovery_as_collision);
}
