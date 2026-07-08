extends SceneTree

var frames: int = 0
var body: RigidBody3D
var ground: StaticBody3D

func _initialize() -> void:
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


func _process(delta: float) -> bool:
	frames += 1
	if frames == 300:
		print("Body Y after 300 frames (5s): ", body.global_position.y)
		print("Body linear velocity: ", body.linear_velocity)
		if body.global_position.y > 0.4 and body.global_position.y < 0.6:
			print("RESULT: PASS - body rested on ground near expected height (~0.5)")
		else:
			print("RESULT: FAIL - body did not rest at expected height")

		# Raycast test: cast straight down from above the ground.
		var space_state: PhysicsDirectSpaceState3D = get_root().get_world_3d().direct_space_state
		var query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(Vector3(3, 5, 3), Vector3(3, -5, 3))
		var result: Dictionary = space_state.intersect_ray(query)
		if result.has("position"):
			print("Raycast hit position: ", result["position"])
			if abs(result["position"].y - 0.0) < 0.1:
				print("RESULT: PASS - raycast hit ground at expected height")
			else:
				print("RESULT: FAIL - raycast hit unexpected height")
		else:
			print("RESULT: FAIL - raycast did not hit anything")

		quit()
	return false
