extends SceneTree

var failures: int = 0
var body: RigidBody3D
var ground: StaticBody3D

func _initialize() -> void:
	print("Active physics engine setting: ", ProjectSettings.get_setting("physics/3d/physics_engine"))

	ground = StaticBody3D.new()
	var ground_shape: CollisionShape3D = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = Vector3(10, 1, 10)
	ground_shape.shape = box
	ground.add_child(ground_shape)
	ground.position = Vector3(0, -0.5, 0)
	root.add_child(ground)

	body = RigidBody3D.new()
	var body_shape: CollisionShape3D = CollisionShape3D.new()
	var sphere: SphereShape3D = SphereShape3D.new()
	sphere.radius = 0.5
	body_shape.shape = sphere
	body.add_child(body_shape)
	body.position = Vector3(0, 2, 0)
	root.add_child(body)

	call_deferred("_run")


func _run() -> void:
	for i in 300:
		await physics_frame

	print("Body Y after 300 physics frames: ", body.global_position.y)
	print("Body linear velocity: ", body.linear_velocity)
	_assert_result(abs(body.global_position.y - 0.5) < 0.08, "body rested on ground near expected center height (~0.5)")

	var space_state: PhysicsDirectSpaceState3D = get_root().get_world_3d().direct_space_state
	var query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(Vector3(3, 5, 3), Vector3(3, -5, 3))
	var result: Dictionary = space_state.intersect_ray(query)
	if result.has("position"):
		print("Raycast hit position: ", result["position"])
		_assert_result(abs(result["position"].y - 0.0) < 0.1, "raycast hit ground at expected height")
	else:
		_assert_result(false, "raycast hit ground at expected height")

	if failures == 0:
		print("RESULT: PASS - settle checks passed")
	else:
		print("RESULT: FAIL - ", failures, " settle check(s) failed")
	quit(1 if failures > 0 else 0)


func _assert_result(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		print("FAIL: ", message)
