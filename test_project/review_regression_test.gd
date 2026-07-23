extends SceneTree

var failures: int = 0


class StateProbeBody:
	extends RigidBody3D

	var sample_count: int = 0
	var total_gravity: Vector3
	var total_linear_damp: float = 0.0
	var total_angular_damp: float = 0.0
	var detach_after_samples: int = 0
	var detached: bool = false
	var gravity_before_detach: Vector3
	var gravity_after_detach: Vector3
	var linear_damp_before_detach: float = 0.0
	var linear_damp_after_detach: float = 0.0

	func _integrate_forces(state: PhysicsDirectBodyState3D) -> void:
		sample_count += 1
		total_gravity = state.get_total_gravity()
		total_linear_damp = state.get_total_linear_damp()
		total_angular_damp = state.get_total_angular_damp()
		if not detached and detach_after_samples > 0 and sample_count >= detach_after_samples:
			gravity_before_detach = total_gravity
			linear_damp_before_detach = total_linear_damp
			PhysicsServer3D.body_set_space(get_rid(), RID())
			gravity_after_detach = state.get_total_gravity()
			linear_damp_after_detach = state.get_total_linear_damp()
			detached = true


class ContactProbeBody:
	extends RigidBody3D

	var contact_position: Vector3
	var contact_normal: Vector3
	var found_contact: bool = false

	func _integrate_forces(state: PhysicsDirectBodyState3D) -> void:
		if state.get_contact_count() <= 0:
			return
		contact_position = state.get_contact_local_position(0)
		contact_normal = state.get_contact_local_normal(0)
		found_contact = true


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	await _test_cylinder_centering()
	await _test_contact_world_coordinates()
	await _test_area_override_modes()
	await _test_default_area_and_body_damp_modes()
	await _test_callback_detach_and_runtime_state_reset()
	await _test_live_collision_exception_refresh()
	await _test_motion_exclusion_lists()
	await _test_motion_recovery_result_contract()
	await _test_character_motion_recovery()

	if failures == 0:
		print("RESULT: PASS - review regression checks passed")
	else:
		print("RESULT: FAIL - ", failures, " review regression check(s) failed")
	quit(1 if failures > 0 else 0)


func _assert_result(condition: bool, message: String) -> void:
	if condition:
		print("PASS: ", message)
	else:
		failures += 1
		print("FAIL: ", message)


func _is_close(value: float, expected: float, tolerance: float = 0.05) -> bool:
	return absf(value - expected) <= tolerance


func _clear_scene() -> void:
	for child in root.get_children():
		child.queue_free()
	await process_frame
	await physics_frame
	await physics_frame


func _make_box_shape(size: Vector3) -> CollisionShape3D:
	var collision := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	collision.shape = shape
	return collision


func _make_sphere_shape(radius: float) -> CollisionShape3D:
	var collision := CollisionShape3D.new()
	var shape := SphereShape3D.new()
	shape.radius = radius
	collision.shape = shape
	return collision


func _make_cylinder_shape(radius: float, height: float) -> CollisionShape3D:
	var collision := CollisionShape3D.new()
	var shape := CylinderShape3D.new()
	shape.radius = radius
	shape.height = height
	collision.shape = shape
	return collision


func _make_platform(position: Vector3 = Vector3.ZERO) -> StaticBody3D:
	var platform := StaticBody3D.new()
	platform.position = position
	platform.add_child(_make_box_shape(Vector3(4, 0.5, 4)))
	root.add_child(platform)
	return platform


func _make_probe(position: Vector3) -> StateProbeBody:
	var body := StateProbeBody.new()
	body.position = position
	body.can_sleep = false
	body.add_child(_make_sphere_shape(0.25))
	root.add_child(body)
	return body


func _test_cylinder_centering() -> void:
	await _clear_scene()
	var target := StaticBody3D.new()
	target.add_child(_make_cylinder_shape(0.75, 2.0))
	root.add_child(target)
	await physics_frame
	await physics_frame

	var query := PhysicsRayQueryParameters3D.create(Vector3(0, 3, 0), Vector3(0, -3, 0))
	var hit: Dictionary = root.get_world_3d().direct_space_state.intersect_ray(query)
	_assert_result(not hit.is_empty() and hit.get("collider") == target, "ray query returns the cylinder collider object")
	var position: Vector3 = hit.get("position", Vector3())
	_assert_result(_is_close(position.y, 1.0), "cylinder collision hull is centered on its Godot transform")


