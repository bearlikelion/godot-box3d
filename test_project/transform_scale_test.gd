## Regression: body transform scale must apply to collision geometry.
##
## Godot allows scale on a body's transform (imported scenes commonly park a
## mesh's full transform, scale included, on its StaticBody3D while the shape
## data stays at identity). Box3D body transforms are rigid, so the extension
## bakes the composed scale into the engine-side shape geometry. This test
## builds a scaled + rotated concave (trimesh) body and verifies rays, shape
## queries, and character motion resolve against the SCALED surface.
extends SceneTree




var frames := 0
var failed := 0
var c_scaled: CharacterBody3D
var c_rot: CharacterBody3D
var r_scaled: RigidBody3D

func _check(cond: bool, msg: String) -> void:
	print(("ok:   " if cond else "FAIL: ") + msg)
	if not cond: failed += 1

func _mk_map_piece(xf: Transform3D) -> void:
	var mesh := BoxMesh.new()
	mesh.size = Vector3(10, 1, 10)
	var body := StaticBody3D.new()
	body.transform = xf
	var mi := MeshInstance3D.new(); mi.mesh = mesh; body.add_child(mi)
	var cs := CollisionShape3D.new(); cs.shape = mesh.create_trimesh_shape(); body.add_child(cs)
	root.add_child(body)

func _mk_char(pos: Vector3) -> CharacterBody3D:
	var c := CharacterBody3D.new()
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new(); cap.radius = 0.4; cap.height = 1.6; cs.shape = cap
	c.add_child(cs); c.position = pos; root.add_child(c)
	return c

func _initialize() -> void:
	# Scaled piece: 4x2x4 -> spans x[20,60], top at y=0.
	_mk_map_piece(Transform3D(Basis.from_scale(Vector3(4, 2, 4)), Vector3(40, -1.0, 0)))
	c_scaled = _mk_char(Vector3(52, 0.81, 0))
	r_scaled = RigidBody3D.new()
	var rcs := CollisionShape3D.new(); var s := SphereShape3D.new(); s.radius = 0.5; rcs.shape = s
	r_scaled.add_child(rcs); r_scaled.position = Vector3(30, 2, 0); root.add_child(r_scaled)
	# Rotated (45 deg yaw) + scaled piece at x=100: top at y=0.
	var b := Basis(Vector3.UP, deg_to_rad(45.0)) * Basis.from_scale(Vector3(3, 2, 3))
	_mk_map_piece(Transform3D(b, Vector3(100, -1.0, 0)))
	c_rot = _mk_char(Vector3(100, 0.81, 0))

func _physics_process(delta: float) -> bool:
	frames += 1
	for c: CharacterBody3D in [c_scaled, c_rot]:
		c.velocity.y -= 9.8 * delta
		c.move_and_slide()
		if c.is_on_floor(): c.velocity.y = 0.0
	if frames == 90:
		_check(c_scaled.is_on_floor() and c_scaled.position.y > 0.5,
				"character stands on SCALED imported piece (y=%.2f)" % c_scaled.position.y)
		_check(c_rot.is_on_floor() and c_rot.position.y > 0.5,
				"character stands on ROTATED+SCALED piece (y=%.2f)" % c_rot.position.y)
		_check(r_scaled.position.y > 0.3 and r_scaled.position.y < 0.7,
				"rigid sphere rests on scaled piece (y=%.2f)" % r_scaled.position.y)
		var ray := PhysicsRayQueryParameters3D.create(Vector3(55, 5, 10), Vector3(55, -5, 10), 0xFFFFFFFF)
		var hit := root.get_world_3d().direct_space_state.intersect_ray(ray)
		_check(not hit.is_empty() and absf(hit["position"].y) < 0.05,
				"ray hits scaled piece's EXTENDED footprint (x=55,z=10 outside unscaled bounds; got %s)" % [hit.get("position")])
		print("TRANSFORM SCALE TEST " + ("PASSED" if failed == 0 else "FAILED (%d)" % failed))
		quit(0 if failed == 0 else 1)
		return true
	return false
