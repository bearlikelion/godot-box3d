extends SceneTree

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var floor_body := StaticBody3D.new()
	floor_body.position.y = -0.5
	var floor_collision := CollisionShape3D.new()
	var floor_shape := BoxShape3D.new()
	floor_shape.size = Vector3(20, 1, 20)
	floor_collision.shape = floor_shape
	floor_body.add_child(floor_collision)
	root.add_child(floor_body)
	var character := CharacterBody3D.new()
	character.position.y = 1.6
	var collision := CollisionShape3D.new()
	var sphere := SphereShape3D.new()
	sphere.radius = 0.25
	collision.shape = sphere
	character.add_child(collision)
	root.add_child(character)
	await physics_frame
	await physics_frame
	for recovery in [false, true]:
		for exclusion in ["body", "object", "none"]:
			var parameters := PhysicsTestMotionParameters3D.new()
			parameters.from = character.global_transform
			parameters.from.origin.y = 0.2 if recovery else 1.6
			parameters.motion = Vector3.ZERO if recovery else Vector3(0, -2, 0)
			parameters.recovery_as_collision = true
			if exclusion == "body":
				parameters.exclude_bodies = [floor_body.get_rid()]
			elif exclusion == "object":
				parameters.exclude_objects = [floor_body.get_instance_id()]
			var result := PhysicsTestMotionResult3D.new()
			var hit := PhysicsServer3D.body_test_motion(character.get_rid(), parameters, result)
			if exclusion == "none":
				if not hit or result.get_collision_count() == 0:
					_fail("an earlier exclusion leaked into the next motion query")
			elif hit or result.get_collision_count() or not result.get_travel().is_equal_approx(parameters.motion) or not result.get_remainder().is_zero_approx():
				_fail("%s exclusion was ignored during %s" % [exclusion, "recovery" if recovery else "casting"])
	if failures == 0:
		print("RESULT: PASS - body/object exclusions apply to recovery and casting without leaking between queries")
	quit(1 if failures else 0)


func _fail(message: String) -> void:
	failures += 1
	print("RESULT: FAIL - ", message)
