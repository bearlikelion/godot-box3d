#pragma once

#include "../misc/box3d_collision_filter.hpp"

#include <godot_cpp/classes/physics_direct_space_state3d_extension.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/rid.hpp>

#include <cstdint>

using namespace godot;

// Builds a broad-phase b3QueryFilter plus Godot's exact one-way collision mask.
// Box3D requires query category and candidate mask to match in both directions;
// Godot queries only scan the candidate's layer. The reserved broad-phase bit
// admits the candidate, then should_report() applies collision_mask exactly.
struct Box3DQueryFilter3D {
	b3QueryFilter filter = box3d_godot_query_filter(UINT32_MAX);
	uint32_t collision_mask = UINT32_MAX;
	HashSet<RID> exclude;
	const PhysicsDirectSpaceState3DExtension* direct_state = nullptr;
	bool collide_with_bodies = true;
	bool collide_with_areas = false;
	bool pick_ray = false;
	bool body_test_motion = false;

	Box3DQueryFilter3D() = default;

	Box3DQueryFilter3D(uint32_t p_collision_mask, bool p_collide_with_bodies, bool p_collide_with_areas) :
			filter(box3d_godot_query_filter(p_collision_mask)),
			collision_mask(p_collision_mask),
			collide_with_bodies(p_collide_with_bodies), collide_with_areas(p_collide_with_areas) {
	}

	void set_collision_mask(uint32_t p_collision_mask) {
		collision_mask = p_collision_mask;
		filter = box3d_godot_query_filter(p_collision_mask);
	}

	bool should_exclude(const RID& p_rid) const {
		return exclude.has(p_rid) || (direct_state != nullptr && direct_state->is_body_excluded_from_query(p_rid));
	}
};
