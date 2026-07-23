extends SceneTree

var failures: int = 0
var anchor: StaticBody3D
var door: RigidBody3D
var hinge: HingeJoint3D

func _initialize() -> void:
	anchor = StaticBody3D.new()
	var anchor_shape: CollisionShape3D = CollisionShape3D.new()
	var anchor_box: BoxShape3D = BoxShape3D.new()
	anchor_box.size = Vector3(0.2, 2, 0.2)
	anchor_shape.shape = anchor_box
	anchor.add_child(anchor_shape)
	anchor.position = Vector3(0, 0, 0)
	root.add_child(anchor)

	door = RigidBody3D.new()
	var door_shape: CollisionShape3D = CollisionShape3D.new()
	var door_box: BoxShape3D = BoxShape3D.new()
	door_box.size = Vector3(2, 2, 0.1)
	door_shape.shape = door_box
	door.add_child(door_shape)
	door.position = Vector3(1, 0, 0)
	door.gravity_scale = 0.0
	door.can_sleep = false
	root.add_child(door)

	hinge = HingeJoint3D.new()
	root.add_child(hinge)
	hinge.node_a = hinge.get_path_to(anchor)
	hinge.node_b = hinge.get_path_to(door)
	hinge.position = Vector3(0, 0, 0)

	call_deferred("_run")


func _run() -> void:
	for i in 2:
		await physics_frame

	door.apply_torque_impulse(Vector3(0, 0, 3.0))

	var max_rotation: float = 0.0
	for i in 90:
		await physics_frame
		max_rotation = maxf(max_rotation, absf(door.rotation.z))

	print("Door position: ", door.global_position)
	print("Door rotation Z (deg): ", rad_to_deg(door.rotation.z))
	print("Maximum rotation Z (deg): ", rad_to_deg(max_rotation))
	var dist_from_anchor: float = door.global_position.distance_to(Vector3(0, 0, 0))
	print("Distance from anchor: ", dist_from_anchor)
	if max_rotation > 0.05 and dist_from_anchor < 1.5:
		print("RESULT: PASS - hinge joint constrained rotation around anchor")
	else:
		failures += 1
		print("RESULT: FAIL - hinge joint did not behave as expected")
	quit(1 if failures > 0 else 0)
