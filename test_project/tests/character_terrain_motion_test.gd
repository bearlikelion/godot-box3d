extends SceneTree

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var fixtures: Array[Dictionary] = []
	for index in 3:
		var floor_body := StaticBody3D.new()
		floor_body.position = Vector3(index * 20.0, -0.5, 0)
		var collision := CollisionShape3D.new()
		var kind: String = ["slope", "trimesh seam", "heightfield seam"][index]
		if index == 0:
			var box := BoxShape3D.new()
			box.size = Vector3(10, 1, 10)
			collision.shape = box
			floor_body.rotation.z = PI / 6.0
		elif index == 1:
			var mesh := BoxMesh.new()
			mesh.size = Vector3(10, 1, 10)
			collision.shape = mesh.create_trimesh_shape()
		else:
			var heightmap := HeightMapShape3D.new()
			heightmap.map_width = 9
			heightmap.map_depth = 9
			var heights := PackedFloat32Array()
			heights.resize(81)
			heights.fill(0.0)
			heightmap.map_data = heights
			collision.shape = heightmap
			floor_body.position.y = 0.0
		floor_body.add_child(collision)
		root.add_child(floor_body)
		var character := CharacterBody3D.new()
		var solid := CollisionShape3D.new()
		var capsule := CapsuleShape3D.new()
		capsule.radius = 0.35
		capsule.height = 1.8
		solid.shape = capsule
		character.add_child(solid)
		character.floor_snap_length = 0.15
		character.position = Vector3(index * 20.0 + (0.5 if index == 2 else -1.0), 0.88, 0.5)
		if index == 0:
			character.position.y = -0.5 + 0.5 / cos(PI / 6.0) - tan(PI / 6.0) + 0.35 / cos(PI / 6.0) + 0.55 - 0.02
		root.add_child(character)
		fixtures.append({"body": character, "floor": floor_body, "kind": kind, "start": character.position.x, "grounded": 0})
	await physics_frame
	await physics_frame
	for frame in 150:
		for fixture in fixtures:
			var character: CharacterBody3D = fixture.body
			character.velocity = Vector3(1, -1, 0)
			character.move_and_slide()
			if character.is_on_floor():
				fixture.grounded += 1
			var expected_y := 0.9
			if fixture.kind == "slope":
				expected_y = -0.5 + 0.5 / cos(PI / 6.0) + tan(PI / 6.0) * (character.position.x - fixture.floor.position.x) + 0.35 / cos(PI / 6.0) + 0.55
			if not character.position.is_finite() or absf(character.position.y - expected_y) > 0.045:
				_fail("%s lost support on frame %d: %s (expected y %.4f)" % [fixture.kind, frame, character.position, expected_y])
				quit(1)
				return
			for index in character.get_slide_collision_count():
				var hit := character.get_slide_collision(index)
				if not hit.get_normal().is_normalized() or hit.get_collider_rid() != fixture.floor.get_rid():
					_fail(fixture.kind + " returned an invalid movement contact")
		await physics_frame
	for fixture in fixtures:
		if fixture.body.position.x - fixture.start < 0.5 or fixture.grounded < 145:
			_fail("%s stalled or lost its grounded state (%d/150 frames)" % [fixture.kind, fixture.grounded])
	if failures == 0:
		print("RESULT: PASS - move_and_slide stays grounded while climbing slopes and crossing mesh/heightfield seams")
	quit(1 if failures else 0)


func _fail(message: String) -> void:
	failures += 1
	print("RESULT: FAIL - ", message)
