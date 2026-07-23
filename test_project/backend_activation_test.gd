extends SceneTree


func _initialize() -> void:
	var requested_backend: String = ProjectSettings.get_setting("physics/3d/physics_engine", "")
	var extension_loaded: bool = ClassDB.class_exists(&"Box3DPhysicsServer3D")
	print("Requested physics engine: ", requested_backend)
	print("Box3D extension class registered: ", extension_loaded)
	if requested_backend != "Box3D Physics (Extension)" or not extension_loaded:
		print("RESULT: FAIL - Box3D extension is not active")
		quit(1)
		return
	print("RESULT: PASS - Box3D extension is active")
	quit(0)
