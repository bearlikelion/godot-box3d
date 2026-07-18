#include "box3d_physics_server_3d.hpp"

#include "../joints/box3d_hinge_joint_impl_3d.hpp"
#include "../joints/box3d_joint_impl_3d.hpp"
#include "../joints/box3d_pin_joint_impl_3d.hpp"
#include "../joints/box3d_slider_joint_impl_3d.hpp"
#include "../misc/type_conversions.hpp"
#include "../objects/box3d_area_impl_3d.hpp"
#include "../objects/box3d_body_impl_3d.hpp"
#include "../objects/box3d_physics_direct_body_state_3d.hpp"
#include "../objects/box3d_shaped_object_impl_3d.hpp"
#include "../shapes/box3d_box_shape_impl_3d.hpp"
#include "../shapes/box3d_capsule_shape_impl_3d.hpp"
#include "../shapes/box3d_concave_polygon_shape_impl_3d.hpp"
#include "../shapes/box3d_convex_polygon_shape_impl_3d.hpp"
#include "../shapes/box3d_heightmap_shape_impl_3d.hpp"
#include "../shapes/box3d_shape_impl_3d.hpp"
#include "../shapes/box3d_sphere_shape_impl_3d.hpp"
#include "../shapes/box3d_world_boundary_shape_impl_3d.hpp"
#include "../spaces/box3d_physics_direct_space_state_3d.hpp"
#include "../spaces/box3d_space_3d.hpp"

#include <box3d/box3d.h>

Box3DPhysicsServer3D* Box3DPhysicsServer3D::singleton = nullptr;

Box3DPhysicsServer3D::Box3DPhysicsServer3D() {
	singleton = this;
}

Box3DPhysicsServer3D::~Box3DPhysicsServer3D() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

Box3DShapedObjectImpl3D* Box3DPhysicsServer3D::_get_shaped_object(const RID& p_rid) const {
	if (Box3DBodyImpl3D* body = body_owner.get_or_null(p_rid)) {
		return body;
	}
	if (Box3DAreaImpl3D* area = area_owner.get_or_null(p_rid)) {
		return area;
	}
	return nullptr;
}

// --- Shapes ---

RID Box3DPhysicsServer3D::_world_boundary_shape_create() {
	auto* shape = memnew(Box3DWorldBoundaryShapeImpl3D);
	const RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID Box3DPhysicsServer3D::_separation_ray_shape_create() {
	ERR_FAIL_V_MSG(RID(), "Box3D: SeparationRayShape3D is not supported in this version of the Box3D extension.");
}

RID Box3DPhysicsServer3D::_sphere_shape_create() {
	auto* shape = memnew(Box3DSphereShapeImpl3D);
	const RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID Box3DPhysicsServer3D::_box_shape_create() {
	auto* shape = memnew(Box3DBoxShapeImpl3D);
	const RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID Box3DPhysicsServer3D::_capsule_shape_create() {
	auto* shape = memnew(Box3DCapsuleShapeImpl3D);
	const RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID Box3DPhysicsServer3D::_cylinder_shape_create() {
	ERR_FAIL_V_MSG(RID(), "Box3D: CylinderShape3D is not supported in this version of the Box3D extension.");
}

RID Box3DPhysicsServer3D::_convex_polygon_shape_create() {
	auto* shape = memnew(Box3DConvexPolygonShapeImpl3D);
	const RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID Box3DPhysicsServer3D::_concave_polygon_shape_create() {
	auto* shape = memnew(Box3DConcavePolygonShapeImpl3D);
	const RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID Box3DPhysicsServer3D::_heightmap_shape_create() {
	auto* shape = memnew(Box3DHeightMapShapeImpl3D);
	const RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID Box3DPhysicsServer3D::_custom_shape_create() {
	ERR_FAIL_V_MSG(RID(), "Box3D: custom shapes are not supported.");
}

void Box3DPhysicsServer3D::_shape_set_data(const RID& p_shape, const Variant& p_data) {
	Box3DShapeImpl3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	shape->set_data(p_data);
}

void Box3DPhysicsServer3D::_shape_set_custom_solver_bias(const RID& p_shape, double p_bias) {
	// No Box3D equivalent.
}

void Box3DPhysicsServer3D::_shape_set_margin(const RID& p_shape, double p_margin) {
	// No Box3D equivalent (Box3D shapes have no configurable collision margin).
}

double Box3DPhysicsServer3D::_shape_get_margin(const RID& p_shape) const {
	return 0.0;
}

PhysicsServer3D::ShapeType Box3DPhysicsServer3D::_shape_get_type(const RID& p_shape) const {
	Box3DShapeImpl3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, PhysicsServer3D::SHAPE_CUSTOM);
	return shape->get_type();
}

Variant Box3DPhysicsServer3D::_shape_get_data(const RID& p_shape) const {
	Box3DShapeImpl3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, Variant());
	return shape->get_data();
}

double Box3DPhysicsServer3D::_shape_get_custom_solver_bias(const RID& p_shape) const {
	return 0.0;
}

// --- Space ---

RID Box3DPhysicsServer3D::_space_create() {
	auto* space = memnew(Box3DSpace3D);
	const RID rid = space_owner.make_rid(space);
	space->set_rid(rid);

	const RID default_area_rid = _area_create();
	Box3DAreaImpl3D* default_area = area_owner.get_or_null(default_area_rid);
	ERR_FAIL_NULL_V(default_area, rid);
	default_area->set_default_area(true);
	default_area->set_space(space);
	space->set_default_area(default_area);

	return rid;
}

void Box3DPhysicsServer3D::_space_set_active(const RID& p_space, bool p_active) {
	Box3DSpace3D* space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);
	space->set_active(p_active);
	if (p_active) {
		active_spaces.insert(space);
	} else {
		active_spaces.erase(space);
	}
}

bool Box3DPhysicsServer3D::_space_is_active(const RID& p_space) const {
	Box3DSpace3D* space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, false);
	return space->is_active();
}

void Box3DPhysicsServer3D::_space_set_param(const RID& p_space, PhysicsServer3D::SpaceParameter p_param, double p_value) {
	Box3DSpace3D* space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);
	space->set_param(p_param, p_value);
}

double Box3DPhysicsServer3D::_space_get_param(const RID& p_space, PhysicsServer3D::SpaceParameter p_param) const {
	Box3DSpace3D* space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, 0.0);
	return space->get_param(p_param);
}

PhysicsDirectSpaceState3D* Box3DPhysicsServer3D::_space_get_direct_state(const RID& p_space) {
	Box3DSpace3D* space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, nullptr);
	return space->get_direct_state();
}

