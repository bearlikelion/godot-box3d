#pragma once

#include "../misc/type_conversions.hpp"

#include <godot_cpp/classes/physics_direct_space_state3d_extension.hpp>
#include <godot_cpp/classes/physics_server3d_extension.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/rid.hpp>

#include <box3d/types.h>

using namespace godot;

// Builds a b3QueryFilter matching Godot's mask-vs-layer query semantics, plus a
// side-channel RID exclude-set since Box3D has no native per-query RID-exclude list.
// Callers check should_exclude() from inside their b3*ResultFcn/b3CastResultFcn callback
// alongside whatever the callback itself needs.
struct Box3DQueryFilter3D {
	b3QueryFilter filter = godot_to_b3_query_filter(UINT32_MAX);
	HashSet<RID> exclude;
	const PhysicsDirectSpaceState3DExtension* direct_state = nullptr;
	const PhysicsServer3DExtension* motion_server = nullptr;
	bool collide_with_bodies = true;
	bool collide_with_areas = false;

	Box3DQueryFilter3D() = default;

	Box3DQueryFilter3D(uint32_t p_collision_mask, bool p_collide_with_bodies, bool p_collide_with_areas) :
			collide_with_bodies(p_collide_with_bodies), collide_with_areas(p_collide_with_areas) {
		set_collision_mask(p_collision_mask);
	}

	void set_collision_mask(uint32_t p_collision_mask) { filter = godot_to_b3_query_filter(p_collision_mask); }

	bool should_exclude(const RID& p_rid, uint64_t p_object_id = 0) const {
		return exclude.has(p_rid) || (direct_state != nullptr && direct_state->is_body_excluded_from_query(p_rid)) ||
				(motion_server != nullptr && (motion_server->body_test_motion_is_excluding_body(p_rid) ||
						motion_server->body_test_motion_is_excluding_object(p_object_id)));
	}
};
