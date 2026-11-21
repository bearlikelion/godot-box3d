extends SceneTree

const SLEEPING := PhysicsServer3D.BODY_STATE_SLEEPING
const TRANSFORM := PhysicsServer3D.BODY_STATE_TRANSFORM

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	var floor_body := StaticBody3D.new()
	floor_body.position.y = -0.5
	_add_box(floor_body, Vector3(12, 1, 12))
	var lifted := RigidBody3D.new()
	lifted.position = Vector3(-2, 2, 0)
	lifted.lock_rotation = true
	_add_box(lifted, Vector3.ONE)
	var turned := RigidBody3D.new()
	turned.position = Vector3(2, 2, 0)
	turned.rotation.y = 0.37
	_add_box(turned, Vector3.ONE)

	for frame in 180:
		await physics_frame
		if _is_sleeping(lifted) and _is_sleeping(turned):
			break
	_check(_is_sleeping(lifted) and _is_sleeping(turned), "both bodies settled and went to sleep")
	if failures > 0:
		quit(1)
		return

	for body: RigidBody3D in [lifted, turned]:
		var rid := body.get_rid()
		PhysicsServer3D.body_set_state(rid, TRANSFORM, PhysicsServer3D.body_get_state(rid, TRANSFORM))
		_check(_is_sleeping(body), "rewriting the current physics pose did not wake the body")

	# Exercise the scene transform path used by scripts and inspector edits.
	var raised_y := lifted.global_position.y + 5.0
	lifted.global_position.y = raised_y
	lifted.force_update_transform()
	_check(not _is_sleeping(lifted), "moving a rotation-locked rigid body woke it")

	var turned_rid := turned.get_rid()
	var pose: Transform3D = PhysicsServer3D.body_get_state(turned_rid, TRANSFORM)
	pose.basis = pose.basis.rotated(Vector3.UP, 0.45)
	PhysicsServer3D.body_set_state(turned_rid, TRANSFORM, pose)
	_check(not _is_sleeping(turned), "a rotation-only physics transform change woke the body")

	for frame in 20:
		await physics_frame
	_check(lifted.global_position.y < raised_y - 0.2 and lifted.linear_velocity.y < -0.5,
			"the lifted body fell under gravity without an explicit wake")
	if failures == 0:
		print("RESULT: PASS - changed rigid-body poses wake sleepers; identical writes leave them asleep")
	quit(1 if failures > 0 else 0)


func _add_box(body: PhysicsBody3D, size: Vector3) -> void:
	var shape := BoxShape3D.new()
	shape.size = size
	var collision := CollisionShape3D.new()
	collision.shape = shape
	body.add_child(collision)
	root.add_child(body)


func _is_sleeping(body: RigidBody3D) -> bool:
	return PhysicsServer3D.body_get_state(body.get_rid(), SLEEPING)


func _check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		print("RESULT: FAIL - ", description)
