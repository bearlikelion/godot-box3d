extends SceneTree

var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _query(shape: Shape3D, position: Vector3, margin: float = 0.0) -> PhysicsShapeQueryParameters3D:
	var query := PhysicsShapeQueryParameters3D.new()
	query.shape = shape
	query.transform.origin = position
	query.margin = margin
	return query


func _body(shape: Shape3D, position: Vector3) -> StaticBody3D:
	var body := StaticBody3D.new()
	body.position = position
	var collision := CollisionShape3D.new()
	collision.shape = shape
	body.add_child(collision)
	root.add_child(body)
	return body


func _run() -> void:
	var box := BoxShape3D.new()
	box.size = Vector3.ONE
	var solid := _body(box, Vector3.ZERO)
	var mesh := BoxMesh.new()
	mesh.size = Vector3.ONE
	var terrain := _body(mesh.create_trimesh_shape(), Vector3(10, 0, 0))
	var heights := HeightMapShape3D.new()
	heights.map_width = 3
	heights.map_depth = 3
	heights.map_data = PackedFloat32Array([0, 0, 0, 0, 0, 0, 0, 0, 0])
	_body(heights, Vector3(20, 0, 0))
	await physics_frame
	await physics_frame
	var state := root.world_3d.direct_space_state
	var pairs := state.collide_shape(_query(box, Vector3(0.6, 0, 0)))
	_check_pairs(pairs, Vector3.RIGHT, 0.4, "overlapping boxes")
	var small_box := BoxShape3D.new()
	small_box.size = Vector3(1, 0.4, 1)
	var rotated_query := _query(small_box, Vector3(0.7, 0.6, 0.7))
	rotated_query.transform.basis = Basis(Vector3.UP, PI / 4.0)
	var clipped := state.collide_shape(rotated_query)
	_check(not clipped.is_empty(), "rotated partial overlap has contacts")
	for index in range(0, clipped.size(), 2):
		var query_point: Vector3 = rotated_query.transform.affine_inverse() * clipped[index]
		var target_point := clipped[index + 1]
		_check(absf(query_point.x) <= 0.51 and absf(query_point.y) <= 0.21 and absf(query_point.z) <= 0.51, "contact lies within the rotated query surface")
		_check(absf(target_point.x) <= 0.51 and absf(target_point.y) <= 0.51 and absf(target_point.z) <= 0.51, "clipped contact lies within the target surface")
	var sphere := SphereShape3D.new()
	sphere.radius = 0.25
	_check_pairs(state.collide_shape(_query(sphere, Vector3(10.6, 0, 0))), Vector3.RIGHT, 0.15, "sphere against mesh")
	_check_pairs(state.collide_shape(_query(sphere, Vector3(20, 0.1, 0))), Vector3.UP, 0.15, "sphere against heightmap")
	var overlap := _query(sphere, Vector3(0.6, 0, 0))
	var rest := state.get_rest_info(overlap)
	_check(not rest.is_empty() and rest.get("rid") == solid.get_rid(), "rest info identifies the existing overlap")
	_check(rest.get("normal", Vector3.ZERO).distance_to(Vector3.RIGHT) < 0.01, "rest info reports a geometric normal")
	_check(rest.get("point", Vector3.ZERO).distance_to(Vector3(0.5, 0, 0)) < 0.01, "rest info lies on the target surface")
	var clear := _query(sphere, Vector3(2, 0, 0))
	clear.motion = Vector3(-3, 0, 0)
	_check(state.get_rest_info(clear).is_empty(), "rest info ignores requested motion")
	var cast := _query(sphere, Vector3(2, 0, 0))
	cast.motion = Vector3(-2, 0, 0)
	var uninflated := state.cast_motion(cast)
	_check(absf(uninflated[0] - 0.625) < 0.001, "safe motion stops before the independently calculated surface")
	cast.margin = 0.1
	var inflated := state.cast_motion(cast)
	_check(absf(inflated[0] - 0.575) < 0.001 and inflated[1] >= inflated[0], "cast motion honors margin and ordered safe/unsafe fractions")
	var gap := _query(sphere, Vector3(0.8, 0, 0))
	_check(state.intersect_shape(gap).is_empty(), "the gap is clear without a margin")
	gap.margin = 0.1
	_check(not state.intersect_shape(gap).is_empty(), "intersection includes a positive-distance margin shell")
	_check(not state.collide_shape(gap).is_empty(), "contact query includes a positive-distance margin shell")
	var margin_rest := state.get_rest_info(gap)
	_check(not margin_rest.is_empty() and margin_rest.get("normal", Vector3.ZERO).distance_to(Vector3.RIGHT) < 0.01, "rest info includes a margin shell with a valid normal")
	var capped := state.collide_shape(_query(box, Vector3(0.6, 0, 0)), 1)
	_check(capped.size() == 2, "contact pairs respect max_results")
	var excluded := _query(sphere, Vector3(10.6, 0, 0))
	excluded.exclude = [terrain.get_rid()]
	_check(state.collide_shape(excluded).is_empty() and state.get_rest_info(excluded).is_empty(), "terrain queries honor exclusions")
	var masked := _query(sphere, Vector3(0.6, 0, 0))
	masked.collision_mask = 2
	_check(state.collide_shape(masked).is_empty() and state.get_rest_info(masked).is_empty(), "contact queries honor masks")
	# Rotating contact velocity is omega cross the world-space contact offset.
	var rotating := AnimatableBody3D.new()
	rotating.position = Vector3(30, 0, 0)
	rotating.rotation.y = PI / 2.0
	var collision := CollisionShape3D.new()
	collision.shape = box
	rotating.add_child(collision)
	root.add_child(rotating)
	await physics_frame
	PhysicsServer3D.body_set_state(rotating.get_rid(), PhysicsServer3D.BODY_STATE_ANGULAR_VELOCITY, Vector3(0, 0, 2))
	var rotating_rest := state.get_rest_info(_query(sphere, Vector3(30.6, 0, 0)))
	_check(rotating_rest.get("linear_velocity", Vector3.ZERO).distance_to(Vector3(0, 1, 0)) < 0.03, "rest info reports velocity at a rotating contact")
	var direct_body := PhysicsServer3D.body_get_direct_state(rotating.get_rid())
	_check(direct_body.get_velocity_at_local_position(Vector3(0.5, 0, 0)).distance_to(Vector3(0, 1, 0)) < 0.03, "body point velocity uses a world-axis relative offset even when the body is rotated")
	if failures == 0:
		print("RESULT: PASS - direct queries report surface pairs, rest normals, margins, filtering and contact velocity")
	quit(1 if failures else 0)


func _check_pairs(pairs: Array[Vector3], normal: Vector3, depth: float, label: String) -> void:
	_check(not pairs.is_empty() and pairs.size() % 2 == 0, label + " returns contact pairs")
	for index in range(0, pairs.size(), 2):
		var correction := pairs[index + 1] - pairs[index]
		_check(correction.distance_to(normal * depth) < 0.02, label + " has the independently calculated penetration vector: " + str(correction))


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		print("RESULT: FAIL - ", message)
