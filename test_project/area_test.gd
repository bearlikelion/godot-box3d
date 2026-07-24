extends SceneTree

var frames: int = 0
var body: RigidBody3D
var area: Area3D
var entered: bool = false
var exited: bool = false

func _on_body_entered(b: Node3D) -> void:
	entered = true
	print("Area entered by: ", b.name)

func _on_body_exited(b: Node3D) -> void:
	exited = true
	print("Area exited by: ", b.name)

func _initialize() -> void:
	area = Area3D.new()
	area.name = "TestArea"
	var area_shape: CollisionShape3D = CollisionShape3D.new()
	var area_box: BoxShape3D = BoxShape3D.new()
	area_box.size = Vector3(4, 4, 4)
	area_shape.shape = area_box
	area.add_child(area_shape)
	area.position = Vector3(0, 0, 0)
	area.body_entered.connect(_on_body_entered)
	area.body_exited.connect(_on_body_exited)
	root.add_child(area)

	body = RigidBody3D.new()
	body.name = "FallingBody"
	body.gravity_scale = 0.0
	var body_shape: CollisionShape3D = CollisionShape3D.new()
	var sphere: SphereShape3D = SphereShape3D.new()
	sphere.radius = 0.3
	body_shape.shape = sphere
	body.add_child(body_shape)
	body.position = Vector3(0, 10, 0)
	body.linear_velocity = Vector3(0, -5, 0)
	root.add_child(body)


func _process(delta: float) -> bool:
	frames += 1
	if frames == 600:
		print("Entered: ", entered, " Exited: ", exited)
		var passed: bool = entered and exited
		if passed:
			print("RESULT: PASS - area entered and exited signals fired")
		else:
			print("RESULT: FAIL - area monitoring signals did not fire as expected")
		quit(0 if passed else 1)
	return false
