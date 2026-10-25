extends SceneTree

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	# A non-flat 5x5 grid centered at the shape origin: y = 0.15*x + 0.1*z.
	var heightmap := HeightMapShape3D.new()
	heightmap.map_width = 5
	heightmap.map_depth = 5
	var heights := PackedFloat32Array()
	for z in range(-2, 3):
		for x in range(-2, 3):
			heights.append(0.15 * x + 0.1 * z)
	heightmap.map_data = heights
	var fixtures: Array[Dictionary] = []
	for index in 2:
		var floor_body := StaticBody3D.new()
		floor_body.position = Vector3(index * 20.0, 0.5, 0)
		var collision := CollisionShape3D.new()
		collision.shape = heightmap
		if index == 1:
			floor_body.rotation.y = -PI / 4.0
			collision.position = Vector3(2, 0.75, -1)
			collision.rotation.y = PI / 2.0
		floor_body.add_child(collision)
		root.add_child(floor_body)
		fixtures.append({"floor": floor_body, "collision": collision})
	await physics_frame
	await physics_frame
	for fixture in fixtures:
		_check_grid(fixture)
	if failures:
		quit(1)
		return

	# Moving one attachment must not change another owner of the same resource.
	fixtures[1].collision.position.y += 0.4
	await physics_frame
	await physics_frame
	for fixture in fixtures:
		_check_grid(fixture)

	# Constrain sideways rolling so the independent plane height predicts support.
	for fixture in fixtures:
		var sphere_body := RigidBody3D.new()
		sphere_body.axis_lock_linear_x = true
		sphere_body.axis_lock_linear_z = true
		sphere_body.axis_lock_angular_x = true
		sphere_body.axis_lock_angular_y = true
		sphere_body.axis_lock_angular_z = true
		var solid := CollisionShape3D.new()
		var sphere := SphereShape3D.new()
		sphere.radius = 0.25
		solid.shape = sphere
		sphere_body.add_child(solid)
		var surface: Vector3 = fixture.collision.global_transform * Vector3(-1.25, -0.2625, -0.75)
		sphere_body.position = surface + Vector3.UP * 2.0
		root.add_child(sphere_body)
		fixture["body"] = sphere_body
		fixture["expected_y"] = surface.y + 0.25 * sqrt(1.0 + 0.15 * 0.15 + 0.1 * 0.1)
	for frame in 180:
		await physics_frame
	for fixture in fixtures:
		if not fixture.body.position.is_finite() or absf(fixture.body.position.y - fixture.expected_y) > 0.025:
			_fail("rigid body did not rest on its centered, transformed heightmap: %s (expected y %.4f)" % [fixture.body.position, fixture.expected_y])
	await _check_cell_geometry()
	if failures == 0:
		print("RESULT: PASS - centered/transformed heightmaps support bodies, preserve shared owners and honor cell diagonals/NaN holes")
	quit(1 if failures else 0)


func _check_grid(fixture: Dictionary) -> void:
	var collision: CollisionShape3D = fixture.collision
	var space := collision.get_world_3d().direct_space_state
	# Hand-derived heights; asymmetric points catch origin and row-order mistakes.
	for sample in [Vector3(-1.25, -0.2625, -0.75), Vector3.ZERO, Vector3(1.25, 0.2625, 0.75)]:
		var point: Vector3 = collision.global_transform * sample
		var ray := PhysicsRayQueryParameters3D.create(point + Vector3.UP * 3.0, point - Vector3.UP * 3.0)
		var hit := space.intersect_ray(ray)
		var expected_normal := (collision.global_basis * Vector3(-0.15, 1, -0.1)).normalized()
		if hit.is_empty():
			_fail("centered heightmap ray missed at local point %s" % sample)
		elif hit.rid != fixture.floor.get_rid() or hit.shape != 0 or hit.position.distance_to(point) > 0.002 or hit.normal.dot(expected_normal) < 0.999:
			_fail("heightmap returned the wrong surface, normal or collider: %s (expected %s)" % [hit, point])
	var outside: Vector3 = collision.global_transform * Vector3(2.5, 0, 0)
	var outside_ray := PhysicsRayQueryParameters3D.create(outside + Vector3.UP * 3.0, outside - Vector3.UP * 3.0)
	if not space.intersect_ray(outside_ray).is_empty():
		_fail("heightmap extends beyond its centered grid bounds")


func _fail(message: String) -> void:
	failures += 1
	print("RESULT: FAIL - ", message)


func _check_cell_geometry() -> void:
	var fixtures := [
		{"width": 2, "data": PackedFloat32Array([0, 0, 0, 1]), "rays": [Vector3(-0.25, 0, -0.25), Vector3(0.25, 0.5, 0.25)]},
		{"width": 3, "data": PackedFloat32Array([0, 0, 0, 0, NAN, 0, 0, 0, 0]), "rays": [Vector3(-0.75, 0, -0.75), Vector3(0.75, 0, 0.75)]},
		{"width": 2, "data": PackedFloat32Array([NAN, NAN, NAN, NAN]), "rays": []},
	]
	for index in fixtures.size():
		var fixture: Dictionary = fixtures[index]
		var floor_body := StaticBody3D.new()
		floor_body.position.x = 60 + index * 10
		var collision := CollisionShape3D.new()
		var heightmap := HeightMapShape3D.new()
		heightmap.map_width = fixture.width
		heightmap.map_depth = fixture.width
		heightmap.map_data = fixture.data
		collision.shape = heightmap
		floor_body.add_child(collision)
		root.add_child(floor_body)
		await physics_frame
		var space := floor_body.get_world_3d().direct_space_state
		for sample in fixture.rays:
			var point: Vector3 = floor_body.position + sample
			var ray := PhysicsRayQueryParameters3D.create(point + Vector3.UP * 2, point - Vector3.UP * 2)
			var hit := space.intersect_ray(ray)
			if hit.is_empty() or hit.position.distance_to(point) > 0.002:
				_fail("heightmap cell diagonal or surviving hole triangle is incorrect at %s: %s" % [sample, hit])
		if index >= 1:
			var hole := floor_body.position + Vector3(-0.25, 0, -0.25)
			var ray := PhysicsRayQueryParameters3D.create(hole + Vector3.UP * 2, hole - Vector3.UP * 2)
			if not space.intersect_ray(ray).is_empty():
				_fail("a NaN vertex did not leave a hole in the heightmap")
