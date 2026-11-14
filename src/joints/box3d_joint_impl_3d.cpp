#include "box3d_joint_impl_3d.hpp"

#include "../misc/type_conversions.hpp"
#include "../objects/box3d_body_impl_3d.hpp"
#include "../spaces/box3d_space_3d.hpp"

#include <box3d/box3d.h>

Box3DJointImpl3D::Box3DJointImpl3D(
		Box3DBodyImpl3D* p_body_a,
		Box3DBodyImpl3D* p_body_b,
		const Transform3D& p_local_frame_a,
		const Transform3D& p_local_frame_b) :
		local_frame_a(p_local_frame_a),
		local_frame_b(p_local_frame_b),
		body_a(p_body_a),
		body_b(p_body_b) {
	if (body_a != nullptr) {
		body_a->add_joint(this);
	}
	if (body_b != nullptr) {
		body_b->add_joint(this);
	}
}

Box3DJointImpl3D::~Box3DJointImpl3D() {
	_destroy_joint_id();
	if (body_a != nullptr) {
		body_a->remove_joint(this);
	}
	if (body_b != nullptr) {
		body_b->remove_joint(this);
	}
}

void Box3DJointImpl3D::remove_body(Box3DBodyImpl3D* p_body) {
	_destroy_joint_id();
	p_body->remove_joint(this);
	if (body_a == p_body) {
		body_a = nullptr;
	}
	if (body_b == p_body) {
		body_b = nullptr;
	}
	endpoints_valid = false;
}

void Box3DJointImpl3D::set_collision_disabled(bool p_disabled) {
	collision_disabled = p_disabled;
	if (has_joint_id()) {
		b3Joint_SetCollideConnected(joint_id, !p_disabled);
	}
}

void Box3DJointImpl3D::set_local_frame_a(const Transform3D& p_frame) {
	// PinJoint3D rewrites its anchors on every transform notification, so skip no-op writes.
	if (local_frame_a == p_frame) {
		return;
	}
	local_frame_a = p_frame;
	if (has_joint_id()) {
		b3Joint_SetLocalFrameA(get_joint_id(), godot_to_b3_transform(p_frame));
	}
}

void Box3DJointImpl3D::set_local_frame_b(const Transform3D& p_frame) {
	if (local_frame_b == p_frame) {
		return;
	}
	local_frame_b = p_frame;
	if (has_joint_id()) {
		b3Joint_SetLocalFrameB(get_joint_id(), godot_to_b3_transform(p_frame));
	}
}

void Box3DJointImpl3D::rebuild() {
	_destroy_joint_id();

	if (!endpoints_valid || body_a == nullptr) {
		return;
	}
	if (!body_a->has_body_id() || body_a->get_space() == nullptr) {
		return;
	}
	// A deferred two-body joint must never pass IDs from different native worlds.
	if (body_b != nullptr && (!body_b->has_body_id() || body_b->get_space() != body_a->get_space())) {
		return;
	}

	const b3Transform frame_a = godot_to_b3_transform(local_frame_a);
	const b3Transform frame_b = godot_to_b3_transform(local_frame_b);
	const b3WorldId world_id = body_a->get_space()->get_world_id();

	const b3BodyId endpoint_b = body_b != nullptr ? body_b->get_body_id() : body_a->get_space()->get_world_anchor_body();
	joint_id = _create_joint_id(world_id, body_a->get_body_id(), endpoint_b, frame_a, frame_b);

	if (has_joint_id()) {
		b3Joint_SetCollideConnected(joint_id, !collision_disabled);
	}
}

void Box3DJointImpl3D::_destroy_joint_id() {
	if (has_joint_id()) {
		// b3DestroyBody already destroyed this joint if either body went away first.
		if (b3Joint_IsValid(joint_id)) {
			b3DestroyJoint(joint_id, true);
		}
		joint_id = b3_nullJointId;
	}
}
