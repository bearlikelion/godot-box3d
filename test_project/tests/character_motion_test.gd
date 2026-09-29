extends SceneTree

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _make_box(size: Vector3) -> CollisionShape3D:
	var node := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	node.shape = shape
	return node


func _run() -> void:
	var floor := StaticBody3D.new()
	floor.position = Vector3(0, -0.5, 0)
	floor.add_child(_make_box(Vector3(20, 1, 20)))
	root.add_child(floor)

	var wall := StaticBody3D.new()
	wall.position = Vector3(0, 3, -3)
	wall.add_child(_make_box(Vector3(20, 6, 0.2)))
	root.add_child(wall)

	var character := CharacterBody3D.new()
	character.position = Vector3(0, 0.9, 0)
	var collision := CollisionShape3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.35
	capsule.height = 1.8
	collision.shape = capsule
	character.add_child(collision)
	root.add_child(character)
	await physics_frame

	var minimum_z := character.position.z
	var invalid_normal := false
	for frame in 120:
		character.velocity = Vector3(1.5, -0.5, -9.0)
		character.move_and_slide()
		minimum_z = minf(minimum_z, character.position.z)
		for index in character.get_slide_collision_count():
			var normal := character.get_slide_collision(index).get_normal()
			if not normal.is_normalized():
				failures += 1
				invalid_normal = true
				print("RESULT: FAIL - collision %d on frame %d returned an invalid normal: %s" % [index, frame, normal])
				break
		if invalid_normal:
			break
		await physics_frame

	if not invalid_normal and minimum_z <= -2.57:
		failures += 1
		print("RESULT: FAIL - CharacterBody3D crossed the wall under sustained pressure (z=%.4f)" % minimum_z)
	if not invalid_normal and absf(character.position.y - 0.9) > 0.03:
		failures += 1
		print("RESULT: FAIL - CharacterBody3D did not remain grounded (y=%.4f)" % character.position.y)

	if failures == 0:
		print("RESULT: PASS - CharacterBody3D returns valid normals and remains grounded at a wall")
	quit(1 if failures > 0 else 0)
