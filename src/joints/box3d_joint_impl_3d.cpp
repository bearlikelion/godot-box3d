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
}

Box3DJointImpl3D::~Box3DJointImpl3D() {
	_destroy_joint_id();
}

bool Box3DJointImpl3D::has_joint_id() const {
	return B3_IS_NON_NULL(joint_id) && b3Joint_IsValid(joint_id);
}

void Box3DJointImpl3D::_wake_bodies() {
	if (has_joint_id()) {
		b3Joint_WakeBodies(joint_id);
	}
}

void Box3DJointImpl3D::set_collision_disabled(bool p_disabled) {
	collision_disabled = p_disabled;
	if (has_joint_id()) {
		b3Joint_SetCollideConnected(joint_id, !p_disabled);
		_wake_bodies();
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
		_wake_bodies();
	}
}

void Box3DJointImpl3D::set_local_frame_b(const Transform3D& p_frame) {
	if (local_frame_b == p_frame) {
		return;
	}
	local_frame_b = p_frame;
	if (has_joint_id()) {
		b3Joint_SetLocalFrameB(get_joint_id(), godot_to_b3_transform(p_frame));
		_wake_bodies();
	}
}

void Box3DJointImpl3D::rebuild() {
	_destroy_joint_id();

	if (body_a == nullptr || body_b == nullptr) {
		return;
	}
	if (!body_a->has_body_id() || !body_b->has_body_id()) {
		return;
	}
	if (body_a->get_space() == nullptr) {
		return;
	}

	const b3Transform frame_a = godot_to_b3_transform(local_frame_a);
	const b3Transform frame_b = godot_to_b3_transform(local_frame_b);
	const b3WorldId world_id = body_a->get_space()->get_world_id();

	joint_id = _create_joint_id(world_id, body_a->get_body_id(), body_b->get_body_id(), frame_a, frame_b);

	if (has_joint_id()) {
		b3Joint_SetCollideConnected(joint_id, !collision_disabled);
	}
}

void Box3DJointImpl3D::_destroy_joint_id() {
	if (has_joint_id()) {
		b3DestroyJoint(joint_id, true);
	}
	joint_id = b3_nullJointId;
}
