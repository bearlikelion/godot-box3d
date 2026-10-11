extends SceneTree

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _box(size: Vector3) -> CollisionShape3D:
	var collision := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	collision.shape = shape
	return collision


func _query(body: CharacterBody3D, from: Transform3D, motion: Vector3 = Vector3.ZERO) -> PhysicsTestMotionResult3D:
	var parameters := PhysicsTestMotionParameters3D.new()
	parameters.from = from
	parameters.motion = motion
	parameters.margin = 0.001
	parameters.max_collisions = 8
	parameters.recovery_as_collision = true
	var result := PhysicsTestMotionResult3D.new()
	PhysicsServer3D.body_test_motion(body.get_rid(), parameters, result)
	return result


func _run() -> void:
	var floor_body := StaticBody3D.new()
	floor_body.position.y = -0.5
	floor_body.add_child(_box(Vector3(8, 1, 8)))
	root.add_child(floor_body)
	var wall := StaticBody3D.new()
	wall.position = Vector3(-0.5, 2, 0)
	wall.add_child(_box(Vector3(1, 4, 8)))
	root.add_child(wall)
	var character := CharacterBody3D.new()
	character.position = Vector3(0.48, 0.48, 0)
	var disabled := _box(Vector3(10, 10, 10))
	disabled.disabled = true
	character.add_child(disabled)
	# Local offset is baked into the live hull and must also apply to the
	# hypothetical query transform. Both attachments share the same resource.
	var collision := _box(Vector3.ONE)
	collision.position.z = 0.25
	character.add_child(collision)
	var second := CollisionShape3D.new()
	second.shape = collision.shape
	second.position.z = -0.75
	character.add_child(second)
	root.add_child(character)
	var left := StaticBody3D.new()
	left.position = Vector3(18.55, 1, 0)
	left.add_child(_box(Vector3(1, 4, 4)))
	root.add_child(left)
	var right := StaticBody3D.new()
	right.position = Vector3(21.45, 1, 0)
	right.add_child(_box(Vector3(1, 4, 4)))
	root.add_child(right)
	var trapped := CharacterBody3D.new()
	trapped.position = Vector3(20, 1, 0)
	trapped.add_child(_box(Vector3(2, 1, 1)))
	root.add_child(trapped)
	var corner := StaticBody3D.new()
	corner.position.x = 40.0
	var faces := PackedVector3Array()
	for part in [{"size": Vector3(8, 1, 8), "offset": Vector3(0, -0.5, 0)}, {"size": Vector3(1, 4, 8), "offset": Vector3(-0.5, 2, 0)}]:
		var mesh := BoxMesh.new()
		mesh.size = part.size
		for vertex in mesh.get_faces():
			faces.append(vertex + part.offset)
	var corner_shape := ConcavePolygonShape3D.new()
	corner_shape.set_faces(faces)
	var corner_collision := CollisionShape3D.new()
	corner_collision.shape = corner_shape
	corner.add_child(corner_collision)
	root.add_child(corner)
	var mesh_character := CharacterBody3D.new()
	mesh_character.position = Vector3(40.48, 0.48, 0)
	mesh_character.add_child(_box(Vector3.ONE))
	root.add_child(mesh_character)
	await physics_frame
	await physics_frame
	var original := character.global_transform
	var result := _query(character, original)
	if result.get_travel().x < 0.018 or result.get_travel().y < 0.018 or result.get_travel().length() > 0.05:
		_fail("floor/wall compound recovery did not satisfy both surfaces: %s" % result.get_travel())
	var floor_found := false
	var wall_found := false
	for index in result.get_collision_count():
		if result.get_collision_local_shape(index) not in [1, 2]:
			_fail("recovery reported a disabled or incorrect local shape")
		floor_found = floor_found or result.get_collider_rid(index) == floor_body.get_rid()
		wall_found = wall_found or result.get_collider_rid(index) == wall.get_rid()
	if not floor_found or not wall_found:
		_fail("compound recovery did not report floor and wall contacts")
	var corner_result := _query(mesh_character, mesh_character.global_transform)
	var corner_floor := false
	var corner_wall := false
	for index in corner_result.get_collision_count():
		corner_floor = corner_floor or corner_result.get_collision_normal(index).dot(Vector3.UP) > 0.99
		corner_wall = corner_wall or corner_result.get_collision_normal(index).dot(Vector3.RIGHT) > 0.99
	if not corner_floor or not corner_wall:
		_fail("one concave collider must preserve distinct floor and wall recovery normals")
	for repeat in 20:
		var repeated := _query(character, original)
		if not repeated.get_travel().is_equal_approx(result.get_travel()) or not character.global_transform.is_equal_approx(original):
			_fail("read-only recovery changed the live body or was not repeatable")
	PhysicsServer3D.body_set_collision_priority(left.get_rid(), 5.0)
	if PhysicsServer3D.body_get_collision_priority(left.get_rid()) != 5.0:
		_fail("collision priority was not stored")
	var prefer_left := _query(trapped, trapped.global_transform)
	PhysicsServer3D.body_set_collision_priority(left.get_rid(), 1.0)
	PhysicsServer3D.body_set_collision_priority(right.get_rid(), 5.0)
	var prefer_right := _query(trapped, trapped.global_transform)
	if prefer_left.get_travel().x < 0.02 or prefer_right.get_travel().x > -0.02:
		_fail("higher-priority walls did not receive stronger recovery protection: %s / %s" % [prefer_left.get_travel(), prefer_right.get_travel()])
	if not prefer_left.get_travel().is_finite() or not prefer_right.get_travel().is_finite() or prefer_left.get_travel().length() > 0.11 or prefer_right.get_travel().length() > 0.11:
		_fail("impossible opposing contacts caused unbounded recovery")
	if failures == 0:
		print("RESULT: PASS - compound floor/wall recovery, read-only queries and bounded priority-aware opposing contacts")
	quit(1 if failures else 0)


func _fail(message: String) -> void:
	failures += 1
	print("RESULT: FAIL - ", message)
