#include "box3d_physics_direct_space_state_3d.hpp"

#include "../misc/box3d_shape_proxy.hpp"
#include "../misc/box3d_recovery_geometry.hpp"
#include "../misc/type_conversions.hpp"
#include "../objects/box3d_area_impl_3d.hpp"
#include "../objects/box3d_body_impl_3d.hpp"
#include "../objects/box3d_shaped_object_impl_3d.hpp"
#include "../servers/box3d_physics_server_3d.hpp"
#include "../shapes/box3d_concave_polygon_shape_impl_3d.hpp"
#include "../shapes/box3d_separation_ray_shape_impl_3d.hpp"
#include "../shapes/box3d_shape_impl_3d.hpp"
#include "box3d_query_filter_3d.hpp"
#include "box3d_space_3d.hpp"

#include <box3d/box3d.h>
#include <box3d/collision.h>

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
	if (p_filter.should_exclude(object->get_rid(), object->get_instance_id())) {
		return false;
	}
	r_object = object;
	return true;
}

// The engine reads result colliders as its own Object, not the godot-cpp wrapper ObjectDB returns.
Object* engine_object(uint64_t p_instance_id) {
	return reinterpret_cast<Object*>(internal::gdextension_interface_object_get_instance_from_id(p_instance_id));
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
	result.collider = engine_object(result.collider_id);
	result.shape = object->find_shape_index(p_shape_id);
	ctx->count++;
	return true;
}

struct ContactQueryContext {
	const Box3DQueryFilter3D* filter = nullptr;
	const Box3DContactQuery3D* geometry = nullptr;
	b3Transform transform = b3Transform_identity;
	float margin = 0.0f;
	Vector3* results = nullptr;
	PhysicsServer3DExtensionShapeRestInfo* rest = nullptr;
	float best_separation = FLT_MAX;
	int32_t max_results = 0;
	int32_t count = 0;
};

bool contact_query_result_fcn(b3ShapeId p_shape_id, void* p_context) {
	auto* ctx = static_cast<ContactQueryContext*>(p_context);
	if (ctx->rest == nullptr && ctx->count >= ctx->max_results) {
		return false;
	}

	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	Box3DShapedObjectImpl3D* object = nullptr;
	if (!should_report(b3Body_GetUserData(body_id), *ctx->filter, object)) {
		return true;
	}

	LocalVector<Box3DRecoveryContact> contacts;
	ctx->geometry->contacts(ctx->transform, p_shape_id, ctx->margin, contacts);
	for (const Box3DRecoveryContact& contact : contacts) {
		// Native manifolds store the midpoint and signed surface separation.
		const b3Vec3 query_point = b3MulAdd(contact.point, 0.5f * contact.separation, contact.normal);
		const b3Vec3 target_point = b3MulAdd(contact.point, -0.5f * contact.separation, contact.normal);
		if (ctx->rest != nullptr) {
			if (contact.separation >= ctx->best_separation) {
				continue;
			}
			ctx->best_separation = contact.separation;
			ctx->rest->point = b3_to_godot(target_point);
			ctx->rest->normal = b3_to_godot(contact.normal);
			ctx->rest->rid = object->get_rid();
			ctx->rest->collider_id = object->get_instance_id();
			ctx->rest->shape = object->find_shape_index(p_shape_id);
			ctx->rest->linear_velocity = dynamic_cast<Box3DBodyImpl3D*>(object) ?
					b3_to_godot(b3Body_GetWorldPointVelocity(body_id, target_point)) : Vector3();
			ctx->count = 1;
			continue;
		}
		ctx->results[ctx->count * 2 + 0] = b3_to_godot(query_point);
		ctx->results[ctx->count * 2 + 1] = b3_to_godot(target_point);
		ctx->count++;
		if (ctx->count >= ctx->max_results) {
			return false;
		}
	}
	return true;
}

b3AABB proxy_bounds(const b3ShapeProxy& p_proxy, float p_margin) {
	b3Vec3 lower = p_proxy.points[0];
	b3Vec3 upper = lower;
	for (int i = 1; i < p_proxy.count; i++) {
		lower = b3Min(lower, p_proxy.points[i]);
		upper = b3Max(upper, p_proxy.points[i]);
	}
	const float radius = p_proxy.radius + p_margin;
	const b3Vec3 inflation{radius, radius, radius};
	return {b3Sub(lower, inflation), b3Add(upper, inflation)};
}

