extends SceneTree

var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var space: RID = root.world_3d.space
	_check(space.is_valid(), "world has a valid physics space")
	if not space.is_valid():
		quit(1)
		return

	var expected_gravity: float = 3.25
	PhysicsServer3D.area_set_param(space, PhysicsServer3D.AREA_PARAM_GRAVITY, expected_gravity)
	var gravity: float = float(PhysicsServer3D.area_get_param(space, PhysicsServer3D.AREA_PARAM_GRAVITY))
	_check(is_equal_approx(gravity, expected_gravity), "space RID reads and writes default-area parameters")

	var expected_instance_id: int = get_instance_id()
	PhysicsServer3D.area_attach_object_instance_id(space, expected_instance_id)
	var instance_id: int = PhysicsServer3D.area_get_object_instance_id(space)
	_check(instance_id == expected_instance_id, "space RID reads and writes the default-area instance ID")

	if failures == 0:
		print("RESULT: PASS - space RID resolves to the default area")
	else:
		print("RESULT: FAIL - ", failures, " default-area assertion(s) failed")
	quit(1 if failures > 0 else 0)


func _check(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		push_error("FAIL: " + message)
