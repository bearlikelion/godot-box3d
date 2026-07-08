class_name Box3DDemoScene
extends Node3D

var _test_dropdown: OptionButton
var _current_test: Node3D
var _result_label: Label

func _ready() -> void:
	_add_camera()
	_add_light()
	_add_ui()

func _add_camera() -> void:
	var camera := Camera3D.new()
	camera.position = Vector3(0, 15, 30)
	camera.rotation_degrees = Vector3(-20, 0, 0)
	add_child(camera)

func _add_light() -> void:
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-50, -30, 0)
	light.shadow_enabled = true
	add_child(light)

func _add_ui() -> void:
	var canvas_layer := CanvasLayer.new()
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_top", 20)
	margin.add_theme_constant_override("margin_left", 20)
	
	var vbox := VBoxContainer.new()
	var hbox := HBoxContainer.new()
	_test_dropdown = OptionButton.new()
	_test_dropdown.custom_minimum_size = Vector2(200, 0)
	
	var run_button := Button.new()
	run_button.text = "Run Visual Test"
	run_button.pressed.connect(_on_run_pressed)
	
	hbox.add_child(_test_dropdown)
	hbox.add_child(run_button)
	vbox.add_child(hbox)
	
	_result_label = Label.new()
	_result_label.name = "ResultLabel"
	_result_label.add_theme_color_override("font_color", Color(1, 1, 0))
	vbox.add_child(_result_label)
	
	margin.add_child(vbox)
	canvas_layer.add_child(margin)
	add_child(canvas_layer)
	
	_populate_tests()

func _populate_tests() -> void:
	var dir := DirAccess.open("res://")
	if dir:
		dir.list_dir_begin()
		var file_name := dir.get_next()
		while file_name != "":
			if not dir.current_is_dir() and file_name.ends_with(".gd") and file_name != "demo_scene.gd":
				# Only visual tests can attach to a Node3D. The headless regression
				# suite extends SceneTree (run those with `godot --headless --script
				# <test>.gd`); offering them here would fail at set_script.
				var s := load("res://" + file_name) as Script
				if s and s.get_instance_base_type() == &"Node3D":
					_test_dropdown.add_item(file_name)
			file_name = dir.get_next()

func _on_run_pressed() -> void:
	if _test_dropdown.item_count == 0:
		return
	if is_instance_valid(_current_test):
		_current_test.queue_free()
		_result_label.text = ""
	var selected_file := _test_dropdown.get_item_text(_test_dropdown.selected)
	var script := load("res://" + selected_file) as Script
	if script:
		_current_test = Node3D.new()
		_current_test.set_script(script)
		add_child(_current_test)
