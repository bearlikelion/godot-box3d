extends SceneTree
## Regression: jump repeatedly on slopes; track penetration below the slope plane
## continuously. Sinking/lock/fall-through must show as growing penetration.
var frames := 0
var cases: Array = []  # [body, deg, kind, max_pen, fell]

func _mk_slope_mesh(deg: float, x0: float) -> void:
	var b := StaticBody3D.new(); var cs := CollisionShape3D.new()
	var tri := ConcavePolygonShape3D.new()
	var n := 8; var faces := PackedVector3Array()
	var t := tan(deg_to_rad(deg))
	for i in n:  # 8x8 grid of quads, 40m slope, CW front faces like Godot importers
		for j in n:
			var x1 := -20.0 + 40.0 * i / n; var x2 := -20.0 + 40.0 * (i + 1) / n
			var z1 := -20.0 + 40.0 * j / n; var z2 := -20.0 + 40.0 * (j + 1) / n
			var p11 := Vector3(x1, x1 * t, z1); var p21 := Vector3(x2, x2 * t, z1)
			var p12 := Vector3(x1, x1 * t, z2); var p22 := Vector3(x2, x2 * t, z2)
			faces.append_array([p11, p21, p22, p11, p22, p12])
	tri.set_faces(faces); cs.shape = tri; b.add_child(cs)
	b.position = Vector3(x0, 0, 0); root.add_child(b)

func _mk_slope_hull(deg: float, x0: float) -> void:
	var b := StaticBody3D.new(); var cs := CollisionShape3D.new()
	var hull := ConvexPolygonShape3D.new()
	var t := tan(deg_to_rad(deg))
	hull.points = PackedVector3Array([
		Vector3(-20, -20 * t, -20), Vector3(20, 20 * t, -20), Vector3(20, 20 * t, 20), Vector3(-20, -20 * t, 20),
		Vector3(-20, -20 * t - 2, -20), Vector3(20, 20 * t - 2, -20), Vector3(20, 20 * t - 2, 20), Vector3(-20, -20 * t - 2, 20)])
	cs.shape = hull; b.add_child(cs); b.position = Vector3(x0, 0, 0); root.add_child(b)

func _mk_char(x0: float, deg: float, kind: String) -> void:
	var c := CharacterBody3D.new(); var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new(); cap.radius = 0.4; cap.height = 1.6; cs.shape = cap
	c.add_child(cs); c.position = Vector3(x0, 1.5, 0); root.add_child(c)
	cases.append([c, deg, kind, -99.0, false, x0])

func _initialize() -> void:
	_mk_slope_hull(20.0, 0.0);    _mk_char(0.0, 20.0, "hull")
	_mk_slope_hull(35.0, 100.0);  _mk_char(100.0, 35.0, "hull")
	_mk_slope_mesh(20.0, 200.0);  _mk_char(200.0, 20.0, "mesh")
	_mk_slope_mesh(35.0, 300.0);  _mk_char(300.0, 35.0, "mesh")

func _physics_process(delta: float) -> bool:
	frames += 1
	for cse in cases:
		var c: CharacterBody3D = cse[0]
		var t := tan(deg_to_rad(cse[1])); var x0: float = cse[5]
		c.velocity.y -= 9.8 * delta
		c.velocity.x = (2.0 if cse[1] < 30.0 else -2.0) if cse[2] == "mesh" else 0.0   # mesh: walk up (20deg) / down (35deg) while jumping
		c.velocity.z = 0.0
		if c.is_on_floor() and frames > 10:
			c.velocity.y = 6.0   # jump every landing
		c.move_and_slide()
		# penetration of capsule BOTTOM below the analytic slope surface
		var lx := c.position.x - x0
		var surf_y := lx * t
		# TRUE capsule bottom: Godot capsule height (1.6) INCLUDES the caps, so the
		# bottom pole is center - 0.8. Penetration measured along the plane normal:
		# signed distance of the bottom SPHERE CENTER (center - 0.4) to the plane,
		# minus radius.
		var deg: float = cse[1]
		var n := Vector3(-sin(deg_to_rad(deg)), cos(deg_to_rad(deg)), 0)
		var sphere_c := c.position + Vector3(x0 * 0.0, -0.4, 0)
		var dist_n := n.dot(Vector3(lx, c.position.y - 0.4, c.position.z))
		var pen := 0.4 - dist_n
		cse[3] = maxf(cse[3], pen)
		if c.position.y < surf_y - 3.0: cse[4] = true
	if frames >= 450:  # stay on the 40m slope with 2 m/s walkers
		var ok := true
		for cse in cases:
			print("%s %.0fdeg: max_pen=%.3fm  fell_through=%s" % [cse[2], cse[1], cse[3], cse[4]])
			if cse[3] > 0.02 or cse[4]:
				ok = false
		print("SLOPE TEST " + ("PASSED" if ok else "FAILED"))
		quit(0 if ok else 1); return true
	return false
