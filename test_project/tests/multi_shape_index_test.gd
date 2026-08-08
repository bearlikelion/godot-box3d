extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var query_target: StaticBody3D = StaticBody3D.new()
	query_target.name = "MultiShapeQueryTarget"
	_add_box(query_target, Vector3(-4, 0, 0), Vector3(2, 2, 2), true)
	_add_box(query_target, Vector3(4, 0, 0), Vector3(2, 2, 2), false)
	root.add_child(query_target)

	var ground: StaticBody3D = StaticBody3D.new()
	ground.name = "MultiShapeGround"
	ground.position = Vector3(0, -0.5, 20)
	_add_box(ground, Vector3(-8, 0, 0), Vector3(6, 1, 6), true)
	_add_box(ground, Vector3.ZERO, Vector3(6, 1, 6), false)
	root.add_child(ground)

	var body: RigidBody3D = RigidBody3D.new()
	body.name = "MultiShapeBody"
	body.position = Vector3(0, 3, 20)
	body.contact_monitor = true
	body.max_contacts_reported = 8
	body.can_sleep = false
	_add_sphere(body, Vector3(8, 0, 0), true)
	_add_sphere(body, Vector3.ZERO, false)
	root.add_child(body)

	await physics_frame
	await physics_frame

	var space_state: PhysicsDirectSpaceState3D = root.world_3d.direct_space_state
	var ray_query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(
		Vector3(4, 3, 0),
		Vector3(4, -3, 0),
	)
	var ray_hit: Dictionary = space_state.intersect_ray(ray_query)
	_check(ray_hit.get("rid") == query_target.get_rid(), "ray query hits the multi-shape body")
	_check(ray_hit.get("shape", -1) == 1, "ray query reports the active second shape")

	var point_query: PhysicsPointQueryParameters3D = PhysicsPointQueryParameters3D.new()
	point_query.position = Vector3(4, 0, 0)
	var point_hit: Dictionary = _find_rid(space_state.intersect_point(point_query), query_target.get_rid())
	_check(not point_hit.is_empty(), "point query hits the multi-shape body")
	_check(point_hit.get("shape", -1) == 1, "point query reports the active second shape")

	var query_box: BoxShape3D = BoxShape3D.new()
	query_box.size = Vector3(0.5, 0.5, 0.5)
	var shape_query: PhysicsShapeQueryParameters3D = PhysicsShapeQueryParameters3D.new()
	shape_query.shape = query_box
	shape_query.transform = Transform3D(Basis(), Vector3(4, 0, 0))
	var shape_hit: Dictionary = _find_rid(space_state.intersect_shape(shape_query), query_target.get_rid())
	_check(not shape_hit.is_empty(), "shape query hits the multi-shape body")
	_check(shape_hit.get("shape", -1) == 1, "shape query reports the active second shape")

	for frame in 180:
		await physics_frame

	var body_state: PhysicsDirectBodyState3D = PhysicsServer3D.body_get_direct_state(body.get_rid())
	var contact_index: int = _find_contact(body_state, ground.get_rid())
	_check(contact_index >= 0, "multi-shape body reports its resting ground contact")
	if contact_index >= 0:
		_check(body_state.get_contact_local_shape(contact_index) == 1, "contact reports the body's active second shape")
		_check(body_state.get_contact_collider_shape(contact_index) == 1, "contact reports the ground's active second shape")

	if failures == 0:
		print("RESULT: PASS - queries and contacts report multi-shape indices")
	else:
		print("RESULT: FAIL - ", failures, " multi-shape index assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _add_box(owner: CollisionObject3D, position: Vector3, size: Vector3, disabled: bool) -> void:
	var collision: CollisionShape3D = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = size
	collision.position = position
	collision.shape = box
	collision.disabled = disabled
	owner.add_child(collision)


func _add_sphere(owner: CollisionObject3D, position: Vector3, disabled: bool) -> void:
	var collision: CollisionShape3D = CollisionShape3D.new()
	var sphere: SphereShape3D = SphereShape3D.new()
	sphere.radius = 0.5
	collision.position = position
	collision.shape = sphere
	collision.disabled = disabled
	owner.add_child(collision)


func _find_rid(hits: Array[Dictionary], collider_rid: RID) -> Dictionary:
	for hit in hits:
		if hit.get("rid") == collider_rid:
			return hit
	return {}


func _find_contact(state: PhysicsDirectBodyState3D, collider_rid: RID) -> int:
	for index in state.get_contact_count():
		if state.get_contact_collider(index) == collider_rid:
			return index
	return -1


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
