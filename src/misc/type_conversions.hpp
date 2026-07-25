#pragma once

#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/plane.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <box3d/math_functions.h>
#include <box3d/types.h>

using namespace godot;

// Box3D is built in single precision (no BOX3D_DOUBLE_PRECISION), so b3Pos/b3WorldTransform
// are plain typedefs of b3Vec3/b3Transform. All conversions below operate directly on
// b3Vec3/b3Transform, which also serves b3Pos/b3WorldTransform call sites.

_FORCE_INLINE_ Vector3 b3_to_godot(const b3Vec3& p_v) {
	return Vector3(p_v.x, p_v.y, p_v.z);
}

_FORCE_INLINE_ b3Vec3 godot_to_b3(const Vector3& p_v) {
	return b3Vec3{p_v.x, p_v.y, p_v.z};
}

_FORCE_INLINE_ Quaternion b3_to_godot(const b3Quat& p_q) {
	return Quaternion(p_q.v.x, p_q.v.y, p_q.v.z, p_q.s);
}

_FORCE_INLINE_ b3Quat godot_to_b3(const Quaternion& p_q) {
	b3Quat q;
	q.v.x = p_q.x;
	q.v.y = p_q.y;
	q.v.z = p_q.z;
	q.s = p_q.w;
	return q;
}

_FORCE_INLINE_ Transform3D b3_to_godot(const b3Transform& p_t) {
	const Basis basis(b3_to_godot(p_t.q));
	return Transform3D(basis, b3_to_godot(p_t.p));
}

_FORCE_INLINE_ b3Transform godot_to_b3_transform(const Transform3D& p_t) {
	b3Transform t;
	t.p = godot_to_b3(p_t.origin);
	t.q = godot_to_b3(p_t.basis.get_rotation_quaternion());
	return t;
}

_FORCE_INLINE_ AABB b3_to_godot(const b3AABB& p_aabb) {
	const Vector3 lower = b3_to_godot(p_aabb.lowerBound);
	const Vector3 upper = b3_to_godot(p_aabb.upperBound);
	return AABB(lower, upper - lower);
}

_FORCE_INLINE_ b3AABB godot_to_b3(const AABB& p_aabb) {
	b3AABB aabb;
	aabb.lowerBound = godot_to_b3(p_aabb.position);
	aabb.upperBound = godot_to_b3(p_aabb.position + p_aabb.size);
	return aabb;
}

// Box3D requires both objects to accept each other, while Godot creates a collision
// pair when either object's mask accepts the other object's layer. Packing Godot's
// 32-bit layer and mask into opposite halves of Box3D's 64-bit fields makes both of
// Box3D's tests evaluate to the same Godot OR expression:
//
//   (mask_a & layer_b) != 0 || (layer_a & mask_b) != 0
_FORCE_INLINE_ b3Filter godot_to_b3_filter(uint32_t p_layer, uint32_t p_mask) {
	b3Filter filter = b3DefaultFilter();
	filter.categoryBits = (uint64_t)p_layer | ((uint64_t)p_mask << 32);
	filter.maskBits = (uint64_t)p_mask | ((uint64_t)p_layer << 32);
	return filter;
}

// Godot direct-space queries only compare their collision mask with a target's layer.
// Mirror that comparison into both halves expected by Box3D's symmetric query filter.
_FORCE_INLINE_ b3QueryFilter godot_to_b3_query_filter(uint32_t p_collision_mask) {
	b3QueryFilter filter = b3DefaultQueryFilter();
	filter.categoryBits = (uint64_t)p_collision_mask << 32;
	filter.maskBits = (uint64_t)p_collision_mask;
	return filter;
}

_FORCE_INLINE_ Plane b3_to_godot(const b3Plane& p_plane) {
	return Plane(b3_to_godot(p_plane.normal), p_plane.offset);
}
