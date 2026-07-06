extends SceneTree

var failures: int = 0
var contact_body_entered: bool = false
var contact_body: ContactProbeBody
var exception_body: RigidBody3D
var damping_body: RigidBody3D


class ContactProbeBody:
	extends RigidBody3D

	var max_contact_count: int = 0
	var contact_collider_name: String = ""

	func _integrate_forces(state: PhysicsDirectBodyState3D) -> void:
		max_contact_count = max(max_contact_count, state.get_contact_count())
		if state.get_contact_count() > 0 and contact_collider_name.is_empty():
			var collider: Node = state.get_contact_collider_object(0) as Node
			if collider != null:
				contact_collider_name = collider.name


func _initialize() -> void:
	print("Active physics engine setting: ", ProjectSettings.get_setting("physics/3d/physics_engine"))
	call_deferred("_run")


func _run() -> void:
	_setup_query_contract()
	await physics_frame
	await physics_frame
	_test_query_contract()

	_setup_multishape_motion_contract()
	await physics_frame
	await physics_frame
	_test_multishape_motion_contract()

	_setup_contact_and_exception_contract()
	for i in 180:
		await physics_frame
	_test_contact_and_exception_contract()

	_setup_area_damping_contract()
	for i in 90:
		await physics_frame
	_test_area_damping_contract()

	if failures == 0:
		print("RESULT: PASS - physics contract checks passed")
	else:
		print("RESULT: FAIL - ", failures, " physics contract check(s) failed")
	quit(1 if failures > 0 else 0)


