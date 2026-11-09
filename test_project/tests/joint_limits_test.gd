extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var anchor: StaticBody3D = StaticBody3D.new()
	root.add_child(anchor)
	var rotor: RigidBody3D = _body(Vector3(0, 0, 0))
	var hinge: HingeJoint3D = HingeJoint3D.new()
	root.add_child(hinge)
	hinge.node_a = hinge.get_path_to(anchor)
	hinge.node_b = hinge.get_path_to(rotor)
	hinge.set_param(HingeJoint3D.PARAM_LIMIT_LOWER, -0.4)
	hinge.set_param(HingeJoint3D.PARAM_LIMIT_UPPER, 0.4)
	hinge.set_flag(HingeJoint3D.FLAG_USE_LIMIT, true)
	# Keep this fixture useful with both the old torque mapping and corrected impulse units.
	hinge.set_param(HingeJoint3D.PARAM_MOTOR_MAX_IMPULSE, 100.0 / 60.0)
	hinge.set_param(HingeJoint3D.PARAM_MOTOR_TARGET_VELOCITY, 2.0)
	hinge.set_flag(HingeJoint3D.FLAG_ENABLE_MOTOR, true)
	for frame in 90:
		await physics_frame
	_check(rotor.rotation.z > 0.3 and rotor.rotation.z < 0.5, "hinge motor reaches and respects its upper limit")
	rotor.sleeping = true
	hinge.set_param(HingeJoint3D.PARAM_MOTOR_TARGET_VELOCITY, -2.0)
	for frame in 90:
		await physics_frame
	_check(rotor.rotation.z < -0.3 and rotor.rotation.z > -0.5, "reversing a live motor reaches its lower limit")
	hinge.set_flag(HingeJoint3D.FLAG_ENABLE_MOTOR, false)
	hinge.set_flag(HingeJoint3D.FLAG_USE_LIMIT, false)
	rotor.angular_velocity = Vector3(0, 0, 1)
	for frame in 45:
		await physics_frame
	_check(rotor.rotation.z > 0.1, "disabling the live hinge limit allows free rotation")
	hinge.free()
	rotor.free()

	var carriage: RigidBody3D = _body(Vector3(0, 0, 0))
	var slider: SliderJoint3D = SliderJoint3D.new()
	root.add_child(slider)
	slider.node_a = slider.get_path_to(anchor)
	slider.node_b = slider.get_path_to(carriage)
	slider.set_param(SliderJoint3D.PARAM_LINEAR_LIMIT_LOWER, -0.5)
	slider.set_param(SliderJoint3D.PARAM_LINEAR_LIMIT_UPPER, 0.5)
	await physics_frame
	carriage.linear_velocity = Vector3(3, 2, 1)
	carriage.angular_velocity = Vector3(1, 2, 3)
	for frame in 60:
		await physics_frame
	_check(carriage.position.x > 0.4 and carriage.position.x < 0.6, "slider enforces its upper translation limit")
	_check(absf(carriage.position.y) < 0.03 and absf(carriage.position.z) < 0.03 and carriage.rotation.length() < 0.03, "slider blocks transverse motion and rotation")
	carriage.linear_velocity = Vector3(-3, 0, 0)
	for frame in 60:
		await physics_frame
	_check(carriage.position.x < -0.4 and carriage.position.x > -0.6, "slider enforces its lower translation limit")
	slider.set_param(SliderJoint3D.PARAM_LINEAR_LIMIT_LOWER, -1.0)
	carriage.linear_velocity = Vector3(-3, 0, 0)
	for frame in 60:
		await physics_frame
	_check(carriage.position.x < -0.9 and carriage.position.x > -1.1, "editing a live slider limit changes the constraint")
	carriage.sleeping = true
	slider.set_param(SliderJoint3D.PARAM_LINEAR_LIMIT_LOWER, -0.2)
	for frame in 60:
		await physics_frame
	_check(carriage.position.x > -0.25, "editing a slider limit wakes a sleeping constrained body")
	carriage.free()
	await physics_frame
	slider.free()
	anchor.free()
	_invalidated_handle()
	if failures == 0:
		print("RESULT: PASS - live hinge motors/limits, slider limits and connected-body teardown")
	quit(1 if failures > 0 else 0)


func _invalidated_handle() -> void:
	var space: RID = root.world_3d.space
	var body_a: RID = PhysicsServer3D.body_create()
	var body_b: RID = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_space(body_a, space)
	PhysicsServer3D.body_set_space(body_b, space)
	var joint: RID = PhysicsServer3D.joint_create()
	PhysicsServer3D.joint_make_hinge(joint, body_a, Transform3D.IDENTITY, body_b, Transform3D.IDENTITY)
	PhysicsServer3D.body_set_space(body_b, RID())
	PhysicsServer3D.hinge_joint_set_flag(joint, PhysicsServer3D.HINGE_JOINT_FLAG_ENABLE_MOTOR, true)
	PhysicsServer3D.free_rid(body_b)
	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_TARGET_VELOCITY, 2.0)
	_check(PhysicsServer3D.hinge_joint_get_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_TARGET_VELOCITY) == 2.0, "setters safely retain cached state after the native joint is destroyed")
	PhysicsServer3D.free_rid(joint)
	PhysicsServer3D.free_rid(body_a)


func _body(position: Vector3) -> RigidBody3D:
	var body: RigidBody3D = RigidBody3D.new()
	body.position = position
	body.gravity_scale = 0.0
	body.can_sleep = false
	body.linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	body.angular_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	body.linear_damp = 0.0
	body.angular_damp = 0.0
	var collision: CollisionShape3D = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = Vector3.ONE
	collision.shape = box
	body.add_child(collision)
	root.add_child(body)
	return body


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		print("RESULT: FAIL - ", message)
