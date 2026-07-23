#pragma once

#include <box3d/types.h>

#include <cstdint>

// Godot exposes 32 physics layers while Box3D uses 64-bit filters. Reserve one
// otherwise-unreachable bit so Box3D's mandatory bilateral broad-phase test does
// not discard pairs before the extension can apply Godot's asymmetric rules.
//
// Physical pairs are filtered exactly by the custom world callback. Queries are
// filtered exactly by Box3DQueryFilter3D::collision_mask in should_report().
constexpr uint64_t BOX3D_GODOT_BROADPHASE_BIT = UINT64_C(1) << 63;

inline b3Filter box3d_godot_shape_filter(uint32_t p_layer, uint32_t p_mask) {
	b3Filter filter = b3DefaultFilter();
	filter.categoryBits = (uint64_t)p_layer | BOX3D_GODOT_BROADPHASE_BIT;
	filter.maskBits = (uint64_t)p_mask | BOX3D_GODOT_BROADPHASE_BIT;
	return filter;
}

inline b3QueryFilter box3d_godot_query_filter(uint32_t p_collision_mask) {
	b3QueryFilter filter = b3DefaultQueryFilter();
	filter.categoryBits = BOX3D_GODOT_BROADPHASE_BIT;
	filter.maskBits = (uint64_t)p_collision_mask | BOX3D_GODOT_BROADPHASE_BIT;
	return filter;
}
