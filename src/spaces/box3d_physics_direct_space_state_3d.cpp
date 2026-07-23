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

struct CollideShapeContext {
	const Box3DQueryFilter3D* filter = nullptr;
	const b3ShapeProxy* query_proxy = nullptr;
	Vector3* results = nullptr;
	int32_t max_pairs = 0;
	int32_t count = 0;
};

Box3DShapedObjectImpl3D* object_from_shape(b3ShapeId p_shape_id) {
	if (B3_IS_NULL(p_shape_id) || !b3Shape_IsValid(p_shape_id)) {
		return nullptr;
	}
	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	return static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(body_id));
}

bool should_report(b3ShapeId p_shape_id, const Box3DQueryFilter3D& p_filter, Box3DShapedObjectImpl3D*& r_object, int32_t& r_shape_index) {
	auto* object = object_from_shape(p_shape_id);
	if (object == nullptr) {
		return false;
	}
	if ((object->get_collision_layer() & p_filter.collision_mask) == 0) {
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
	if (p_filter.body_test_motion) {
		const Box3DPhysicsServer3D* server = Box3DPhysicsServer3D::get_singleton();
		if (server != nullptr &&
				(server->body_test_motion_is_excluding_body(object->get_rid()) || server->body_test_motion_is_excluding_object(object->get_instance_id()))) {
			return false;
		}
	}
	if (p_filter.pick_ray && !object->is_ray_pickable()) {
		return false;
	}
	r_object = object;
	r_shape_index = object->find_shape_index(p_shape_id);
	return true;
}

bool overlap_result_fcn(b3ShapeId p_shape_id, void* p_context) {
	auto* ctx = static_cast<OverlapContext*>(p_context);
	if (ctx->count >= ctx->max_results) {
		return false;
	}

	Box3DShapedObjectImpl3D* object = nullptr;
	int32_t shape_index = -1;
	if (!should_report(p_shape_id, *ctx->filter, object, shape_index)) {
		return true;
	}

	PhysicsServer3DExtensionShapeResult& result = ctx->results[ctx->count];
	result.rid = object->get_rid();
	result.collider_id = object->get_instance_id();
	result.collider = object->get_instance_unsafe();
	result.shape = shape_index;
	ctx->count++;
	return true;
}

b3Vec3 proxy_center(const b3ShapeProxy& p_proxy) {
	if (p_proxy.count <= 0 || p_proxy.points == nullptr) {
		return b3Vec3_zero;
	}

	b3Vec3 center = b3Vec3_zero;
	for (int32_t i = 0; i < p_proxy.count; i++) {
		center = b3Add(center, p_proxy.points[i]);
	}
	return b3MulSV(1.0f / (float)p_proxy.count, center);
}

b3Vec3 closest_point_on_aabb(const b3AABB& p_aabb, b3Vec3 p_point) {
	return b3Vec3{
		CLAMP(p_point.x, p_aabb.lowerBound.x, p_aabb.upperBound.x),
		CLAMP(p_point.y, p_aabb.lowerBound.y, p_aabb.upperBound.y),
		CLAMP(p_point.z, p_aabb.lowerBound.z, p_aabb.upperBound.z),
	};
}

bool make_distance_proxy_for_shape(
		b3ShapeId p_shape_id,
		b3ShapeProxy& r_proxy,
		b3Sphere& r_sphere,
		b3Capsule& r_capsule) {
	switch (b3Shape_GetType(p_shape_id)) {
		case b3_sphereShape:
			r_sphere = b3Shape_GetSphere(p_shape_id);
			r_proxy = b3ShapeProxy{ &r_sphere.center, 1, r_sphere.radius };
			return true;

		case b3_capsuleShape:
			r_capsule = b3Shape_GetCapsule(p_shape_id);
			r_proxy = b3ShapeProxy{ &r_capsule.center1, 2, r_capsule.radius };
			return true;

		case b3_hullShape: {
			const b3HullData* hull = b3Shape_GetHull(p_shape_id);
			const b3Vec3* points = hull != nullptr ? b3GetHullPoints(hull) : nullptr;
			if (hull == nullptr || points == nullptr || hull->vertexCount <= 0) {
				return false;
			}
			r_proxy = b3ShapeProxy{ points, hull->vertexCount, 0.0f };
			return true;
		}

		default:
			return false;
	}
}

bool get_contact_pair(
		b3ShapeId p_shape_id,
		const b3ShapeProxy& p_query_proxy,
		Vector3& r_query_point,
		Vector3& r_collider_point) {
	b3ShapeProxy shape_proxy{};
	b3Sphere sphere{};
	b3Capsule capsule{};
	if (!make_distance_proxy_for_shape(p_shape_id, shape_proxy, sphere, capsule)) {
		const b3Vec3 query_center = proxy_center(p_query_proxy);
		r_query_point = b3_to_godot(query_center);
		r_collider_point = b3_to_godot(closest_point_on_aabb(b3Shape_GetAABB(p_shape_id), query_center));
		return false;
	}

	const b3Transform shape_transform = b3Body_GetTransform(b3Shape_GetBody(p_shape_id));

	b3DistanceInput input{};
	input.proxyA = shape_proxy;
	input.proxyB = p_query_proxy;
	input.transform = b3InvMulTransforms(shape_transform, b3Transform_identity);
	input.useRadii = true;

	b3SimplexCache cache{};
	const b3DistanceOutput output = b3ShapeDistance(&input, &cache, nullptr, 0);

	r_query_point = b3_to_godot(b3TransformPoint(shape_transform, output.pointB));
	r_collider_point = b3_to_godot(b3TransformPoint(shape_transform, output.pointA));
	return true;
}

bool collide_shape_result_fcn(b3ShapeId p_shape_id, void* p_context) {
	auto* ctx = static_cast<CollideShapeContext*>(p_context);
	if (ctx->count >= ctx->max_pairs) {
		return false;
	}

	Box3DShapedObjectImpl3D* object = nullptr;
	int32_t shape_index = -1;
	if (!should_report(p_shape_id, *ctx->filter, object, shape_index)) {
		return true;
	}

	Vector3 query_point;
	Vector3 collider_point;
	get_contact_pair(p_shape_id, *ctx->query_proxy, query_point, collider_point);

	const int32_t point_index = ctx->count * 2;
	ctx->results[point_index] = query_point;
	ctx->results[point_index + 1] = collider_point;
	ctx->count++;

	return ctx->count < ctx->max_pairs;
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
		if (existing.object == p_collision.object &&
				existing.local_shape == p_collision.local_shape &&
				existing.collider_shape == p_collision.collider_shape) {
			if (p_collision.depth > existing.depth || p_collision.fraction < existing.fraction) {
				existing = p_collision;
			}
			return;
		}
	}
	r_collisions.push_back(p_collision);
}