void Box3DPhysicsServer3D::_space_set_debug_contacts(const RID& p_space, int32_t p_max_contacts) {
	// Not implemented in v1 (no debug draw).
}

PackedVector3Array Box3DPhysicsServer3D::_space_get_contacts(const RID& p_space) const {
	return PackedVector3Array();
}

int32_t Box3DPhysicsServer3D::_space_get_contact_count(const RID& p_space) const {
	return 0;
}

// --- Areas ---

RID Box3DPhysicsServer3D::_area_create() {
	auto* area = memnew(Box3DAreaImpl3D);
	const RID rid = area_owner.make_rid(area);
	area->set_rid(rid);
	return rid;
}

void Box3DPhysicsServer3D::_area_set_space(const RID& p_area, const RID& p_space) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	Box3DSpace3D* old_space = area->get_space();
	if (old_space != nullptr) {
		old_space->unregister_area(area);
	}
	Box3DSpace3D* space = space_owner.get_or_null(p_space);
	area->set_space(space);
	if (space != nullptr) {
		space->register_area(area);
	}
}

RID Box3DPhysicsServer3D::_area_get_space(const RID& p_area) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, RID());
	Box3DSpace3D* space = area->get_space();
	return space != nullptr ? space->get_rid() : RID();
}

void Box3DPhysicsServer3D::_area_add_shape(const RID& p_area, const RID& p_shape, const Transform3D& p_transform, bool p_disabled) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	Box3DShapeImpl3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	shape->add_owner(area);
	area->add_shape(shape, p_transform, p_disabled);
}

void Box3DPhysicsServer3D::_area_set_shape(const RID& p_area, int32_t p_shape_idx, const RID& p_shape) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	Box3DShapeImpl3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	shape->add_owner(area);
	area->set_shape(p_shape_idx, shape);
}

void Box3DPhysicsServer3D::_area_set_shape_transform(const RID& p_area, int32_t p_shape_idx, const Transform3D& p_transform) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_shape_transform(p_shape_idx, p_transform);
}

void Box3DPhysicsServer3D::_area_set_shape_disabled(const RID& p_area, int32_t p_shape_idx, bool p_disabled) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_shape_disabled(p_shape_idx, p_disabled);
}

int32_t Box3DPhysicsServer3D::_area_get_shape_count(const RID& p_area) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);
	return area->get_shape_count();
}

RID Box3DPhysicsServer3D::_area_get_shape(const RID& p_area, int32_t p_shape_idx) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, RID());
	Box3DShapeImpl3D* shape = area->get_shape(p_shape_idx);
	return shape != nullptr ? shape->get_rid() : RID();
}

Transform3D Box3DPhysicsServer3D::_area_get_shape_transform(const RID& p_area, int32_t p_shape_idx) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, Transform3D());
	return area->get_shape_transform(p_shape_idx);
}

void Box3DPhysicsServer3D::_area_remove_shape(const RID& p_area, int32_t p_shape_idx) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->remove_shape(p_shape_idx);
}

void Box3DPhysicsServer3D::_area_clear_shapes(const RID& p_area) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->clear_shapes();
}

void Box3DPhysicsServer3D::_area_attach_object_instance_id(const RID& p_area, uint64_t p_id) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_instance_id(p_id);
}

uint64_t Box3DPhysicsServer3D::_area_get_object_instance_id(const RID& p_area) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);
	return area->get_instance_id();
}

void Box3DPhysicsServer3D::_area_set_param(const RID& p_area, PhysicsServer3D::AreaParameter p_param, const Variant& p_value) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_param(p_param, p_value);
}

void Box3DPhysicsServer3D::_area_set_transform(const RID& p_area, const Transform3D& p_transform) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_transform(p_transform);
}

Variant Box3DPhysicsServer3D::_area_get_param(const RID& p_area, PhysicsServer3D::AreaParameter p_param) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, Variant());
	return area->get_param(p_param);
}

Transform3D Box3DPhysicsServer3D::_area_get_transform(const RID& p_area) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, Transform3D());
	return area->get_transform();
}

void Box3DPhysicsServer3D::_area_set_collision_layer(const RID& p_area, uint32_t p_layer) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_collision_layer(p_layer);
}

uint32_t Box3DPhysicsServer3D::_area_get_collision_layer(const RID& p_area) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);
	return area->get_collision_layer();
}