struct RayContext {
	const Box3DQueryFilter3D* filter = nullptr;
	bool hit_from_inside = true;
	bool has_hit = false;
	b3ShapeId shape_id = b3_nullShapeId;
	b3Pos point{};
	b3Vec3 normal{};
	float fraction = 1.0f;
	int32_t face_index = -1;
	b3Vec3 origin{};
	b3Vec3 translation{};
};

int32_t input_face_index(b3ShapeId p_shape_id, Box3DShapedObjectImpl3D* p_object, int p_triangle) {
	const int32_t slot = p_object->find_shape_index(p_shape_id);
	if (p_triangle < 0 || slot < 0 || p_object->get_shape(slot)->get_type() != PhysicsServer3D::SHAPE_CONCAVE_POLYGON) {
		return -1;
	}
	const b3Mesh mesh = b3Shape_GetMesh(p_shape_id);
	// BVH construction reorders triangles; unwelded vertex indices preserve input face groups.
	return b3GetMeshTriangles(mesh.data)[p_triangle].index1 / 3;
}

float cast_result_fcn(b3ShapeId p_shape_id, b3Pos p_point, b3Vec3 p_normal, float p_fraction, uint64_t, int p_triangle, int, void* p_context) {
	auto* ctx = static_cast<RayContext*>(p_context);

	const b3BodyId body_id = b3Shape_GetBody(p_shape_id);
	Box3DShapedObjectImpl3D* object = nullptr;
	if (!should_report(b3Body_GetUserData(body_id), *ctx->filter, object)) {
		return -1.0f;
	}
	if (!ctx->hit_from_inside && p_fraction == 0.0f && b3LengthSquared(p_normal) < 1e-10f) {
		return -1.0f;
	}

	ctx->has_hit = true;
	ctx->shape_id = p_shape_id;
	ctx->point = p_point;
	ctx->normal = p_normal;
	ctx->fraction = p_fraction;
	ctx->face_index = input_face_index(p_shape_id, object, p_triangle);
	return p_fraction;
}

struct MeshRayContext {
	RayContext* ray;
	Box3DShapedObjectImpl3D* object;
	b3ShapeId shape;
	b3Transform transform;
	b3Vec3 origin;
	b3Vec3 translation;
};

bool mesh_ray_triangle(b3Vec3 p_a, b3Vec3 p_b, b3Vec3 p_c, int p_triangle, void* p_context) {
	auto& ctx = *static_cast<MeshRayContext*>(p_context);
	const b3Vec3 edge1 = b3Sub(p_b, p_a);
	const b3Vec3 edge2 = b3Sub(p_c, p_a);
	const b3Vec3 cross = b3Cross(ctx.translation, edge2);
	const float determinant = b3Dot(edge1, cross);
	if (Math::abs(determinant) < 1e-8f) {
		return true;
	}
	const b3Vec3 offset = b3Sub(ctx.origin, p_a);
	const float u = b3Dot(offset, cross) / determinant;
	const b3Vec3 q = b3Cross(offset, edge1);
	const float v = b3Dot(ctx.translation, q) / determinant;
	const float fraction = b3Dot(edge2, q) / determinant;
	if (u < 0.0f || v < 0.0f || u + v > 1.0f || fraction < 0.0f || fraction > ctx.ray->fraction) {
		return true;
	}
	b3Vec3 normal = b3Normalize(b3Cross(edge2, edge1));
	if (b3Dot(normal, ctx.translation) > 0.0f) {
		normal = b3Neg(normal);
	}
	ctx.ray->has_hit = true;
	ctx.ray->shape_id = ctx.shape;
	ctx.ray->fraction = fraction;
	ctx.ray->point = b3MulAdd(ctx.ray->origin, fraction, ctx.ray->translation);
	ctx.ray->normal = b3RotateVector(ctx.transform.q, normal);
	ctx.ray->face_index = input_face_index(ctx.shape, ctx.object, p_triangle);
	return true;
}

