extends SceneTree
## Headless bridge test for the overlay's new components. Run from test_project/ with:
##   godot --headless --script bridge_test.gd
## Exercises, from the Godot side: per-pair collision exceptions on kinematic bodies
## (move_and_slide passes through excepted squad-mates), cylinder shape approximation,
## cone-twist joints, and the Box3DRecording singleton's record/save/validate loop.

var frames: int = 0
var failures: int = 0

var mover: CharacterBody3D
var blocker: CharacterBody3D
var cylinder_body: RigidBody3D
var swing_body: RigidBody3D
var recording_started: bool = false


func _check(cond: bool, msg: String) -> void:
	if cond:
		print("ok:   ", msg)
	else:
		print("FAIL: ", msg)
		failures += 1


func _initialize() -> void:
	# Ground plate.
	var ground := StaticBody3D.new()
	var ground_shape := CollisionShape3D.new()
	var plate := BoxShape3D.new()
	plate.size = Vector3(40, 1, 40)
	ground_shape.shape = plate
	ground.add_child(ground_shape)
	ground.position = Vector3(0, -0.5, 0)
	root.add_child(ground)

	# --- Pair exceptions on kinematic characters (the minion-squad case) ---
	mover = _make_character(Vector3(0, 0.51, 0))
	blocker = _make_character(Vector3(2.0, 0.51, 0))
	root.add_child(mover)
	root.add_child(blocker)
	mover.add_collision_exception_with(blocker)
	blocker.add_collision_exception_with(mover)

	# --- Cylinder shape approximation ---
	cylinder_body = RigidBody3D.new()
	var cyl_shape := CollisionShape3D.new()
	var cyl := CylinderShape3D.new()
	cyl.radius = 0.5
	cyl.height = 1.0
	cyl_shape.shape = cyl
	cylinder_body.add_child(cyl_shape)
	cylinder_body.position = Vector3(8, 3, 0)
	root.add_child(cylinder_body)

	# --- Cone-twist joint: swinging limb under a static anchor ---
	var anchor := StaticBody3D.new()
	var anchor_shape := CollisionShape3D.new()
	var anchor_sphere := SphereShape3D.new()
	anchor_sphere.radius = 0.1
	anchor_shape.shape = anchor_sphere
	anchor.add_child(anchor_shape)
	anchor.position = Vector3(-8, 6, 0)
	root.add_child(anchor)

	swing_body = RigidBody3D.new()
	var swing_shape := CollisionShape3D.new()
	var swing_sphere := SphereShape3D.new()
	swing_sphere.radius = 0.25
	swing_shape.shape = swing_sphere
	swing_body.add_child(swing_shape)
	swing_body.position = Vector3(-8, 4.5, 0)
	root.add_child(swing_body)

	var cone := ConeTwistJoint3D.new()
	cone.name = "ConeTwist"
	cone.position = Vector3(-8, 6, 0)
	cone.swing_span = deg_to_rad(30.0)
	cone.twist_span = deg_to_rad(90.0)
	# Wire node paths BEFORE entering the tree (relative paths to future siblings):
	# the joint then configures exactly once in _ready with both bodies resolved.
	anchor.name = "ConeAnchor"
	swing_body.name = "ConeLimb"
	cone.node_a = NodePath("../ConeAnchor")
	cone.node_b = NodePath("../ConeLimb")
	root.add_child(cone)

	# --- Recording singleton ---
	_check(Engine.has_singleton("Box3DRecording"), "Box3DRecording engine singleton is registered")
	if Engine.has_singleton("Box3DRecording"):
		var rec := Engine.get_singleton("Box3DRecording")
		recording_started = rec.call("start", root.get_world_3d().space, 0)
		_check(recording_started, "recording starts on the live space")
		_check(rec.call("is_recording"), "is_recording reports active")


func _make_character(pos: Vector3) -> CharacterBody3D:
	var character := CharacterBody3D.new()
	var shape := CollisionShape3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.4
	capsule.height = 1.0
	shape.shape = capsule
	character.add_child(shape)
	character.position = pos
	return character


func _physics_process(_delta: float) -> bool:
	frames += 1

	# Drive the mover straight through the excepted blocker for the first second.
	if frames <= 60:
		mover.velocity = Vector3(4.0, 0, 0)
		mover.move_and_slide()

	if frames == 90:
		# 4 m/s for 1 s from x=0 through a blocker at x=2: without the exception the
		# mover stops at ~1.2; with it honored the mover ends near x=4.
		_check(mover.position.x > 3.0, "move_and_slide passes through an excepted kinematic body (x=%.2f)" % mover.position.x)
		var exceptions: Array = mover.get_collision_exceptions()
		_check(exceptions.size() == 1 and exceptions[0] == blocker, "get_collision_exceptions round-trips the excepted body")

		_check(cylinder_body.position.y > 0.4 and cylinder_body.position.y < 0.7,
				"cylinder body rests on the ground via its convex approximation (y=%.2f)" % cylinder_body.position.y)

		var offset: Vector3 = swing_body.position - Vector3(-8, 6, 0)
		_check(offset.length() > 1.2 and offset.length() < 1.8,
				"cone-twist limb stays on its 1.5m tether (r=%.2f)" % offset.length())
		var vertical: Vector3 = Vector3(-8, 4.5, 0) - Vector3(-8, 6, 0)
		var swing_angle: float = rad_to_deg(offset.angle_to(vertical))
		_check(swing_angle < 35.0, "cone limit holds the swing near its 30-degree span (%.1f deg)" % swing_angle)

	if frames == 120 and recording_started:
		var rec := Engine.get_singleton("Box3DRecording")
		rec.call("stop")
		_check(not rec.call("is_recording"), "stop() ends the session")
		var path := "user://bridge_test.b3rec"
		_check(rec.call("save", path), "recording saves to user://")
		_check(rec.call("validate_file", path, 1), "validate_file reproduces the recorded session")
		_check(not rec.call("replay_file_diverged", path, 1), "replay_file_diverged reports no divergence")

	if frames >= 130:
		print("")
		if failures == 0:
			print("BRIDGE TEST PASSED")
		else:
			print("BRIDGE TEST FAILED (%d failure%s)" % [failures, "" if failures == 1 else "s"])
		quit(0 if failures == 0 else 1)
		return true

	return false
