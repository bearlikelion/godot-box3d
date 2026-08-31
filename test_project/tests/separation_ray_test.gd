extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var floor_body := StaticBody3D.new()
	var floor_collision := CollisionShape3D.new()
	var floor_shape := BoxShape3D.new()
	floor_shape.size = Vector3(20.0, 1.0, 20.0)
	floor_collision.shape = floor_shape
	floor_body.add_child(floor_collision)
	floor_body.position = Vector3(0.0, -0.5, 0.0)
	root.add_child(floor_body)

	var body := CharacterBody3D.new()
	body.position = Vector3(0.0, 1.6, 0.0)
	var body_collision := CollisionShape3D.new()
	var body_shape := CapsuleShape3D.new()
	body_shape.radius = 0.25
	body_shape.height = 0.5
	body_collision.shape = body_shape
	body.add_child(body_collision)
	var ray_collision := CollisionShape3D.new()
	ray_collision.rotation.x = PI / 2.0
	var ray_shape := SeparationRayShape3D.new()
	ray_shape.length = 1.5
	ray_collision.shape = ray_shape
	body.add_child(ray_collision)
	root.add_child(body)

	await physics_frame
	await physics_frame

	var disabled_result := PhysicsTestMotionResult3D.new()
	var disabled_hit := PhysicsServer3D.body_test_motion(
			body.get_rid(), _motion_parameters(body, false), disabled_result)
	_check(not disabled_hit, "regular motion ignores a non-sliding separation ray")
	_check(disabled_result.get_travel().is_equal_approx(Vector3(0.0, -0.25, 0.0)),
			"ignored separation ray leaves the requested travel unchanged")

	var enabled_result := PhysicsTestMotionResult3D.new()
	var enabled_hit := PhysicsServer3D.body_test_motion(
			body.get_rid(), _motion_parameters(body, true), enabled_result)
	_check(enabled_hit, "collide_separation_ray detects the floor")
	_check(enabled_result.get_collision_count() == 1, "separation ray reports one collision")
	if enabled_result.get_collision_count() == 1:
		_check(enabled_result.get_collision_normal().is_equal_approx(Vector3.UP),
				"non-sliding separation ray reports the opposite ray direction")
		_check(enabled_result.get_collision_local_shape() == 1,
				"separation ray reports its local shape index")
	_check(absf(enabled_result.get_collision_unsafe_fraction() - 0.4) < 0.03,
			"separation ray stops when its endpoint reaches the floor within solver slop")

	var slope_body := StaticBody3D.new()
	var slope_collision := CollisionShape3D.new()
	var slope_shape := BoxShape3D.new()
	slope_shape.size = Vector3(4.0, 1.0, 4.0)
	slope_collision.shape = slope_shape
	slope_body.add_child(slope_collision)
	slope_body.position = Vector3(5.0, 0.0, 0.0)
	slope_body.rotation.z = PI / 6.0
	root.add_child(slope_body)

	var slope_character := CharacterBody3D.new()
	slope_character.position = Vector3(5.0, 0.5 / cos(PI / 6.0) + 1.6, 0.0)
	var slope_body_collision := CollisionShape3D.new()
	var slope_body_shape := CapsuleShape3D.new()
	slope_body_shape.radius = 0.25
	slope_body_shape.height = 0.5
	slope_body_collision.shape = slope_body_shape
	slope_character.add_child(slope_body_collision)
	var slope_ray_collision := CollisionShape3D.new()
	slope_ray_collision.rotation.x = PI / 2.0
	var slope_ray_shape := SeparationRayShape3D.new()
	slope_ray_shape.length = 1.5
	slope_ray_shape.slide_on_slope = true
	slope_ray_collision.shape = slope_ray_shape
	slope_character.add_child(slope_ray_collision)
	root.add_child(slope_character)

	await physics_frame
	var slope_result := PhysicsTestMotionResult3D.new()
	var slope_hit := PhysicsServer3D.body_test_motion(
			slope_character.get_rid(), _motion_parameters(slope_character, false), slope_result)
	_check(slope_hit, "slide_on_slope rays participate in regular motion")
	_check(slope_result.get_collision_count() == 1, "slide_on_slope ray reports one collision")
	if slope_result.get_collision_count() == 1:
		var expected_normal := slope_body.transform.basis * Vector3.UP
		_check(slope_result.get_collision_normal().dot(expected_normal) > 0.99,
				"slide_on_slope reports the surface normal")

	var data: Dictionary = PhysicsServer3D.shape_get_data(ray_shape.get_rid())
	_check(is_equal_approx(data.get("length", 0.0), 1.5), "shape data preserves ray length")
	_check(data.get("slide_on_slope", true) == false, "shape data preserves slide_on_slope")

	if failures == 0:
		print("RESULT: PASS - separation rays support CharacterBody motion queries")
	else:
		print("RESULT: FAIL - ", failures, " separation ray assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _motion_parameters(body: CharacterBody3D, collide_separation_ray: bool) -> PhysicsTestMotionParameters3D:
	var parameters := PhysicsTestMotionParameters3D.new()
	parameters.from = body.global_transform
	parameters.motion = Vector3(0.0, -0.25, 0.0)
	parameters.margin = 0.001
	parameters.collide_separation_ray = collide_separation_ray
	return parameters


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
