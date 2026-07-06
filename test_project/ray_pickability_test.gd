extends SceneTree

const VIEWPORT_SIZE := Vector2i(800, 600)

var failures: int = 0
var pick_counts: Dictionary = {}
var pick_shape_indices: Dictionary = {}


func _initialize() -> void:
	print("Active physics engine setting: ", ProjectSettings.get_setting("physics/3d/physics_engine"))
	call_deferred("_run")


func _run() -> void:
	root.size = VIEWPORT_SIZE
	root.physics_object_picking = true
	root.physics_object_picking_sort = true
	root.physics_object_picking_first_only = true

	var camera: Camera3D = Camera3D.new()
	camera.name = "PickCamera"
	camera.look_at_from_position(Vector3(0, 0, 6), Vector3.ZERO, Vector3.UP)
	camera.current = true
	root.add_child(camera)

	await _test_pickable_body(camera)
	await _test_unpickable_body(camera)
	await _test_pickable_area(camera)

	if failures == 0:
		print("RESULT: PASS - ray pickability checks passed")
	else:
		print("RESULT: FAIL - ", failures, " ray pickability check(s) failed")
	quit(1 if failures > 0 else 0)


func _assert_result(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		print("FAIL: ", message)


func _make_box_shape(size: Vector3) -> CollisionShape3D:
	var collision: CollisionShape3D = CollisionShape3D.new()
	var shape: BoxShape3D = BoxShape3D.new()
	shape.size = size
	collision.shape = shape
	return collision


func _track_pick_events(target: CollisionObject3D) -> void:
	pick_counts[target.name] = 0
	pick_shape_indices[target.name] = -1
	target.input_event.connect(_on_input_event.bind(target.name))


func _on_input_event(camera: Camera3D, event: InputEvent, event_position: Vector3, normal: Vector3, shape_idx: int, target_name: StringName) -> void:
	if event is InputEventMouseButton and event.pressed:
		pick_counts[target_name] = int(pick_counts.get(target_name, 0)) + 1
		pick_shape_indices[target_name] = shape_idx


func _make_static_target(name: String, ray_pickable: bool) -> StaticBody3D:
	var target: StaticBody3D = StaticBody3D.new()
	target.name = name
	target.set_ray_pickable(ray_pickable)
	target.add_child(_make_box_shape(Vector3(2, 2, 2)))
	root.add_child(target)
	_track_pick_events(target)
	return target


func _make_area_target(name: String, ray_pickable: bool) -> Area3D:
	var target: Area3D = Area3D.new()
	target.name = name
	target.set_ray_pickable(ray_pickable)
	target.add_child(_make_box_shape(Vector3(2, 2, 2)))
	root.add_child(target)
	_track_pick_events(target)
	return target


func _click(camera: Camera3D, world_position: Vector3) -> void:
	var screen_position: Vector2 = camera.unproject_position(world_position)

	var press: InputEventMouseButton = InputEventMouseButton.new()
	press.position = screen_position
	press.global_position = screen_position
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	root.push_input(press)

	var release: InputEventMouseButton = InputEventMouseButton.new()
	release.position = screen_position
	release.global_position = screen_position
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	root.push_input(release)


func _settle() -> void:
	await process_frame
	await physics_frame
	await physics_frame


func _test_pickable_body(camera: Camera3D) -> void:
	var target: StaticBody3D = _make_static_target("PickableBody", true)
	await _settle()
	_click(camera, target.global_position)
	await _settle()

	_assert_result(int(pick_counts[target.name]) == 1, "viewport object picking sends input_event to a ray-pickable StaticBody3D")
	_assert_result(int(pick_shape_indices[target.name]) == 0, "viewport object picking reports the StaticBody3D shape index")

	target.queue_free()
	await _settle()


func _test_unpickable_body(camera: Camera3D) -> void:
	var target: StaticBody3D = _make_static_target("UnpickableBody", false)
	await _settle()
	_click(camera, target.global_position)
	await _settle()

	_assert_result(int(pick_counts[target.name]) == 0, "viewport object picking ignores a StaticBody3D with ray picking disabled")

	target.queue_free()
	await _settle()


func _test_pickable_area(camera: Camera3D) -> void:
	var target: Area3D = _make_area_target("PickableArea", true)
	await _settle()
	_click(camera, target.global_position)
	await _settle()

	_assert_result(int(pick_counts[target.name]) == 1, "viewport object picking sends input_event to a ray-pickable Area3D")
	_assert_result(int(pick_shape_indices[target.name]) == 0, "viewport object picking reports the Area3D shape index")

	target.queue_free()
	await _settle()