struct RayContext {
	const Box3DQueryFilter3D* filter = nullptr;
	LocalVector<MotionCollisionData>* motion_collisions = nullptr;
	bool hit_from_inside = false;
	bool reject_invalid_initial_overlap = false;
	bool has_hit = false;
	b3ShapeId shape_id = b3_nullShapeId;
	Box3DShapedObjectImpl3D* object = nullptr;
	b3Pos point{};
	b3Vec3 normal{};
	float fraction = 1.0f;
	int32_t shape_index = -1;
	int32_t face_index = -1;
	int32_t local_shape = -1;
};

float cast_result_fcn(b3ShapeId p_shape_id, b3Pos p_point, b3Vec3 p_normal, float p_fraction, uint64_t, int p_triangle_index, int, void* p_context) {
	auto* ctx = static_cast<RayContext*>(p_context);

	Box3DShapedObjectImpl3D* object = nullptr;
	int32_t shape_index = -1;
	if (!should_report(p_shape_id, *ctx->filter, object, shape_index)) {
		return -1.0f;
	}
	if (ctx->reject_invalid_initial_overlap && p_fraction <= CMP_EPSILON && b3LengthSquared(p_normal) < 0.25f) {
		return -1.0f;
	}
	if (ctx->motion_collisions != nullptr) {
		MotionCollisionData collision;
		collision.object = object;
		collision.position = b3_to_godot(p_point);
		collision.normal = b3_to_godot(p_normal);
		collision.fraction = p_fraction;
		collision.local_shape = ctx->local_shape;
		collision.collider_shape = shape_index;
		append_motion_collision(*ctx->motion_collisions, collision);
	}
	if (ctx->has_hit && p_fraction >= ctx->fraction) {
		return ctx->fraction;
	}

	ctx->has_hit = true;
	ctx->shape_id = p_shape_id;
	ctx->object = object;
	ctx->point = p_point;
	ctx->normal = p_normal;
	ctx->fraction = p_fraction;
	ctx->shape_index = shape_index;
	ctx->face_index = p_triangle_index;
	return p_fraction;
}

