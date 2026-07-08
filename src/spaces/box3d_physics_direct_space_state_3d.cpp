#include "box3d_physics_direct_space_state_3d.hpp"

#include "../misc/box3d_shape_proxy.hpp"
#include "../misc/type_conversions.hpp"
#include "../objects/box3d_area_impl_3d.hpp"
#include "../objects/box3d_body_impl_3d.hpp"
#include "../objects/box3d_shaped_object_impl_3d.hpp"
#include "../servers/box3d_physics_server_3d.hpp"
#include "../shapes/box3d_capsule_shape_impl_3d.hpp"
#include "../shapes/box3d_shape_impl_3d.hpp"
#include "../shapes/box3d_sphere_shape_impl_3d.hpp"
#include "box3d_query_filter_3d.hpp"
#include "box3d_space_3d.hpp"

#include <cfloat>

#include <box3d/box3d.h>

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
	// v1 does not implement contact-manifold-level shape collision (only overlap/ray/cast
	// queries); PhysicsServer3D::collide_shape() will report no collisions.
	*p_result_count = 0;
	return false;
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

namespace {

// --- Character-motion support (see test_body_motion below) -------------------------

constexpr int MOTION_MAX_PLANES = 32;

struct MoverPlanes {
	b3PlaneResult planes[MOTION_MAX_PLANES];
	b3ShapeId shape_ids[MOTION_MAX_PLANES];
	bool is_static[MOTION_MAX_PLANES];
	int count = 0;
	const Box3DQueryFilter3D* filter = nullptr;
};

// Contact slop for the anti-encroachment clamp below: movers may end a call resting
// this deep inside a shape (prevents contact flicker) but never deeper.
constexpr float CHARACTER_CONTACT_SLOP = 0.005f;

bool motion_accept_shape(b3ShapeId p_shape_id, const Box3DQueryFilter3D& p_filter) {
	// Sensors never block motion: this skips every Area3D (areas back themselves with
	// all-sensor shapes) plus any sensor shape on a body.
	if (b3Shape_IsSensor(p_shape_id)) {
		return false;
	}
	auto* object = static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(b3Shape_GetBody(p_shape_id)));
	if (object == nullptr) {
		return false;
	}
	return !p_filter.should_exclude(object->get_rid());
}

bool mover_filter_fcn(b3ShapeId p_shape_id, void* p_context) {
	const auto* filter = static_cast<const Box3DQueryFilter3D*>(p_context);
	return motion_accept_shape(p_shape_id, *filter);
}

bool mover_plane_fcn(b3ShapeId p_shape_id, const b3PlaneResult* p_plane, int p_plane_count, void* p_context) {
	(void)p_plane_count;
	auto* ctx = static_cast<MoverPlanes*>(p_context);
	if (!motion_accept_shape(p_shape_id, *ctx->filter)) {
		return true;
	}
	if (ctx->count < MOTION_MAX_PLANES) {
		ctx->planes[ctx->count] = *p_plane;
		ctx->shape_ids[ctx->count] = p_shape_id;
		ctx->is_static[ctx->count] = b3Body_GetType(b3Shape_GetBody(p_shape_id)) == b3_staticBody;
		ctx->count++;
	}
	return ctx->count < MOTION_MAX_PLANES;
}

// Penetration depth of a capsule against an outward contact plane (>= 0; 0 = separated
// or exactly touching). The capsule's support point toward the plane is the deeper
// endpoint's sphere surface.
// Depth of the mover against a plane returned by b3CollideMover, evaluated at a pose
// displaced by p_delta from the pose the query was issued at.
//
// CONVENTION (see engine src/sphere.c b3CollideMoverAndSphere and src/mover.c
// b3SolvePlanes): mover planes are MOVER-RELATIVE constraints, not world planes.
// The offset is built as `totalRadius - distance`, i.e. it IS the mover's penetration
// (positive) or negative clearance at the queried pose, and
// b3PlaneSeparation(plane, delta) = dot(n, delta) - offset is the separation after
// displacing the mover by delta. Re-deriving depth from world capsule geometry against
// these planes is wrong everywhere except a horizontal plane through the world origin
// -- which is exactly the case every flat-ground test exercised, hiding the bug until
// slopes (offsets of tens of meters -> a clamp meant to trim millimeters floored the
// motion fraction to zero, reading as sink/lock/fall-through in move_and_slide).
float mover_plane_depth(const b3Plane& p_plane, const b3Vec3& p_delta) {
	return MAX(0.0f, p_plane.offset - b3Dot(p_plane.normal, p_delta));
}

