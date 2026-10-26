extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	await _inside_hits()
	await _mesh_hits()
	await _heightmap_hits()
	if failures == 0:
		print("RESULT: PASS - ray inside/backface flags, input face indices and filtering")
	quit(1 if failures > 0 else 0)


func _inside_hits() -> void:
	var shapes: Array[Shape3D] = [BoxShape3D.new(), SphereShape3D.new(), CapsuleShape3D.new()]
	for shape in shapes:
		var body: StaticBody3D = StaticBody3D.new()
		var collision: CollisionShape3D = CollisionShape3D.new()
		collision.shape = shape
		body.add_child(collision)
		root.add_child(body)
		await physics_frame
		await physics_frame
		var state: PhysicsDirectSpaceState3D = root.world_3d.direct_space_state
		var query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(Vector3.ZERO, Vector3(2, 0, 0))
		query.hit_from_inside = false
		_check(state.intersect_ray(query).is_empty(), "disabled inside hit: %s" % shape.get_class())
		query.hit_from_inside = true
		var hit: Dictionary = state.intersect_ray(query)
		_check(hit.get("collider") == body and hit.get("position") == Vector3.ZERO and hit.get("normal") == Vector3.ZERO,
				"enabled inside hit has origin and zero normal: %s" % shape.get_class())
		_check(hit.get("face_index", -2) == -1, "convex ray hits have no mesh face index")
		body.queue_free()
		await process_frame
		await physics_frame


func _mesh_hits() -> void:
	var terrain: StaticBody3D = StaticBody3D.new()
	terrain.position = Vector3(10, 0, 0)
	terrain.rotation.z = 0.3
	var collision: CollisionShape3D = CollisionShape3D.new()
	collision.position = Vector3(1, 0.4, -0.25)
	collision.rotation.x = 0.25
	var mesh: ConcavePolygonShape3D = ConcavePolygonShape3D.new()
	mesh.set_faces(PackedVector3Array([
		Vector3(5, 0, -1), Vector3(7, 0, -1), Vector3(6, 0, 1),
		Vector3(-1, 0, -1), Vector3(1, 0, -1), Vector3(0, 0, 1),
	]))
	mesh.backface_collision = true
	collision.shape = mesh
	terrain.add_child(collision)
	root.add_child(terrain)
	await physics_frame
	await physics_frame
	var data: Dictionary = PhysicsServer3D.shape_get_data(mesh.get_rid())
	_check(data.get("backface_collision", false), "mesh shape data preserves the backface setting")
	var transform: Transform3D = terrain.global_transform * collision.transform
	var point: Vector3 = transform.origin
	var normal: Vector3 = transform.basis * Vector3.UP
	var state: PhysicsDirectSpaceState3D = root.world_3d.direct_space_state
	var front: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(point + normal, point - normal)
	front.hit_back_faces = false
	var front_hit: Dictionary = state.intersect_ray(front)
	_check(front_hit.get("face_index", -1) == 1, "front ray reports the original input triangle index")
	_check(front_hit.get("normal", Vector3.ZERO).distance_to(normal) < 0.01, "front ray has the transformed surface normal")
	var back: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(point - normal, point + normal)
	back.hit_back_faces = false
	_check(state.intersect_ray(back).is_empty(), "disabled backface hit is omitted")
	back.hit_back_faces = true
	var hit: Dictionary = state.intersect_ray(back)
	_check(hit.get("collider") == terrain and hit.get("face_index", -1) == 1,
			"backface ray reports the collider and original input triangle index")
	_check(hit.get("position", Vector3.ZERO).distance_to(point) < 0.01 and hit.get("normal", Vector3.ZERO).distance_to(-normal) < 0.01,
			"backface hit has the transformed position and opposes the ray")
	back.exclude = [terrain.get_rid()]
	_check(state.intersect_ray(back).is_empty(), "backface fallback honors exclusions")
	back.exclude = []
	back.collision_mask = 2
	_check(state.intersect_ray(back).is_empty(), "backface fallback honors collision masks")
	back.collision_mask = 1
	back.collide_with_bodies = false
	_check(state.intersect_ray(back).is_empty(), "backface fallback honors the body filter")
	back.collide_with_bodies = true
	var blocker: StaticBody3D = StaticBody3D.new()
	var blocker_collision: CollisionShape3D = CollisionShape3D.new()
	var blocker_shape: SphereShape3D = SphereShape3D.new()
	blocker_shape.radius = 0.1
	blocker_collision.shape = blocker_shape
	blocker.add_child(blocker_collision)
	blocker.position = point - normal * 0.5
	root.add_child(blocker)
	await physics_frame
	_check(state.intersect_ray(back).get("collider") == blocker, "a farther backface cannot replace a closer convex hit")
	blocker.queue_free()
	await process_frame
	mesh.backface_collision = false
	await physics_frame
	_check(state.intersect_ray(back).is_empty(), "the concave resource must also enable backface collision")
	terrain.queue_free()
	await process_frame
	await physics_frame


func _heightmap_hits() -> void:
	var terrain: StaticBody3D = StaticBody3D.new()
	terrain.position = Vector3(30, 0, 0)
	var collision: CollisionShape3D = CollisionShape3D.new()
	var heights: HeightMapShape3D = HeightMapShape3D.new()
	heights.map_width = 3
	heights.map_depth = 3
	heights.map_data = PackedFloat32Array([0, 0, 0, 0, 0, 0, 0, 0, 0])
	collision.shape = heights
	terrain.add_child(collision)
	root.add_child(terrain)
	await physics_frame
	await physics_frame
	var state: PhysicsDirectSpaceState3D = root.world_3d.direct_space_state
	var point: Vector3 = terrain.position + Vector3(0.5, 0, 0.25)
	var query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(point - Vector3.UP, point + Vector3.UP)
	query.hit_back_faces = true
	var hit: Dictionary = state.intersect_ray(query)
	_check(hit.get("collider") == terrain and hit.get("normal", Vector3.ZERO).distance_to(Vector3.DOWN) < 0.01,
			"heightmap backface hits oppose the ray")
	_check(hit.get("face_index", -2) == -1, "heightmap hits have no concave input face index")
	query.hit_back_faces = false
	_check(state.intersect_ray(query).is_empty(), "heightmap backface hits honor the query flag")
	terrain.queue_free()
	await process_frame
	await physics_frame


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		print("RESULT: FAIL - ", message)
