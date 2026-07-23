extends Node3D

const EXPECTED_ENGINE := "Box3D Physics (Extension)"

var _body: RigidBody3D
var _area_entered := false
var _door: RigidBody3D
var _initial_door_rotation := 0.0
var _status_label: Label
var _result_lines: Array[String] = []


func _ready() -> void:
    _create_status_ui()
    _append_status("Godot Box3D portable smoke test")
    _append_status("Platform: %s" % OS.get_name())
    _append_status("Requested backend: %s" % ProjectSettings.get_setting("physics/3d/physics_engine", ""))
    _append_status("Extension class registered: %s" % ClassDB.class_exists(&"Box3DPhysicsServer3D"))

    _create_camera_and_light()
    _create_ground()
    _create_falling_body_and_area()
    _create_hinge_door()
    _run_checks()


func _create_status_ui() -> void:
    var layer := CanvasLayer.new()
    add_child(layer)

    var panel := ColorRect.new()
    panel.position = Vector2(16, 16)
    panel.size = Vector2(700, 250)
    panel.color = Color(0.02, 0.02, 0.02, 0.82)
    layer.add_child(panel)

    _status_label = Label.new()
    _status_label.position = Vector2(16, 12)
    _status_label.size = Vector2(668, 226)
    _status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    panel.add_child(_status_label)


func _append_status(line: String) -> void:
    _result_lines.append(line)
    _status_label.text = "\n".join(_result_lines)
    print(line)


func _create_camera_and_light() -> void:
    var camera := Camera3D.new()
    camera.position = Vector3(0, 5, 12)
    camera.rotation_degrees = Vector3(-18, 0, 0)
    add_child(camera)

    var light := DirectionalLight3D.new()
    light.rotation_degrees = Vector3(-50, -30, 0)
    light.shadow_enabled = true
    add_child(light)


func _create_ground() -> void:
    var ground := StaticBody3D.new()
    ground.position = Vector3(0, -0.5, 0)

    var collision := CollisionShape3D.new()
    var shape := BoxShape3D.new()
    shape.size = Vector3(12, 1, 12)
    collision.shape = shape
    ground.add_child(collision)

    var mesh := MeshInstance3D.new()
    var box_mesh := BoxMesh.new()
    box_mesh.size = shape.size
    mesh.mesh = box_mesh
    ground.add_child(mesh)
    add_child(ground)


func _create_falling_body_and_area() -> void:
    var area := Area3D.new()
    area.position = Vector3(0, 2.5, 0)
    area.body_entered.connect(func(_entered_body: Node3D) -> void: _area_entered = true)

    var area_collision := CollisionShape3D.new()
    var area_shape := BoxShape3D.new()
    area_shape.size = Vector3(3, 1, 3)
    area_collision.shape = area_shape
    area.add_child(area_collision)
    add_child(area)

    _body = RigidBody3D.new()
    _body.position = Vector3(0, 6, 0)

    var collision := CollisionShape3D.new()
    var shape := SphereShape3D.new()
    shape.radius = 0.5
    collision.shape = shape
    _body.add_child(collision)

    var mesh := MeshInstance3D.new()
    var sphere_mesh := SphereMesh.new()
    sphere_mesh.radius = 0.5
    sphere_mesh.height = 1.0
    mesh.mesh = sphere_mesh
    _body.add_child(mesh)
    add_child(_body)


func _create_hinge_door() -> void:
    var anchor := StaticBody3D.new()
    anchor.name = "HingeAnchor"
    anchor.position = Vector3(3, 1.5, 0)

    var anchor_collision := CollisionShape3D.new()
    var anchor_shape := BoxShape3D.new()
    anchor_shape.size = Vector3(0.2, 3, 0.2)
    anchor_collision.shape = anchor_shape
    anchor.add_child(anchor_collision)
    add_child(anchor)

    _door = RigidBody3D.new()
    _door.name = "HingeDoor"
    _door.position = Vector3(4, 1.5, 0)
    _door.gravity_scale = 0.0
    _door.can_sleep = false

    var door_collision := CollisionShape3D.new()
    var door_shape := BoxShape3D.new()
    door_shape.size = Vector3(2, 3, 0.15)
    door_collision.shape = door_shape
    _door.add_child(door_collision)

    var door_mesh := MeshInstance3D.new()
    var box_mesh := BoxMesh.new()
    box_mesh.size = door_shape.size
    door_mesh.mesh = box_mesh
    _door.add_child(door_mesh)
    add_child(_door)

    var hinge := HingeJoint3D.new()
    hinge.position = Vector3(3, 1.5, 0)
    hinge.node_a = NodePath("../HingeAnchor")
    hinge.node_b = NodePath("../HingeDoor")
    add_child(hinge)

    _initial_door_rotation = _door.rotation.z
    _door.apply_torque_impulse(Vector3(0, 0, 4.0))


func _run_checks() -> void:
    for _frame in 180:
        await get_tree().physics_frame

    var failures: Array[String] = []
    var requested_backend: String = ProjectSettings.get_setting("physics/3d/physics_engine", "")
    if requested_backend != EXPECTED_ENGINE:
        failures.append("Project physics backend is not Box3D")
    if not ClassDB.class_exists(&"Box3DPhysicsServer3D"):
        failures.append("Box3D extension class is not registered")
    if _body.global_position.y >= 5.0 or _body.global_position.y <= -3.0:
        failures.append("Falling body did not contact the ground correctly")
    if not _area_entered:
        failures.append("Area body_entered signal did not fire")
    if absf(_door.rotation.z - _initial_door_rotation) < 0.02:
        failures.append("Hinge-connected body did not rotate")

    _append_status("Body Y after 180 frames: %.3f" % _body.global_position.y)
    _append_status("Area event received: %s" % _area_entered)
    _append_status("Door rotation delta: %.3f" % absf(_door.rotation.z - _initial_door_rotation))

    if failures.is_empty():
        _append_status("RESULT: PASS")
    else:
        for failure in failures:
            _append_status("FAIL: %s" % failure)
        _append_status("RESULT: FAIL")
