extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var anchor: StaticBody3D = StaticBody3D.new()
	anchor.position = Vector3(0, 5, 0)
	var anchor_shape: CollisionShape3D = CollisionShape3D.new()
	var anchor_box: BoxShape3D = BoxShape3D.new()
	anchor_box.size = Vector3(0.4, 0.4, 0.4)
	anchor_shape.shape = anchor_box
	anchor.add_child(anchor_shape)
	root.add_child(anchor)

	var ball: RigidBody3D = RigidBody3D.new()
	ball.position = Vector3(0, 3, 0)
	ball.can_sleep = false
	var ball_shape: CollisionShape3D = CollisionShape3D.new()
	var sphere: SphereShape3D = SphereShape3D.new()
	sphere.radius = 0.3
	ball_shape.shape = sphere
	ball.add_child(ball_shape)
	root.add_child(ball)

	var joint: PinJoint3D = PinJoint3D.new()
	root.add_child(joint)
	joint.node_a = joint.get_path_to(anchor)
	joint.node_b = joint.get_path_to(ball)
	joint.position = anchor.position

	await process_frame
	await process_frame

	var joint_rid: RID = joint.get_rid()
	PhysicsServer3D.pin_joint_set_local_a(joint_rid, Vector3(1, 0, 0))
	_check(
		PhysicsServer3D.pin_joint_get_local_a(joint_rid).is_equal_approx(Vector3(1, 0, 0)),
		"local anchor A round-trips through the server",
	)

	PhysicsServer3D.pin_joint_set_local_b(joint_rid, Vector3(0, 0.5, 0))
	_check(
		PhysicsServer3D.pin_joint_get_local_b(joint_rid).is_equal_approx(Vector3(0, 0.5, 0)),
		"local anchor B round-trips through the server",
	)

	for frame in 180:
		await physics_frame

	var settled_x: float = ball.global_position.x
	_check(settled_x > 0.5, "moving anchor A drags the hanging body toward it")

	# Move the anchor to the opposite side; the body must follow back across.
	ball.sleeping = true
	PhysicsServer3D.pin_joint_set_local_a(joint_rid, Vector3(-1, 0, 0))

	for frame in 240:
		await physics_frame

	_check(
		ball.global_position.x < settled_x - 0.5,
		"moving anchor A again moves the body in the solver, not just the cache",
	)

	if failures == 0:
		print("RESULT: PASS - pin joint anchors can change after creation")
	else:
		print("RESULT: FAIL - ", failures, " pin joint anchor assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