b3AABB proxy_aabb(const b3ShapeProxy& p_proxy) {
	if (p_proxy.points == nullptr || p_proxy.count <= 0) {
		return b3AABB{ b3Vec3_zero, b3Vec3_zero };
	}
	b3Vec3 lower = p_proxy.points[0];
	b3Vec3 upper = p_proxy.points[0];
	for (int32_t i = 1; i < p_proxy.count; i++) {
		lower = b3Min(lower, p_proxy.points[i]);
		upper = b3Max(upper, p_proxy.points[i]);
	}
	const b3Vec3 radius{ p_proxy.radius, p_proxy.radius, p_proxy.radius };
	return b3AABB{ b3Sub(lower, radius), b3Add(upper, radius) };
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
	Box3DShapedObjectImpl3D* object = nullptr;
	int32_t collider_shape = -1;
	if (!should_report(p_shape_id, *ctx->filter, object, collider_shape)) {
		return true;
	}

	// OverlapShape performs the exact convex overlap test first. Box3D does not expose
	// an EPA-style penetration vector here, so use the minimum AABB overlap axis as a
	// conservative depenetration direction for the confirmed overlapping pair.
	const b3AABB target_aabb = b3Shape_GetAABB(p_shape_id);
	const float depths[3] = {
		MIN(ctx->query_aabb.upperBound.x, target_aabb.upperBound.x) - MAX(ctx->query_aabb.lowerBound.x, target_aabb.lowerBound.x),
		MIN(ctx->query_aabb.upperBound.y, target_aabb.upperBound.y) - MAX(ctx->query_aabb.lowerBound.y, target_aabb.lowerBound.y),
		MIN(ctx->query_aabb.upperBound.z, target_aabb.upperBound.z) - MAX(ctx->query_aabb.lowerBound.z, target_aabb.lowerBound.z),
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
	const float motion_axis = axis == 0 ? (float)ctx->preferred_motion.x : axis == 1 ? (float)ctx->preferred_motion.y : (float)ctx->preferred_motion.z;
	const float direction = Math::is_equal_approx(query_axis, target_axis) ? (motion_axis > 0.0f ? -1.0f : 1.0f) : (query_axis > target_axis ? 1.0f : -1.0f);
	Vector3 normal;
	normal[axis] = direction;
	const real_t depth = depths[axis];
	*ctx->accumulated_push += normal * (depth + 0.0005f);
	(*ctx->push_count)++;

	MotionCollisionData collision;
	collision.object = object;
	collision.position = b3_to_godot(closest_point_on_aabb(target_aabb, query_center));
	collision.normal = normal;
	collision.depth = depth;
	collision.local_shape = ctx->local_shape;
	collision.collider_shape = collider_shape;
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
		const Vector3 relative_position = p_source.position - center_of_mass;
		r_target.collider_velocity = body->get_linear_velocity() + body->get_angular_velocity().cross(relative_position);
	}
}

