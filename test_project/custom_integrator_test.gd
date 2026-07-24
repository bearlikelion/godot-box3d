extends SceneTree

class CallbackBody:
	extends RigidBody3D

	var callback_count: int = 0

	func _integrate_forces(state: PhysicsDirectBodyState3D) -> void:
		callback_count += 1
		state.linear_velocity = Vector3(0, 2, 0)


class BuiltinIntegrationBody:
	extends RigidBody3D

	var callback_count: int = 0

	func _integrate_forces(state: PhysicsDirectBodyState3D) -> void:
		callback_count += 1
		state.integrate_forces()


class DetachingBody:
	extends RigidBody3D

	var callback_count: int = 0
	var detached: bool = false

	func _integrate_forces(_state: PhysicsDirectBodyState3D) -> void:
		callback_count += 1
		if callback_count == 2:
			PhysicsServer3D.body_set_space(get_rid(), RID())
			detached = true


var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	_check(ProjectSettings.get_setting("physics/3d/physics_engine", "") == "Box3D Physics (Extension)", "test project requests the Box3D backend")
	_check(ClassDB.class_exists(&"Box3DPhysicsServer3D"), "Box3D extension is loaded")
	if failures > 0:
		quit(1)
		return

	var omitted: RigidBody3D = _make_body(RigidBody3D.new(), Vector3(0, 5, 0))
	omitted.custom_integrator = true
	omitted.linear_damp = 8.0
	omitted.angular_damp = 8.0
	omitted.linear_velocity = Vector3(2, 0, 0)
	omitted.add_constant_central_force(Vector3(0, -100, 0))
	omitted.apply_central_force(Vector3(0, -100, 0))

	var callback_body: CallbackBody = _make_body(CallbackBody.new(), Vector3(5, 2, 0)) as CallbackBody
	callback_body.custom_integrator = true

	var builtin_body: BuiltinIntegrationBody = _make_body(BuiltinIntegrationBody.new(), Vector3(10, 5, 0)) as BuiltinIntegrationBody
	builtin_body.custom_integrator = true

	var impulse_body: RigidBody3D = _make_body(RigidBody3D.new(), Vector3(15, 2, 0))
	impulse_body.custom_integrator = true
	impulse_body.apply_central_impulse(Vector3(2, 0, 0))

	var standard_force_body: RigidBody3D = _make_body(RigidBody3D.new(), Vector3(20, 2, 0))
	standard_force_body.gravity_scale = 0.0
	standard_force_body.add_constant_central_force(Vector3(4, 0, 0))
	standard_force_body.apply_central_force(Vector3(4, 0, 0))

	var detaching_body: DetachingBody = _make_body(DetachingBody.new(), Vector3(25, 2, 0)) as DetachingBody
	detaching_body.custom_integrator = true

	for frame in 60:
		await physics_frame

	_check(PhysicsServer3D.body_is_omitting_force_integration(omitted.get_rid()), "omit-force state round-trips through PhysicsServer3D")
	_check(abs(omitted.global_position.y - 5.0) < 0.5, "custom integrator suppresses gravity and applied/constant forces")
	_check(omitted.global_position.x > 1.0, "custom integrator suppresses configured damping")
	_check(callback_body.callback_count >= 30 and callback_body.global_position.y > 3.0, "force-integration callback runs and controls velocity")
	_check(builtin_body.callback_count >= 30 and builtin_body.global_position.y < 3.0, "state.integrate_forces opts back into built-in gravity")
	_check(impulse_body.global_position.x > 15.5 and abs(impulse_body.global_position.y - 2.0) < 0.5, "custom integrator preserves impulses while omitting standard forces")
	_check(standard_force_body.global_position.x > 21.0, "standard integration still applies transient and constant forces")
	_check(detaching_body.detached and detaching_body.callback_count == 2, "integration callbacks can detach their body safely")

	PhysicsServer3D.body_set_constant_force(omitted.get_rid(), Vector3.ZERO)
	omitted.linear_velocity = Vector3.ZERO
	omitted.linear_damp = 0.0
	omitted.custom_integrator = false
	_check(not PhysicsServer3D.body_is_omitting_force_integration(omitted.get_rid()), "omit-force state can be disabled")
	for frame in 60:
		await physics_frame
	_check(omitted.global_position.y < 4.0, "disabling custom integration restores gravity")

	if failures == 0:
		print("RESULT: PASS - custom integrator contract is honored")
	else:
		print("RESULT: FAIL - ", failures, " custom integrator assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _make_body(body: RigidBody3D, position: Vector3) -> RigidBody3D:
	body.position = position
	body.can_sleep = false
	var collision_shape: CollisionShape3D = CollisionShape3D.new()
	var shape: SphereShape3D = SphereShape3D.new()
	shape.radius = 0.5
	collision_shape.shape = shape
	body.add_child(collision_shape)
	root.add_child(body)
	return body


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
