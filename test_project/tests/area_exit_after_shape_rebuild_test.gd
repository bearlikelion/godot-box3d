extends SceneTree

const SETTLE_FRAMES: int = 5

var area: Area3D
var body: CharacterBody3D
var hull: CollisionShape3D
var frames: int = 0
var entered: bool = false
var exited: bool = false


func _initialize() -> void:
	area = Area3D.new()
	var area_shape: CollisionShape3D = CollisionShape3D.new()
	var area_box: BoxShape3D = BoxShape3D.new()
	area_box.size = Vector3(4.0, 4.0, 4.0)
	area_shape.shape = area_box
	area.add_child(area_shape)
	area.body_entered.connect(func(_b: Node3D) -> void: entered = true)
	area.body_exited.connect(func(_b: Node3D) -> void: exited = true)
	root.add_child(area)

	body = CharacterBody3D.new()
	hull = CollisionShape3D.new()
	var box: BoxShape3D = BoxShape3D.new()
	box.size = Vector3(0.8, 1.8, 0.8)
	hull.shape = box
	hull.position.y = 0.9
	body.add_child(hull)
	root.add_child(body)


func _physics_process(_delta: float) -> bool:
	frames += 1
	# Moving the hull rebuilds its Box3D shape, like a crouch, while inside the area.
	if frames == SETTLE_FRAMES:
		(hull.shape as BoxShape3D).size.y = 1.4
		hull.position.y = 0.7
	elif frames == SETTLE_FRAMES * 2:
		body.global_position = Vector3(0.0, 20.0, 0.0)
	elif frames == SETTLE_FRAMES * 3:
		var inside: bool = area.get_overlapping_bodies().has(body)
		print("entered=", entered, " exited=", exited, " still_inside=", inside)
		if entered and exited and not inside:
			print("RESULT: PASS - body exit reported after its shape was rebuilt")
			quit(0)
		else:
			print("RESULT: FAIL - area kept a body whose shape was rebuilt")
			quit(1)
		return true
	return false
