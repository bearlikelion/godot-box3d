#pragma once

#include <godot_cpp/classes/physics_direct_space_state3d_extension.hpp>
#include <godot_cpp/classes/physics_server3d_extension_motion_result.hpp>
#include <godot_cpp/classes/physics_server3d_extension_shape_rest_info.hpp>
#include <godot_cpp/classes/physics_server3d_extension_shape_result.hpp>
#include <godot_cpp/templates/hash_set.hpp>

using namespace godot;

class Box3DShapedObjectImpl3D;
class Box3DSpace3D;

// Query overrides: raycasts and shape queries against one Box3DSpace3D. Box3D's own filter
// (b3QueryFilter) has no native per-query RID-exclude list, so a side-channel HashSet<RID>
// is checked from inside each b3*ResultFcn callback (see Box3DQueryFilter3D).
class Box3DPhysicsDirectSpaceState3D final : public PhysicsDirectSpaceState3DExtension {
	GDCLASS(Box3DPhysicsDirectSpaceState3D, PhysicsDirectSpaceState3DExtension)

public:
	void set_space(Box3DSpace3D* p_space) { space = p_space; }

	Box3DSpace3D* get_space() const { return space; }

	bool _intersect_ray(
			const Vector3& p_from,
			const Vector3& p_to,
			uint32_t p_collision_mask,
			bool p_collide_with_bodies,
			bool p_collide_with_areas,
			bool p_hit_from_inside,
			bool p_hit_back_faces,
			bool p_pick_ray,
			PhysicsServer3DExtensionRayResult* p_result) override;

	int32_t _intersect_point(
			const Vector3& p_position,
			uint32_t p_collision_mask,
			bool p_collide_with_bodies,
			bool p_collide_with_areas,
			PhysicsServer3DExtensionShapeResult* p_results,
			int32_t p_max_results) override;

	int32_t _intersect_shape(
			const RID& p_shape_rid,
			const Transform3D& p_transform,
			const Vector3& p_motion,
			double p_margin,
			uint32_t p_collision_mask,
			bool p_collide_with_bodies,
			bool p_collide_with_areas,
			PhysicsServer3DExtensionShapeResult* p_results,
			int32_t p_max_results) override;

	bool _cast_motion(
			const RID& p_shape_rid,
			const Transform3D& p_transform,
			const Vector3& p_motion,
			double p_margin,
			uint32_t p_collision_mask,
			bool p_collide_with_bodies,
			bool p_collide_with_areas,
			float* p_closest_safe,
			float* p_closest_unsafe,
			PhysicsServer3DExtensionShapeRestInfo* p_info) override;

	bool _collide_shape(
			const RID& p_shape_rid,
			const Transform3D& p_transform,
			const Vector3& p_motion,
			double p_margin,
			uint32_t p_collision_mask,
			bool p_collide_with_bodies,
			bool p_collide_with_areas,
			void* p_results,
			int32_t p_max_results,
			int32_t* p_result_count) override;

	bool _rest_info(
			const RID& p_shape_rid,
			const Transform3D& p_transform,
			const Vector3& p_motion,
			double p_margin,
			uint32_t p_collision_mask,
			bool p_collide_with_bodies,
			bool p_collide_with_areas,
			PhysicsServer3DExtensionShapeRestInfo* p_info) override;

	Vector3 _get_closest_point_to_object_volume(const RID& p_object, const Vector3& p_point) const override;

	// Used by Box3DPhysicsServer3DExtension::_body_test_motion.
	bool test_body_motion(
			Box3DShapedObjectImpl3D& p_body,
			const Transform3D& p_transform,
			const Vector3& p_motion,
			double p_margin,
			int32_t p_max_collisions,
			bool p_recovery_as_collision,
			PhysicsServer3DExtensionMotionResult* p_result) const;

protected:
	static void _bind_methods() {}

private:
	Box3DSpace3D* space = nullptr;
};
