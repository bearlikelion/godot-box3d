extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	_check(ProjectSettings.get_setting("physics/3d/physics_engine", "") == "Box3D Physics (Extension)", "test project requests the Box3D backend")
	_check(ClassDB.class_exists(&"Box3DPhysicsServer3D"), "Box3D extension is loaded")
	if failures > 0:
		quit(1)
		return

	var accepts_floor: StaticBody3D = _make_floor(Vector3(-4, -0.5, 0), 1, 0)
	var floor_accepts: StaticBody3D = _make_floor(Vector3(0, -0.5, 0), 1, 2)
	_make_floor(Vector3(4, -0.5, 0), 1, 0)

	var body_accepts: RigidBody3D = _make_body(Vector3(-4, 3, 0), 2, 1)
	var accepted_by_floor: RigidBody3D = _make_body(Vector3(0, 3, 0), 2, 0)
	var no_match: RigidBody3D = _make_body(Vector3(4, 3, 0), 2, 0)

	for frame in 240:
		await physics_frame

	_check(body_accepts.global_position.y > -1.0, "body mask accepting the floor creates a collision pair")
	_check(accepted_by_floor.global_position.y > -1.0, "floor mask accepting the body creates a collision pair")
	_check(no_match.global_position.y < -5.0, "objects with no layer/mask match do not collide")

	var query_target: StaticBody3D = _make_floor(Vector3(10, 0, 0), 4, 0)
	await physics_frame
	var query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(Vector3(10, 3, 0), Vector3(10, -3, 0))
	query.collision_mask = 4
	var hit: Dictionary = root.world_3d.direct_space_state.intersect_ray(query)
	_check(not hit.is_empty(), "direct queries match a target layer regardless of the target mask")

	accepts_floor.queue_free()
	floor_accepts.queue_free()
	query_target.queue_free()
	if failures == 0:
		print("RESULT: PASS - collision filters match Godot semantics")
	else:
		print("RESULT: FAIL - ", failures, " collision filter assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _make_floor(position: Vector3, layer: int, mask: int) -> StaticBody3D:
	var floor_body: StaticBody3D = StaticBody3D.new()
	floor_body.position = position
	floor_body.collision_layer = layer
	floor_body.collision_mask = mask
	var collision_shape: CollisionShape3D = CollisionShape3D.new()
	var shape: BoxShape3D = BoxShape3D.new()
	shape.size = Vector3(3, 1, 3)
	collision_shape.shape = shape
	floor_body.add_child(collision_shape)
	root.add_child(floor_body)
	return floor_body


func _make_body(position: Vector3, layer: int, mask: int) -> RigidBody3D:
	var body: RigidBody3D = RigidBody3D.new()
	body.position = position
	body.collision_layer = layer
	body.collision_mask = mask
	body.can_sleep = false
	var collision_shape: CollisionShape3D = CollisionShape3D.new()
	var shape: SphereShape3D = SphereShape3D.new()
	shape.radius = 0.5
	collision_shape.shape = shape
	body.add_child(collision_shape)
	root.add_child(body)
	return body


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
