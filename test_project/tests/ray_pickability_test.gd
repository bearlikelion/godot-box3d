extends SceneTree

var failures: int = 0
var pick_counts: Dictionary = {}
var pick_shape_indices: Dictionary = {}


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	root.size = Vector2i(800, 600)
	root.physics_object_picking = true
	# Headless windows receive no OS mouse-enter notification. Godot 4.3 predates
	# this method and already treats the root window as entered.
	if root.has_method("notify_mouse_entered"):
		root.call("notify_mouse_entered")
	var camera: Camera3D = Camera3D.new()
	camera.position = Vector3(0, 0, 6)
	camera.current = true
	root.add_child(camera)
	await _test_pickability(camera, false)
	await _test_pickability(camera, true)
	if failures == 0:
		print("RESULT: PASS - viewport picking respects body/area toggles without filtering scripted rays")
	quit(1 if failures > 0 else 0)


func _make_target(areas: bool, label: String, position: Vector3) -> CollisionObject3D:
	var target: CollisionObject3D = Area3D.new() if areas else StaticBody3D.new()
	target.name = label
	target.position = position
	var shape: BoxShape3D = BoxShape3D.new()
	shape.size = Vector3(2, 2, 2)
	var collision: CollisionShape3D = CollisionShape3D.new()
	collision.shape = shape
	target.add_child(collision)
	pick_counts[label] = 0
	pick_shape_indices[label] = -1
	target.input_event.connect(_on_input_event.bind(label))
	root.add_child(target)
	return target


func _on_input_event(_camera: Camera3D, event: InputEvent, _position: Vector3,
		_normal: Vector3, shape_index: int, label: String) -> void:
	if event is InputEventMouseButton and event.pressed:
		pick_counts[label] = int(pick_counts[label]) + 1
		pick_shape_indices[label] = shape_index


func _click(camera: Camera3D) -> void:
	for pressed in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.position = camera.unproject_position(Vector3.ZERO)
		event.global_position = event.position
		event.button_index = MOUSE_BUTTON_LEFT
		event.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed else 0
		event.pressed = pressed
		root.push_input(event, true)
	await _settle()


func _settle() -> void:
	await process_frame
	await physics_frame
	await physics_frame


func _test_pickability(camera: Camera3D, areas: bool) -> void:
	var kind: String = "area" if areas else "body"
	var front: CollisionObject3D = _make_target(areas, kind + "_front", Vector3.ZERO)
	var rear: CollisionObject3D = _make_target(areas, kind + "_rear", Vector3(0, 0, -3))
	await _settle()
	await _click(camera)
	_check(int(pick_counts[front.name]) == 1 and int(pick_counts[rear.name]) == 0,
			"the nearest pickable %s receives the viewport input event" % kind)
	_check(int(pick_shape_indices[front.name]) == 0, "%s picking reports the local shape index" % kind)

	front.input_ray_pickable = false
	await _settle()
	var front_before: int = int(pick_counts[front.name])
	var rear_before: int = int(pick_counts[rear.name])
	await _click(camera)
	_check(int(pick_counts[front.name]) == front_before,
			"disabling %s picking suppresses its input event" % kind)
	_check(int(pick_counts[rear.name]) == rear_before + 1 and int(pick_shape_indices[rear.name]) == 0,
			"an unpickable %s does not block the pickable object behind it" % kind)
	var query: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(Vector3(0, 0, 6), Vector3(0, 0, -6))
	query.collide_with_bodies = not areas
	query.collide_with_areas = areas
	var hit: Dictionary = root.world_3d.direct_space_state.intersect_ray(query)
	_check(hit.get("rid", RID()) == front.get_rid() and hit.get("collider") == front,
			"ordinary scripted rays still hit the unpickable foreground %s" % kind)

	front.input_ray_pickable = true
	await _settle()
	front_before = int(pick_counts[front.name])
	rear_before = int(pick_counts[rear.name])
	await _click(camera)
	_check(int(pick_counts[front.name]) == front_before + 1 and int(pick_counts[rear.name]) == rear_before,
			"reenabling %s picking restores nearest-hit selection" % kind)

	front.input_ray_pickable = false
	rear.input_ray_pickable = false
	await _settle()
	front_before = int(pick_counts[front.name])
	rear_before = int(pick_counts[rear.name])
	await _click(camera)
	_check(int(pick_counts[front.name]) == front_before and int(pick_counts[rear.name]) == rear_before,
			"no input event is delivered when both %s targets are unpickable" % kind)
	front.queue_free()
	rear.queue_free()
	await _settle()


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		print("RESULT: FAIL - ", message)
