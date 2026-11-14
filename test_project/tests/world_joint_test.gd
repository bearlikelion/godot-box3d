extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var fixtures: Array[Dictionary] = []
	for kind in 3:
		for slot in 2:
			var frame := Transform3D(Basis(Vector3.UP, 0.6), Vector3(12 + kind * 5, 8, -9 + slot * 4))
			var body := RigidBody3D.new()
			body.transform = frame
			body.gravity_scale = 0.0
			body.can_sleep = false
			body.collision_layer = 0
			body.collision_mask = 0
			var collision := CollisionShape3D.new()
			collision.shape = BoxShape3D.new()
			body.add_child(collision)
			root.add_child(body)
			var joint: Joint3D
			if kind == 0:
				joint = PinJoint3D.new()
			elif kind == 1:
				joint = HingeJoint3D.new()
			else:
				joint = SliderJoint3D.new()
			joint.transform = frame
			root.add_child(joint)
			# Joint3D normalizes either one-body NodePath configuration to server body A.
			if slot == 0:
				joint.node_a = joint.get_path_to(body)
			else:
				joint.node_b = joint.get_path_to(body)
			if kind == 1:
				joint.set_param(HingeJoint3D.PARAM_LIMIT_LOWER, -0.2)
				joint.set_param(HingeJoint3D.PARAM_LIMIT_UPPER, 0.35)
				joint.set_flag(HingeJoint3D.FLAG_USE_LIMIT, true)
				joint.set_param(HingeJoint3D.PARAM_MOTOR_MAX_IMPULSE, 1.0)
				joint.set_param(HingeJoint3D.PARAM_MOTOR_TARGET_VELOCITY, 2.0)
				joint.set_flag(HingeJoint3D.FLAG_ENABLE_MOTOR, true)
			elif kind == 2:
				joint.set_param(SliderJoint3D.PARAM_LINEAR_LIMIT_LOWER, -0.4)
				joint.set_param(SliderJoint3D.PARAM_LINEAR_LIMIT_UPPER, 0.4)
			fixtures.append({"body": body, "joint": joint, "frame": frame, "kind": kind})
	await physics_frame
	for fixture in fixtures:
		fixture.body.apply_central_impulse(fixture.frame.basis * Vector3(3, 2, 1))
	for tick in 90:
		await physics_frame
	for fixture in fixtures:
		var body: RigidBody3D = fixture.body
		var relative: Transform3D = fixture.frame.affine_inverse() * body.transform
		print("World joint ", fixture.kind, " position=", relative.origin, " rotation=", relative.basis.get_euler())
		_check(body.position.is_finite() and body.angular_velocity.is_finite(), "finite one-body joint state")
		if fixture.kind == 0:
			_check(relative.origin.length() < 0.04, "pin holds a non-origin world anchor")
			PhysicsServer3D.pin_joint_set_local_b(fixture.joint.get_rid(), fixture.frame.origin + Vector3.UP)
		elif fixture.kind == 1:
			# The adapter's native hinge angle is frame B relative to A. With B
			# fixed, the moving body's rotation has the opposite sign.
			_check(relative.origin.length() < 0.04 and absf(relative.basis.get_euler().z + 0.35) < 0.04, "rotated world hinge motor respects upper limit")
			fixture.joint.set_param(HingeJoint3D.PARAM_MOTOR_TARGET_VELOCITY, -2.0)
		else:
			_check(absf(relative.origin.x - 0.4) < 0.04 and Vector2(relative.origin.y, relative.origin.z).length() < 0.04, "rotated world slider respects translation axis and limit")
			body.apply_central_impulse(fixture.frame.basis * Vector3(-8, 0, 0))
	for tick in 90:
		await physics_frame
	for fixture in fixtures:
		var relative: Transform3D = fixture.frame.affine_inverse() * fixture.body.transform
		if fixture.kind == 0:
			_check(fixture.body.position.distance_to(fixture.frame.origin + Vector3.UP) < 0.04, "live world pin anchor moves the body")
		elif fixture.kind == 1:
			_check(absf(relative.basis.get_euler().z - 0.2) < 0.04, "world hinge reverses after live motor edit")
		else:
			_check(absf(relative.origin.x + 0.4) < 0.04, "world slider reaches the opposite limit")
		fixture.joint.free()
		fixture.body.free()
	if not "--nodes-only" in OS.get_cmdline_user_args():
		await _check_rid_lifecycle()
	print("RESULT: ", "PASS" if failures == 0 else "FAIL", " - world joints and endpoint lifecycle")
	quit(0 if failures == 0 else 1)


