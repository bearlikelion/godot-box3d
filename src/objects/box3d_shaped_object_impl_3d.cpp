#include "box3d_shaped_object_impl_3d.hpp"

#include "../misc/type_conversions.hpp"
#include "../shapes/box3d_box_shape_impl_3d.hpp"
#include "../shapes/box3d_capsule_shape_impl_3d.hpp"
#include "../shapes/box3d_concave_polygon_shape_impl_3d.hpp"
#include "../shapes/box3d_convex_polygon_shape_impl_3d.hpp"
#include "../shapes/box3d_heightmap_shape_impl_3d.hpp"
#include "../shapes/box3d_shape_impl_3d.hpp"
#include "../shapes/box3d_sphere_shape_impl_3d.hpp"
#include "../shapes/box3d_world_boundary_shape_impl_3d.hpp"
#include "../spaces/box3d_space_3d.hpp"

#include <box3d/box3d.h>
#include <box3d/collision.h>

namespace {

// Builds and attaches a live b3ShapeId for the given shape instance, dispatching by
// concrete Box3DShapeImpl3D subtype. p_is_sensor marks every shape on an area body as a
// sensor (per the plan's area-bridging design). Returns b3_nullShapeId if the shape has no
// buildable geometry yet (e.g. a convex hull with fewer than 4 points).
b3ShapeId create_box3d_shape(
		b3BodyId p_body_id,
		Box3DShapeInstance3D& p_instance,
		uint32_t p_layer,
		uint32_t p_mask,
		bool p_is_sensor,
		void* p_user_data,
		float p_friction,
		float p_restitution) {
	Box3DShapeImpl3D* shape = p_instance.get_shape();
	if (shape == nullptr || p_instance.is_disabled()) {
		return b3_nullShapeId;
	}

	b3ShapeDef def = b3DefaultShapeDef();
	def.userData = p_user_data;
	def.filter = godot_to_b3_filter(p_layer, p_mask);
	def.isSensor = p_is_sensor;
	// Box3D requires *both* sides of a sensor overlap to opt into sensor events (see
	// b3ShapeDef::enableSensorEvents doc comment: "This applies to sensors and
	// non-sensors."), so every shape enables it, not just the sensor side, or Godot's
	// Area3D body_entered/body_exited would never fire.
	def.enableSensorEvents = true;
	def.enableContactEvents = !p_is_sensor;
	def.density = 1.0f;
	def.baseMaterial = b3DefaultSurfaceMaterial();
	def.baseMaterial.friction = p_friction;
	def.baseMaterial.restitution = p_restitution;

	const Transform3D& local = p_instance.get_transform();
	const PhysicsServer3D::ShapeType type = shape->get_type();

	switch (type) {
		case PhysicsServer3D::SHAPE_SPHERE: {
			auto* sphere_shape = static_cast<Box3DSphereShapeImpl3D*>(shape);
			b3Sphere sphere;
			sphere.center = godot_to_b3(local.origin);
			sphere.radius = (float)sphere_shape->get_radius();
			return b3CreateSphereShape(p_body_id, &def, &sphere);
		}

		case PhysicsServer3D::SHAPE_CAPSULE: {
			auto* capsule_shape = static_cast<Box3DCapsuleShapeImpl3D*>(shape);
			const float radius = (float)capsule_shape->get_radius();
			const float height = (float)capsule_shape->get_height();
			const float half_seg = MAX(0.0f, height * 0.5f - radius);
			const Vector3 local_c1 = local.xform(Vector3(0, half_seg, 0));
			const Vector3 local_c2 = local.xform(Vector3(0, -half_seg, 0));
			b3Capsule capsule;
			capsule.center1 = godot_to_b3(local_c1);
			capsule.center2 = godot_to_b3(local_c2);
			capsule.radius = radius;
			return b3CreateCapsuleShape(p_body_id, &def, &capsule);
		}

		case PhysicsServer3D::SHAPE_BOX: {
			auto* box_shape = static_cast<Box3DBoxShapeImpl3D*>(shape);
			const Vector3 half = box_shape->get_half_extents();
			const b3Transform box_transform = godot_to_b3_transform(local);
			b3BoxHull box_hull = b3MakeTransformedBoxHull((float)half.x, (float)half.y, (float)half.z, box_transform);
			return b3CreateHullShape(p_body_id, &def, &box_hull.base);
		}

		case PhysicsServer3D::SHAPE_CONVEX_POLYGON: {
			auto* convex_shape = static_cast<Box3DConvexPolygonShapeImpl3D*>(shape);
			const b3HullData* hull = convex_shape->get_hull();
			if (hull == nullptr) {
				return b3_nullShapeId;
			}
			if (local.origin == Vector3() && local.basis.is_equal_approx(Basis())) {
				return b3CreateHullShape(p_body_id, &def, hull);
			}
			const b3Transform hull_transform = godot_to_b3_transform(local);
			return b3CreateTransformedHullShape(p_body_id, &def, hull, hull_transform, b3Vec3{1.0f, 1.0f, 1.0f});
		}

		case PhysicsServer3D::SHAPE_CONCAVE_POLYGON: {
			auto* mesh_shape = static_cast<Box3DConcavePolygonShapeImpl3D*>(shape);
			const b3MeshData* mesh = mesh_shape->get_mesh();
			if (mesh == nullptr) {
				return b3_nullShapeId;
			}
			def.invokeContactCreation = true;
			return b3CreateMeshShape(p_body_id, &def, mesh, b3Vec3{1.0f, 1.0f, 1.0f});
		}

		case PhysicsServer3D::SHAPE_HEIGHTMAP: {
			auto* height_shape = static_cast<Box3DHeightMapShapeImpl3D*>(shape);
			const b3HeightFieldData* height_field = height_shape->get_height_field();
			if (height_field == nullptr) {
				return b3_nullShapeId;
			}
			def.invokeContactCreation = true;
			return b3CreateHeightFieldShape(p_body_id, &def, height_field);
		}

		case PhysicsServer3D::SHAPE_WORLD_BOUNDARY: {
			auto* boundary_shape = static_cast<Box3DWorldBoundaryShapeImpl3D*>(shape);
			const Plane plane = boundary_shape->get_plane();
			const Vector3 normal = plane.normal.normalized();
			// Build a box hull plate centered `distance` along `normal` from the origin,
			// oriented so its local +Y faces along the plane normal.
			const Vector3 up(0, 1, 0);
			Basis basis;
			if (Math::abs(normal.dot(up)) > 0.999f) {
				basis = Basis::from_euler(Vector3(normal.y < 0 ? Math_PI : 0, 0, 0));
			} else {
				const Vector3 axis = up.cross(normal).normalized();
				const real_t angle = Math::acos(CLAMP(up.dot(normal), -1.0, 1.0));
				basis = Basis(axis, angle);
			}
			const Vector3 origin = normal * (real_t)(plane.d - Box3DWorldBoundaryShapeImpl3D::PLATE_HALF_THICKNESS);
			const Transform3D plate_transform(basis, origin);
			const Transform3D combined = local * plate_transform;
			const b3Transform box_transform = godot_to_b3_transform(combined);
			b3BoxHull box_hull = b3MakeTransformedBoxHull(
					Box3DWorldBoundaryShapeImpl3D::PLATE_HALF_EXTENT,
					Box3DWorldBoundaryShapeImpl3D::PLATE_HALF_THICKNESS,
					Box3DWorldBoundaryShapeImpl3D::PLATE_HALF_EXTENT,
					box_transform);
			return b3CreateHullShape(p_body_id, &def, &box_hull.base);
		}

		default: {
			ERR_FAIL_V_MSG(b3_nullShapeId, "Box3D: unsupported shape type used on a body.");
		}
	}
}

} // namespace