void Box3DPhysicsServer3D::_area_set_collision_mask(const RID& p_area, uint32_t p_mask) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_collision_mask(p_mask);
}

uint32_t Box3DPhysicsServer3D::_area_get_collision_mask(const RID& p_area) const {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);
	return area->get_collision_mask();
}

void Box3DPhysicsServer3D::_area_set_monitorable(const RID& p_area, bool p_monitorable) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_monitorable(p_monitorable);
}

void Box3DPhysicsServer3D::_area_set_ray_pickable(const RID& p_area, bool p_enable) {
	// Not implemented in v1 (no editor picking support).
}

void Box3DPhysicsServer3D::_area_set_monitor_callback(const RID& p_area, const Callable& p_callback) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_body_monitor_callback(p_callback);
}

void Box3DPhysicsServer3D::_area_set_area_monitor_callback(const RID& p_area, const Callable& p_callback) {
	Box3DAreaImpl3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_area_monitor_callback(p_callback);
}

// --- Bodies ---

RID Box3DPhysicsServer3D::_body_create() {
	auto* body = memnew(Box3DBodyImpl3D);
	const RID rid = body_owner.make_rid(body);
	body->set_rid(rid);
	return rid;
}

void Box3DPhysicsServer3D::_body_set_space(const RID& p_body, const RID& p_space) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	Box3DSpace3D* old_space = body->get_space();
	if (old_space != nullptr) {
		old_space->unregister_body(body);
	}
	Box3DSpace3D* space = space_owner.get_or_null(p_space);
	body->set_space(space);
	if (space != nullptr) {
		space->register_body(body);
	}
}

RID Box3DPhysicsServer3D::_body_get_space(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, RID());
	Box3DSpace3D* space = body->get_space();
	return space != nullptr ? space->get_rid() : RID();
}

void Box3DPhysicsServer3D::_body_set_mode(const RID& p_body, PhysicsServer3D::BodyMode p_mode) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_mode(p_mode);
}

PhysicsServer3D::BodyMode Box3DPhysicsServer3D::_body_get_mode(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, PhysicsServer3D::BODY_MODE_STATIC);
	return body->get_mode();
}

void Box3DPhysicsServer3D::_body_add_shape(const RID& p_body, const RID& p_shape, const Transform3D& p_transform, bool p_disabled) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	Box3DShapeImpl3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	shape->add_owner(body);
	body->add_shape(shape, p_transform, p_disabled);
}

void Box3DPhysicsServer3D::_body_set_shape(const RID& p_body, int32_t p_shape_idx, const RID& p_shape) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	Box3DShapeImpl3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	shape->add_owner(body);
	body->set_shape(p_shape_idx, shape);
}

void Box3DPhysicsServer3D::_body_set_shape_transform(const RID& p_body, int32_t p_shape_idx, const Transform3D& p_transform) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_shape_transform(p_shape_idx, p_transform);
}

void Box3DPhysicsServer3D::_body_set_shape_disabled(const RID& p_body, int32_t p_shape_idx, bool p_disabled) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_shape_disabled(p_shape_idx, p_disabled);
}

int32_t Box3DPhysicsServer3D::_body_get_shape_count(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_shape_count();
}

RID Box3DPhysicsServer3D::_body_get_shape(const RID& p_body, int32_t p_shape_idx) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, RID());
	Box3DShapeImpl3D* shape = body->get_shape(p_shape_idx);
	return shape != nullptr ? shape->get_rid() : RID();
}

Transform3D Box3DPhysicsServer3D::_body_get_shape_transform(const RID& p_body, int32_t p_shape_idx) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Transform3D());
	return body->get_shape_transform(p_shape_idx);
}

void Box3DPhysicsServer3D::_body_remove_shape(const RID& p_body, int32_t p_shape_idx) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->remove_shape(p_shape_idx);
}

void Box3DPhysicsServer3D::_body_clear_shapes(const RID& p_body) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->clear_shapes();
}

void Box3DPhysicsServer3D::_body_attach_object_instance_id(const RID& p_body, uint64_t p_id) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_instance_id(p_id);
}

uint64_t Box3DPhysicsServer3D::_body_get_object_instance_id(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_instance_id();
}

void Box3DPhysicsServer3D::_body_set_enable_continuous_collision_detection(const RID& p_body, bool p_enable) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_ccd_enabled(p_enable);
}

bool Box3DPhysicsServer3D::_body_is_continuous_collision_detection_enabled(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);
	return body->is_ccd_enabled();
}

void Box3DPhysicsServer3D::_body_set_collision_layer(const RID& p_body, uint32_t p_layer) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_collision_layer(p_layer);
}

uint32_t Box3DPhysicsServer3D::_body_get_collision_layer(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_collision_layer();
}

void Box3DPhysicsServer3D::_body_set_collision_mask(const RID& p_body, uint32_t p_mask) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_collision_mask(p_mask);
}

uint32_t Box3DPhysicsServer3D::_body_get_collision_mask(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_collision_mask();
}

void Box3DPhysicsServer3D::_body_set_collision_priority(const RID& p_body, double p_priority) {
	// No Box3D equivalent (collision priority affects only the built-in solver's
	// impulse weighting, which Box3D does not expose).
}

double Box3DPhysicsServer3D::_body_get_collision_priority(const RID& p_body) const {
	return 1.0;
}

void Box3DPhysicsServer3D::_body_set_user_flags(const RID& p_body, uint32_t p_flags) {
	// Not used by Box3D.
}