// Builds the world-space motion capsule for a body at p_from: exact for capsule and
// sphere movement colliders; a conservative bounding capsule (from the shape's local
// AABB) for box/convex/cylinder ones. Box3D's character-mover queries -- the machinery
// behind move_and_slide here -- are capsule-based, so non-capsule movement colliders
// trade exact footprints for robust sliding/depenetration. Solver-side collision (the
// shape rigid bodies and queries see) is unaffected.
bool build_motion_capsule(const Box3DShapedObjectImpl3D& p_body, const Transform3D& p_from, b3Capsule& r_capsule) {
	Box3DShapeImpl3D* shape = p_body.get_shape(0);
	if (shape == nullptr) {
		return false;
	}

	// Godot may hand a scaled body transform (scaled CharacterBody3D or scaled shape
	// locals); the engine capsule is metric, so bake the composed scale into the
	// dimensions and use the orthonormalized frame for placement, mirroring
	// build_shape's convention.
	Transform3D shape_world = p_from * p_body.get_shape_transform(0);
	const Vector3 world_scale = shape_world.basis.get_scale();
	shape_world.basis = shape_world.basis.orthonormalized();

	switch (shape->get_type()) {
		case PhysicsServer3D::SHAPE_CAPSULE: {
			const auto* capsule = static_cast<const Box3DCapsuleShapeImpl3D*>(shape);
			const float radius = (float)capsule->get_radius() * MAX(world_scale.x, world_scale.z);
			const float half_seg = MAX(0.0f, (float)capsule->get_height() * 0.5f * world_scale.y - radius);
			r_capsule.center1 = godot_to_b3(shape_world.xform(Vector3(0, -half_seg, 0)));
			r_capsule.center2 = godot_to_b3(shape_world.xform(Vector3(0, half_seg, 0)));
			r_capsule.radius = radius;
			return true;
		}
		case PhysicsServer3D::SHAPE_SPHERE: {
			const auto* sphere = static_cast<const Box3DSphereShapeImpl3D*>(shape);
			r_capsule.center1 = godot_to_b3(shape_world.origin);
			r_capsule.center2 = r_capsule.center1;
			r_capsule.radius = (float)sphere->get_radius() * MAX(world_scale.x, MAX(world_scale.y, world_scale.z));
			return true;
		}
		default: {
			AABB aabb = shape->get_aabb();
			aabb.position *= world_scale;
			aabb.size *= world_scale;
			if (aabb.size == Vector3()) {
				return false;
			}
			WARN_PRINT_ONCE("Box3D: this body's movement collider is not a capsule or sphere; body_test_motion (move_and_slide) approximates it with a bounding capsule. Solver-side collision keeps the exact shape.");
			const float radius = 0.5f * (float)MAX(aabb.size.x, aabb.size.z);
			const float half_seg = MAX(0.0f, 0.5f * (float)aabb.size.y - radius);
			const Vector3 center = aabb.get_center();
			r_capsule.center1 = godot_to_b3(shape_world.xform(center + Vector3(0, -half_seg, 0)));
			r_capsule.center2 = godot_to_b3(shape_world.xform(center + Vector3(0, half_seg, 0)));
			r_capsule.radius = radius;
			return radius > 0.0f;
		}
	}
}

} // namespace

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

	// Built on Box3D's geometric character-mover API: b3World_CollideMover gathers
	// outward contact planes (valid normals even while touching/overlapping, which raw
	// shape casts do not provide), b3SolvePlanes turns them into a depenetration delta,
	// and b3World_CastMover sweeps with encroachment handling so a mover resting on a
	// surface still slides along it instead of reporting an immediate zero-length hit.
	// All positions are absolute world space (query origin = b3Pos_zero), matching the
	// rest of this file.

	b3Capsule capsule;
	if (!build_motion_capsule(p_body, p_transform, capsule)) {
		p_result->travel = p_motion;
		p_result->remainder = Vector3();
		return false;
	}

	Box3DQueryFilter3D filter;
	filter.filter.maskBits = (uint64_t)p_body.get_collision_mask();
	filter.exclude.insert(p_body.get_rid());

	// Per-pair collision exceptions must be honored here, in the query layer: kinematic
	// bodies (every CharacterBody3D) never generate solver contacts against each other,
	// so add_collision_exception_with would otherwise have no effect on move_and_slide.
	// The set merged in is the symmetric union maintained by Box3DPairExceptions3D, so
	// "A excepts B" suppresses the hit regardless of which side registered it.
	if (const auto* body = dynamic_cast<const Box3DBodyImpl3D*>(&p_body)) {
		for (const RID& excluded_rid : body->get_query_exclusions()) {
			filter.exclude.insert(excluded_rid);
		}
	}

	const b3WorldId world_id = space->get_world_id();
	const float margin = MAX(0.0f, (float)p_margin);

	// --- 1. Recovery: depenetrate from anything within the margin skin. -------------
	b3Capsule inflated = capsule;
	inflated.radius += margin;

	MoverPlanes recovery_planes;
	recovery_planes.filter = &filter;
	b3World_CollideMover(world_id, b3Pos_zero, &inflated, filter.filter, mover_plane_fcn, &recovery_planes);

	Vector3 recovery;
	if (recovery_planes.count > 0) {
		b3CollisionPlane solver_planes[MOTION_MAX_PLANES];
		for (int i = 0; i < recovery_planes.count; i++) {
			solver_planes[i].plane = recovery_planes.planes[i].plane;
			solver_planes[i].pushLimit = FLT_MAX;
			solver_planes[i].push = 0.0f;
			solver_planes[i].clipVelocity = true;
		}
		const b3PlaneSolverResult solved = b3SolvePlanes(b3Vec3_zero, solver_planes, recovery_planes.count);
		recovery = b3_to_godot(solved.delta);

		const b3Vec3 delta = solved.delta;
		capsule.center1 = b3Add(capsule.center1, delta);
		capsule.center2 = b3Add(capsule.center2, delta);
	}
	const bool recovered = recovery.length_squared() > 1.0e-12f;

	// --- 2. Sweep: encroachment-aware capsule cast along the motion. ----------------
	const float fraction = b3World_CastMover(world_id, b3Pos_zero, &capsule, godot_to_b3(p_motion), filter.filter, mover_filter_fcn, (void*)&filter);

	// Godot semantics: travel includes depenetration, so the caller's body lands at the
	// recovered position plus the safe portion of the motion.
	p_result->travel = recovery + p_motion * fraction;
	p_result->remainder = p_motion * (1.0f - fraction);
	p_result->collision_safe_fraction = fraction;
	p_result->collision_unsafe_fraction = fraction;

	bool blocked = fraction < 1.0f;
	const bool report_recovery = recovered && p_recovery_as_collision;
	if (!blocked && !report_recovery) {
		return false;
	}

	// --- 3. Contacts: gather planes at the final position for normals/positions. ----
	const b3Vec3 end_delta = godot_to_b3(p_motion * fraction);
	b3Capsule end_capsule = capsule;
	end_capsule.center1 = b3Add(end_capsule.center1, end_delta);
	end_capsule.center2 = b3Add(end_capsule.center2, end_delta);

	// Unblocked moves touching only STATIC geometry need no fresh query: the planes
	// gathered during recovery describe the same touching set (the end pose differs by
	// at most one frame's travel), and this is the dominant grounded-walking case --
	// reusing them cuts the per-call mover queries from three to two for every unit
	// that is simply standing or marching. Any non-static contact forces a fresh end
	// query, both for accuracy and because the anti-encroachment clamp below needs
	// end-pose depths.
	bool recovery_all_static = true;
	for (int i = 0; i < recovery_planes.count; i++) {
		if (!recovery_planes.is_static[i]) {
			recovery_all_static = false;
			break;
		}
	}
	MoverPlanes contact_planes;
	if (!blocked && recovery_all_static) {
		contact_planes = recovery_planes;
	} else {
		b3Capsule end_inflated = end_capsule;
		end_inflated.radius += margin;
		contact_planes.filter = &filter;
		b3World_CollideMover(world_id, b3Pos_zero, &end_inflated, filter.filter, mover_plane_fcn, &contact_planes);
	}

	// --- Anti-encroachment clamp. -----------------------------------------------------
	// b3World_CastMover's encroachment lets a mover that STARTS touching a surface
	// approach it by up to B3_LINEAR_SLOP per cast, floored only when the capsule's
	// core segment is within 2*B3_LINEAR_SLOP of the surface -- i.e. nearly a full
	// radius deep (distance.c, b3ShapeCast). The design contract is that the caller's
	// per-frame recovery pass pushes back out, yielding a stable ~slop equilibrium.
	// That contract breaks in two situations, so travel is clamped to never end deeper
	// than a per-type slop inside any contact plane the motion opposes:
	//
	//  * NON-STATIC (CHARACTER_CONTACT_SLOP, 5 mm): pressed character crowds
	//    sink-then-eject and oscillate at frame rate (measured ~70% of moving frames
	//    reversing in a converging swarm; 0% after the clamp).
	//  * STATIC (margin + 2*B3_LINEAR_SLOP, ~1.1 cm at default margin): wedge geometry
	//    -- e.g. jumping under a ramp's sloped underside -- makes recovery infeasible
	//    (pushing out of the ceiling pushes into the floor), so the cast's per-call
	//    allowance ratchets unopposed: measured 38 cm of floor penetration in a jump
	//    loop under a 25-degree underside, then lock-in or fall-through. The static
	//    slop is deliberately looser than the character slop so the mover's designed
	//    ~5 mm transient encroachment over trimesh seams and creases stays untouched;
	//    only runaway accumulation is stopped.
	const float STATIC_CONTACT_SLOP = margin + 2.0f * B3_LINEAR_SLOP;
	const float motion_len = (float)p_motion.length();
	if (motion_len > 1.0e-6f) {
		const b3Vec3 motion_dir = godot_to_b3(p_motion / motion_len);
		// Planes were queried with the margin-inflated capsule: real penetration is
		// offset minus margin. On the plane-reuse path the query pose was the START
		// pose, so evaluate depth at the end pose's displacement from it; on the
		// fresh-end-query path that displacement is zero.
		const b3Vec3 depth_delta = (!blocked && recovery_all_static)
				? godot_to_b3(p_result->travel)
				: b3Vec3_zero;
		float backoff = 0.0f;
		for (int i = 0; i < contact_planes.count; i++) {
			const float slop = contact_planes.is_static[i] ? STATIC_CONTACT_SLOP : CHARACTER_CONTACT_SLOP;
			const float depth = mover_plane_depth(contact_planes.planes[i].plane, depth_delta) - margin;
			if (depth <= slop) {
				continue;
			}
			const float opposition = -b3Dot(contact_planes.planes[i].plane.normal, motion_dir);
			if (opposition > 0.05f) {
				backoff = MAX(backoff, (depth - slop) / opposition);
			}
		}
		if (backoff > 0.0f) {
			const float clamped_fraction = MAX(0.0f, fraction - backoff / motion_len);
			const b3Vec3 clamp_delta = godot_to_b3(p_motion * (clamped_fraction - fraction));
			end_capsule.center1 = b3Add(end_capsule.center1, clamp_delta);
			end_capsule.center2 = b3Add(end_capsule.center2, clamp_delta);
			p_result->travel = recovery + p_motion * clamped_fraction;
			p_result->remainder = p_motion * (1.0f - clamped_fraction);
			p_result->collision_safe_fraction = clamped_fraction;
			p_result->collision_unsafe_fraction = clamped_fraction;
			// The shortened move is a real obstruction: report it as blocked so the
			// slide loop receives the opposing plane instead of an unobstructed move.
			blocked = true;
		}
	}

	const int32_t max_collisions = MIN(p_max_collisions, (int32_t)32);
	float deepest = 0.0f;
	int32_t written = 0;
	for (int i = 0; i < contact_planes.count && written < max_collisions; i++) {
		const b3PlaneResult& plane = contact_planes.planes[i];
		auto* other = static_cast<Box3DShapedObjectImpl3D*>(b3Body_GetUserData(b3Shape_GetBody(contact_planes.shape_ids[i])));
		if (other == nullptr) {
			continue;
		}
		// Native mover-plane depth (see mover_plane_depth); subtract the query margin
		// so margin-only grazes report 0 against the true capsule. Reuse-path planes
		// were queried at the start pose, so evaluate at the end pose's displacement.
		const b3Vec3 report_delta = (!blocked && recovery_all_static)
				? godot_to_b3(p_result->travel)
				: b3Vec3_zero;
		const float depth = MAX(0.0f, mover_plane_depth(plane.plane, report_delta) - margin);
		deepest = MAX(deepest, depth);

		PhysicsServer3DExtensionMotionCollision& collision = p_result->collisions[written];
		collision.position = b3_to_godot(plane.point);
		collision.normal = b3_to_godot(plane.plane.normal);
		collision.collider_velocity = Vector3();
		collision.collider_angular_velocity = Vector3();
		collision.depth = depth;
		collision.local_shape = 0;
		collision.collider = other->get_rid();
		collision.collider_id = other->get_instance_id();
		collision.collider_shape = 0;
		written++;
	}
	p_result->collision_count = written;
	p_result->collision_depth = deepest;

	if (written == 0) {
		if (!blocked) {
			// Recovery-only report with no surviving contact plane: nothing meaningful
			// to describe, so report an unobstructed move.
			return false;
		}
		// Blocked sweep but every end plane was filtered/degenerate: synthesize one
		// valid collision opposing the motion so callers (slide loops) never receive a
		// zero normal.
		if (p_max_collisions > 0) {
			PhysicsServer3DExtensionMotionCollision& collision = p_result->collisions[0];
			const Vector3 direction = p_motion.normalized();
			collision.position = b3_to_godot(end_capsule.center1);
			collision.normal = direction == Vector3() ? Vector3(0, 1, 0) : -direction;
			collision.collider_velocity = Vector3();
			collision.collider_angular_velocity = Vector3();
			collision.depth = 0.0f;
			collision.local_shape = 0;
			collision.collider = RID();
			collision.collider_id = ObjectID();
			collision.collider_shape = 0;
			p_result->collision_count = 1;
		}
	}

	return true;
}
