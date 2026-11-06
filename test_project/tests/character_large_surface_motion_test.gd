extends SceneTree

const MARGIN: float = 0.001
const HALF_HEIGHT: float = 0.9

var failures: int = 0
var checks: int = 0


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	if PhysicsServer3D.get_class() != &"Box3DPhysicsServer3D":
		_fail("Box3D backend did not activate")
		quit(1)
		return
	# Rotate the whole pair so the same contact works on floors, slopes, walls and ceilings.
	for basis in [Basis.IDENTITY, Basis(Vector3.FORWARD, PI / 6.0), Basis(Vector3.FORWARD, PI / 2.0), Basis(Vector3.RIGHT, PI)]:
		for width in [100.0, 1000.0, 10000.0, -1.0]:
			var floor_body: StaticBody3D = StaticBody3D.new()
			var floor_collision: CollisionShape3D = CollisionShape3D.new()
			if width < 0.0:
				floor_collision.shape = WorldBoundaryShape3D.new()
				floor_body.transform = Transform3D(basis, Vector3.ZERO)
			else:
				var floor_shape: BoxShape3D = BoxShape3D.new()
				floor_shape.size = Vector3(width, 10, width)
				floor_collision.shape = floor_shape
				floor_body.transform = Transform3D(basis, basis * Vector3(0, -5, 0))
			floor_body.add_child(floor_collision)
			root.add_child(floor_body)
			for kind in ["cylinder", "box"]:
				var body: CharacterBody3D = CharacterBody3D.new()
				body.collision_layer = 2
				body.collision_mask = 1
				body.position.y = 20.0
				var collision: CollisionShape3D = CollisionShape3D.new()
				if kind == "cylinder":
					var cylinder: CylinderShape3D = CylinderShape3D.new()
					cylinder.radius = 0.35
					cylinder.height = HALF_HEIGHT * 2.0
					collision.shape = cylinder
				else:
					var box: BoxShape3D = BoxShape3D.new()
					box.size = Vector3(0.7, HALF_HEIGHT * 2.0, 0.7)
					collision.shape = box
				body.add_child(collision)
				root.add_child(body)
				await physics_frame
				await physics_frame
				for position in [Vector2.ZERO, Vector2(0.1, 0), Vector2(-1.13, 0.1), Vector2(3.17, -3.17)]:
					for gap in [0.001, 0.1, 1.0]:
						var label: String = "%s width=%s gap=%s position=%s normal=%s" % [kind, width, gap, position, basis.y]
						_check_motion(body, floor_body, basis, position, gap,
							Vector3.DOWN * maxf(0.02, 2.0 * gap), true, label)
					# Tangent and separating sweeps inside native slop must stay clear outside the margin.
					for motion in [Vector3.RIGHT * 0.2, Vector3.UP * 0.2]:
						_check_motion(body, floor_body, basis, position, 0.003, motion, false,
							"%s width=%s clear motion=%s normal=%s" % [kind, width, motion, basis.y])
					_check_motion(body, floor_body, basis, position, 0.1, Vector3.DOWN * 0.098, false,
						"%s width=%s sweep stops short normal=%s" % [kind, width, basis.y])
				body.queue_free()
				await process_frame
			floor_body.queue_free()
			await process_frame
	if failures == 0:
		print("RESULT: PASS - ", checks, " large-surface motion checks on floors, slopes, walls and ceilings")
	quit(1 if failures else 0)


func _check_motion(body: CharacterBody3D, floor_body: StaticBody3D, basis: Basis,
		position: Vector2, gap: float, motion: Vector3, expected_hit: bool, label: String) -> void:
	var parameters: PhysicsTestMotionParameters3D = PhysicsTestMotionParameters3D.new()
	parameters.from = Transform3D(basis, basis * Vector3(position.x, HALF_HEIGHT + gap, position.y))
	parameters.motion = basis * motion
	parameters.margin = MARGIN
	parameters.recovery_as_collision = true
	var result: PhysicsTestMotionResult3D = PhysicsTestMotionResult3D.new()
	var hit: bool = PhysicsServer3D.body_test_motion(body.get_rid(), parameters, result)
	checks += 1
	if hit != expected_hit:
		_fail(label + " hit=%s, expected %s" % [hit, expected_hit])
		return
	if not result.get_travel().is_finite():
		_fail(label + " returned non-finite travel")
	elif expected_hit:
		var normal: Vector3 = result.get_collision_normal()
		var travel: float = result.get_travel().dot(basis.y)
		if result.get_collider_rid() != floor_body.get_rid() or not normal.is_normalized() or normal.dot(basis.y) < 0.99:
			_fail(label + " returned an invalid collider or contact normal")
		# Motion queries deliberately back off by native linear slop plus margin.
		if travel + gap < -0.0005 or travel + gap > 0.007:
			_fail(label + " stopped at the wrong distance: " + str(travel))
	elif result.get_travel().distance_to(parameters.motion) > MARGIN:
		_fail(label + " blocked a clear sweep: " + str(result.get_travel()))


func _fail(message: String) -> void:
	failures += 1
	print("RESULT: FAIL - ", message)
