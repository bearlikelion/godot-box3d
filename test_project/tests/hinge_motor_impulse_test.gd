extends SceneTree

var failures: int = 0
var samples: Array[Vector2] = []
var recording: bool = false
var axis: Vector3 = Vector3.FORWARD


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var original_rate: int = Engine.physics_ticks_per_second
	var original_scale: float = Engine.time_scale
	var anchor: StaticBody3D = StaticBody3D.new()
	root.add_child(anchor)
	var rotor: RigidBody3D = RigidBody3D.new()
	rotor.gravity_scale = 0.0
	rotor.can_sleep = false
	rotor.linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	rotor.angular_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	rotor.linear_damp = 0.0
	rotor.angular_damp = 0.0
	rotor.inertia = Vector3.ONE
	var collision: CollisionShape3D = CollisionShape3D.new()
	collision.shape = BoxShape3D.new()
	rotor.add_child(collision)
	root.add_child(rotor)
	var frame: Transform3D = Transform3D(Basis(Vector3.UP, 0.7) * Basis(Vector3.RIGHT, 0.3), Vector3.ZERO)
	axis = frame.basis.z
	var joint: RID = PhysicsServer3D.joint_create()
	PhysicsServer3D.joint_make_hinge(joint, anchor.get_rid(), frame, rotor.get_rid(), frame)
	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_TARGET_VELOCITY, 100.0)
	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_MAX_IMPULSE, 0.01)
	PhysicsServer3D.hinge_joint_set_flag(joint, PhysicsServer3D.HINGE_JOINT_FLAG_ENABLE_MOTOR, true)
	PhysicsServer3D.body_set_force_integration_callback(rotor.get_rid(), _sample)
	await process_frame

	for profile in [Vector2(30, 1), Vector2(60, 1), Vector2(120, 1), Vector2(60, 0.5), Vector2(60, 2)]:
		Engine.physics_ticks_per_second = int(profile.x)
		Engine.time_scale = profile.y
		await process_frame
		await _measure(0.01, "rate=%d scale=%.1f" % [int(profile.x), profile.y], profile.y / profile.x)
		_check(is_equal_approx(float(PhysicsServer3D.hinge_joint_get_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_MAX_IMPULSE)), 0.01), "getter retains the requested impulse")

	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_MAX_IMPULSE, 0.02)
	await _measure(0.02, "live impulse edit")
	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_MAX_IMPULSE, 0.0)
	await _measure(0.0, "zero impulse")
	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_MAX_IMPULSE, 0.02)
	PhysicsServer3D.hinge_joint_set_flag(joint, PhysicsServer3D.HINGE_JOINT_FLAG_ENABLE_MOTOR, false)
	await _measure(0.0, "disabled motor")
	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_TARGET_VELOCITY, -100.0)
	PhysicsServer3D.hinge_joint_set_flag(joint, PhysicsServer3D.HINGE_JOINT_FLAG_ENABLE_MOTOR, true)
	await _measure(-0.02, "reversed motor")
	PhysicsServer3D.body_set_force_integration_callback(rotor.get_rid(), Callable())
	PhysicsServer3D.free_rid(joint)
	rotor.free()
	anchor.free()
	await physics_frame
	await _dynamic_pair()
	Engine.physics_ticks_per_second = original_rate
	Engine.time_scale = original_scale
	if failures == 0:
		print("RESULT: PASS - hinge motor impulse budget across tick rates, time scales and live edits")
	quit(1 if failures > 0 else 0)


func _dynamic_pair() -> void:
	var bodies: Array[RigidBody3D] = []
	for index in 2:
		var body: RigidBody3D = RigidBody3D.new()
		body.gravity_scale = 0.0
		body.can_sleep = false
		body.collision_layer = 0
		body.collision_mask = 0
		body.inertia = Vector3.ONE
		body.angular_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
		body.angular_damp = 0.0
		var collision: CollisionShape3D = CollisionShape3D.new()
		collision.shape = BoxShape3D.new()
		body.add_child(collision)
		root.add_child(body)
		bodies.append(body)
	axis = Vector3(0, 0, 1)
	var joint: RID = PhysicsServer3D.joint_create()
	PhysicsServer3D.joint_make_hinge(joint, bodies[0].get_rid(), Transform3D.IDENTITY, bodies[1].get_rid(), Transform3D.IDENTITY)
	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_TARGET_VELOCITY, 100.0)
	PhysicsServer3D.hinge_joint_set_param(joint, PhysicsServer3D.HINGE_JOINT_MOTOR_MAX_IMPULSE, 0.01)
	PhysicsServer3D.hinge_joint_set_flag(joint, PhysicsServer3D.HINGE_JOINT_FLAG_ENABLE_MOTOR, true)
	PhysicsServer3D.body_set_force_integration_callback(bodies[1].get_rid(), _sample)
	await _measure(0.01, "two dynamic endpoints and a newly created native joint")
	var total_spin: Vector3 = bodies[0].angular_velocity + bodies[1].angular_velocity
	_check(total_spin.length() < 0.0005, "dynamic endpoints receive equal and opposite angular impulse")
	PhysicsServer3D.body_set_force_integration_callback(bodies[1].get_rid(), Callable())
	PhysicsServer3D.free_rid(joint)
	for body in bodies:
		body.free()
	await physics_frame


func _sample(state: PhysicsDirectBodyState3D) -> void:
	if recording and state.step > 0.0:
		samples.append(Vector2(state.step, state.angular_velocity.dot(axis)))


func _measure(expected: float, label: String, expected_step: float = 0.0) -> void:
	samples.clear()
	recording = true
	for frame in 40:
		await physics_frame
		if samples.size() >= 8:
			break
	recording = false
	if samples.size() < 8:
		_check(false, label + " did not receive enough physics samples")
		return
	var ok: bool = true
	if expected_step > 0.0:
		ok = absf(samples[-1].x - expected_step) < 0.00001
	var observed: float = 0.0
	for index in range(1, samples.size()):
		var change: float = samples[index].y - samples[index - 1].y
		observed += change
		ok = ok and absf(change - expected) < 0.0005
	print("Motor impulse ", label, ": dt=", samples[-1].x, " expected=", expected, " observed=", observed / (samples.size() - 1))
	_check(ok, label + " uses the full-tick angular impulse, not a torque or substep impulse")


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		print("RESULT: FAIL - ", message)