uint32_t Box3DPhysicsServer3D::_body_get_user_flags(const RID& p_body) const {
	return 0;
}

void Box3DPhysicsServer3D::_body_set_param(const RID& p_body, PhysicsServer3D::BodyParameter p_param, const Variant& p_value) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	switch (p_param) {
		case PhysicsServer3D::BODY_PARAM_BOUNCE:
			body->set_bounce(p_value);
			break;
		case PhysicsServer3D::BODY_PARAM_FRICTION:
			body->set_friction(p_value);
			break;
		case PhysicsServer3D::BODY_PARAM_MASS:
			body->set_mass(p_value);
			break;
		case PhysicsServer3D::BODY_PARAM_INERTIA:
			body->set_inertia(p_value);
			break;
		case PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS:
			body->set_center_of_mass(p_value);
			break;
		case PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE:
			body->set_gravity_scale(p_value);
			break;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE:
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE:
			// Box3D always applies damping as a simple replace mode; COMBINE mode has no
			// direct equivalent and is treated the same as REPLACE.
			break;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP:
			body->set_linear_damping(p_value);
			break;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP:
			body->set_angular_damping(p_value);
			break;
		default:
			break;
	}
}

Variant Box3DPhysicsServer3D::_body_get_param(const RID& p_body, PhysicsServer3D::BodyParameter p_param) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Variant());
	switch (p_param) {
		case PhysicsServer3D::BODY_PARAM_BOUNCE:
			return body->get_bounce();
		case PhysicsServer3D::BODY_PARAM_FRICTION:
			return body->get_friction();
		case PhysicsServer3D::BODY_PARAM_MASS:
			return body->get_mass();
		case PhysicsServer3D::BODY_PARAM_INERTIA:
			return body->get_inertia();
		case PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS:
			return body->get_center_of_mass();
		case PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE:
			return body->get_gravity_scale();
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE:
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE:
			return PhysicsServer3D::BODY_DAMP_MODE_COMBINE;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP:
			return body->get_linear_damping();
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP:
			return body->get_angular_damping();
		default:
			return Variant();
	}
}

void Box3DPhysicsServer3D::_body_reset_mass_properties(const RID& p_body) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_mass_from_shapes();
}

void Box3DPhysicsServer3D::_body_set_state(const RID& p_body, PhysicsServer3D::BodyState p_state, const Variant& p_value) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM:
			body->set_transform(p_value);
			break;
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY:
			body->set_linear_velocity(p_value);
			break;
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY:
			body->set_angular_velocity(p_value);
			break;
		case PhysicsServer3D::BODY_STATE_SLEEPING:
			body->set_sleeping(p_value);
			break;
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP:
			body->set_sleep_enabled(p_value);
			break;
		default:
			break;
	}
}

Variant Box3DPhysicsServer3D::_body_get_state(const RID& p_body, PhysicsServer3D::BodyState p_state) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Variant());
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM:
			return body->get_transform();
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY:
			return body->get_linear_velocity();
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY:
			return body->get_angular_velocity();
		case PhysicsServer3D::BODY_STATE_SLEEPING:
			return body->is_sleeping();
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP:
			return body->is_sleep_enabled();
		default:
			return Variant();
	}
}

void Box3DPhysicsServer3D::_body_apply_central_impulse(const RID& p_body, const Vector3& p_impulse) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_central_impulse(p_impulse);
}

void Box3DPhysicsServer3D::_body_apply_impulse(const RID& p_body, const Vector3& p_impulse, const Vector3& p_position) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_impulse(p_impulse, p_position);
}

void Box3DPhysicsServer3D::_body_apply_torque_impulse(const RID& p_body, const Vector3& p_impulse) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_torque_impulse(p_impulse);
}

void Box3DPhysicsServer3D::_body_apply_central_force(const RID& p_body, const Vector3& p_force) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_central_force(p_force);
}

void Box3DPhysicsServer3D::_body_apply_force(const RID& p_body, const Vector3& p_force, const Vector3& p_position) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_force(p_force, p_position);
}

void Box3DPhysicsServer3D::_body_apply_torque(const RID& p_body, const Vector3& p_torque) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_torque(p_torque);
}

void Box3DPhysicsServer3D::_body_add_constant_central_force(const RID& p_body, const Vector3& p_force) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->add_constant_central_force(p_force);
}

void Box3DPhysicsServer3D::_body_add_constant_force(const RID& p_body, const Vector3& p_force, const Vector3& p_position) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->add_constant_force(p_force, p_position);
}

void Box3DPhysicsServer3D::_body_add_constant_torque(const RID& p_body, const Vector3& p_torque) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->add_constant_torque(p_torque);
}

void Box3DPhysicsServer3D::_body_set_constant_force(const RID& p_body, const Vector3& p_force) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_constant_force(p_force);
}

Vector3 Box3DPhysicsServer3D::_body_get_constant_force(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Vector3());
	return body->get_constant_force();
}

void Box3DPhysicsServer3D::_body_set_constant_torque(const RID& p_body, const Vector3& p_torque) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_constant_torque(p_torque);
}

Vector3 Box3DPhysicsServer3D::_body_get_constant_torque(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Vector3());
	return body->get_constant_torque();
}

void Box3DPhysicsServer3D::_body_set_axis_velocity(const RID& p_body, const Vector3& p_axis_velocity) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	const Vector3 axis = p_axis_velocity.normalized();
	Vector3 velocity = body->get_linear_velocity();
	velocity -= axis * axis.dot(velocity);
	velocity += p_axis_velocity;
	body->set_linear_velocity(velocity);
}

