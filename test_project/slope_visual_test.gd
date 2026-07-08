extends Node3D
## Visual test: a character capsule jumps continuously on a 25-degree slope.
## Watch for the capsule resting ON the surface between jumps. The label
## reports the maximum penetration observed; more than 2 cm is a failure
## (regression guard for the mover-plane depth fix -- characters used to sink
## into sloped geometry, lock in place, or fall through).

const SLOPE_DEG := 25.0
const RUN_FRAMES := 600

var frames := 0
var max_pen := -99.0
var character: CharacterBody3D

func _add_visual(parent: Node3D, mesh: Mesh, color: Color) -> void:
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mi.material_override = mat
	parent.add_child(mi)

func _ready() -> void:
	var t := tan(deg_to_rad(SLOPE_DEG))

	# Sloped trimesh floor (front faces up, Godot winding).
	var floor_body := StaticBody3D.new()
	var cs := CollisionShape3D.new()
	var tri := ConcavePolygonShape3D.new()
	var faces := PackedVector3Array()
	var n := 8
	for i in n:
		for j in n:
			var x1 := -12.0 + 24.0 * i / n
			var x2 := -12.0 + 24.0 * (i + 1) / n
			var z1 := -12.0 + 24.0 * j / n
			var z2 := -12.0 + 24.0 * (j + 1) / n
			faces.append_array([
				Vector3(x1, x1 * t, z1), Vector3(x2, x2 * t, z2), Vector3(x2, x2 * t, z1),
				Vector3(x1, x1 * t, z1), Vector3(x1, x1 * t, z2), Vector3(x2, x2 * t, z2)])
	tri.set_faces(faces)
	cs.shape = tri
	floor_body.add_child(cs)
	add_child(floor_body)
	var floor_mesh := ArrayMesh.new()
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for v in faces:
		st.add_vertex(v)
	st.generate_normals()
	st.commit(floor_mesh)
	_add_visual(floor_body, floor_mesh, Color(0.35, 0.45, 0.35))

	# Jumping capsule character.
	character = CharacterBody3D.new()
	var ccs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.4
	cap.height = 1.6
	ccs.shape = cap
	character.add_child(ccs)
	var cap_mesh := CapsuleMesh.new()
	cap_mesh.radius = 0.4
	cap_mesh.height = 1.6
	_add_visual(character, cap_mesh, Color(0.9, 0.5, 0.2))
	character.position = Vector3(0, 2.0, 0)
	add_child(character)

func _physics_process(delta: float) -> void:
	frames += 1
	character.velocity.y -= 9.8 * delta
	character.velocity.x = 1.5   # walk uphill while jumping
	character.velocity.z = 0.0
	if character.is_on_floor() and frames > 10:
		character.velocity.y = 5.0
	character.move_and_slide()
	if character.position.x > 8.0:
		character.position.x = -8.0
		character.position.y = character.position.x * tan(deg_to_rad(SLOPE_DEG)) + 2.0

	# Penetration of the capsule's bottom sphere against the analytic slope plane.
	var slope_normal := Vector3(-sin(deg_to_rad(SLOPE_DEG)), cos(deg_to_rad(SLOPE_DEG)), 0)
	var sphere_center := character.position + Vector3(0, -0.4, 0)
	var pen := 0.4 - slope_normal.dot(sphere_center)
	max_pen = maxf(max_pen, pen)

	if frames == RUN_FRAMES:
		var ok := max_pen <= 0.02
		var msg := "RESULT: %s - max slope penetration %.3f m (limit 0.02)" % [
			"PASS" if ok else "FAIL", max_pen]
		print(msg)
		var lbl := get_tree().root.find_child("ResultLabel", true, false)
		if lbl and lbl is Label:
			lbl.text = msg
		set_physics_process(false)