func _test_contact_world_coordinates() -> void:
	await _clear_scene()
	var platform := _make_platform(Vector3(10, 0, 4))
	platform.name = "WorldContactPlatform"
	var body := ContactProbeBody.new()
	body.position = Vector3(10, 3, 4)
	body.contact_monitor = true
	body.max_contacts_reported = 4
	body.can_sleep = false
	body.add_child(_make_sphere_shape(0.5))
	root.add_child(body)
	for i in 120:
		await physics_frame

	_assert_result(body.found_contact, "direct body state reports a resting contact")
	_assert_result(_is_close(body.contact_position.x, 10.0, 0.15) and _is_close(body.contact_position.z, 4.0, 0.15), "contact position is reported in world coordinates")
	_assert_result(absf(body.contact_normal.y) > 0.9 and absf(body.contact_normal.x) < 0.1 and absf(body.contact_normal.z) < 0.1, "contact normal is reported in world coordinates")


func _make_override_area(mode: Area3D.SpaceOverride, position: Vector3 = Vector3.ZERO) -> Area3D:
	var area := Area3D.new()
	area.position = position
	area.gravity_space_override = mode
	area.gravity = 5.0
	area.gravity_direction = Vector3.DOWN
	area.linear_damp_space_override = Area3D.SPACE_OVERRIDE_COMBINE
	area.angular_damp_space_override = Area3D.SPACE_OVERRIDE_COMBINE
	area.linear_damp = 8.0
	area.angular_damp = 7.0
	area.add_child(_make_box_shape(Vector3(12, 12, 12)))
	root.add_child(area)
	return area


func _test_area_override_modes() -> void:
	await _clear_scene()
	_make_override_area(Area3D.SPACE_OVERRIDE_COMBINE_REPLACE)
	var combine_replace := _make_probe(Vector3.ZERO)
	combine_replace.linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	combine_replace.angular_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	combine_replace.linear_damp = 0.0
	combine_replace.angular_damp = 0.0
	for i in 8:
		await physics_frame

	_assert_result(_is_close(combine_replace.total_gravity.y, -5.0), "COMBINE_REPLACE excludes default gravity")
	_assert_result(_is_close(combine_replace.total_linear_damp, 0.0) and _is_close(combine_replace.total_angular_damp, 0.0), "body REPLACE damping overrides Area3D damping")

	await _clear_scene()
	_make_override_area(Area3D.SPACE_OVERRIDE_REPLACE_COMBINE)
	var replace_combine := _make_probe(Vector3.ZERO)
	var default_gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8)
	for i in 8:
		await physics_frame
	_assert_result(_is_close(replace_combine.total_gravity.y, -(default_gravity + 5.0)), "REPLACE_COMBINE includes default gravity")


func _test_default_area_and_body_damp_modes() -> void:
	await _clear_scene()
	var body := _make_probe(Vector3.ZERO)
	body.gravity_scale = 1.0
	body.linear_damp_mode = RigidBody3D.DAMP_MODE_COMBINE
	body.angular_damp_mode = RigidBody3D.DAMP_MODE_COMBINE
	body.linear_damp = 0.0
	body.angular_damp = 0.0
	for i in 4:
		await physics_frame

	var default_gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8)
	var default_linear_damp: float = ProjectSettings.get_setting("physics/3d/default_linear_damp", 0.1)
	var default_angular_damp: float = ProjectSettings.get_setting("physics/3d/default_angular_damp", 0.1)
	_assert_result(_is_close(body.total_gravity.length(), default_gravity), "space RID parameters configure default gravity")
	_assert_result(_is_close(body.total_linear_damp, default_linear_damp), "default linear damping is included")
	_assert_result(_is_close(body.total_angular_damp, default_angular_damp), "default angular damping is included")


func _test_callback_detach_and_runtime_state_reset() -> void:
	await _clear_scene()
	var area := _make_override_area(Area3D.SPACE_OVERRIDE_COMBINE_REPLACE)
	area.linear_damp_space_override = Area3D.SPACE_OVERRIDE_REPLACE
	area.linear_damp = 8.0
	var body := _make_probe(Vector3.ZERO)
	body.linear_damp_mode = RigidBody3D.DAMP_MODE_COMBINE
	body.linear_damp = 2.0
	body.detach_after_samples = 4
	for i in 10:
		await physics_frame

	_assert_result(body.detached, "force-integration callback can detach its body safely")
	_assert_result(_is_close(body.gravity_before_detach.y, -5.0), "callback observes the active Area3D gravity before detach")
	_assert_result(body.gravity_after_detach.is_zero_approx(), "detaching a body clears stale runtime gravity")
	_assert_result(_is_close(body.linear_damp_before_detach, 10.0) and _is_close(body.linear_damp_after_detach, 2.0), "detaching a body clears stale runtime damping")


