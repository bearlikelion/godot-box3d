#include "box3d_recovery_geometry.hpp"

#include <box3d/collision.h>
#include <box3d/math_functions.h>

namespace {

struct Geometry {
	b3ShapeType type;
	b3Sphere sphere{};
	b3Capsule capsule{};
	const b3HullData* hull = nullptr;

	explicit Geometry(b3ShapeId p_id) : type(b3Shape_GetType(p_id)) {
		switch (type) {
			case b3_sphereShape: sphere = b3Shape_GetSphere(p_id); break;
			case b3_capsuleShape: capsule = b3Shape_GetCapsule(p_id); break;
			case b3_hullShape: hull = b3Shape_GetHull(p_id); break;
			default: break;
		}
	}

	b3ShapeProxy proxy() const {
		switch (type) {
			case b3_sphereShape: return b3ShapeProxy{&sphere.center, 1, sphere.radius};
			case b3_capsuleShape: return b3ShapeProxy{&capsule.center1, 2, capsule.radius};
			case b3_hullShape: return b3ShapeProxy{b3GetHullPoints(hull), hull->vertexCount, 0.0f};
			default: return b3ShapeProxy{};
		}
	}

	int rank() const {
		return type == b3_hullShape ? 2 : type == b3_capsuleShape ? 1 : 0;
	}
};

void append_manifold(const b3LocalManifold& p_manifold, b3Transform p_transform,
		bool p_flip, float p_margin, godot::LocalVector<Box3DRecoveryContact>& r_contacts) {
	const b3Vec3 normal = b3RotateVector(p_transform.q, p_manifold.normal);
	if (b3LengthSquared(normal) < 0.5f) {
		return;
	}
	for (int i = 0; i < p_manifold.pointCount; i++) {
		const b3LocalManifoldPoint& point = p_manifold.points[i];
		if (point.separation <= p_margin + 1e-5f) {
			r_contacts.push_back({b3TransformPoint(p_transform, point.point),
					p_flip ? b3Neg(normal) : normal, point.separation});
		}
	}
}

// Manifolds give real penetration; GJK only fills the positive-distance margin
// shell when a native manifold does not cover that distance. Never infer depth
// from coincident GJK witnesses or from an AABB.
void append_margin_contact(b3ShapeProxy p_target, b3ShapeProxy p_query,
		b3Transform p_query_to_target, b3Transform p_target_transform, float p_margin,
		godot::LocalVector<Box3DRecoveryContact>& r_contacts) {
	b3DistanceInput input{};
	input.proxyA = p_target;
	input.proxyB = p_query;
	input.transform = p_query_to_target;
	input.useRadii = true;
	b3SimplexCache cache{};
	const b3DistanceOutput output = b3ShapeDistance(&input, &cache, nullptr, 0);
	if (output.distance > 1e-6f && output.distance <= p_margin) {
		const b3Vec3 normal = b3Normalize(b3Sub(output.pointB, output.pointA));
		r_contacts.push_back({b3TransformPoint(p_target_transform, output.pointA),
				b3RotateVector(p_target_transform.q, normal), output.distance});
	}
}

struct TriangleContext {
	const Geometry* query;
	b3Transform target_to_query;
	b3Transform query_transform;
	float margin;
	const b3MeshData* mesh;
	const b3HeightFieldData* height_field;
	godot::LocalVector<Box3DRecoveryContact>* contacts;
};

bool triangle_contact(b3Vec3 p_a, b3Vec3 p_b, b3Vec3 p_c, int p_index, void* p_context) {
	auto& ctx = *static_cast<TriangleContext*>(p_context);
	const b3Vec3 triangle[] = {b3TransformPoint(ctx.target_to_query, p_a),
			b3TransformPoint(ctx.target_to_query, p_b), b3TransformPoint(ctx.target_to_query, p_c)};
	b3LocalManifoldPoint points[32];
	b3LocalManifold manifold{};
	manifold.points = points;
	b3SimplexCache simplex_cache{};
	b3SATCache sat_cache{};
	const Geometry& query = *ctx.query;
	if (query.type == b3_sphereShape) {
		b3CollideTriangleAndSphere(&manifold, 32, triangle, &query.sphere);
	} else if (query.type == b3_capsuleShape) {
		b3CollideTriangleAndCapsule(&manifold, 32, triangle, &query.capsule, &simplex_cache);
	} else if (query.type == b3_hullShape) {
		const uint8_t* flags = ctx.mesh ? b3GetMeshFlags(ctx.mesh) : b3GetHeightFieldFlags(ctx.height_field);
		const int triangle_flags = flags ? flags[p_index] : 0;
		b3CollideTriangleAndHull(&manifold, 32, triangle[0], triangle[1], triangle[2], triangle_flags, query.hull, &sat_cache, true);
	}
	const uint32_t count_before = ctx.contacts->size();
	append_manifold(manifold, ctx.query_transform, false, ctx.margin, *ctx.contacts);
	if (ctx.contacts->size() == count_before) {
		// Terrain is one-sided, like the simulated fixtures. Margin contacts on
		// the back of a triangle must not create a second, opposing surface.
		const b3Vec3 face_normal = b3Normalize(b3Cross(b3Sub(triangle[1], triangle[0]), b3Sub(triangle[2], triangle[0])));
		const b3ShapeProxy proxy = query.proxy();
		if (b3Dot(face_normal, b3Sub(proxy.points[0], triangle[0])) >= 0.0f) {
			append_margin_contact({triangle, 3, 0.0f}, proxy, b3Transform_identity, ctx.query_transform, ctx.margin, *ctx.contacts);
		}
	}
	return true;
}

} // namespace

