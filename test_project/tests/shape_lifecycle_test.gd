extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	await _test_shared_shape_live_update()
	await _test_live_concave_update()
	await _test_live_heightmap_update()
	await _test_shape_attachment_mutations()
	await _test_free_attached_shape_rid()

	if failures == 0:
		print("RESULT: PASS - shared shape lifecycle is safe and live")
	else:
		print("RESULT: FAIL - ", failures, " shape lifecycle assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _test_shared_shape_live_update() -> void:
	var shared: SphereShape3D = SphereShape3D.new()
	shared.radius = 0.5
	var left: StaticBody3D = _make_static_body(Vector3(-2, 0, 0), shared)
	var right: StaticBody3D = _make_static_body(Vector3(2, 0, 0), shared)
	# Exercise duplicate references from one owner as well as sharing between owners.
	var duplicate: CollisionShape3D = CollisionShape3D.new()
	duplicate.position = Vector3(0, 0, 3)
	duplicate.shape = shared
	left.add_child(duplicate)

	await physics_frame
	await physics_frame
	_check_ray_height(-2.0, 0.5, "the first body uses the initial shared radius")
	_check_ray_height(2.0, 0.5, "the second body uses the initial shared radius")

	shared.radius = 1.25
	await physics_frame
	await physics_frame
	_check_ray_height(-2.0, 1.25, "live data updates rebuild the first shared fixture")
	_check_ray_height(2.0, 1.25, "live data updates rebuild the second shared fixture")

	left.queue_free()
	await physics_frame
	shared.radius = 0.75
	await physics_frame
	await physics_frame
	_check_ray_height(2.0, 0.75, "a surviving owner updates after another owner is freed")

	right.queue_free()
	await physics_frame


func _test_live_concave_update() -> void:
	var shape: ConcavePolygonShape3D = ConcavePolygonShape3D.new()
	shape.set_faces(_horizontal_quad(0.0))
	var body: StaticBody3D = _make_static_body(Vector3(0, 0, 0), shape)

	await physics_frame
	await physics_frame
	_check_ray_height(0.0, 0.0, "the initial concave mesh is queryable")

	# Concave and heightfield fixtures retain pointers to their source geometry, so this
	# specifically verifies the old fixture is gone before set_data replaces that buffer.
	shape.set_faces(_horizontal_quad(1.5))
	await physics_frame
	await physics_frame
	_check_ray_height(0.0, 1.5, "live concave data safely replaces retained geometry")

	body.queue_free()
	await physics_frame


func _test_live_heightmap_update() -> void:
	var shape: HeightMapShape3D = HeightMapShape3D.new()
	shape.map_width = 2
	shape.map_depth = 2
	shape.map_data = PackedFloat32Array([0.0, 0.0, 0.0, 0.0])
	var body: StaticBody3D = _make_static_body(Vector3(0, 0, 0), shape)

	await physics_frame
	await physics_frame
	_check_ray_height(0.0, 0.0, "the initial heightmap is queryable")

	shape.map_data = PackedFloat32Array([1.0, 1.0, 1.0, 1.0])
	await physics_frame
	await physics_frame
	_check_ray_height(0.0, 1.0, "live heightmap data safely replaces retained geometry")

	body.queue_free()
	await physics_frame


func _test_shape_attachment_mutations() -> void:
	var body: RID = PhysicsServer3D.body_create()
	var first: RID = PhysicsServer3D.sphere_shape_create()
	var second: RID = PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(first, 0.5)
	PhysicsServer3D.shape_set_data(second, Vector3(0.5, 0.5, 0.5))
	PhysicsServer3D.body_add_shape(body, first)
	PhysicsServer3D.body_add_shape(body, first, Transform3D(Basis(), Vector3(0, 2, 0)))
	_check(PhysicsServer3D.body_get_shape_count(body) == 2, "one shape can be attached twice")

	PhysicsServer3D.body_set_shape(body, 0, second)
	PhysicsServer3D.body_remove_shape(body, 0)
	PhysicsServer3D.shape_set_data(second, Vector3(0.75, 0.75, 0.75))
	PhysicsServer3D.shape_set_data(first, 0.75)
	_check(PhysicsServer3D.body_get_shape_count(body) == 1, "replace and remove preserve attachment bookkeeping")

	PhysicsServer3D.body_clear_shapes(body)
	PhysicsServer3D.shape_set_data(first, 1.0)
	_check(PhysicsServer3D.body_get_shape_count(body) == 0, "clearing removes every shape reference")

	PhysicsServer3D.free_rid(second)
	PhysicsServer3D.free_rid(first)
	PhysicsServer3D.free_rid(body)


func _test_free_attached_shape_rid() -> void:
	var body: RID = PhysicsServer3D.body_create()
	var shape: RID = PhysicsServer3D.sphere_shape_create()
	PhysicsServer3D.shape_set_data(shape, 0.5)
	PhysicsServer3D.body_add_shape(body, shape)
	PhysicsServer3D.body_add_shape(body, shape, Transform3D(Basis(), Vector3(0, 2, 0)))

	PhysicsServer3D.free_rid(shape)
	_check(PhysicsServer3D.body_get_shape_count(body) == 0, "freeing an attached shape removes every reference")
	PhysicsServer3D.free_rid(body)


func _make_static_body(position: Vector3, shape: Shape3D) -> StaticBody3D:
	var body: StaticBody3D = StaticBody3D.new()
	body.position = position
	var collision: CollisionShape3D = CollisionShape3D.new()
	collision.shape = shape
	body.add_child(collision)
	root.add_child(body)
	return body


func _horizontal_quad(y: float) -> PackedVector3Array:
	return PackedVector3Array([
		Vector3(-1, y, -1), Vector3(1, y, -1), Vector3(1, y, 1),
		Vector3(-1, y, -1), Vector3(1, y, 1), Vector3(-1, y, 1),
	])


func _check_ray_height(x: float, expected_y: float, message: String) -> void:
	var query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(
		Vector3(x, 4, 0),
		Vector3(x, -2, 0),
	)
	var hit: Dictionary = root.world_3d.direct_space_state.intersect_ray(query)
	_check(not hit.is_empty() and absf(float(hit.get("position", Vector3()).y) - expected_y) <= 0.06, message)


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
