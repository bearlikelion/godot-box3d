#include "box3d_pair_exceptions_3d.hpp"

#include "../objects/box3d_body_impl_3d.hpp"
#include "../spaces/box3d_space_3d.hpp"

#include <box3d/box3d.h>
#include <box3d/math_functions.h>

void Box3DPairExceptions3D::add(Box3DBodyImpl3D* p_body, const RID& p_other_rid, Box3DBodyImpl3D* p_other_body) {
	ERR_FAIL_NULL(p_body);

	// The directional record and the querying body's exclusion set are kept even when
	// the counterpart does not resolve, so stale/late RIDs behave like they do under
	// Godot Physics and Jolt (harmlessly inert until the body exists).
	p_body->add_collision_exception(p_other_rid);
	p_body->add_query_exclusion(p_other_rid);

	if (p_other_body == nullptr || p_other_body == p_body) {
		return;
	}

	// Exceptions resolve symmetrically in Godot's solver, so both bodies exclude each
	// other from motion-test queries regardless of which side registered the exception.
	p_other_body->add_query_exclusion(p_body->get_rid());

	const int64_t existing = _find_pair(p_body, p_other_body);
	if (existing >= 0) {
		Pair& pair = pairs[existing];
		if (pair.body_a == p_body) {
			pair.a_excepts_b = true;
		} else {
			pair.b_excepts_a = true;
		}
		_sync_joint(pair);
		return;
	}

	Pair pair;
	pair.body_a = p_body;
	pair.body_b = p_other_body;
	pair.a_excepts_b = true;
	pair.joint_id = b3_nullJointId;
	_sync_joint(pair);
	pairs.push_back(pair);
}

void Box3DPairExceptions3D::remove(Box3DBodyImpl3D* p_body, const RID& p_other_rid, Box3DBodyImpl3D* p_other_body) {
	ERR_FAIL_NULL(p_body);

	p_body->remove_collision_exception(p_other_rid);

	if (p_other_body == nullptr) {
		// Counterpart no longer resolves; any pair it participated in was already torn
		// down by remove_body when it was freed. Only the directional RID record (and
		// this body's own query exclusion of the stale RID) needed clearing.
		p_body->remove_query_exclusion(p_other_rid);
		return;
	}

	const int64_t index = _find_pair(p_body, p_other_body);
	if (index < 0) {
		p_body->remove_query_exclusion(p_other_rid);
		return;
	}

	Pair& pair = pairs[index];
	if (pair.body_a == p_body) {
		pair.a_excepts_b = false;
	} else {
		pair.b_excepts_a = false;
	}

	if (pair.a_excepts_b || pair.b_excepts_a) {
		// The other direction still holds the exception; the pair stays live and both
		// bodies keep excluding each other (symmetric resolution).
		return;
	}

	_destroy_joint(pair);
	pair.body_a->remove_query_exclusion(pair.body_b->get_rid());
	pair.body_b->remove_query_exclusion(pair.body_a->get_rid());
	pairs.remove_at(index);
}

void Box3DPairExceptions3D::refresh_body(Box3DBodyImpl3D* p_body) {
	for (Pair& pair : pairs) {
		if (pair.body_a == p_body || pair.body_b == p_body) {
			_sync_joint(pair);
		}
	}
}

void Box3DPairExceptions3D::remove_body(Box3DBodyImpl3D* p_body) {
	for (int64_t i = (int64_t)pairs.size() - 1; i >= 0; i--) {
		Pair& pair = pairs[i];
		if (pair.body_a != p_body && pair.body_b != p_body) {
			continue;
		}
		_destroy_joint(pair);
		Box3DBodyImpl3D* other = pair.body_a == p_body ? pair.body_b : pair.body_a;
		if (other != nullptr) {
			other->remove_query_exclusion(p_body->get_rid());
			// The counterpart's directional RID record is intentionally left in place:
			// Godot itself keeps stale RIDs in exception lists after the excepted body
			// is freed, and _body_get_collision_exceptions should round-trip them.
		}
		pairs.remove_at(i);
	}
}

int64_t Box3DPairExceptions3D::_find_pair(const Box3DBodyImpl3D* p_body_a, const Box3DBodyImpl3D* p_body_b) const {
	for (int64_t i = 0; i < (int64_t)pairs.size(); i++) {
		const Pair& pair = pairs[i];
		if ((pair.body_a == p_body_a && pair.body_b == p_body_b) ||
				(pair.body_a == p_body_b && pair.body_b == p_body_a)) {
			return i;
		}
	}
	return -1;
}

void Box3DPairExceptions3D::_sync_joint(Pair& p_pair) {
	_destroy_joint(p_pair);

	Box3DBodyImpl3D* body_a = p_pair.body_a;
	Box3DBodyImpl3D* body_b = p_pair.body_b;
	if (body_a == nullptr || body_b == nullptr) {
		return;
	}
	if (!body_a->has_body_id() || !body_b->has_body_id()) {
		return;
	}
	if (body_a->get_space() == nullptr || body_a->get_space() != body_b->get_space()) {
		return;
	}

	// Box3D asserts against joints connecting two STATIC bodies (kinematic-kinematic is
	// fine). A static-static pair never generates contacts anyway, so there is nothing
	// for a filter joint to suppress; the query-layer exclusions still apply.
	if (body_a->get_mode() == PhysicsServer3D::BODY_MODE_STATIC &&
			body_b->get_mode() == PhysicsServer3D::BODY_MODE_STATIC) {
		return;
	}

	b3FilterJointDef def = b3DefaultFilterJointDef();
	def.base.bodyIdA = body_a->get_body_id();
	def.base.bodyIdB = body_b->get_body_id();
	def.base.localFrameA = b3Transform_identity;
	def.base.localFrameB = b3Transform_identity;

	p_pair.joint_id = b3CreateFilterJoint(body_a->get_space()->get_world_id(), &def);
}

void Box3DPairExceptions3D::_destroy_joint(Pair& p_pair) {
	// b3DestroyBody destroys attached joints itself, so the cached id may already be
	// stale; generational id validation makes that detectable and safe to skip.
	if (B3_IS_NON_NULL(p_pair.joint_id) && b3Joint_IsValid(p_pair.joint_id)) {
		// b3DestroyJoint alone does not re-pair still-overlapping bodies in the broad
		// phase, so removing an exception while the bodies overlap would leave them
		// contactless until one of them moved. Flipping collideConnected first routes
		// through the engine's re-pair path (it buffers broadphase moves for the
		// bodies' shapes), after which the joint can be destroyed.
		b3Joint_SetCollideConnected(p_pair.joint_id, true);
		b3DestroyJoint(p_pair.joint_id, true);
	}
	p_pair.joint_id = b3_nullJointId;
}