bool backface_ray_result_fcn(b3ShapeId p_shape_id, void* p_context) {
	auto& ray = *static_cast<RayContext*>(p_context);
	const b3ShapeType native_type = b3Shape_GetType(p_shape_id);
	if (native_type != b3_meshShape && native_type != b3_heightShape) {
		return true;
	}
	Box3DShapedObjectImpl3D* object = nullptr;
	const b3BodyId body = b3Shape_GetBody(p_shape_id);
	if (!should_report(b3Body_GetUserData(body), *ray.filter, object)) {
		return true;
	}
	const int32_t slot = object->find_shape_index(p_shape_id);
	if (slot < 0) {
		return true;
	}
	const auto* shape = object->get_shape(slot);
	if (shape->get_type() == PhysicsServer3D::SHAPE_CONCAVE_POLYGON) {
		if (!static_cast<const Box3DConcavePolygonShapeImpl3D*>(shape)->is_backface_collision_enabled()) {
			return true;
		}
	} else if (shape->get_type() != PhysicsServer3D::SHAPE_HEIGHTMAP) {
		return true;
	}
	const b3Transform transform = b3Body_GetTransform(body);
	MeshRayContext ctx{&ray, object, p_shape_id, transform,
			b3InvTransformPoint(transform, ray.origin), b3InvRotateVector(transform.q, ray.translation)};
	const b3Vec3 end = b3MulAdd(ctx.origin, ray.fraction, ctx.translation);
	const b3AABB bounds{b3Min(ctx.origin, end), b3Max(ctx.origin, end)};
	if (native_type == b3_meshShape) {
		const b3Mesh mesh = b3Shape_GetMesh(p_shape_id);
		b3QueryMesh(&mesh, bounds, mesh_ray_triangle, &ctx);
	} else {
		b3QueryHeightField(b3Shape_GetHeightField(p_shape_id), bounds, mesh_ray_triangle, &ctx);
	}
	return true;
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
				existing.collider_shape == p_collision.collider_shape &&
				existing.normal.dot(p_collision.normal) > 0.99) {
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
	b3ShapeId query_shape = b3_nullShapeId;
	b3Transform query_transform{};
	b3Vec3 motion{};
	float margin = 0.0f;
	bool override_normal = false;
	Vector3 normal_override;
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

	float fraction = p_fraction;
	b3Vec3 normal = p_normal;
	b3Pos point = p_point;
	if (B3_IS_NON_NULL(ctx->query_shape)) {
		// Box3D casts hit anything within linear slop, which snags coplanar seams; Godot hits on touch.
		Box3DRecoveryContact contact{};
		fraction = box3d_cast_to_contact(
				ctx->query_shape, ctx->query_transform, ctx->motion, p_shape_id, ctx->margin, contact);
		if (fraction < 0.0f) {
			return -1.0f;
		}
		normal = contact.normal;
		point = contact.point;
	} else if (!ctx->override_normal && p_fraction <= CMP_EPSILON && b3LengthSquared(p_normal) < 0.25f) {
		return -1.0f;
	}

	const int32_t collider_shape = object->find_shape_index(p_shape_id);
	MotionCollisionData collision;
	collision.object = object;
	collision.position = b3_to_godot(point);
	collision.normal = ctx->override_normal ? ctx->normal_override : b3_to_godot(normal);
	collision.fraction = fraction;
	collision.local_shape = ctx->local_shape;
	collision.collider_shape = collider_shape;
	append_motion_collision(*ctx->collisions, collision);

	if (ctx->has_hit && fraction >= ctx->fraction) {
		return ctx->fraction;
	}
	ctx->has_hit = true;
	ctx->object = object;
	ctx->point = point;
	ctx->normal = ctx->override_normal ? godot_to_b3(ctx->normal_override) : normal;
	ctx->fraction = fraction;
	ctx->collider_shape = collider_shape;
	return fraction;
}

struct RecoveryPlane {
	b3CollisionPlane plane{};
	real_t priority = 1.0;
	bool immovable = true;
};

struct RecoveryContext {
	const Box3DQueryFilter3D* filter = nullptr;
	b3ShapeId query_shape = b3_nullShapeId;
	b3Transform query_transform{};
	float margin = 0.0f;
	int32_t local_shape = -1;
	LocalVector<RecoveryPlane>* planes = nullptr;
	LocalVector<MotionCollisionData>* collisions = nullptr;
};

void append_recovery_plane(LocalVector<RecoveryPlane>& r_planes, const Vector3& p_normal,
		real_t p_depth, const Box3DBodyImpl3D& p_body) {
	const bool immovable = p_body.get_mode() == PhysicsServer3D::BODY_MODE_STATIC ||
			p_body.get_mode() == PhysicsServer3D::BODY_MODE_KINEMATIC;
	const real_t priority = p_body.get_collision_priority();
	// Co-planar triangle/manifold points describe one translation constraint.
	for (RecoveryPlane& existing : r_planes) {
		if (existing.immovable == immovable && Math::is_equal_approx(existing.priority, priority) &&
				b3Dot(existing.plane.plane.normal, godot_to_b3(p_normal)) > 0.9999f) {
			existing.plane.plane.offset = MAX(existing.plane.plane.offset, (float)p_depth + B3_LINEAR_SLOP);
			existing.plane.pushLimit = immovable ? FLT_MAX : MAX(existing.plane.pushLimit, (float)p_depth);
			return;
		}
	}
	RecoveryPlane entry;
	entry.immovable = immovable;
	entry.priority = priority;
	entry.plane.plane = {godot_to_b3(p_normal), (float)p_depth + B3_LINEAR_SLOP};
	// A movable obstacle may overlap when a character is trapped under a load.
	// Bound its push rather than allowing it to accumulate against an immovable
	// support forever. Queries never move or apply impulses to that obstacle.
	entry.plane.pushLimit = immovable ? FLT_MAX : MAX(0.0f, (float)p_depth);
	r_planes.push_back(entry);
}

