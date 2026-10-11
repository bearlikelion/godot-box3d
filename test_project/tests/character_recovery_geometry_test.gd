extends SceneTree

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _query_shape(kind: String) -> Shape3D:
	match kind:
		"sphere":
			var shape := SphereShape3D.new()
			shape.radius = 0.35
			return shape
		"capsule":
			var shape := CapsuleShape3D.new()
			shape.radius = 0.35
			shape.height = 1.8
			return shape
		"cylinder":
			var shape := CylinderShape3D.new()
			shape.radius = 0.35
			shape.height = 1.8
			return shape
		"convex":
			var shape := ConvexPolygonShape3D.new()
			var points := PackedVector3Array()
			for x in [-0.35, 0.35]:
				for y in [-0.9, 0.9]:
					for z in [-0.35, 0.35]:
						points.append(Vector3(x, y, z))
			shape.points = points
			return shape
	var box := BoxShape3D.new()
	box.size = Vector3(0.7, 1.8, 0.7)
	return box


func _support(kind: String, normal: Vector3) -> float:
	if kind == "sphere":
		return 0.35
	if kind == "capsule":
		return 0.35 + 0.55 * normal.y
	return 0.35 * absf(normal.x) + 0.9 * normal.y


func _run() -> void:
	var fixtures: Array[Dictionary] = []
	for floor_kind in ["box", "slope", "trimesh", "heightmap", "boundary"]:
		for query_kind in ["sphere", "capsule", "box", "cylinder", "convex"]:
			var floor_body := StaticBody3D.new()
			floor_body.collision_layer = 1 << fixtures.size()
			floor_body.position = Vector3(fixtures.size() * 20.0, -0.5, 0)
			var surface := CollisionShape3D.new()
			var half_thickness := 0.5
			match floor_kind:
				"trimesh":
					var mesh := BoxMesh.new()
					mesh.size = Vector3(10, 1, 10)
					surface.shape = mesh.create_trimesh_shape()
				"heightmap":
					var heights := HeightMapShape3D.new()
					heights.map_width = 5
					heights.map_depth = 5
					var data := PackedFloat32Array()
					data.resize(25)
					data.fill(0.0)
					heights.map_data = data
					surface.shape = heights
					floor_body.position.y = 0.0
					half_thickness = 0.0
				"boundary":
					var boundary := WorldBoundaryShape3D.new()
					boundary.plane = Plane(Vector3.UP, 0)
					surface.shape = boundary
					floor_body.position.y = 0.0
					half_thickness = 0.0
				_:
					var box := BoxShape3D.new()
					box.size = Vector3(10, 1, 10)
					surface.shape = box
					if floor_kind == "slope":
						floor_body.rotation.z = PI / 6.0
			floor_body.add_child(surface)
			root.add_child(floor_body)
			var normal := floor_body.global_basis * Vector3.UP
			var character := CharacterBody3D.new()
			character.collision_mask = floor_body.collision_layer
			character.collision_layer = 0
			var collision := CollisionShape3D.new()
			collision.shape = _query_shape(query_kind)
			character.add_child(collision)
			character.position = floor_body.position + normal * (half_thickness + _support(query_kind, normal) - 0.02)
			if floor_kind == "heightmap":
				character.position += Vector3(1, 0, 1)
			root.add_child(character)
			fixtures.append({"label": floor_kind + "/" + query_kind, "body": character, "floor": floor_body, "normal": normal})
	await physics_frame
	await physics_frame
	for fixture in fixtures:
		var character: CharacterBody3D = fixture.body
		var parameters := PhysicsTestMotionParameters3D.new()
		parameters.from = character.global_transform
		parameters.margin = 0.001
		parameters.max_collisions = 8
		parameters.recovery_as_collision = true
		var result := PhysicsTestMotionResult3D.new()
		var hit := PhysicsServer3D.body_test_motion(character.get_rid(), parameters, result)
		var travel := result.get_travel()
		var expected: Vector3 = fixture.normal
		if not hit or result.get_collision_count() == 0:
			_fail(fixture.label + " did not report its 2 cm overlap")
			continue
		if not travel.is_finite() or travel.dot(expected) < 0.015 or travel.length() > 0.045:
			_fail("%s used an incorrect recovery displacement: %s" % [fixture.label, travel])
		if travel.length() > 0.001 and travel.normalized().dot(expected) < 0.98:
			_fail("%s recovered along a bounding-box axis instead of the surface normal" % fixture.label)
		if result.get_collision_normal().dot(expected) < 0.98:
			_fail("%s returned an incorrect contact normal: %s" % [fixture.label, result.get_collision_normal()])
		if result.get_collider_rid() != fixture.floor.get_rid() or result.get_collision_local_shape() != 0 or result.get_collider_shape() != 0:
			_fail(fixture.label + " returned incorrect collider/shape metadata")
		if not character.global_transform.is_equal_approx(parameters.from):
			_fail(fixture.label + " mutated the character during a read-only query")
		parameters.recovery_as_collision = false
		var unreported := PhysicsTestMotionResult3D.new()
		if PhysicsServer3D.body_test_motion(character.get_rid(), parameters, unreported) or not unreported.get_travel().is_equal_approx(travel):
			_fail(fixture.label + " incorrectly gated recovery on recovery_as_collision")
		parameters.from.origin += travel
		var settled := PhysicsTestMotionResult3D.new()
		if PhysicsServer3D.body_test_motion(character.get_rid(), parameters, settled) or settled.get_travel().length() > 0.003:
			_fail(fixture.label + " did not converge after recovery")
		# A margin wider than native speculative contacts must still use actual
		# positive-distance geometry, including against mesh triangles.
		parameters.from.origin += expected * 0.025
		parameters.margin = 0.06
		parameters.recovery_as_collision = true
		var margin_result := PhysicsTestMotionResult3D.new()
		if not PhysicsServer3D.body_test_motion(character.get_rid(), parameters, margin_result) or margin_result.get_travel().dot(expected) < 0.02 or margin_result.get_travel().length() > 0.045:
			_fail(fixture.label + " did not respect the positive-distance margin shell")
	if failures == 0:
		print("RESULT: PASS - actual contact recovery for five convex shapes on primitive, slope and terrain surfaces")
	quit(1 if failures else 0)


func _fail(message: String) -> void:
	failures += 1
	print("RESULT: FAIL - ", message)
