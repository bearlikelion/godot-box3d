extends SceneTree

class DampingProbe:
	extends RigidBody3D

	var sample_count: int = 0
	var total_linear_damp: float = 0.0
	var total_angular_damp: float = 0.0

	func _integrate_forces(state: PhysicsDirectBodyState3D) -> void:
		sample_count += 1
		total_linear_damp = state.get_total_linear_damp()
		total_angular_damp = state.get_total_angular_damp()


var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var body: DampingProbe = _make_probe()
	body.linear_damp_mode = RigidBody3D.DAMP_MODE_COMBINE
	body.angular_damp_mode = RigidBody3D.DAMP_MODE_COMBINE
	body.linear_damp = 2.0
	body.angular_damp = 3.0

	for frame in 4:
		await physics_frame

	var default_linear: float = ProjectSettings.get_setting("physics/3d/default_linear_damp", 0.1)
	var default_angular: float = ProjectSettings.get_setting("physics/3d/default_angular_damp", 0.1)
	_check(body.sample_count > 0, "direct body state samples damping")
	_check(
		_is_close(body.total_linear_damp, default_linear + 2.0)
				and _is_close(body.total_angular_damp, default_angular + 3.0),
		"COMBINE body damping includes the default area",
	)

	var area: Area3D = _make_area()
	area.linear_damp_space_override = Area3D.SPACE_OVERRIDE_COMBINE
	area.angular_damp_space_override = Area3D.SPACE_OVERRIDE_COMBINE
	area.linear_damp = 5.0
	area.angular_damp = 7.0
	for frame in 4:
		await physics_frame
	_check(
		_is_close(body.total_linear_damp, default_linear + 7.0)
				and _is_close(body.total_angular_damp, default_angular + 10.0),
		"COMBINE body damping includes Area3D and default damping",
	)

	body.linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	body.angular_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	for frame in 2:
		await physics_frame
	_check(
		_is_close(body.total_linear_damp, 2.0) and _is_close(body.total_angular_damp, 3.0),
		"REPLACE body damping excludes Area3D and default damping",
	)
	_check(
		PhysicsServer3D.body_get_param(body.get_rid(), PhysicsServer3D.BODY_PARAM_LINEAR_DAMP_MODE)
				== PhysicsServer3D.BODY_DAMP_MODE_REPLACE
				and PhysicsServer3D.body_get_param(body.get_rid(), PhysicsServer3D.BODY_PARAM_ANGULAR_DAMP_MODE)
				== PhysicsServer3D.BODY_DAMP_MODE_REPLACE,
		"body damping modes round-trip through PhysicsServer3D",
	)

	body.linear_damp_mode = RigidBody3D.DAMP_MODE_COMBINE
	body.angular_damp_mode = RigidBody3D.DAMP_MODE_COMBINE
	area.linear_damp_space_override = Area3D.SPACE_OVERRIDE_COMBINE_REPLACE
	area.angular_damp_space_override = Area3D.SPACE_OVERRIDE_COMBINE_REPLACE
	for frame in 2:
		await physics_frame
	_check(
		_is_close(body.total_linear_damp, 7.0) and _is_close(body.total_angular_damp, 10.0),
		"COMBINE_REPLACE Area3D damping excludes the default area",
	)

	if failures == 0:
		print("RESULT: PASS - body and default damping modes are honored")
	else:
		print("RESULT: FAIL - ", failures, " damping mode assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _make_probe() -> DampingProbe:
	var body: DampingProbe = DampingProbe.new()
	body.gravity_scale = 0.0
	body.can_sleep = false
	var collision: CollisionShape3D = CollisionShape3D.new()
	var sphere: SphereShape3D = SphereShape3D.new()
	sphere.radius = 0.5
	collision.shape = sphere
	body.add_child(collision)
	root.add_child(body)
	return body


func _make_area() -> Area3D:
	var area: Area3D = Area3D.new()
	var collision: CollisionShape3D = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = Vector3(10, 10, 10)
	collision.shape = box
	area.add_child(collision)
	root.add_child(area)
	return area


func _is_close(actual: float, expected: float) -> bool:
	return absf(actual - expected) < 0.001


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