void fill_rest_info(const RayContext& p_context, PhysicsServer3DExtensionShapeRestInfo* p_info) {
	if (p_info == nullptr || p_context.object == nullptr) {
		return;
	}
	p_info->point = b3_to_godot(p_context.point);
	p_info->normal = b3_to_godot(p_context.normal);
	p_info->rid = p_context.object->get_rid();
	p_info->collider_id = p_context.object->get_instance_id();
	p_info->shape = p_context.shape_index;

	auto* body = dynamic_cast<Box3DBodyImpl3D*>(p_context.object);
	if (body != nullptr) {
		p_info->linear_velocity = body->get_linear_velocity();
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
	filter.pick_ray = p_pick_ray;
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

	if (context.object == nullptr) {
		return false;
	}

	p_result->position = b3_to_godot(context.point);
	p_result->normal = b3_to_godot(context.normal);
	p_result->rid = context.object->get_rid();
	p_result->collider_id = context.object->get_instance_id();
	p_result->collider = context.object->get_instance_unsafe();
	p_result->shape = context.shape_index;
	p_result->face_index = context.face_index;
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

	const Box3DShapeProxy3D shape_proxy(shape, p_transform, p_margin);
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

	const Box3DShapeProxy3D shape_proxy(shape, p_transform, p_margin);
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
	fill_rest_info(context, p_info);
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
	ERR_FAIL_NULL_V(space, false);

	if (p_result_count != nullptr) {
		*p_result_count = 0;
	}
	if (p_results == nullptr || p_max_results <= 0) {
		return false;
	}

	Box3DShapeImpl3D* shape = Box3DPhysicsServer3D::get_singleton()->get_shape(p_shape_rid);
	ERR_FAIL_NULL_V(shape, false);

	const Box3DShapeProxy3D shape_proxy(shape, p_transform, p_margin);
	if (!shape_proxy.is_supported()) {
		return false;
	}

	Box3DQueryFilter3D filter(p_collision_mask, p_collide_with_bodies, p_collide_with_areas);
	filter.direct_state = this;

	CollideShapeContext context;
	context.filter = &filter;
	context.query_proxy = &shape_proxy.get_proxy();
	context.results = static_cast<Vector3*>(p_results);
	context.max_pairs = p_max_results;

	b3World_OverlapShape(space->get_world_id(), b3Vec3_zero, &shape_proxy.get_proxy(), filter.filter, collide_shape_result_fcn, &context);

	if (p_result_count != nullptr) {
		*p_result_count = context.count;
	}
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

	const Box3DShapeProxy3D shape_proxy(shape, p_transform, p_margin);
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

	if (context.object == nullptr) {
		return false;
	}

	fill_rest_info(context, p_info);
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
	filter.body_test_motion = true;
	filter.exclude.insert(p_body.get_rid());
	if (auto* body = dynamic_cast<Box3DBodyImpl3D*>(&p_body)) {
		for (const RID& exception : body->get_collision_exceptions()) {
			filter.exclude.insert(exception);
		}
	}

	const double margin = MAX(p_margin, 0.001);
	Transform3D recovered_transform = p_transform;
	LocalVector<MotionCollisionData> recovery_collisions;
	bool found_supported_shape = false;
	bool recovered = false;

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
			b3World_OverlapShape(space->get_world_id(), b3Vec3_zero, &shape_proxy.get_proxy(), filter.filter, recovery_result_fcn, &context);
		}

		if (push_count == 0) {
			break;
		}
		const Vector3 recovery_step = accumulated_push / (real_t)push_count;
		if (recovery_step.length_squared() <= CMP_EPSILON) {
			break;
		}
		recovered_transform.origin += recovery_step;
		recovered = true;
	}

	RayContext best_context;
	best_context.filter = &filter;
	best_context.reject_invalid_initial_overlap = true;
	int32_t best_local_shape = -1;
	bool found_collision = false;
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

		RayContext context;
		context.filter = &filter;
		context.motion_collisions = &cast_collisions;
		context.reject_invalid_initial_overlap = true;
		context.local_shape = i;

		if (!p_motion.is_zero_approx()) {
			b3World_CastShape(space->get_world_id(), b3Vec3_zero, &shape_proxy.get_proxy(), godot_to_b3(p_motion), filter.filter, cast_result_fcn, &context);
		}

		if (context.has_hit && (!found_collision || context.fraction < best_context.fraction)) {
			best_context = context;
			best_local_shape = i;
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
	const float safe_fraction = found_collision ? MAX(0.0f, unsafe_fraction - 0.001f) : 1.0f;
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
		collision.local_shape = best_local_shape;
		collision.collider_shape = best_context.shape_index;
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
					(Math::is_equal_approx(reported_collisions[j].depth, reported_collisions[best].depth) && reported_collisions[j].fraction < reported_collisions[best].fraction)) {
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
