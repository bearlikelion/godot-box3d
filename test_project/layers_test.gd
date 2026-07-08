extends SceneTree
## Validates Godot collision-layer semantics through the Box3D bridge:
## one-directional queries/motion, OR-rule solver pairs, area mask rule,
## and live layer changes reaching engine filters.

var frames := 0
var failed := 0
var char_ok: CharacterBody3D      # mask=1 on layer=1/mask=0 ground -> stands
var char_wrong: CharacterBody3D   # mask=2 on layer=1 ground -> falls (by design)
var rigid_scans: RigidBody3D      # layer=4 mask=1 -> rests on mask-0 ground (OR rule)
var rigid_a: RigidBody3D          # layer=2 mask=8 -> rests ON static blocker layer=8 mask=0
var ghost_a: RigidBody3D          # layer=2 mask=4 vs ghost_b: disjoint -> pass through
var area_hits := 0
var area_wrong_hits := 0
var late_ground: StaticBody3D

func _check(cond: bool, msg: String) -> void:
	print(("ok:   " if cond else "FAIL: ") + msg)
	if not cond: failed += 1

func _mk_ground(pos: Vector3, size: Vector3, layer: int, mask: int) -> StaticBody3D:
	var g := StaticBody3D.new()
	g.collision_layer = layer
	g.collision_mask = mask
	var cs := CollisionShape3D.new()
	var b := BoxShape3D.new(); b.size = size; cs.shape = b
	g.add_child(cs); g.position = pos
	root.add_child(g)
	return g

func _mk_char(pos: Vector3, mask: int) -> CharacterBody3D:
	var c := CharacterBody3D.new()
	c.collision_layer = 1; c.collision_mask = mask
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new(); cap.radius = 0.4; cap.height = 1.6; cs.shape = cap
	c.add_child(cs); c.position = pos
	root.add_child(c)
	return c

func _mk_rigid(pos: Vector3, layer: int, mask: int) -> RigidBody3D:
	var r := RigidBody3D.new()
	r.collision_layer = layer; r.collision_mask = mask
	var cs := CollisionShape3D.new()
	var s := SphereShape3D.new(); s.radius = 0.5; cs.shape = s
	r.add_child(cs); r.position = pos
	root.add_child(r)
	return r

func _initialize() -> void:
	_mk_ground(Vector3(0, -0.5, 0), Vector3(60, 1, 20), 1, 0)   # world: layer=1 mask=0
	char_ok = _mk_char(Vector3(0, 0.81, 0), 1)
	char_wrong = _mk_char(Vector3(3, 0.81, 0), 2)
	rigid_scans = _mk_rigid(Vector3(6, 2, 0), 4, 1)
	# One-directional solver: A(layer=2,mask=8) onto static blocker(layer=8,mask=0) at y=2
	_mk_ground(Vector3(9, 2, 0), Vector3(2, 0.5, 2), 8, 0)
	rigid_a = _mk_rigid(Vector3(9, 4, 0), 2, 8)
	# Disjoint rigids: ghost_a falls onto ghost_b; neither scans the other; both scan world(1)
	var ghost_b := _mk_rigid(Vector3(12, 0.5, 0), 8, 16 | 1)
	ghost_b.freeze = true
	ghost_a = _mk_rigid(Vector3(12, 3, 0), 2, 4 | 1)
	# Areas: right mask detects char_ok (layer=1); wrong mask must not.
	var area_r := Area3D.new(); area_r.collision_mask = 1; area_r.monitoring = true
	var acs := CollisionShape3D.new(); var ab := BoxShape3D.new(); ab.size = Vector3(2,4,2); acs.shape = ab
	area_r.add_child(acs); area_r.position = Vector3(0, 1, 0); root.add_child(area_r)
	area_r.body_entered.connect(func(_b): area_hits += 1)
	var area_w := Area3D.new(); area_w.collision_mask = 2; area_w.monitoring = true
	var acs2 := CollisionShape3D.new(); var ab2 := BoxShape3D.new(); ab2.size = Vector3(2,4,2); acs2.shape = ab2
	area_w.add_child(acs2); area_w.position = Vector3(0, 1, 0); root.add_child(area_w)
	area_w.body_entered.connect(func(_b): area_wrong_hits += 1)
	# Late-layer ground for runtime-change test, far lane
	late_ground = _mk_ground(Vector3(20, -0.5, 0), Vector3(4, 1, 4), 4, 0)

func _physics_process(delta: float) -> bool:
	frames += 1
	for c: CharacterBody3D in [char_ok, char_wrong]:
		c.velocity.y -= 9.8 * delta
		c.move_and_slide()
		if c.is_on_floor(): c.velocity.y = 0.0
	if frames == 90:
		_check(char_ok.is_on_floor() and char_ok.position.y > 0.5,
				"character (mask=1) stands on mask-0 world geometry")
		_check(char_wrong.position.y < -3.0,
				"character (mask=2) falls through layer-1 ground BY DESIGN (y=%.1f)" % char_wrong.position.y)
		_check(rigid_scans.position.y > 0.3 and rigid_scans.position.y < 0.7,
				"rigid (mask=1) rests on mask-0 ground via OR rule (y=%.2f)" % rigid_scans.position.y)
		_check(rigid_a.position.y > 2.5,
				"one-directional pair: A scans B, B doesn't scan A -> still collide (y=%.2f)" % rigid_a.position.y)
		_check(ghost_a.position.y < 1.2,
				"disjoint rigids pass through each other, land on world (y=%.2f)" % ghost_a.position.y)
		_check(area_hits >= 1, "area (mask=1) detected the layer-1 body")
		_check(area_wrong_hits == 0, "area (mask=2) ignored the layer-1 body")
		var ray := PhysicsRayQueryParameters3D.create(Vector3(20, 5, 0), Vector3(20, -1, 0), 2)
		var hit := root.get_world_3d().direct_space_state.intersect_ray(ray)
		_check(hit.is_empty(), "ray (mask=2) misses layer-4 ground")
		late_ground.collision_layer = 2
	if frames == 92:
		var ray := PhysicsRayQueryParameters3D.create(Vector3(20, 5, 0), Vector3(20, -1, 0), 2)
		var hit := root.get_world_3d().direct_space_state.intersect_ray(ray)
		_check(not hit.is_empty(), "ray (mask=2) hits ground after live layer change to 2")
		print("LAYERS TEST " + ("PASSED" if failed == 0 else "FAILED (%d)" % failed))
		quit(0 if failed == 0 else 1)
		return true
	return false
