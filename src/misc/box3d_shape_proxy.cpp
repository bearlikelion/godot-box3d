#include "box3d_shape_proxy.hpp"

#include "type_conversions.hpp"

#include "../shapes/box3d_box_shape_impl_3d.hpp"
#include "../shapes/box3d_capsule_shape_impl_3d.hpp"
#include "../shapes/box3d_convex_polygon_shape_impl_3d.hpp"
#include "../shapes/box3d_cylinder_shape_impl_3d.hpp"
#include "../shapes/box3d_shape_impl_3d.hpp"
#include "../shapes/box3d_sphere_shape_impl_3d.hpp"

#include <box3d/collision.h>

Box3DShapeProxy3D::Box3DShapeProxy3D(const Box3DShapeImpl3D* p_shape, const Transform3D& p_transform) {
	if (p_shape == nullptr) {
		return;
	}

	switch (p_shape->get_type()) {
		case PhysicsServer3D::SHAPE_SPHERE: {
			const auto* sphere = static_cast<const Box3DSphereShapeImpl3D*>(p_shape);
			points.resize(1);
			points[0] = godot_to_b3(p_transform.origin);
			proxy.points = points.ptr();
			proxy.count = 1;
			proxy.radius = (float)sphere->get_radius();
			supported = true;
			break;
		}

		case PhysicsServer3D::SHAPE_CAPSULE: {
			const auto* capsule = static_cast<const Box3DCapsuleShapeImpl3D*>(p_shape);
			const float radius = (float)capsule->get_radius();
			const float half_seg = MAX(0.0f, (float)capsule->get_height() * 0.5f - radius);
			points.resize(2);
			points[0] = godot_to_b3(p_transform.xform(Vector3(0, half_seg, 0)));
			points[1] = godot_to_b3(p_transform.xform(Vector3(0, -half_seg, 0)));
			proxy.points = points.ptr();
			proxy.count = 2;
			proxy.radius = radius;
			supported = true;
			break;
		}

		case PhysicsServer3D::SHAPE_BOX: {
			const auto* box = static_cast<const Box3DBoxShapeImpl3D*>(p_shape);
			const Vector3 half = box->get_half_extents();
			points.resize(8);
			int i = 0;
			for (int sx = -1; sx <= 1; sx += 2) {
				for (int sy = -1; sy <= 1; sy += 2) {
					for (int sz = -1; sz <= 1; sz += 2) {
						const Vector3 corner(half.x * sx, half.y * sy, half.z * sz);
						points[i++] = godot_to_b3(p_transform.xform(corner));
					}
				}
			}
			proxy.points = points.ptr();
			proxy.count = 8;
			proxy.radius = 0.0f;
			supported = true;
			break;
		}

		case PhysicsServer3D::SHAPE_CONVEX_POLYGON: {
			const auto* convex = static_cast<const Box3DConvexPolygonShapeImpl3D*>(p_shape);
			const b3HullData* hull = convex->get_hull();
			if (hull == nullptr) {
				break;
			}
			const b3Vec3* hull_points = b3GetHullPoints(hull);
			const int count = hull->vertexCount;
			if (hull_points == nullptr || count <= 0) {
				break;
			}
			points.resize(count);
			for (int i = 0; i < count; i++) {
				points[i] = godot_to_b3(p_transform.xform(b3_to_godot(hull_points[i])));
			}
			proxy.points = points.ptr();
			proxy.count = count;
			proxy.radius = 0.0f;
			supported = true;
			break;
		}

		case PhysicsServer3D::SHAPE_CYLINDER: {
			const auto* cylinder = static_cast<const Box3DCylinderShapeImpl3D*>(p_shape);
			const b3HullData* hull = cylinder->get_hull();
			if (hull == nullptr) {
				break;
			}
			const b3Vec3* hull_points = b3GetHullPoints(hull);
			const int count = hull->vertexCount;
			if (hull_points == nullptr || count <= 0) {
				break;
			}
			points.resize(count);
			for (int i = 0; i < count; i++) {
				points[i] = godot_to_b3(p_transform.xform(b3_to_godot(hull_points[i])));
			}
			proxy.points = points.ptr();
			proxy.count = count;
			proxy.radius = 0.0f;
			supported = true;
			break;
		}

		default:
			// Mesh/heightfield/world-boundary shapes have no finite point-cloud proxy.
			break;
	}
}