void Box3DPhysicsServer3D::_body_set_axis_lock(const RID& p_body, PhysicsServer3D::BodyAxis p_axis, bool p_lock) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_axis_lock(p_axis, p_lock);
}

bool Box3DPhysicsServer3D::_body_is_axis_locked(const RID& p_body, PhysicsServer3D::BodyAxis p_axis) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);
	return body->get_axis_lock(p_axis);
}

void Box3DPhysicsServer3D::_body_add_collision_exception(const RID& p_body, const RID& p_excepted_body) {
	// v1: full per-pair collision exception lists are a non-goal. The common single-group
	// case is handled via b3Filter.groupIndex instead (see the plan's Non-goals section).
	WARN_PRINT_ONCE("Box3D: per-pair collision exceptions are not implemented in this version; use collision layers/masks instead.");
}

void Box3DPhysicsServer3D::_body_remove_collision_exception(const RID& p_body, const RID& p_excepted_body) {
	// See _body_add_collision_exception.
}

TypedArray<RID> Box3DPhysicsServer3D::_body_get_collision_exceptions(const RID& p_body) const {
	return TypedArray<RID>();
}

void Box3DPhysicsServer3D::_body_set_max_contacts_reported(const RID& p_body, int32_t p_amount) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_max_contacts_reported(p_amount);
}

int32_t Box3DPhysicsServer3D::_body_get_max_contacts_reported(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_max_contacts_reported();
}

void Box3DPhysicsServer3D::_body_set_contacts_reported_depth_threshold(const RID& p_body, double p_threshold) {
	// No Box3D equivalent.
}

double Box3DPhysicsServer3D::_body_get_contacts_reported_depth_threshold(const RID& p_body) const {
	return 0.0;
}

void Box3DPhysicsServer3D::_body_set_omit_force_integration(const RID& p_body, bool p_enable) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_omit_force_integration(p_enable);
}

bool Box3DPhysicsServer3D::_body_is_omitting_force_integration(const RID& p_body) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);
	return body->is_omitting_force_integration();
}

void Box3DPhysicsServer3D::_body_set_state_sync_callback(const RID& p_body, const Callable& p_callable) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_state_sync_callback(p_callable);
}

void Box3DPhysicsServer3D::_body_set_force_integration_callback(const RID& p_body, const Callable& p_callable, const Variant& p_userdata) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_force_integration_callback(p_callable, p_userdata);
}

void Box3DPhysicsServer3D::_body_set_ray_pickable(const RID& p_body, bool p_enable) {
	// Not implemented in v1 (no editor picking support).
}

bool Box3DPhysicsServer3D::_body_test_motion(
		const RID& p_body,
		const Transform3D& p_from,
		const Vector3& p_motion,
		double p_margin,
		int32_t p_max_collisions,
		bool p_collide_separation_ray,
		bool p_recovery_as_collision,
		PhysicsServer3DExtensionMotionResult* p_result) const {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);
	Box3DSpace3D* space = body->get_space();
	ERR_FAIL_NULL_V(space, false);

	return space->get_direct_state()->test_body_motion(*body, p_from, p_motion, p_margin, p_max_collisions, p_recovery_as_collision, p_result);
}

PhysicsDirectBodyState3D* Box3DPhysicsServer3D::_body_get_direct_state(const RID& p_body) {
	Box3DBodyImpl3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, nullptr);
	if (!body->has_body_id()) {
		return nullptr;
	}
	return body->get_direct_state_or_null();
}

// --- Joints ---

RID Box3DPhysicsServer3D::_joint_create() {
	// Placeholder RID: the concrete Box3DJointImpl3D is only constructed once
	// _joint_make_pin/_joint_make_hinge/_joint_make_slider is called (see below), since
	// only then are the joint type and both body RIDs known.
	const RID rid = joint_owner.make_rid(nullptr);
	return rid;
}

void Box3DPhysicsServer3D::_joint_clear(const RID& p_joint) {
	Box3DJointImpl3D* joint = joint_owner.get_or_null(p_joint);
	if (joint != nullptr) {
		memdelete(joint);
		joint_owner.replace(p_joint, nullptr);
	}
}

void Box3DPhysicsServer3D::_joint_make_pin(const RID& p_joint, const RID& p_body_a, const Vector3& p_local_a, const RID& p_body_b, const Vector3& p_local_b) {
	_joint_clear(p_joint);

	Box3DBodyImpl3D* body_a = body_owner.get_or_null(p_body_a);
	Box3DBodyImpl3D* body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_NULL(body_a);
	ERR_FAIL_NULL(body_b);

	auto* joint = memnew(Box3DPinJointImpl3D(body_a, body_b, Transform3D(Basis(), p_local_a), Transform3D(Basis(), p_local_b)));
	joint->set_rid(p_joint);
	joint_owner.replace(p_joint, joint);
	joint->rebuild();
}

void Box3DPhysicsServer3D::_pin_joint_set_param(const RID& p_joint, PhysicsServer3D::PinJointParam p_param, double p_value) {
	auto* joint = dynamic_cast<Box3DPinJointImpl3D*>(joint_owner.get_or_null(p_joint));
	ERR_FAIL_NULL(joint);
	joint->set_param(p_param, p_value);
}

double Box3DPhysicsServer3D::_pin_joint_get_param(const RID& p_joint, PhysicsServer3D::PinJointParam p_param) const {
	auto* joint = dynamic_cast<Box3DPinJointImpl3D*>(joint_owner.get_or_null(p_joint));
	ERR_FAIL_NULL_V(joint, 0.0);
	return joint->get_param(p_param);
}

