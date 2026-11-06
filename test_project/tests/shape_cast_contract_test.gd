extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _body(position: Vector3) -> StaticBody3D:
	var body: StaticBody3D = StaticBody3D.new()
	var collision: CollisionShape3D = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = Vector3.ONE
	collision.shape = box
	body.add_child(collision)
	body.position = position
	root.add_child(body)
	return body


func _run() -> void:
	if PhysicsServer3D.get_class() != &"Box3DPhysicsServer3D":
		_check(false, "Box3D backend did not activate")
		quit(1)
		return
	var overlapped: StaticBody3D = _body(Vector3.ZERO)
	var sphere: SphereShape3D = SphereShape3D.new()
	sphere.radius = 0.25
	var query: PhysicsShapeQueryParameters3D = PhysicsShapeQueryParameters3D.new()
	query.shape = sphere
	query.transform.origin = Vector3(2, 0, 0)
	query.motion = Vector3.UP
	await physics_frame
	await physics_frame
	var state: PhysicsDirectSpaceState3D = root.world_3d.direct_space_state
	_check(state.cast_motion(query) == PackedFloat32Array([1.0, 1.0]), "an unobstructed cast returns full travel")
	query.motion = Vector3.ZERO
	_check(state.cast_motion(query) == PackedFloat32Array([1.0, 1.0]), "a clear stationary cast returns full travel")
	query.transform.origin = Vector3.ZERO
	query.motion = Vector3(2, 0, 0)
	_check(not state.intersect_shape(query).is_empty(), "the initial pose overlaps the first target")
	_check(state.cast_motion(query) == PackedFloat32Array([1.0, 1.0]), "a cast ignores its initial overlap")
	var blocker: StaticBody3D = _body(Vector3(2, 0, 0))
	await physics_frame
	await physics_frame
	query.motion = Vector3(4, 0, 0)
	var blocked: PackedFloat32Array = state.cast_motion(query)
	_check(blocked.size() == 2 and absf(blocked[1] - 0.3125) < 0.003 and blocked[0] <= blocked[1], "ignoring an initial overlap still finds a later obstacle")
	query.margin = 0.1
	var inflated: PackedFloat32Array = state.cast_motion(query)
	_check(inflated.size() == 2 and absf(inflated[1] - 0.2875) < 0.003 and inflated[0] <= inflated[1], "the later obstacle still honors the query margin")
	query.exclude = [blocker.get_rid()]
	_check(state.cast_motion(query) == PackedFloat32Array([1.0, 1.0]), "excluding the later obstacle leaves full travel")
	overlapped.queue_free()
	blocker.queue_free()
	await process_frame
	if failures == 0:
		print("RESULT: PASS - shape casts return full travel, ignore initial overlaps and find later obstacles")
	quit(1 if failures else 0)


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		print("RESULT: FAIL - ", message)
