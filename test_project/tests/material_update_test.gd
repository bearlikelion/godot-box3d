extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _box(size: Vector3) -> CollisionShape3D:
	var collision: CollisionShape3D = CollisionShape3D.new()
	var shape: BoxShape3D = BoxShape3D.new()
	shape.size = size
	collision.shape = shape
	return collision


func _run() -> void:
	var floor_body: StaticBody3D = StaticBody3D.new()
	floor_body.add_child(_box(Vector3(100, 1, 20)))
	var floor_material: PhysicsMaterial = PhysicsMaterial.new()
	floor_material.friction = 0.0
	floor_body.physics_material_override = floor_material
	root.add_child(floor_body)
	var slider: RigidBody3D = RigidBody3D.new()
	slider.position = Vector3(0, 1, 0)
	slider.can_sleep = false
	slider.lock_rotation = true
	slider.linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	slider.linear_damp = 0.0
	slider.add_child(_box(Vector3.ONE))
	root.add_child(slider)
	for frame in 30:
		await physics_frame
	slider.linear_velocity = Vector3(4, 0, 0)
	for frame in 30:
		await physics_frame
	_check(slider.linear_velocity.x > 3.5, "initial frictionless floor preserves sliding speed")
	floor_material.friction = 1.0
	for frame in 60:
		await physics_frame
	_check(absf(slider.linear_velocity.x) < 0.2, "editing a live material updates fixture friction")
	floor_material.friction = 0.0
	slider.linear_velocity = Vector3(4, 0, 0)
	for frame in 30:
		await physics_frame
	_check(slider.linear_velocity.x > 3.5, "editing friction back to zero takes effect")
	var slider_material: PhysicsMaterial = PhysicsMaterial.new()
	slider_material.rough = true
	slider.physics_material_override = slider_material
	for frame in 60:
		await physics_frame
	_check(slider.position.is_finite() and absf(slider.linear_velocity.x) < 0.2, "rough material overrides a frictionless surface without invalid native coefficients")
	slider_material.rough = false
	slider.linear_velocity = Vector3(4, 0, 0)
	for frame in 30:
		await physics_frame
	_check(slider.linear_velocity.x > 3.5, "clearing rough restores ordinary minimum-friction mixing")
	floor_material.friction = 0.25
	slider.linear_velocity = Vector3(4, 0, 0)
	for frame in 30:
		await physics_frame
	_check(slider.linear_velocity.x > 2.5 and slider.linear_velocity.x < 3.1, "ordinary materials use the lower friction, not the geometric mean")

	var ball: RigidBody3D = RigidBody3D.new()
	ball.position = Vector3(0, 4, 5)
	ball.can_sleep = false
	ball.linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	ball.linear_damp = 0.0
	var ball_collision: CollisionShape3D = CollisionShape3D.new()
	var sphere: SphereShape3D = SphereShape3D.new()
	sphere.radius = 0.5
	ball_collision.shape = sphere
	ball.add_child(ball_collision)
	var ball_material: PhysicsMaterial = PhysicsMaterial.new()
	ball.physics_material_override = ball_material
	root.add_child(ball)
	await physics_frame
	ball_material.bounce = 0.8
	var bounced: bool = false
	for frame in 100:
		await physics_frame
		bounced = bounced or ball.linear_velocity.y > 2.0
	_check(bounced, "editing a live material updates fixture restitution")
	floor_material.bounce = 0.8
	ball_material.absorbent = true
	ball.position = Vector3(0, 4, 5)
	ball.linear_velocity = Vector3.ZERO
	bounced = false
	for frame in 100:
		await physics_frame
		bounced = bounced or ball.linear_velocity.y > 2.0
	_check(ball.position.is_finite() and not bounced, "absorbent restitution subtracts from the opposing surface")
	root.remove_child(ball)
	ball.free()
	root.remove_child(slider)
	slider.free()
	root.remove_child(floor_body)
	floor_body.free()
	if failures == 0:
		print("RESULT: PASS - live friction and restitution material changes affect collision response")
	quit(1 if failures > 0 else 0)


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		print("RESULT: FAIL - ", message)
