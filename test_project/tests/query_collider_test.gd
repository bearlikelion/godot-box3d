class_name QueryColliderTest
extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	_check(ProjectSettings.get_setting("physics/3d/physics_engine", "") == "Box3D Physics", "test project requests the Box3D backend")
	_check(ClassDB.class_exists(&"Box3DPhysicsServer3D"), "Box3D extension is loaded")
	if failures > 0:
		quit(1)
		return

	var floor_body: StaticBody3D = StaticBody3D.new()
	var collision_shape: CollisionShape3D = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = Vector3(4, 1, 4)
	collision_shape.shape = box
	floor_body.add_child(collision_shape)
	root.add_child(floor_body)
	await physics_frame
	await physics_frame

	var space: PhysicsDirectSpaceState3D = root.world_3d.direct_space_state
	var ray_hit: Dictionary = space.intersect_ray(
			PhysicsRayQueryParameters3D.create(Vector3(0, 3, 0), Vector3(0, -3, 0)))
	_check(ray_hit.get("collider") == floor_body, "intersect_ray reports the hit node as collider")

	var point: PhysicsPointQueryParameters3D = PhysicsPointQueryParameters3D.new()
	point.position = Vector3.ZERO
	var point_hits: Array[Dictionary] = space.intersect_point(point)
	_check(not point_hits.is_empty() and point_hits[0].get("collider") == floor_body, "intersect_point reports the hit node as collider")

	var sphere: SphereShape3D = SphereShape3D.new()
	sphere.radius = 0.5
	var shape_query: PhysicsShapeQueryParameters3D = PhysicsShapeQueryParameters3D.new()
	shape_query.shape = sphere
	var shape_hits: Array[Dictionary] = space.intersect_shape(shape_query)
	_check(not shape_hits.is_empty() and shape_hits[0].get("collider") == floor_body, "intersect_shape reports the hit node as collider")

	floor_body.queue_free()
	if failures == 0:
		print("RESULT: PASS - query hits carry their collider object")
	else:
		print("RESULT: FAIL - ", failures, " query collider assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