func _assert_result(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		print("FAIL: ", message)


func _make_box_shape(size: Vector3) -> CollisionShape3D:
	var collision: CollisionShape3D = CollisionShape3D.new()
	var shape: BoxShape3D = BoxShape3D.new()
	shape.size = size
	collision.shape = shape
	return collision


func _make_sphere_shape(radius: float) -> CollisionShape3D:
	var collision: CollisionShape3D = CollisionShape3D.new()
	var shape: SphereShape3D = SphereShape3D.new()
	shape.radius = radius
	collision.shape = shape
	return collision


func _setup_query_contract() -> void:
	var target: StaticBody3D = StaticBody3D.new()
	target.name = "QueryTarget"
	target.add_child(_make_box_shape(Vector3(2, 2, 2)))
	root.add_child(target)


func _test_query_contract() -> void:
	var target: StaticBody3D = root.get_node("QueryTarget") as StaticBody3D
	var space_state: PhysicsDirectSpaceState3D = root.get_world_3d().direct_space_state

	var cylinder: CylinderShape3D = CylinderShape3D.new()
	cylinder.radius = 0.75
	cylinder.height = 2.0

	var query: PhysicsShapeQueryParameters3D = PhysicsShapeQueryParameters3D.new()
	query.shape = cylinder
	query.transform = Transform3D(Basis(), Vector3.ZERO)
	query.collision_mask = 1
	query.collide_with_bodies = true
	query.collide_with_areas = false

	var hits: Array[Dictionary] = space_state.intersect_shape(query, 4)
	_assert_result(hits.size() == 1, "cylinder intersect_shape reports the static target")

	var contact_points: Array[Vector3] = space_state.collide_shape(query, 4)
	_assert_result(contact_points.size() >= 2 and contact_points.size() % 2 == 0, "collide_shape returns contact point pairs")

	query.exclude = [target.get_rid()]
	var excluded_hits: Array[Dictionary] = space_state.intersect_shape(query, 4)
	var excluded_contact_points: Array[Vector3] = space_state.collide_shape(query, 4)
	_assert_result(excluded_hits.is_empty(), "intersect_shape honors excluded RIDs")
	_assert_result(excluded_contact_points.is_empty(), "collide_shape honors excluded RIDs")


func _setup_multishape_motion_contract() -> void:
	var wall: StaticBody3D = StaticBody3D.new()
	wall.name = "MultiShapeWall"
	wall.position = Vector3(3, 0, 0)
	wall.add_child(_make_box_shape(Vector3(0.5, 2, 2)))
	root.add_child(wall)

	var mover: CharacterBody3D = CharacterBody3D.new()
	mover.name = "MultiShapeMover"
	mover.position = Vector3.ZERO

	var first_shape: CollisionShape3D = _make_box_shape(Vector3(0.5, 0.5, 0.5))
	mover.add_child(first_shape)

	var second_shape: CollisionShape3D = _make_box_shape(Vector3(0.5, 0.5, 0.5))
	second_shape.position = Vector3(2, 0, 0)
	mover.add_child(second_shape)

	root.add_child(mover)


func _test_multishape_motion_contract() -> void:
	var mover: CharacterBody3D = root.get_node("MultiShapeMover") as CharacterBody3D
	var collision: KinematicCollision3D = mover.move_and_collide(Vector3(0.75, 0, 0))
	_assert_result(collision != null, "body_test_motion checks enabled shapes after the first one")


func _setup_contact_and_exception_contract() -> void:
	var excluded_platform: StaticBody3D = StaticBody3D.new()
	excluded_platform.name = "ExcludedPlatform"
	excluded_platform.position = Vector3(-3, 0, 0)
	excluded_platform.add_child(_make_box_shape(Vector3(3, 0.5, 3)))
	root.add_child(excluded_platform)

	var catch_platform: StaticBody3D = StaticBody3D.new()
	catch_platform.name = "CatchPlatform"
	catch_platform.position = Vector3(-3, -4, 0)
	catch_platform.add_child(_make_box_shape(Vector3(3, 0.5, 3)))
	root.add_child(catch_platform)

	exception_body = RigidBody3D.new()
	exception_body.name = "ExceptionBody"
	exception_body.position = Vector3(-3, 4, 0)
	exception_body.add_child(_make_sphere_shape(0.4))
	root.add_child(exception_body)
	exception_body.add_collision_exception_with(excluded_platform)

	var contact_platform: StaticBody3D = StaticBody3D.new()
	contact_platform.name = "ContactPlatform"
	contact_platform.position = Vector3(8, 0, 0)
	contact_platform.add_child(_make_box_shape(Vector3(3, 0.5, 3)))
	root.add_child(contact_platform)

	contact_body = ContactProbeBody.new()
	contact_body.name = "ContactBody"
	contact_body.position = Vector3(8, 4, 0)
	contact_body.contact_monitor = true
	contact_body.max_contacts_reported = 4
	contact_body.add_child(_make_sphere_shape(0.4))
	contact_body.body_entered.connect(_on_contact_body_entered)
	root.add_child(contact_body)


func _on_contact_body_entered(body: Node) -> void:
	if body.name == "ContactPlatform":
		contact_body_entered = true


func _test_contact_and_exception_contract() -> void:
	print("Contact probe max contact count: ", contact_body.max_contact_count, " collider: ", contact_body.contact_collider_name)
	_assert_result(contact_body.max_contact_count > 0, "direct body state reports contacts for contact monitor body")
	_assert_result(contact_body.contact_collider_name == "ContactPlatform", "direct body state reports contact collider object")
	_assert_result(contact_body_entered, "RigidBody3D contact monitor reports body_entered")
	_assert_result(exception_body.global_position.y < -1.0, "collision exceptions let a body pass through an excepted platform")


func _setup_area_damping_contract() -> void:
	var area: Area3D = Area3D.new()
	area.name = "DampingArea"
	area.set_gravity_space_override_mode(Area3D.SPACE_OVERRIDE_DISABLED)
	area.set_linear_damp_space_override_mode(Area3D.SPACE_OVERRIDE_REPLACE)
	area.set_angular_damp_space_override_mode(Area3D.SPACE_OVERRIDE_REPLACE)
	area.linear_damp = 8.0
	area.angular_damp = 8.0
	area.add_child(_make_box_shape(Vector3(6, 6, 6)))
	root.add_child(area)

	damping_body = RigidBody3D.new()
	damping_body.name = "DampingBody"
	damping_body.gravity_scale = 0.0
	damping_body.linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	damping_body.angular_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	damping_body.linear_damp = 0.0
	damping_body.angular_damp = 0.0
	damping_body.can_sleep = false
	damping_body.linear_velocity = Vector3(10, 0, 0)
	damping_body.angular_velocity = Vector3(0, 0, 10)
	damping_body.add_child(_make_sphere_shape(0.4))
	root.add_child(damping_body)


func _test_area_damping_contract() -> void:
	_assert_result(damping_body.linear_velocity.length() < 4.0, "Area3D linear damping override slows a body with zero body damping")
	_assert_result(damping_body.angular_velocity.length() < 4.0, "Area3D angular damping override slows a body with zero body damping")
