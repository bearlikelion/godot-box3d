extends SceneTree

# A rigid body must not drive a grounded character through the floor during motion recovery.
var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _box(size: Vector3) -> CollisionShape3D:
	var collision := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	collision.shape = shape
	return collision


func _run() -> void:
	var floor_body := StaticBody3D.new()
	floor_body.position.y = -0.5
	floor_body.add_child(_box(Vector3(30, 1, 20)))
	root.add_child(floor_body)

	var characters: Array[CharacterBody3D] = []
	var loads: Array[RigidBody3D] = []
	for index in 2:
		var character := CharacterBody3D.new()
		character.position = Vector3(index * 6.0, 0.5, 0)
		character.add_child(_box(Vector3(2, 1, 2)))
		root.add_child(character)
		characters.append(character)

		var load_body := RigidBody3D.new()
		load_body.position = character.position + Vector3(0, 1.3, 0)
		load_body.add_child(_box(Vector3(0.4, 0.4, 0.4)))
		root.add_child(load_body)
		loads.append(load_body)

	await physics_frame
	for frame in 240:
		for index in 2:
			var character := characters[index]
			character.velocity = Vector3(index * 0.15, -0.5, 0)
			character.move_and_slide()
			# Even GodotPhysics3D permits about 8 cm of recovery displacement under
			# this falling load. Detect loss of support, not exact solver parity.
			if absf(character.position.y - 0.5) > 0.15:
				_fail("%s character lost its grounded height at frame %d (y=%.4f)" % [
						"stationary" if index == 0 else "moving", frame, character.position.y])
				quit(1)
				return
		await physics_frame

	for index in 2:
		var character := characters[index]
		var load_body := loads[index]
		var label := "stationary" if index == 0 else "moving"
		if absf(load_body.position.y - (character.position.y + 0.7)) > 0.06:
			_fail("rigid body did not remain supported by the %s character (character y=%.4f, rigid y=%.4f)" % [
					label, character.position.y, load_body.position.y])
		if absf(load_body.position.x - character.position.x) > 0.8:
			_fail("rigid body left the %s character's support area" % label)
		print("%s support: character=%s rigid=%s" % [label, character.position, load_body.position])

	# Recovery must still work against a dynamic body when no immovable surface
	# competes with it; merely ignoring all rigid bodies would hide the regression.
	var parameters := PhysicsTestMotionParameters3D.new()
	parameters.from = Transform3D(Basis(), loads[0].global_position)
	parameters.motion = Vector3.ZERO
	parameters.recovery_as_collision = true
	var result := PhysicsTestMotionResult3D.new()
	var hit := PhysicsServer3D.body_test_motion(characters[0].get_rid(), parameters, result)
	if not hit or result.get_travel().length() < 0.01:
		_fail("uncontested dynamic-body overlaps must still recover")
	elif result.get_collision_count() == 0 or result.get_collider_rid() != loads[0].get_rid():
		_fail("dynamic-body recovery must report the supporting rigid body")

	if failures == 0:
		print("RESULT: PASS - stationary and moving characters support a resting rigid body")
	quit(1 if failures > 0 else 0)


func _fail(message: String) -> void:
	failures += 1
	print("RESULT: FAIL - ", message)