void Box3DPhysicsServer3D::_pin_joint_set_local_a(const RID& p_joint, const Vector3& p_local_a) {
	WARN_PRINT_ONCE("Box3D: changing a PinJoint3D's local anchor after creation is not supported; recreate the joint instead.");
}

Vector3 Box3DPhysicsServer3D::_pin_joint_get_local_a(const RID& p_joint) const {
	return Vector3();
}

void Box3DPhysicsServer3D::_pin_joint_set_local_b(const RID& p_joint, const Vector3& p_local_b) {
	WARN_PRINT_ONCE("Box3D: changing a PinJoint3D's local anchor after creation is not supported; recreate the joint instead.");
}

Vector3 Box3DPhysicsServer3D::_pin_joint_get_local_b(const RID& p_joint) const {
	return Vector3();
}

void Box3DPhysicsServer3D::_joint_make_hinge(const RID& p_joint, const RID& p_body_a, const Transform3D& p_hinge_a, const RID& p_body_b, const Transform3D& p_hinge_b) {
	_joint_clear(p_joint);

	Box3DBodyImpl3D* body_a = body_owner.get_or_null(p_body_a);
	Box3DBodyImpl3D* body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_NULL(body_a);
	ERR_FAIL_NULL(body_b);

	// Box3D's revolute joint rotates about the LOCAL Z axis of the joint frames; Godot's
	// HingeJoint3D convention uses local Z as the hinge axis too (the incoming p_hinge_a/b
	// transforms are the joint frames as Godot core builds them for HingeJoint3D), so no
	// remap is required here.
	auto* joint = memnew(Box3DHingeJointImpl3D(body_a, body_b, p_hinge_a, p_hinge_b));
	joint->set_rid(p_joint);
	joint_owner.replace(p_joint, joint);
	joint->rebuild();
}

void Box3DPhysicsServer3D::_joint_make_hinge_simple(const RID& p_joint, const RID& p_body_a, const Vector3& p_pivot_a, const Vector3& p_axis_a, const RID& p_body_b, const Vector3& p_pivot_b, const Vector3& p_axis_b) {
	_joint_clear(p_joint);

	Box3DBodyImpl3D* body_a = body_owner.get_or_null(p_body_a);
	Box3DBodyImpl3D* body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_NULL(body_a);
	ERR_FAIL_NULL(body_b);

	// Build a joint-frame basis that maps local Z to the given hinge axis (shortest-arc
	// rotation from +Z), since Box3D's revolute joint always rotates about local Z.
	const Vector3 z_axis(0, 0, 1);
	const Quaternion rotation_a = Quaternion(z_axis, p_axis_a.normalized());
	const Quaternion rotation_b = Quaternion(z_axis, p_axis_b.normalized());

	const Transform3D frame_a(Basis(rotation_a), p_pivot_a);
	const Transform3D frame_b(Basis(rotation_b), p_pivot_b);

	auto* joint = memnew(Box3DHingeJointImpl3D(body_a, body_b, frame_a, frame_b));
	joint->set_rid(p_joint);
	joint_owner.replace(p_joint, joint);
	joint->rebuild();
}

void Box3DPhysicsServer3D::_hinge_joint_set_param(const RID& p_joint, PhysicsServer3D::HingeJointParam p_param, double p_value) {
	auto* joint = dynamic_cast<Box3DHingeJointImpl3D*>(joint_owner.get_or_null(p_joint));
	ERR_FAIL_NULL(joint);
	joint->set_param(p_param, p_value);
}

double Box3DPhysicsServer3D::_hinge_joint_get_param(const RID& p_joint, PhysicsServer3D::HingeJointParam p_param) const {
	auto* joint = dynamic_cast<Box3DHingeJointImpl3D*>(joint_owner.get_or_null(p_joint));
	ERR_FAIL_NULL_V(joint, 0.0);
	return joint->get_param(p_param);
}

void Box3DPhysicsServer3D::_hinge_joint_set_flag(const RID& p_joint, PhysicsServer3D::HingeJointFlag p_flag, bool p_enabled) {
	auto* joint = dynamic_cast<Box3DHingeJointImpl3D*>(joint_owner.get_or_null(p_joint));
	ERR_FAIL_NULL(joint);
	joint->set_flag(p_flag, p_enabled);
}

bool Box3DPhysicsServer3D::_hinge_joint_get_flag(const RID& p_joint, PhysicsServer3D::HingeJointFlag p_flag) const {
	auto* joint = dynamic_cast<Box3DHingeJointImpl3D*>(joint_owner.get_or_null(p_joint));
	ERR_FAIL_NULL_V(joint, false);
	return joint->get_flag(p_flag);
}

void Box3DPhysicsServer3D::_joint_make_slider(const RID& p_joint, const RID& p_body_a, const Transform3D& p_local_ref_a, const RID& p_body_b, const Transform3D& p_local_ref_b) {
	_joint_clear(p_joint);

	Box3DBodyImpl3D* body_a = body_owner.get_or_null(p_body_a);
	Box3DBodyImpl3D* body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_NULL(body_a);
	ERR_FAIL_NULL(body_b);

	// Box3D's prismatic joint slides along the LOCAL X axis of local frame A, matching
	// Godot's SliderJoint3D convention (which also slides along local X), so no axis
	// remap is required here.
	auto* joint = memnew(Box3DSliderJointImpl3D(body_a, body_b, p_local_ref_a, p_local_ref_b));
	joint->set_rid(p_joint);
	joint_owner.replace(p_joint, joint);
	joint->rebuild();
}

