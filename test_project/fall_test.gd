extends SceneTree

var frames: int = 0
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
	body.position = Vector3(0, 5, 0)
	root.add_child(body)

	print("Initial body Y: ", body.position.y)


func _process(delta: float) -> bool:
	frames += 1
	if frames == 120:
		print("Body Y after 120 physics frames: ", body.global_position.y)
		print("Body linear velocity: ", body.linear_velocity)
		var passed: bool = body.global_position.y < 4.0 and body.global_position.y > -5.0
		if passed:
			print("RESULT: PASS - body fell under gravity and did not tunnel through ground")
		else:
			print("RESULT: FAIL - body did not fall as expected")
		quit(0 if passed else 1)
	return false
