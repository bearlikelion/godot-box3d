extends SceneTree
## Crowd clash benchmark: two waves of 36 capsule characters walk into each
## other on a trimesh floor (worst-case non-excepted contact). Measures
## physics-step cost and vertical "climbing" (units popping onto each other).

var frames := 0
var units: Array[CharacterBody3D] = []
var phys_ms: Array[float] = []
var prev_pos: Dictionary = {}
var prev_delta: Dictionary = {}
var reversals := 0
var samples := 0

func _mk_unit(pos: Vector3, dir: float) -> CharacterBody3D:
	var c := CharacterBody3D.new()
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new(); cap.radius = 0.4; cap.height = 1.6; cs.shape = cap
	c.add_child(cs); c.position = pos
	c.set_meta("dir", dir)
	root.add_child(c)
	return c

func _initialize() -> void:
	var g := StaticBody3D.new()
	var cs := CollisionShape3D.new()
	var tri := ConcavePolygonShape3D.new()
	var s := 60.0
	tri.set_faces(PackedVector3Array([
		Vector3(-s,0,-s), Vector3(s,0,-s), Vector3(s,0,s),
		Vector3(-s,0,-s), Vector3(s,0,s), Vector3(-s,0,s)]))
	cs.shape = tri; g.add_child(cs); root.add_child(g)
	# Converging swarm: every unit steers toward the origin (a shared attack
	# target), the pressure pattern real minion waves produce at towers/chokes.
	for i in 72:
		var ang := TAU * i / 72.0
		var r := 8.0 + (i % 3) * 2.0
		units.append(_mk_unit(Vector3(cos(ang) * r, 0.81, sin(ang) * r), 0.0))

func _physics_process(delta: float) -> bool:
	frames += 1
	var t0 := Time.get_ticks_usec()
	for c in units:
		c.velocity.y -= 9.8 * delta
		var to_center := -c.position
		to_center.y = 0.0
		var planar := to_center.normalized() * 3.0
		c.velocity.x = planar.x
		c.velocity.z = planar.z
		c.move_and_slide()
		if c.is_on_floor(): c.velocity.y = 0.0
	var ms := (Time.get_ticks_usec() - t0) / 1000.0
	if frames > 100: phys_ms.append(ms)  # clash window only
	if frames > 150:
		for c in units:
			var p := Vector3(c.position.x, 0, c.position.z)
			if prev_pos.has(c):
				var d: Vector3 = p - prev_pos[c]
				if prev_delta.has(c):
					samples += 1
					if d.dot(prev_delta[c]) < 0.0 and d.length() > 0.02 and prev_delta[c].length() > 0.02:
						reversals += 1  # VISIBLE reversal: >2cm both frames
				prev_delta[c] = d
			prev_pos[c] = p
	if frames == 150 or frames == 250 or frames == 400:
		var max_y := 0.0; var climbers := 0
		for c in units:
			max_y = maxf(max_y, c.position.y)
			if c.position.y > 1.1: climbers += 1
		print("f%d  max_y=%.2f climbers=%d" % [frames, max_y, climbers])
	if frames >= 400:
		phys_ms.sort()
		var total := 0.0
		for v in phys_ms: total += v
		print("visible oscillation (>2cm reversals): %.2f%% of unit-frames" % (100.0 * reversals / maxf(samples, 1)))
		print("clash motion cost: avg=%.2fms  p95=%.2fms  max=%.2fms  (72 units)" % [
			total / phys_ms.size(), phys_ms[int(phys_ms.size() * 0.95)], phys_ms[-1]])
		var visible_pct := 100.0 * reversals / maxf(samples, 1)
		var ok := visible_pct < 2.0
		print("CROWD BENCH " + ("PASSED" if ok else "FAILED (visible oscillation %.1f%%)" % visible_pct))
		quit(0 if ok else 1)
		return true
	return false
