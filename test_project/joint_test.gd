extends SceneTree

var frames: int = 0
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
	root.add_child(door)

	hinge = HingeJoint3D.new()
	root.add_child(hinge)
	hinge.node_a = hinge.get_path_to(anchor)
	hinge.node_b = hinge.get_path_to(door)
	hinge.position = Vector3(0, 0, 0)

	await process_frame
	await process_frame
	# Apply an angular impulse to swing the door.
	door.apply_torque_impulse(Vector3(0, 0, 3.0))


func _process(delta: float) -> bool:
	frames += 1
	if frames == 60:
		print("Door position: ", door.global_position)
		print("Door rotation Z (deg): ", rad_to_deg(door.rotation.z))
		var dist_from_anchor: float = door.global_position.distance_to(Vector3(0, 0, 0))
		print("Distance from anchor: ", dist_from_anchor)
		var passed: bool = abs(door.rotation.z) > 0.05 and dist_from_anchor < 1.5
		if passed:
			print("RESULT: PASS - hinge joint constrained rotation around anchor")
		else:
			print("RESULT: FAIL - hinge joint did not behave as expected")
		quit(0 if passed else 1)
	return false