func _check_rid_lifecycle() -> void:
	var shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(shape, Vector3.ONE * 0.25)
	var anchor := Vector3(31, 5, -17)
	var body := _rid_body(shape, anchor)
	var joint := PhysicsServer3D.joint_create()
	# Creation and parameters before space attachment must not silently lose the joint.
	PhysicsServer3D.joint_make_pin(joint, body, Vector3.ZERO, RID(), anchor)
	PhysicsServer3D.pin_joint_set_param(joint, PhysicsServer3D.PIN_JOINT_BIAS, 0.9)
	PhysicsServer3D.body_set_space(body, root.world_3d.space)
	PhysicsServer3D.body_apply_central_impulse(body, Vector3(3, 1, 0))
	await _ticks(45)
	_check(_rid_position(body).distance_to(anchor) < 0.04, "joint created before space attachment becomes active")
	var space := PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(space, true)
	PhysicsServer3D.body_set_space(body, RID())
	PhysicsServer3D.body_set_space(body, space)
	PhysicsServer3D.body_apply_central_impulse(body, Vector3(-3, 0, 0))
	await _ticks(45)
	_check(_rid_position(body).distance_to(anchor) < 0.04, "world joint rebuilds in a different space")
	PhysicsServer3D.free_rid(space)
	_check(not PhysicsServer3D.body_get_space(body).is_valid(), "freeing a space detaches surviving body RIDs")
	PhysicsServer3D.body_set_space(body, root.world_3d.space)
	PhysicsServer3D.body_apply_central_impulse(body, Vector3(2, 0, 0))
	await _ticks(45)
	_check(_rid_position(body).distance_to(anchor) < 0.04, "joint survives space destruction and reattachment")
	PhysicsServer3D.free_rid(body)
	# A freed endpoint must not turn into a world endpoint or leave a dangling pointer.
	PhysicsServer3D.pin_joint_set_local_b(joint, anchor + Vector3.ONE)
	PhysicsServer3D.free_rid(joint)
	var a := _rid_body(shape, anchor)
	var b := _rid_body(shape, anchor)
	PhysicsServer3D.body_set_mode(b, PhysicsServer3D.BODY_MODE_STATIC)
	var other_space := PhysicsServer3D.space_create()
	PhysicsServer3D.body_set_space(a, root.world_3d.space)
	PhysicsServer3D.body_set_space(b, other_space)
	joint = PhysicsServer3D.joint_create()
	PhysicsServer3D.joint_make_pin(joint, a, Vector3.ZERO, b, Vector3.ZERO)
	PhysicsServer3D.body_apply_central_impulse(a, Vector3(2, 0, 0))
	await _ticks(20)
	_check(_rid_position(a).distance_to(anchor) > 0.2, "cross-space joint remains deferred without a native assertion")
	PhysicsServer3D.body_set_space(b, root.world_3d.space)
	await _ticks(60)
	_check(_rid_position(a).distance_to(anchor) < 0.04, "two-body joint activates when endpoints share a space")
	PhysicsServer3D.free_rid(b)
	PhysicsServer3D.body_apply_central_impulse(a, Vector3(2, 0, 0))
	await _ticks(20)
	_check(_rid_position(a).distance_to(anchor) > 0.2, "deleted body B does not become a world constraint")
	PhysicsServer3D.joint_make_pin(joint, a, Vector3.ZERO, RID(), anchor)
	await _ticks(60)
	_check(_rid_position(a).distance_to(anchor) < 0.04, "joint RID can be rebound after endpoint deletion")
	PhysicsServer3D.joint_clear(joint)
	PhysicsServer3D.free_rid(joint)
	PhysicsServer3D.free_rid(a)
	PhysicsServer3D.free_rid(other_space)
	PhysicsServer3D.free_rid(shape)


func _rid_body(shape: RID, position: Vector3) -> RID:
	var body := PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(body, PhysicsServer3D.BODY_MODE_RIGID)
	PhysicsServer3D.body_add_shape(body, shape)
	PhysicsServer3D.body_set_collision_layer(body, 0)
	PhysicsServer3D.body_set_collision_mask(body, 0)
	PhysicsServer3D.body_set_param(body, PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
	PhysicsServer3D.body_set_state(body, PhysicsServer3D.BODY_STATE_TRANSFORM, Transform3D(Basis(), position))
	return body


func _rid_position(body: RID) -> Vector3:
	var transform: Transform3D = PhysicsServer3D.body_get_state(body, PhysicsServer3D.BODY_STATE_TRANSFORM)
	return transform.origin


func _ticks(count: int) -> void:
	for tick in count:
		await physics_frame


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