func _test_live_collision_exception_refresh() -> void:
	await _clear_scene()
	var platform := _make_platform()
	var released := RigidBody3D.new()
	released.position = Vector3(0, 2, 0)
	released.can_sleep = false
	released.add_child(_make_sphere_shape(0.5))
	root.add_child(released)
	for i in 90:
		await physics_frame
	released.add_collision_exception_with(platform)
	for i in 60:
		await physics_frame
	_assert_result(released.position.y < -1.0, "adding an exception refreshes an existing contact")

	await _clear_scene()
	platform = _make_platform()
	var restored := RigidBody3D.new()
	restored.position = Vector3(0, 0.7, 0)
	restored.can_sleep = false
	restored.add_child(_make_sphere_shape(0.5))
	root.add_child(restored)
	restored.add_collision_exception_with(platform)
	await physics_frame
	restored.remove_collision_exception_with(platform)
	for i in 90:
		await physics_frame
	_assert_result(restored.position.y > 0.6, "removing an exception refreshes an overlapping pair")


func _test_character_motion_recovery() -> void:
	await _clear_scene()
	_make_platform(Vector3(0, 0, 0))
	var character := CharacterBody3D.new()
	character.position = Vector3(0, 0.74, 0)
	character.add_child(_make_box_shape(Vector3(1, 1, 1)))
	root.add_child(character)
	await physics_frame
	var start_x: float = character.position.x
	for i in 90:
		character.velocity = Vector3(2, -0.5, 0)
		character.move_and_slide()
		await physics_frame
	_assert_result(character.position.x - start_x > 1.0, "grounded CharacterBody3D recovers and moves along the floor")


func _test_motion_exclusion_lists() -> void:
	await _clear_scene()
	var wall := StaticBody3D.new()
	wall.add_child(_make_box_shape(Vector3(0.5, 4, 4)))
	root.add_child(wall)

	var body := CharacterBody3D.new()
	body.position = Vector3(-2, 0, 0)
	body.add_child(_make_box_shape(Vector3.ONE))
	root.add_child(body)
	await physics_frame

	var parameters := PhysicsTestMotionParameters3D.new()
	parameters.from = body.global_transform
	parameters.motion = Vector3(4, 0, 0)
	var baseline_result := PhysicsTestMotionResult3D.new()
	var baseline_collision: bool = PhysicsServer3D.body_test_motion(body.get_rid(), parameters, baseline_result)
	_assert_result(baseline_collision, "body_test_motion detects a non-excluded collider")

	parameters.exclude_bodies = [wall.get_rid()]
	var body_exclusion_result := PhysicsTestMotionResult3D.new()
	var body_exclusion_collision: bool = PhysicsServer3D.body_test_motion(body.get_rid(), parameters, body_exclusion_result)
	_assert_result(not body_exclusion_collision and body_exclusion_result.get_travel().x > 3.9, "body_test_motion honors excluded body RIDs")

	parameters.exclude_bodies = []
	parameters.exclude_objects = [wall.get_instance_id()]
	var object_exclusion_result := PhysicsTestMotionResult3D.new()
	var object_exclusion_collision: bool = PhysicsServer3D.body_test_motion(body.get_rid(), parameters, object_exclusion_result)
	_assert_result(not object_exclusion_collision and object_exclusion_result.get_travel().x > 3.9, "body_test_motion honors excluded object IDs")


func _test_motion_recovery_result_contract() -> void:
	await _clear_scene()
	_make_platform()
	var wall := StaticBody3D.new()
	wall.position = Vector3(0, 2, 0)
	wall.add_child(_make_box_shape(Vector3(0.5, 4, 4)))
	root.add_child(wall)

	var body := CharacterBody3D.new()
	body.position = Vector3(0.2, 0.2, 0)
	body.add_child(_make_box_shape(Vector3(1, 1, 1)))
	root.add_child(body)
	await physics_frame

	var parameters := PhysicsTestMotionParameters3D.new()
	parameters.from = body.global_transform
	parameters.motion = Vector3.ZERO
	parameters.margin = 0.001
	parameters.max_collisions = 2
	parameters.recovery_as_collision = true
	var result := PhysicsTestMotionResult3D.new()
	var collided: bool = PhysicsServer3D.body_test_motion(body.get_rid(), parameters, result)
	var valid_normals: bool = result.get_collision_count() == 2
	for i in result.get_collision_count():
		valid_normals = valid_normals and result.get_collision_normal(i).is_normalized()
	_assert_result(collided and result.get_collision_count() == 2, "body_test_motion reports multiple recovery collisions")
	_assert_result(valid_normals and result.get_collision_depth() > 0.0, "recovery collisions include normalized normals and penetration depth")
	_assert_result(result.get_travel().x > 0.0 and result.get_travel().y > 0.0, "recovery travel depenetrates the body from a corner")