bool recovery_result_fcn(b3ShapeId p_shape_id, void* p_context) {
	auto* ctx = static_cast<RecoveryContext*>(p_context);
	Box3DShapedObjectImpl3D* object = nullptr;
	if (!should_report(b3Body_GetUserData(b3Shape_GetBody(p_shape_id)), *ctx->filter, object)) {
		return true;
	}
	auto* body = dynamic_cast<Box3DBodyImpl3D*>(object);
	if (body == nullptr) {
		return true;
	}
	LocalVector<Box3DRecoveryContact> contacts;
	box3d_recovery_contacts(ctx->query_shape, ctx->query_transform, p_shape_id, ctx->margin, contacts);
	for (const Box3DRecoveryContact& contact : contacts) {
		const real_t depth = MAX(0.0f, ctx->margin - contact.separation);
		const Vector3 normal = b3_to_godot(contact.normal).normalized();
		append_recovery_plane(*ctx->planes, normal, depth, *body);
		MotionCollisionData collision;
		collision.object = object;
		collision.position = b3_to_godot(contact.point);
		collision.normal = normal;
		collision.depth = depth;
		collision.local_shape = ctx->local_shape;
		collision.collider_shape = object->find_shape_index(p_shape_id);
		append_motion_collision(*ctx->collisions, collision);
	}
	return true;
}

