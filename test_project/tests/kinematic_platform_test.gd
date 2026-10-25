extends SceneTree

class MovingPlatform extends AnimatableBody3D:
	var moving := false
	var speed := 0.6
	var angular_speed := 0.0
	func _physics_process(delta: float) -> void:
		if moving:
			var target := transform
			target.origin.x += speed * delta
			target.basis = Basis(Vector3.UP, angular_speed * delta) * target.basis
			transform = target

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var platform := MovingPlatform.new()
	var collision := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(8, 1, 8)
	collision.shape = box
	platform.position.y = -0.5
	platform.add_child(collision)
	root.add_child(platform)
	var character := CharacterBody3D.new()
	var solid := CollisionShape3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.35
	capsule.height = 1.8
	solid.shape = capsule
	character.add_child(solid)
	character.position.y = 0.9
	root.add_child(character)
	await physics_frame
	await physics_frame
	platform.moving = true
	for frame in 120:
		character.velocity = Vector3(0, -1, 0)
		character.move_and_slide()
		await physics_frame
	if absf(platform.position.x - 1.2) > 0.04:
		_fail("animated target motion was lost or applied twice: %s" % platform.position)
	if absf(character.position.x - platform.position.x) > 0.04 or absf(character.position.y - 0.9) > 0.03 or not character.is_on_floor():
		_fail("grounded character did not ride its platform: %s / %s" % [character.position, platform.position])
	if absf(character.get_platform_velocity().x - 0.6) > 0.03:
		_fail("motion collisions did not report the animated platform velocity")
	platform.moving = false
	await physics_frame
	await physics_frame
	var stopped := platform.position
	for frame in 30:
		character.velocity = Vector3(0, -1, 0)
		character.move_and_slide()
		await physics_frame
	if platform.position.distance_to(stopped) > 0.001 or character.get_platform_velocity().length() > 0.03:
		_fail("a stopped animation retained velocity or drifted")
	platform.speed = 0.02
	platform.angular_speed = 0.2
	platform.moving = true
	for frame in 60:
		character.velocity = Vector3(0, -1, 0)
		character.move_and_slide()
		await physics_frame
	if absf(platform.position.x - stopped.x - 0.02) > 0.005 or absf(platform.rotation.y - 0.2) > 0.01:
		_fail("small linear or angular target motion was lost")
	if absf(character.get_platform_angular_velocity().y - 0.2) > 0.03:
		_fail("motion collisions did not report angular platform velocity")
	if failures == 0:
		print("RESULT: PASS - animated kinematic targets move once, carry grounded characters and stop without drift")
	quit(1 if failures else 0)


func _fail(message: String) -> void:
	failures += 1
	print("RESULT: FAIL - ", message)
