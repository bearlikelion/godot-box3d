extends SceneTree
## CSG level-geometry regression: the exact server-call flow game levels use
## (attach empty concave shape, set_faces after; Godot clockwise winding).

var frames := 0
var failed := 0
var c: CharacterBody3D
var r: RigidBody3D

func _check(cond: bool, msg: String) -> void:
	print(("ok:   " if cond else "FAIL: ") + msg)
	if not cond: failed += 1

func _initialize() -> void:
	var csg := CSGBox3D.new()
	csg.size = Vector3(20, 1, 20)
	csg.position = Vector3(0, -0.5, 0)
	csg.use_collision = true
	root.add_child(csg)
	c = CharacterBody3D.new()
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new(); cap.radius = 0.4; cap.height = 1.6; cs.shape = cap
	c.add_child(cs); c.position = Vector3(0, 0.81, 0)
	root.add_child(c)
	r = RigidBody3D.new()
	var rcs := CollisionShape3D.new()
	var s := SphereShape3D.new(); s.radius = 0.5; rcs.shape = s
	r.add_child(rcs); r.position = Vector3(3, 2, 0)
	root.add_child(r)

func _physics_process(delta: float) -> bool:
	frames += 1
	c.velocity.y -= 9.8 * delta
	c.move_and_slide()
	if c.is_on_floor(): c.velocity.y = 0.0
	if frames == 90:
		_check(c.is_on_floor() and c.position.y > 0.5,
				"character stands on CSG floor (y=%.2f)" % c.position.y)
		_check(r.position.y > 0.3 and r.position.y < 0.7,
				"rigid sphere rests on CSG floor (y=%.2f)" % r.position.y)
		var ray := PhysicsRayQueryParameters3D.create(Vector3(1, 5, 1), Vector3(1, -5, 1), 0xFFFFFFFF)
		var hit := root.get_world_3d().direct_space_state.intersect_ray(ray)
		_check(not hit.is_empty() and absf(hit["position"].y) < 0.05,
				"downward ray hits CSG top face at y=0 (got %s)" % [hit.get("position")])
		_check(not hit.is_empty() and hit["normal"].y > 0.9,
				"CSG floor ray normal points up (got %s)" % [hit.get("normal")])
		print("CSG TEST " + ("PASSED" if failed == 0 else "FAILED (%d)" % failed))
		quit(0 if failed == 0 else 1)
		return true
	return false
