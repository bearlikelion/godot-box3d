extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var target: StaticBody3D = StaticBody3D.new()
	var collision: CollisionShape3D = CollisionShape3D.new()
	var cylinder: CylinderShape3D = CylinderShape3D.new()
	cylinder.radius = 0.75
	cylinder.height = 2.0
	collision.shape = cylinder
	target.add_child(collision)
	root.add_child(target)

	await physics_frame
	await physics_frame

	var query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(Vector3(0, 3, 0), Vector3(0, -3, 0))
	var hit: Dictionary = root.world_3d.direct_space_state.intersect_ray(query)
	_check(not hit.is_empty(), "ray query hits the cylinder")
	var position: Vector3 = hit.get("position", Vector3())
	_check(absf(position.y - 1.0) <= 0.05, "cylinder hull is centered on its Godot transform")

	if failures == 0:
		print("RESULT: PASS - cylinder shape is queryable and centered")
	else:
		print("RESULT: FAIL - ", failures, " cylinder assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
