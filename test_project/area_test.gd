extends Node3D

var frames: int = 0
var body: RigidBody3D
var area: Area3D
var entered: bool = false
var exited: bool = false

func _add_mesh(n: Node3D, s: Shape3D) -> void:
	var mi := MeshInstance3D.new()
	if s is BoxShape3D:
		var m := BoxMesh.new(); m.size = s.size; mi.mesh = m
	elif s is SphereShape3D:
		var m := SphereMesh.new(); m.radius = s.radius; m.height = s.radius * 2.0; mi.mesh = m
	var mat := StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color = Color(0.2, 0.6, 1.0, 0.4)
	mi.material_override = mat
	n.add_child(mi)

func _on_body_entered(b: Node3D) -> void:
	entered = true
	print("Area entered by: ", b.name)

func _on_body_exited(b: Node3D) -> void:
	exited = true
	print("Area exited by: ", b.name)

func _ready() -> void:
	area = Area3D.new()
	area.name = "TestArea"
	var area_shape := CollisionShape3D.new()
	var area_box := BoxShape3D.new()
	area_box.size = Vector3(4, 4, 4)
	area_shape.shape = area_box
	area.add_child(area_shape)
	_add_mesh(area, area_box)
	area.position = Vector3(0, 0, 0)
	area.body_entered.connect(_on_body_entered)
	area.body_exited.connect(_on_body_exited)
	add_child(area)

	body = RigidBody3D.new()
	body.name = "FallingBody"
	body.gravity_scale = 0.0
	var body_shape := CollisionShape3D.new()
	var sphere := SphereShape3D.new()
	sphere.radius = 0.3
	body_shape.shape = sphere
	body.add_child(body_shape)
	_add_mesh(body, sphere)
	body.position = Vector3(0, 10, 0)
	body.linear_velocity = Vector3(0, -5, 0)
	add_child(body)

func _process(_delta: float) -> void:
	frames += 1
	if frames == 600:
		var msg := ""
		if entered and exited:
			msg = "RESULT: PASS - area entered and exited signals fired"
		else:
			msg = "RESULT: FAIL - area monitoring signals did not fire as expected"
		_end_test(msg)

func _end_test(msg: String) -> void:
	print(msg)
	var lbl := get_tree().root.find_child("ResultLabel", true, false)
	if lbl and lbl is Label:
		lbl.text = msg
	set_process(false)
	set_physics_process(false)
