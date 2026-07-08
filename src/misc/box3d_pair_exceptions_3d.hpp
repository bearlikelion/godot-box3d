#pragma once

#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/rid.hpp>

#include <box3d/id.h>

using namespace godot;

class Box3DBodyImpl3D;

// Per-pair collision exceptions (PhysicsBody3D.add_collision_exception_with), bridged in
// two cooperating layers:
//
//   1. SOLVER layer (this class): one Box3D *filter joint* per excepted pair. A filter
//      joint's sole purpose is to disable contact generation between its two attached
//      bodies, which is exactly Godot's exception semantic for dynamic bodies. Joints are
//      (re)created whenever both bodies hold a live b3BodyId in the same space, and
//      destroyed when the exception is fully removed or either body leaves. Box3D
//      destroys attached joints itself inside b3DestroyBody, so every cached b3JointId is
//      b3Joint_IsValid-guarded before use (generational ids make stale ids detectable).
//
//   2. QUERY layer (Box3DBodyImpl3D::get_query_exclusions, consumed by
//      Box3DPhysicsDirectSpaceState3D::test_body_motion): kinematic bodies (i.e. every
//      CharacterBody3D) never generate solver contacts against each other in Box3D, so
//      their mutual collision is resolved purely through body_test_motion shape casts.
//      Filter joints do not affect queries, so the symmetric exclusion set kept on each
//      body is merged into the motion-test query filter instead. This class maintains
//      that set alongside the joints so both layers always agree.
//
// Godot stores exceptions directionally (A's list may contain B without B's containing A)
// but resolves them symmetrically (the pair is excluded if either list matches). This
// registry mirrors that: one entry per unordered pair with two directional flags, active
// while either flag is set.
//
// Storage is a flat LocalVector with linear scans: exception counts in practice are small
// (squad-mates, a vehicle and its rider), and this keeps the structure trivially correct
// with zero hashing requirements on RID pairs.
class Box3DPairExceptions3D {
public:
	// Records that p_body excepts p_other (directional). p_other_body may be null when
	// the RID does not (or does not yet) resolve to a live body; the directional record
	// and p_body's query exclusion are still kept so the RID round-trips through
	// _body_get_collision_exceptions, matching Godot's tolerance of stale exception RIDs.
	void add(Box3DBodyImpl3D* p_body, const RID& p_other_rid, Box3DBodyImpl3D* p_other_body);

	// Clears the directional record; the pair (and its filter joint) survives while the
	// other direction is still set.
	void remove(Box3DBodyImpl3D* p_body, const RID& p_other_rid, Box3DBodyImpl3D* p_other_body);

	// Re-syncs every filter joint touching p_body after its space attachment (and thus
	// its b3BodyId) changed. Call after Box3DBodyImpl3D::set_space.
	void refresh_body(Box3DBodyImpl3D* p_body);

	// Drops every pair touching p_body (destroying live joints) and scrubs p_body's RID
	// from every counterpart's exclusion sets. Call before the body is freed.
	void remove_body(Box3DBodyImpl3D* p_body);

private:
	struct Pair {
		Box3DBodyImpl3D* body_a = nullptr;
		Box3DBodyImpl3D* body_b = nullptr;
		bool a_excepts_b = false;
		bool b_excepts_a = false;
		b3JointId joint_id;
	};

	int64_t _find_pair(const Box3DBodyImpl3D* p_body_a, const Box3DBodyImpl3D* p_body_b) const;

	void _sync_joint(Pair& p_pair);

	void _destroy_joint(Pair& p_pair);

	LocalVector<Pair> pairs;
};
