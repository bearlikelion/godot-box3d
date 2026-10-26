extends SceneTree

class MovingPlatform extends AnimatableBody3D:
	var speed: Vector3 = Vector3.ZERO
	var turn_speed: float = 0.0

	func _physics_process(delta: float) -> void:
		var target: Transform3D = transform
		target.origin += speed * delta
		target.basis = Basis(Vector3.UP, turn_speed * delta) * target.basis
		transform = target


var failures: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	await _platform_jump(false, false)
	await _platform_jump(true, false)
	await _platform_jump(false, true)
	if failures == 0:
		print("RESULT: PASS - jumps leave floor contacts and inherit moving/rotating platform velocity")
	quit(1 if failures > 0 else 0)


func _platform_jump(translating: bool, rotating: bool) -> void:
	var label: String = "translating=%s rotating=%s" % [translating, rotating]
	var platform: MovingPlatform = MovingPlatform.new()
	var floor_shape: BoxShape3D = BoxShape3D.new()
	floor_shape.size = Vector3(8, 1, 8)
	var floor_collision: CollisionShape3D = CollisionShape3D.new()
	floor_collision.shape = floor_shape
	platform.position.y = -0.5
	platform.add_child(floor_collision)
	root.add_child(platform)
	var offset: Vector3 = Vector3(2, 1, 0) if rotating else Vector3(0, 1, 0)
	var character: CharacterBody3D = CharacterBody3D.new()
	var capsule: CapsuleShape3D = CapsuleShape3D.new()
	capsule.radius = 0.2
	capsule.height = 1.0
	var character_collision: CollisionShape3D = CollisionShape3D.new()
	character_collision.shape = capsule
	character.add_child(character_collision)
	character.floor_snap_length = 0.15
	character.position = platform.position + offset
	root.add_child(character)
	await physics_frame
	await physics_frame
	platform.speed.x = 0.6 if translating else 0.0
	platform.turn_speed = 0.3 if rotating else 0.0
	for frame in 90:
		character.velocity = Vector3.DOWN
		character.move_and_slide()
		await physics_frame
	var expected: Vector3 = platform.transform * offset
	_check(character.is_on_floor() and character.position.distance_to(expected) < 0.08,
			"platform supports its rider before jumping (%s)" % label)
	var inherited: Vector3 = character.get_platform_velocity()
	character.velocity = Vector3(0, 3, 0)
	character.move_and_slide()
	_check(not character.is_on_floor(), "jump detaches on its first movement (%s)" % label)
	var horizontal: Vector3 = Vector3(character.velocity.x, 0, character.velocity.z)
	var expected_horizontal: Vector3 = Vector3(inherited.x, 0, inherited.z)
	_check(horizontal.distance_to(expected_horizontal) < 0.05,
			"jump inherits platform velocity (%s): actual=%s expected=%s" % [label, horizontal, expected_horizontal])
	if translating or rotating:
		_check(expected_horizontal.length() > 0.5, "moving platform has observable velocity (%s)" % label)
	for frame in 15:
		await physics_frame
		character.velocity.y -= 9.8 / 60.0
		character.move_and_slide()
	_check(character.position.y > 0.8 and not character.is_on_floor(),
			"detached character continues its airborne jump (%s)" % label)
	character.queue_free()
	platform.queue_free()
	await process_frame
	await physics_frame


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		print("RESULT: FAIL - ", message)
