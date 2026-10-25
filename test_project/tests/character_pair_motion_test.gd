extends SceneTree

var failures := 0

func _initialize() -> void:
	call_deferred("_run")

func _character(x: float) -> CharacterBody3D:
	var body := CharacterBody3D.new()
	var collision := CollisionShape3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.4
	capsule.height = 1.8
	collision.shape = capsule
	body.add_child(collision)
	body.position = Vector3(x, 0.9, 0)
	root.add_child(body)
	return body

func _run() -> void:
	var floor_body := StaticBody3D.new()
	var floor_collision := CollisionShape3D.new()
	var floor_shape := BoxShape3D.new()
	floor_shape.size = Vector3(8, 1, 8)
	floor_collision.shape = floor_shape
	floor_body.position.y = -0.5
	floor_body.add_child(floor_collision)
	root.add_child(floor_body)
	for reverse in [false, true]:
		var left := _character(-0.55)
		var right := _character(0.55)
		await physics_frame
		await physics_frame
		var minimum_separation := INF
		for frame in 8:
			left.velocity = Vector3(6, -0.5, 0)
			right.velocity = Vector3(-6, -0.5, 0)
			# Publish deferred Node3D notifications before checking the other body.
			# This isolates server-side target queuing from scene notification timing.
			if reverse:
				right.move_and_slide()
				right.force_update_transform()
				left.move_and_slide()
				left.force_update_transform()
			else:
				left.move_and_slide()
				left.force_update_transform()
				right.move_and_slide()
				right.force_update_transform()
			minimum_separation = minf(minimum_separation, right.position.x - left.position.x)
			await physics_frame
		if minimum_separation < 0.79:
			failures += 1
			print("RESULT: FAIL - approaching 0.8 m capsules overlap after sequential movement: ", minimum_separation, " reverse=", reverse)
		left.queue_free()
		right.queue_free()
		await physics_frame
	if failures == 0:
		print("RESULT: PASS - explicitly synchronized character transforms are visible in either update order")
	quit(1 if failures else 0)
