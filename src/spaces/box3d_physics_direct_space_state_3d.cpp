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
	result.shape = shape_index;
	ctx->count++;
	return true;
}

struct RayContext {
	const Box3DQueryFilter3D* filter = nullptr;
	bool hit_from_inside = false;
	bool has_hit = false;
	b3ShapeId shape_id = b3_nullShapeId;
	Box3DShapedObjectImpl3D* object = nullptr;
	b3Pos point{};
	b3Vec3 normal{};
	float fraction = 1.0f;
	int32_t shape_index = -1;
	int32_t face_index = -1;
};

float cast_result_fcn(b3ShapeId p_shape_id, b3Pos p_point, b3Vec3 p_normal, float p_fraction, uint64_t, int p_triangle_index, int, void* p_context) {
	auto* ctx = static_cast<RayContext*>(p_context);

	Box3DShapedObjectImpl3D* object = nullptr;
	int32_t shape_index = -1;
	if (!should_report(p_shape_id, *ctx->filter, object, shape_index)) {
		return -1.0f;
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
	if (p_result_count != nullptr) {
		*p_result_count = 0;
	}
	if (p_results == nullptr || p_max_results <= 0) {
		return false;
	}

	LocalVector<PhysicsServer3DExtensionShapeResult> shape_results;
	shape_results.resize(p_max_results);
	const int32_t count = _intersect_shape(
			p_shape_rid,
			p_transform,
			p_motion,
			p_margin,
			p_collision_mask,
			p_collide_with_bodies,
			p_collide_with_areas,
			shape_results.ptr(),
			p_max_results);

	auto* points = static_cast<Vector3*>(p_results);
	for (int32_t i = 0; i < count; i++) {
		points[i] = p_transform.origin;
	}
	if (p_result_count != nullptr) {
		*p_result_count = count;
	}
	return count > 0;
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

	int32_t local_shape = -1;
	Box3DShapeImpl3D* first_shape = nullptr;
	for (int32_t i = 0; i < p_body.get_shape_count(); i++) {
		if (!p_body.is_shape_disabled(i)) {
			first_shape = p_body.get_shape(i);
			local_shape = i;
			break;
		}
	}
	if (first_shape == nullptr) {
		p_result->travel = p_motion;
		p_result->remainder = Vector3();
		return false;
	}

	Box3DQueryFilter3D filter;
	filter.filter.maskBits = (uint64_t)p_body.get_collision_mask();
	filter.exclude.insert(p_body.get_rid());

	const Box3DShapeProxy3D shape_proxy(first_shape, p_transform * p_body.get_shape_transform(local_shape), p_margin);
	if (!shape_proxy.is_supported()) {
		p_result->travel = p_motion;
		p_result->remainder = Vector3();
		return false;
	}

	RayContext context;
	context.filter = &filter;

	b3World_CastShape(space->get_world_id(), b3Vec3_zero, &shape_proxy.get_proxy(), godot_to_b3(p_motion), filter.filter, cast_result_fcn, &context);

	if (!context.has_hit) {
		p_result->travel = p_motion;
		p_result->remainder = Vector3();
		return false;
	}

	p_result->travel = p_motion * context.fraction;
	p_result->remainder = p_motion * (1.0f - context.fraction);
	p_result->collision_safe_fraction = context.fraction;
	p_result->collision_unsafe_fraction = context.fraction;

	auto* other = context.object;
	if (other != nullptr && p_max_collisions > 0) {
		PhysicsServer3DExtensionMotionCollision& collision = p_result->collisions[0];
		collision.position = b3_to_godot(context.point);
		collision.normal = b3_to_godot(context.normal);
		collision.collider = other->get_rid();
		collision.collider_id = other->get_instance_id();
		collision.collider_shape = context.shape_index;
		collision.local_shape = local_shape;
		auto* other_body = dynamic_cast<Box3DBodyImpl3D*>(other);
		if (other_body != nullptr) {
			collision.collider_velocity = other_body->get_linear_velocity();
			collision.collider_angular_velocity = other_body->get_angular_velocity();
		}
		collision.depth = 0.0f;
		p_result->collision_count = 1;
	}

	return true;
}
