extends SceneTree
## Reproduces the in-game fall-through: characters under gravity via move_and_slide,
## standing on (a) a BoxShape ground and (b) a trimesh ground (the CSG floor case).

var frames: int = 0
var char_on_box: CharacterBody3D
var char_on_mesh: CharacterBody3D

func _make_char(pos: Vector3) -> CharacterBody3D:
	var c := CharacterBody3D.new()
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.4
	cap.height = 1.6
	cs.shape = cap
	c.add_child(cs)
	c.position = pos
	return c

func _initialize() -> void:
	var box_ground := StaticBody3D.new()
	var bcs := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(10, 1, 10)
	bcs.shape = box
	box_ground.add_child(bcs)
	box_ground.position = Vector3(0, -0.5, 0)
	root.add_child(box_ground)

	# Trimesh ground quad at y=0 spanning x/z in [20-5, 20+5].
	var mesh_ground := StaticBody3D.new()
	var mcs := CollisionShape3D.new()
	var tri := ConcavePolygonShape3D.new()
	var a := Vector3(15, 0, -5); var b := Vector3(25, 0, -5)
	var c := Vector3(25, 0, 5); var d := Vector3(15, 0, 5)
	tri.set_faces(PackedVector3Array([a, b, c, a, c, d]))
	mcs.shape = tri
	mesh_ground.add_child(mcs)
	root.add_child(mesh_ground)

	# Wall at x=3 on the box ground: the walker must stop against it, not pass through.
	var wall := StaticBody3D.new()
	var wcs := CollisionShape3D.new()
	var wbox := BoxShape3D.new()
	wbox.size = Vector3(0.5, 4, 10)
	wcs.shape = wbox
	wall.add_child(wcs)
	wall.position = Vector3(3.25, 2, 0)
	root.add_child(wall)

	# 20-degree trimesh ramp on the mesh ground ahead of the walker.
	var ramp := StaticBody3D.new()
	var rcs := CollisionShape3D.new()
	var rtri := ConcavePolygonShape3D.new()
	var r0 := Vector3(22, 0, -5); var r1 := Vector3(25, 1.1, -5)
	var r2 := Vector3(25, 1.1, 5); var r3 := Vector3(22, 0, 5)
	rtri.set_faces(PackedVector3Array([r0, r1, r2, r0, r2, r3]))
	rcs.shape = rtri
	ramp.add_child(rcs)
	root.add_child(ramp)

	char_on_box = _make_char(Vector3(0, 0.81, 0))   # capsule half-height above y=0
	char_on_mesh = _make_char(Vector3(20, 0.81, 0))
	root.add_child(char_on_box)
	root.add_child(char_on_mesh)

func _physics_process(delta: float) -> bool:
	frames += 1
	for c: CharacterBody3D in [char_on_box, char_on_mesh]:
		c.velocity.y -= 9.8 * delta
		c.velocity.x = 2.0 if frames > 30 else 0.0  # then walk while grounded
		c.move_and_slide()
		if c.is_on_floor():
			c.velocity.y = 0.0
	if frames % 30 == 0:
		print("f%d  box: x=%.2f y=%.3f floor=%s | mesh: x=%.2f y=%.3f floor=%s" % [
			frames, char_on_box.position.x, char_on_box.position.y, char_on_box.is_on_floor(),
			char_on_mesh.position.x, char_on_mesh.position.y, char_on_mesh.is_on_floor()])
	if frames >= 150:
		# Box walker: stayed grounded, walked, and stopped against the wall near x=2.6.
		var ok_box := char_on_box.is_on_floor() and char_on_box.position.y > 0.5 \
				and char_on_box.position.x > 2.0 and char_on_box.position.x < 3.1
		# Mesh walker: stayed grounded and climbed the trimesh ramp (x past 22, y risen).
		var ok_mesh := char_on_mesh.is_on_floor() and char_on_mesh.position.x > 22.5 \
				and char_on_mesh.position.y > 0.9
		print("RESULT box-ground+wall: ", "PASS" if ok_box else "FAIL",
				" (x=%.2f y=%.2f)" % [char_on_box.position.x, char_on_box.position.y])
		print("RESULT mesh-ground+ramp: ", "PASS" if ok_mesh else "FAIL",
				" (x=%.2f y=%.2f)" % [char_on_mesh.position.x, char_on_mesh.position.y])
		quit(0 if (ok_box and ok_mesh) else 1)
		return true
	return false
