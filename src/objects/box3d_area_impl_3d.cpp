#include "box3d_area_impl_3d.hpp"

#include "../misc/type_conversions.hpp"

#include <box3d/box3d.h>

b3BodyId Box3DAreaImpl3D::_create_body_id(b3WorldId p_world_id) {
	b3BodyDef def = b3DefaultBodyDef();
	const b3Transform t = godot_to_b3_transform(get_transform());
	def.type = b3_kinematicBody;
	def.position = t.p;
	def.rotation = t.q;
	def.userData = this;
	def.isAwake = true;
	def.isEnabled = true;
	return b3CreateBody(p_world_id, &def);
}

Variant Box3DAreaImpl3D::get_param(PhysicsServer3D::AreaParameter p_param) const {
	switch (p_param) {
		case PhysicsServer3D::AREA_PARAM_GRAVITY_OVERRIDE_MODE:
			return gravity_mode;
		case PhysicsServer3D::AREA_PARAM_GRAVITY:
			return gravity;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_VECTOR:
			return gravity_vector;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_IS_POINT:
			return point_gravity;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE:
			return point_gravity_distance;
		case PhysicsServer3D::AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE:
			return linear_damp_mode;
		case PhysicsServer3D::AREA_PARAM_LINEAR_DAMP:
			return linear_damp;
		case PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE:
			return angular_damp_mode;
		case PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP:
			return angular_damp;
		case PhysicsServer3D::AREA_PARAM_PRIORITY:
			return priority;
		default:
			return Variant();
	}
}

void Box3DAreaImpl3D::set_param(PhysicsServer3D::AreaParameter p_param, const Variant& p_value) {
	switch (p_param) {
		case PhysicsServer3D::AREA_PARAM_GRAVITY_OVERRIDE_MODE:
			gravity_mode = (OverrideMode)(int)p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_GRAVITY:
			gravity = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_VECTOR:
			gravity_vector = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_IS_POINT:
			point_gravity = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE:
			point_gravity_distance = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE:
			linear_damp_mode = (OverrideMode)(int)p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_LINEAR_DAMP:
			linear_damp = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE:
			angular_damp_mode = (OverrideMode)(int)p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP:
			angular_damp = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_PRIORITY:
			priority = p_value;
			break;
		default:
			break;
	}
}

Vector3 Box3DAreaImpl3D::compute_gravity(const Vector3& p_position) const {
	if (!point_gravity) {
		// Godot does not normalize the direction, so its length scales the strength.
		return gravity_vector * gravity;
	}

	// When gravity is a point, Godot carries the center in gravity_vector, in area space.
	const Vector3 to_center = get_transform().xform(gravity_vector) - p_position;
	if (point_gravity_distance <= 0.0f) {
		return to_center.normalized() * gravity;
	}
	const real_t distance_squared = to_center.length_squared();
	if (distance_squared <= 0.0) {
		return Vector3();
	}
	const real_t strength = gravity * point_gravity_distance * point_gravity_distance / distance_squared;
	return to_center.normalized() * strength;
}

bool Box3DAreaImpl3D::add_overlap(const RID& p_other) {
	int32_t* count = overlaps.getptr(p_other);
	if (count == nullptr) {
		overlaps.insert(p_other, 1);
		return true;
	}
	(*count)++;
	return false;
}

bool Box3DAreaImpl3D::remove_overlap(const RID& p_other) {
	int32_t* count = overlaps.getptr(p_other);
	if (count == nullptr) {
		return false;
	}
	(*count)--;
	if (*count <= 0) {
		overlaps.erase(p_other);
		return true;
	}
	return false;
}