Box3DShapedObjectImpl3D::~Box3DShapedObjectImpl3D() {
	_destroy_body_id();
}

Transform3D Box3DShapedObjectImpl3D::get_transform() const {
	if (has_body_id()) {
		return b3_to_godot(b3Body_GetTransform(body_id));
	}
	return cached_transform;
}

void Box3DShapedObjectImpl3D::set_transform(const Transform3D& p_transform) {
	cached_transform = p_transform;
	if (has_body_id()) {
		const b3Transform t = godot_to_b3_transform(p_transform);
		b3Body_SetTransform(body_id, t.p, t.q);
	}
}

void Box3DShapedObjectImpl3D::add_shape(Box3DShapeImpl3D* p_shape, const Transform3D& p_transform, bool p_disabled) {
	Box3DShapeInstance3D instance(p_shape, p_transform);
	instance.set_disabled(p_disabled);
	instance.set_index((uint32_t)shapes.size());
	shapes.push_back(instance);

	if (has_body_id()) {
		_create_shape_instance(shapes[shapes.size() - 1]);
		_shapes_changed();
	}
}

void Box3DShapedObjectImpl3D::remove_shape(const Box3DShapeImpl3D* p_shape) {
	for (int32_t i = (int32_t)shapes.size() - 1; i >= 0; i--) {
		if (shapes[i].get_shape() == p_shape) {
			remove_shape(i);
		}
	}
}

