extends SceneTree

const SAFE_MARGIN: float = 0.001
const HULL_SIZE: Vector3 = Vector3(0.8, 1.8, 0.8)

var body: CharacterBody3D
var frames: int = 0


func _initialize() -> void:
	_add_floor(Vector3(-1.0, -0.5, 0.0))
	_add_floor(Vector3(1.0, -0.5, 0.0))

	body = CharacterBody3D.new()
	var hull: CollisionShape3D = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = HULL_SIZE
	hull.shape = box
	hull.position.y = HULL_SIZE.y * 0.5
	body.add_child(hull)
	root.add_child(body)


func _physics_process(_delta: float) -> bool:
	frames += 1
	if frames < 3:
		return false

	# Both gaps sit inside Box3D's linear slop, where its own casts misreport.
	body.global_position = Vector3(-0.5, 0.003, 0.0)
	var probe: KinematicCollision3D = body.move_and_collide(
		Vector3.DOWN * 0.05, true, SAFE_MARGIN, false
	)
	var probe_ok: bool = probe != null and probe.get_normal().y > 0.99

	body.global_position = Vector3(-0.5, 0.002, 0.0)
	var slide: KinematicCollision3D = body.move_and_collide(
		Vector3(1.0, 0.0, 0.0), false, SAFE_MARGIN, false
	)
	var slide_ok: bool = slide == null and is_equal_approx(body.global_position.x, 0.5)

	print("probe_hit=", probe != null, " slide_hit=", slide != null, " x=", body.global_position.x)
	if probe_ok and slide_ok:
		print("RESULT: PASS - motion within linear slop finds the floor and crosses a flush seam")
		quit(0)
	else:
		print("RESULT: FAIL - near-contact motion missed the floor or snagged on the seam")
		quit(1)
	return true


func _add_floor(position: Vector3) -> void:
	var floor_body: StaticBody3D = StaticBody3D.new()
	var shape: CollisionShape3D = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = Vector3(2.0, 1.0, 2.0)
	shape.shape = box
	floor_body.add_child(shape)
	floor_body.position = position
	root.add_child(floor_body)
