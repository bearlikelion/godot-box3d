extends SceneTree


func _initialize() -> void:
	var requested_backend: String = str(ProjectSettings.get_setting("physics/3d/physics_engine", ""))
	var extension_loaded: bool = ClassDB.class_exists(&"Box3DPhysicsServer3D")
	var active_backend: String = PhysicsServer3D.get_class()
	var passed: bool = requested_backend == "Box3D Physics" and extension_loaded and active_backend == "Box3DPhysicsServer3D"

	print("Requested physics backend: ", requested_backend)
	print("Box3D extension loaded: ", extension_loaded)
	print("Active physics server: ", active_backend)
	if passed:
		print("RESULT: PASS - Box3D backend is active")
	else:
		push_error("RESULT: FAIL - Box3D backend is not active")
	quit(0 if passed else 1)