void Box3DPhysicsServer3D::_slider_joint_set_param(const RID& p_joint, PhysicsServer3D::SliderJointParam p_param, double p_value) {
	auto* joint = dynamic_cast<Box3DSliderJointImpl3D*>(joint_owner.get_or_null(p_joint));
	ERR_FAIL_NULL(joint);
	joint->set_param(p_param, p_value);
}

double Box3DPhysicsServer3D::_slider_joint_get_param(const RID& p_joint, PhysicsServer3D::SliderJointParam p_param) const {
	auto* joint = dynamic_cast<Box3DSliderJointImpl3D*>(joint_owner.get_or_null(p_joint));
	ERR_FAIL_NULL_V(joint, 0.0);
	return joint->get_param(p_param);
}

void Box3DPhysicsServer3D::_joint_make_cone_twist(const RID& p_joint, const RID& p_body_a, const Transform3D& p_local_ref_a, const RID& p_body_b, const Transform3D& p_local_ref_b) {
	ERR_FAIL_MSG("Box3D: ConeTwistJoint3D is not supported in this version of the Box3D extension.");
}

void Box3DPhysicsServer3D::_cone_twist_joint_set_param(const RID& p_joint, PhysicsServer3D::ConeTwistJointParam p_param, double p_value) {
}

double Box3DPhysicsServer3D::_cone_twist_joint_get_param(const RID& p_joint, PhysicsServer3D::ConeTwistJointParam p_param) const {
	return 0.0;
}

void Box3DPhysicsServer3D::_joint_make_generic_6dof(const RID& p_joint, const RID& p_body_a, const Transform3D& p_local_ref_a, const RID& p_body_b, const Transform3D& p_local_ref_b) {
	ERR_FAIL_MSG("Box3D: Generic6DOFJoint3D is not supported in this version of the Box3D extension.");
}

void Box3DPhysicsServer3D::_generic_6dof_joint_set_param(const RID& p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param, double p_value) {
}

double Box3DPhysicsServer3D::_generic_6dof_joint_get_param(const RID& p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param) const {
	return 0.0;
}

void Box3DPhysicsServer3D::_generic_6dof_joint_set_flag(const RID& p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag, bool p_enable) {
}

bool Box3DPhysicsServer3D::_generic_6dof_joint_get_flag(const RID& p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag) const {
	return false;
}

PhysicsServer3D::JointType Box3DPhysicsServer3D::_joint_get_type(const RID& p_joint) const {
	Box3DJointImpl3D* joint = joint_owner.get_or_null(p_joint);
	if (joint == nullptr) {
		return PhysicsServer3D::JOINT_TYPE_MAX;
	}
	return joint->get_type();
}

void Box3DPhysicsServer3D::_joint_set_solver_priority(const RID& p_joint, int32_t p_priority) {
	// No Box3D equivalent.
}

int32_t Box3DPhysicsServer3D::_joint_get_solver_priority(const RID& p_joint) const {
	return 0;
}

void Box3DPhysicsServer3D::_joint_disable_collisions_between_bodies(const RID& p_joint, bool p_disable) {
	Box3DJointImpl3D* joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_collision_disabled(p_disable);
}

bool Box3DPhysicsServer3D::_joint_is_disabled_collisions_between_bodies(const RID& p_joint) const {
	Box3DJointImpl3D* joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);
	return joint->is_collision_disabled();
}

// --- Soft bodies (non-goal) ---

RID Box3DPhysicsServer3D::_soft_body_create() {
	ERR_FAIL_V_MSG(RID(), "Box3D: SoftBody3D is not supported in this version of the Box3D extension.");
}

void Box3DPhysicsServer3D::_soft_body_update_rendering_server(const RID& p_body, PhysicsServer3DRenderingServerHandler* p_rendering_server_handler) {
}

void Box3DPhysicsServer3D::_soft_body_set_space(const RID& p_body, const RID& p_space) {
}

RID Box3DPhysicsServer3D::_soft_body_get_space(const RID& p_body) const {
	return RID();
}

void Box3DPhysicsServer3D::_soft_body_set_ray_pickable(const RID& p_body, bool p_enable) {
}

void Box3DPhysicsServer3D::_soft_body_set_collision_layer(const RID& p_body, uint32_t p_layer) {
}

uint32_t Box3DPhysicsServer3D::_soft_body_get_collision_layer(const RID& p_body) const {
	return 0;
}

void Box3DPhysicsServer3D::_soft_body_set_collision_mask(const RID& p_body, uint32_t p_mask) {
}

uint32_t Box3DPhysicsServer3D::_soft_body_get_collision_mask(const RID& p_body) const {
	return 0;
}

void Box3DPhysicsServer3D::_soft_body_add_collision_exception(const RID& p_body, const RID& p_excepted_body) {
}

void Box3DPhysicsServer3D::_soft_body_remove_collision_exception(const RID& p_body, const RID& p_excepted_body) {
}

TypedArray<RID> Box3DPhysicsServer3D::_soft_body_get_collision_exceptions(const RID& p_body) const {
	return TypedArray<RID>();
}

void Box3DPhysicsServer3D::_soft_body_set_state(const RID& p_body, PhysicsServer3D::BodyState p_state, const Variant& p_variant) {
}