void box3d_recovery_contacts(b3ShapeId p_query, b3Transform p_query_transform,
		b3ShapeId p_target, float p_margin, godot::LocalVector<Box3DRecoveryContact>& r_contacts) {
	const Geometry query(p_query);
	const Geometry target(p_target);
	if (query.proxy().count == 0) {
		return;
	}
	const b3Transform target_transform = b3Body_GetTransform(b3Shape_GetBody(p_target));
	if (target.type == b3_meshShape || target.type == b3_heightShape) {
		const b3Transform query_to_target = b3InvMulTransforms(target_transform, p_query_transform);
		const b3ShapeProxy proxy = query.proxy();
		b3Vec3 lower = b3TransformPoint(query_to_target, proxy.points[0]);
		b3Vec3 upper = lower;
		for (int i = 1; i < proxy.count; i++) {
			const b3Vec3 point = b3TransformPoint(query_to_target, proxy.points[i]);
			lower = b3Min(lower, point);
			upper = b3Max(upper, point);
		}
		const float radius = proxy.radius + p_margin;
		const b3Vec3 inflation{radius, radius, radius};
		const b3AABB bounds{b3Sub(lower, inflation), b3Add(upper, inflation)};
		TriangleContext ctx{&query, b3InvMulTransforms(p_query_transform, target_transform), p_query_transform,
				p_margin, nullptr, nullptr, &r_contacts};
		if (target.type == b3_meshShape) {
			const b3Mesh mesh = b3Shape_GetMesh(p_target);
			ctx.mesh = mesh.data;
			b3QueryMesh(&mesh, bounds, triangle_contact, &ctx);
		} else {
			ctx.height_field = b3Shape_GetHeightField(p_target);
			b3QueryHeightField(ctx.height_field, bounds, triangle_contact, &ctx);
		}
		return;
	}

	// Box3D's public pair routines require hull > capsule > sphere ordering.
	const bool flip = target.rank() < query.rank();
	const Geometry& a = flip ? query : target;
	const Geometry& b = flip ? target : query;
	const b3Transform transform_a = flip ? p_query_transform : target_transform;
	const b3Transform transform_b = flip ? target_transform : p_query_transform;
	const b3Transform b_to_a = b3InvMulTransforms(transform_a, transform_b);
	b3LocalManifoldPoint points[32];
	b3LocalManifold manifold{};
	manifold.points = points;
	b3SimplexCache simplex_cache{};
	b3SATCache sat_cache{};
	if (a.type == b3_sphereShape && b.type == b3_sphereShape) {
		b3CollideSpheres(&manifold, 32, &a.sphere, &b.sphere, b_to_a);
	} else if (a.type == b3_capsuleShape && b.type == b3_sphereShape) {
		b3CollideCapsuleAndSphere(&manifold, 32, &a.capsule, &b.sphere, b_to_a);
	} else if (a.type == b3_capsuleShape && b.type == b3_capsuleShape) {
		b3CollideCapsules(&manifold, 32, &a.capsule, &b.capsule, b_to_a);
	} else if (a.type == b3_hullShape && b.type == b3_sphereShape) {
		b3CollideHullAndSphere(&manifold, 32, a.hull, &b.sphere, b_to_a, &simplex_cache);
	} else if (a.type == b3_hullShape && b.type == b3_capsuleShape) {
		b3CollideHullAndCapsule(&manifold, 32, a.hull, &b.capsule, b_to_a, &simplex_cache);
	} else if (a.type == b3_hullShape && b.type == b3_hullShape) {
		b3CollideHulls(&manifold, 32, a.hull, b.hull, b_to_a, &sat_cache);
		// Native manifold reduction can retain speculative points while dropping
		// the deepest point of a many-sided hull on a slope. The SAT cache keeps
		// the actual clipped-face separation; recover that geometric constraint
		// using its support vertex rather than treating the reduced set as clear.
		if (manifold.pointCount > 0 && sat_cache.separation <= p_margin &&
				(sat_cache.type == b3_faceAxisA || sat_cache.type == b3_faceAxisB)) {
			const b3Vec3 support = sat_cache.type == b3_faceAxisA ?
					b3TransformPoint(b_to_a, b3GetHullPoints(b.hull)[sat_cache.indexB]) :
					b3GetHullPoints(a.hull)[sat_cache.indexA];
			const float direction = sat_cache.type == b3_faceAxisA ? -0.5f : 0.5f;
			const b3Vec3 point = b3MulAdd(support, direction * sat_cache.separation, manifold.normal);
			const b3Vec3 normal = b3RotateVector(transform_a.q, manifold.normal);
			r_contacts.push_back({b3TransformPoint(transform_a, point), flip ? b3Neg(normal) : normal, sat_cache.separation});
		}
	}
	const uint32_t count_before = r_contacts.size();
	append_manifold(manifold, transform_a, flip, p_margin, r_contacts);
	if (r_contacts.size() == count_before && target.proxy().count > 0) {
		append_margin_contact(target.proxy(), query.proxy(), b3InvMulTransforms(target_transform, p_query_transform),
				target_transform, p_margin, r_contacts);
	}
}