void Box3DShapedObjectImpl3D::remove_shape(int32_t p_index) {
	ERR_FAIL_INDEX(p_index, (int32_t)shapes.size());
	_destroy_shape_instance(shapes[p_index]);
	shapes.remove_at(p_index);
	for (uint32_t i = p_index; i < shapes.size(); i++) {
		shapes[i].set_index(i);
	}
	_shapes_changed();
}

void Box3DShapedObjectImpl3D::set_shape(int32_t p_index, Box3DShapeImpl3D* p_shape) {
	ERR_FAIL_INDEX(p_index, (int32_t)shapes.size());
	_destroy_shape_instance(shapes[p_index]);
	shapes[p_index].set_shape(p_shape);
	if (has_body_id()) {
		_create_shape_instance(shapes[p_index]);
	}
	_shapes_changed();
}

void Box3DShapedObjectImpl3D::clear_shapes() {
	for (int32_t i = (int32_t)shapes.size() - 1; i >= 0; i--) {
		_destroy_shape_instance(shapes[i]);
	}
	shapes.clear();
	_shapes_changed();
}

Box3DShapeImpl3D* Box3DShapedObjectImpl3D::get_shape(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int32_t)shapes.size(), nullptr);
	return shapes[p_index].get_shape();
}

Transform3D Box3DShapedObjectImpl3D::get_shape_transform(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int32_t)shapes.size(), Transform3D());
	return shapes[p_index].get_transform();
}

void Box3DShapedObjectImpl3D::set_shape_transform(int32_t p_index, const Transform3D& p_transform) {
	ERR_FAIL_INDEX(p_index, (int32_t)shapes.size());
	Box3DShapeInstance3D& instance = shapes[p_index];
	instance.set_transform(p_transform);
	if (instance.has_shape_id()) {
		_destroy_shape_instance(instance);
		_create_shape_instance(instance);
		_shapes_changed();
	}
}

bool Box3DShapedObjectImpl3D::is_shape_disabled(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int32_t)shapes.size(), false);
	return shapes[p_index].is_disabled();
}

void Box3DShapedObjectImpl3D::set_shape_disabled(int32_t p_index, bool p_disabled) {
	ERR_FAIL_INDEX(p_index, (int32_t)shapes.size());
	Box3DShapeInstance3D& instance = shapes[p_index];
	if (instance.is_disabled() == p_disabled) {
		return;
	}
	instance.set_disabled(p_disabled);
	if (p_disabled) {
		_destroy_shape_instance(instance);
	} else if (has_body_id()) {
		_create_shape_instance(instance);
	}
	_shapes_changed();
}

void Box3DShapedObjectImpl3D::set_space(Box3DSpace3D* p_space) {
	if (space == p_space) {
		return;
	}

	if (space != nullptr) {
		cached_transform = get_transform();
		for (auto& instance : shapes) {
			_destroy_shape_instance(instance);
		}
		_destroy_body_id();
	}

	Box3DObjectImpl3D::set_space(p_space);

	if (space != nullptr) {
		body_id = _create_body_id(space->get_world_id());
		rebuild_shapes();
	}
}

void Box3DShapedObjectImpl3D::rebuild_shapes() {
	if (!has_body_id()) {
		return;
	}
	for (auto& instance : shapes) {
		if (!instance.is_disabled()) {
			_create_shape_instance(instance);
		}
	}
	_shapes_changed();
}

void Box3DShapedObjectImpl3D::_destroy_body_id() {
	if (has_body_id()) {
		for (auto& instance : shapes) {
			instance.set_shape_id(b3_nullShapeId);
		}
		b3DestroyBody(body_id);
		body_id = b3_nullBodyId;
	}
}

void Box3DShapedObjectImpl3D::_create_shape_instance(Box3DShapeInstance3D& p_instance) {
	if (p_instance.has_shape_id() || !has_body_id()) {
		return;
	}
	const b3ShapeId shape_id = create_box3d_shape(body_id, p_instance, collision_layer, collision_mask, _is_sensor_body(), &p_instance, _get_shape_friction(), _get_shape_restitution());
	p_instance.set_shape_id(shape_id);
}

void Box3DShapedObjectImpl3D::_destroy_shape_instance(Box3DShapeInstance3D& p_instance) {
	if (p_instance.has_shape_id()) {
		b3DestroyShape(p_instance.get_shape_id(), true);
		p_instance.set_shape_id(b3_nullShapeId);
	}
}
