extends SceneTree
## Jump-on-slope stability guard. Reproduces the encroachment-ratchet failure:
## b3World_CastMover lets an initially-touching mover approach a surface by up
## to B3_LINEAR_SLOP per cast (floored near a full radius deep). Under wedge
## geometry -- jumping beneath a ramp's sloped underside -- recovery is
## infeasible and the allowance ratchets unopposed: 38 cm of floor penetration,
## then lock-in or fall-through, before the static anti-encroachment clamp.
## This test fails if either scenario ever exceeds ~design-equilibrium depth.
## FAN: a slope built as a triangle fan -- landing near the hub overlaps MANY
##      triangles at once (plane-buffer overflow candidate).
## WEDGE: jumping into the sloped UNDERSIDE of a ramp (recovery-infeasible
##      candidate: floor below, slope above, gap shrinks along x).
var frames := 0
var cases: Array = []

func _mk_char(pos: Vector3) -> CharacterBody3D:
	var c := CharacterBody3D.new()
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new(); cap.radius = 0.4; cap.height = 1.6; cs.shape = cap
	c.add_child(cs); c.position = pos; root.add_child(c)
	return c

func _initialize() -> void:
	# --- FAN slope: 96 skinny triangles sharing a hub, tilted 30deg ---
	var ang := deg_to_rad(30.0)
	var b := Basis(Vector3(0,0,1), ang)
	var faces := PackedVector3Array()
	var N := 96
	for i in N:
		var a0 := TAU * i / N; var a1 := TAU * (i+1) / N
		faces.append_array([b * Vector3.ZERO,
			b * Vector3(cos(a0)*14, 0, sin(a0)*14),
			b * Vector3(cos(a1)*14, 0, sin(a1)*14)])
	var fan := StaticBody3D.new(); var fcs := CollisionShape3D.new()
	var ftri := ConcavePolygonShape3D.new(); ftri.set_faces(faces)
	fcs.shape = ftri; fan.add_child(fcs); root.add_child(fan)
	var n := Vector3(-sin(ang), cos(ang), 0)
	cases.append({"c": _mk_char(n * 0.801), "n": n, "d": 0.0, "label": "fan30 ",
		"mode": "fan", "max_pen": -99.0, "worst": -99.0, "lock": 0, "min_y": 99.0})

	# --- WEDGE: flat floor + ramp underside 25deg descending toward +x ---
	var w := StaticBody3D.new(); var wcs := CollisionShape3D.new()
	var wtri := ConcavePolygonShape3D.new()
	var wf := PackedVector3Array()
	# floor y=0
	wf.append_array([Vector3(-20,0,-8), Vector3(20,0,-8), Vector3(20,0,8),
		Vector3(-20,0,-8), Vector3(20,0,8), Vector3(-20,0,8)])
	# ceiling: y = 3.5 at x=-20 descending to y = 0.9 at x=+20 (frontface DOWN)
	wf.append_array([Vector3(-20,3.5,-8), Vector3(20,0.9,8), Vector3(20,0.9,-8),
		Vector3(-20,3.5,-8), Vector3(-20,3.5,8), Vector3(20,0.9,8)])
	wtri.set_faces(wf); wcs.shape = wtri; w.add_child(wcs)
	w.position = Vector3(100, 0, 0); root.add_child(w)
	cases.append({"c": _mk_char(Vector3(100 - 15, 0.801, 0)), "n": Vector3.UP,
		"d": 0.0, "label": "wedge ", "mode": "wedge",
		"max_pen": -99.0, "worst": -99.0, "lock": 0, "min_y": 99.0})

func _physics_process(delta: float) -> bool:
	frames += 1
	for e in cases:
		var c: CharacterBody3D = e.c
		var prev: Vector3 = c.global_position
		c.velocity.y -= 9.8 * delta
		if c.is_on_floor():
			c.velocity.y = 6.0
		if e.mode == "fan":
			# small orbit around the hub so landings hit the dense center often
			var to_hub := -Vector3(c.position.x, 0, c.position.z)
			c.velocity.x = clampf(to_hub.x, -2.0, 2.0)
			c.velocity.z = clampf(to_hub.z + 1.5, -2.0, 2.0)
		else:
			c.velocity.x = 3.0  # drive into the shrinking gap
			c.velocity.z = 0.0
		c.move_and_slide()
		if e.mode == "wedge" and c.position.x > 100 + 18.0:
			c.position = Vector3(100 - 15, 2.0, 0); c.velocity = Vector3.ZERO
		var pen: float
		if e.mode == "fan":
			pen = (e.d + 0.4) - e.n.dot(c.global_position + Vector3(0, -0.4, 0))
		else:
			pen = 0.4 - (c.global_position.y - 0.4)  # below floor = pen>0... floor at y=0: bottom sphere center y-0.4, pen = -(y-0.8)
			pen = 0.8 - c.global_position.y
		e.max_pen = maxf(e.max_pen, pen)
		e.worst = maxf(e.worst, pen)
		e.min_y = minf(e.min_y, c.global_position.y)
		if c.global_position.distance_squared_to(prev) < 0.000001 and frames > 60:
			e.lock += 1
	if frames % 180 == 0:
		var line := "f%4d " % frames
		for e in cases:
			line += "| %s pen=%+.3f lock=%d min_y=%.2f " % [e.label, e.max_pen, e.lock, e.min_y]
			e.max_pen = -99.0
		print(line)
	if frames >= 1080:
		var line := "WORST "
		var ok := true
		for e in cases:
			line += "| %s %+.3f " % [e.label, e.worst]
			# 5 cm bound: design equilibrium is ~5-11 mm; the ratchet failure was
			# 380 mm. (The junction cases can read a few cm of false "penetration"
			# where the analytic slope plane extends past the crease onto the
			# floor, hence the loose-but-decisive threshold.)
			if e.worst > 0.05 or e.min_y < 0.5:
				ok = false
		print(line)
		print("WEDGE TEST " + ("PASSED" if ok else "FAILED"))
		quit(0 if ok else 1)
		return true
	return false