Variant Box3DPhysicsServer3D::_soft_body_get_state(const RID& p_body, PhysicsServer3D::BodyState p_state) const {
	return Variant();
}

void Box3DPhysicsServer3D::_soft_body_set_transform(const RID& p_body, const Transform3D& p_transform) {
}

void Box3DPhysicsServer3D::_soft_body_set_simulation_precision(const RID& p_body, int32_t p_simulation_precision) {
}

int32_t Box3DPhysicsServer3D::_soft_body_get_simulation_precision(const RID& p_body) const {
	return 0;
}

void Box3DPhysicsServer3D::_soft_body_set_total_mass(const RID& p_body, double p_total_mass) {
}

double Box3DPhysicsServer3D::_soft_body_get_total_mass(const RID& p_body) const {
	return 0.0;
}

void Box3DPhysicsServer3D::_soft_body_set_linear_stiffness(const RID& p_body, double p_stiffness) {
}

double Box3DPhysicsServer3D::_soft_body_get_linear_stiffness(const RID& p_body) const {
	return 0.0;
}

void Box3DPhysicsServer3D::_soft_body_set_pressure_coefficient(const RID& p_body, double p_pressure_coefficient) {
}

double Box3DPhysicsServer3D::_soft_body_get_pressure_coefficient(const RID& p_body) const {
	return 0.0;
}

void Box3DPhysicsServer3D::_soft_body_set_damping_coefficient(const RID& p_body, double p_damping_coefficient) {
}

double Box3DPhysicsServer3D::_soft_body_get_damping_coefficient(const RID& p_body) const {
	return 0.0;
}

void Box3DPhysicsServer3D::_soft_body_set_drag_coefficient(const RID& p_body, double p_drag_coefficient) {
}

double Box3DPhysicsServer3D::_soft_body_get_drag_coefficient(const RID& p_body) const {
	return 0.0;
}

void Box3DPhysicsServer3D::_soft_body_set_mesh(const RID& p_body, const RID& p_mesh) {
}

AABB Box3DPhysicsServer3D::_soft_body_get_bounds(const RID& p_body) const {
	return AABB();
}

void Box3DPhysicsServer3D::_soft_body_move_point(const RID& p_body, int32_t p_point_index, const Vector3& p_global_position) {
}

Vector3 Box3DPhysicsServer3D::_soft_body_get_point_global_position(const RID& p_body, int32_t p_point_index) const {
	return Vector3();
}

void Box3DPhysicsServer3D::_soft_body_remove_all_pinned_points(const RID& p_body) {
}

void Box3DPhysicsServer3D::_soft_body_pin_point(const RID& p_body, int32_t p_point_index, bool p_pin) {
}

bool Box3DPhysicsServer3D::_soft_body_is_point_pinned(const RID& p_body, int32_t p_point_index) const {
	return false;
}

// --- Lifecycle ---

void Box3DPhysicsServer3D::_free_rid(const RID& p_rid) {
	// Joints and shapes free before bodies/areas that reference them; bodies/areas free
	// before shapes they hold (mirrors JoltPhysicsServer3DExtension::_free_rid's ordering).
	if (Box3DJointImpl3D* joint = joint_owner.get_or_null(p_rid)) {
		memdelete(joint);
		joint_owner.free(p_rid);
		return;
	}

	if (Box3DBodyImpl3D* body = body_owner.get_or_null(p_rid)) {
		if (Box3DSpace3D* space = body->get_space()) {
			space->unregister_body(body);
		}
		memdelete(body);
		body_owner.free(p_rid);
		return;
	}

	if (Box3DAreaImpl3D* area = area_owner.get_or_null(p_rid)) {
		if (Box3DSpace3D* space = area->get_space()) {
			space->unregister_area(area);
		}
		memdelete(area);
		area_owner.free(p_rid);
		return;
	}

	if (Box3DShapeImpl3D* shape = shape_owner.get_or_null(p_rid)) {
		memdelete(shape);
		shape_owner.free(p_rid);
		return;
	}

	if (Box3DSpace3D* space = space_owner.get_or_null(p_rid)) {
		Box3DAreaImpl3D* default_area = space->get_default_area();
		if (default_area != nullptr) {
			const RID default_area_rid = default_area->get_rid();
			space->unregister_area(default_area);
			memdelete(default_area);
			area_owner.free(default_area_rid);
		}
		active_spaces.erase(space);
		memdelete(space);
		space_owner.free(p_rid);
		return;
	}
}

void Box3DPhysicsServer3D::_set_active(bool p_active) {
	active = p_active;
}

void Box3DPhysicsServer3D::_init() {
}

void Box3DPhysicsServer3D::_step(double p_step) {
	if (!active) {
		return;
	}
	for (Box3DSpace3D* space : active_spaces) {
		space->step((float)p_step);
	}
}

void Box3DPhysicsServer3D::_sync() {
}

void Box3DPhysicsServer3D::_flush_queries() {
	if (!active) {
		return;
	}
	for (Box3DSpace3D* space : active_spaces) {
		space->flush_queries();
	}
}

void Box3DPhysicsServer3D::_end_sync() {
}

void Box3DPhysicsServer3D::_finish() {
}

bool Box3DPhysicsServer3D::_is_flushing_queries() const {
	for (Box3DSpace3D* space : active_spaces) {
		if (space->is_flushing_queries()) {
			return true;
		}
	}
	return false;
}

int32_t Box3DPhysicsServer3D::_get_process_info(PhysicsServer3D::ProcessInfo p_process_info) {
	return 0;
}