Vector3 solve_recovery(LocalVector<RecoveryPlane>& p_planes) {
	// Resolve movable contacts first; supporting static/kinematic surfaces have
	// the final say. Within a class, higher Godot collision priorities win when
	// no translation can satisfy all planes (for example a too-narrow passage).
	for (uint32_t i = 1; i < p_planes.size(); i++) {
		uint32_t j = i;
		while (j > 0 && (p_planes[j - 1].immovable > p_planes[j].immovable ||
				(p_planes[j - 1].immovable == p_planes[j].immovable &&
						p_planes[j - 1].priority > p_planes[j].priority))) {
			SWAP(p_planes[j - 1], p_planes[j]);
			j--;
		}
	}
	LocalVector<b3CollisionPlane> planes;
	for (const RecoveryPlane& entry : p_planes) {
		planes.push_back(entry.plane);
	}
	return b3_to_godot(b3SolvePlanes(b3Vec3_zero, planes.ptr(), (int)planes.size()).delta);
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
	context.origin = origin;
	context.translation = translation;

	b3World_CastRay(space->get_world_id(), origin, translation, filter.filter, cast_result_fcn, &context);
	if (p_hit_back_faces) {
		const b3Vec3 end = b3MulAdd(origin, context.fraction, translation);
		b3World_OverlapAABB(space->get_world_id(), {b3Min(origin, end), b3Max(origin, end)},
				filter.filter, backface_ray_result_fcn, &context);
	}

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
	p_result->collider = engine_object(p_result->collider_id);
	p_result->shape = object->find_shape_index(context.shape_id);
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

	const float motion_length = (float)p_motion.length();
	*p_closest_safe = motion_length > CMP_EPSILON ?
			MAX(0.0f, context.fraction - B3_LINEAR_SLOP / motion_length) : context.fraction;
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

	const Box3DShapeProxy3D local_proxy(shape, Transform3D());
	const Box3DContactQuery3D geometry(local_proxy.get_proxy());
	ContactQueryContext context;
	context.filter = &filter;
	context.geometry = &geometry;
	context.transform = godot_to_b3_transform(p_transform);
	context.margin = MAX(0.0f, (float)p_margin);
	context.results = static_cast<Vector3*>(p_results);
	context.max_results = p_max_results;

	b3World_OverlapAABB(space->get_world_id(), proxy_bounds(shape_proxy.get_proxy(), context.margin),
			filter.filter, contact_query_result_fcn, &context);

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

	const Box3DShapeProxy3D local_proxy(shape, Transform3D());
	const Box3DContactQuery3D geometry(local_proxy.get_proxy());
	ContactQueryContext context;
	context.filter = &filter;
	context.geometry = &geometry;
	context.transform = godot_to_b3_transform(p_transform);
	context.margin = MAX(0.0f, (float)p_margin);
	context.rest = p_info;

	b3World_OverlapAABB(space->get_world_id(), proxy_bounds(shape_proxy.get_proxy(), context.margin),
			filter.filter, contact_query_result_fcn, &context);
	return context.count > 0;
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
		bool p_collide_separation_ray,
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
	filter.motion_server = Box3DPhysicsServer3D::get_singleton();
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
		LocalVector<RecoveryPlane> planes;

		for (int32_t i = 0; i < p_body.get_shape_count(); i++) {
			if (p_body.is_shape_disabled(i)) {
				continue;
			}
			Box3DShapeImpl3D* shape = p_body.get_shape(i);
			if (shape == nullptr) {
				continue;
			}
			if (shape->get_type() == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
				const auto* ray = static_cast<const Box3DSeparationRayShapeImpl3D*>(shape);
				found_supported_shape = true;
				const Transform3D shape_transform = recovered_transform * p_body.get_shape_transform(i);
				const Vector3 ray_vector = shape_transform.basis.get_column(2) * ray->get_length();
				const real_t ray_length = ray_vector.length();
				if (ray_length <= CMP_EPSILON) {
					continue;
				}
				const Vector3 ray_direction = ray_vector / ray_length;
				RayContext ray_context;
				ray_context.filter = &filter;
				b3World_CastRay(space->get_world_id(), godot_to_b3(shape_transform.origin),
						godot_to_b3(ray_direction * (ray_length + margin)), filter.filter, cast_result_fcn, &ray_context);
				if (!ray_context.has_hit) {
					continue;
				}
				const Vector3 surface_normal = b3_to_godot(ray_context.normal).normalized();
				const real_t facing = surface_normal.dot(-ray_direction);
				const real_t penetration = (ray_length + margin) * (1.0 - ray_context.fraction) - margin;
				if (facing <= CMP_EPSILON || penetration <= CMP_EPSILON) {
					continue;
				}
				const Vector3 normal = ray->get_slide_on_slope() ? surface_normal : -ray_direction;
				const real_t depth = ray->get_slide_on_slope() ? penetration * facing : penetration;
				auto* object = dynamic_cast<Box3DBodyImpl3D*>(
						static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(b3Shape_GetBody(ray_context.shape_id))));
				if (object != nullptr) {
					append_recovery_plane(planes, normal, depth, *object);
					MotionCollisionData collision;
					collision.object = object;
					collision.position = b3_to_godot(ray_context.point);
					collision.normal = normal;
					collision.depth = depth;
					collision.local_shape = i;
					collision.collider_shape = object->find_shape_index(ray_context.shape_id);
					append_motion_collision(recovery_collisions, collision);
				}
				continue;
			}
			if (!p_body.has_shape_id(i)) {
				continue;
			}
			const Box3DShapeProxy3D shape_proxy(shape, recovered_transform * p_body.get_shape_transform(i), margin);
			if (!shape_proxy.is_supported()) {
				continue;
			}
			found_supported_shape = true;

			RecoveryContext context;
			context.filter = &filter;
			context.query_shape = p_body.get_shape_id(i);
			context.query_transform = godot_to_b3_transform(recovered_transform);
			context.margin = (float)margin;
			context.planes = &planes;
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

		if (planes.is_empty()) {
			break;
		}
		const Vector3 accumulated_push = solve_recovery(planes);
		if (!accumulated_push.is_finite() || accumulated_push.length_squared() <= 1e-10) {
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
		const bool is_separation_ray = shape->get_type() == PhysicsServer3D::SHAPE_SEPARATION_RAY;
		const auto* ray = is_separation_ray ? static_cast<const Box3DSeparationRayShapeImpl3D*>(shape) : nullptr;
		if (ray != nullptr && !p_collide_separation_ray && !ray->get_slide_on_slope()) {
			continue;
		}

		const Transform3D shape_transform = recovered_transform * p_body.get_shape_transform(i);
		const Box3DShapeProxy3D shape_proxy(shape, shape_transform, margin);
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
		context.query_shape = p_body.get_shape_id(i);
		context.query_transform = godot_to_b3_transform(recovered_transform);
		context.motion = godot_to_b3(p_motion);
		context.margin = (float)margin;
		context.local_shape = i;
		if (ray != nullptr && !ray->get_slide_on_slope()) {
			context.override_normal = true;
			context.normal_override = -shape_transform.basis.get_column(2).normalized();
		}
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
