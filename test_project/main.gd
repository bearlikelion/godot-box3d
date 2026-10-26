class_name DemoHub
extends Control

const DEMOS: Array[Dictionary] = [
	{"name": "Shapes", "path": "res://demos/shapes.tscn"},
	{"name": "Joints", "path": "res://demos/joints.tscn"},
	{"name": "Areas", "path": "res://demos/areas.tscn"},
	{"name": "Contacts", "path": "res://demos/contacts.tscn"},
	{"name": "Collision exceptions", "path": "res://demos/exceptions.tscn"},
	{"name": "Trimesh", "path": "res://demos/trimesh.tscn"},
	{"name": "Trimesh transforms", "path": "res://demos/trimesh_transform.tscn"},
	{"name": "Benchmark", "path": "res://demos/benchmark.tscn"},
]
const BACKEND_SETTING: String = "physics/3d/physics_engine"
## Dummy runs no simulation, so it is never a useful comparison.
const HIDDEN_BACKENDS: Array[String] = ["Dummy", "DEFAULT"]

@onready var _buttons: VBoxContainer = %Buttons
@onready var _backend_picker: OptionButton = %BackendPicker
@onready var _backend_note: Label = %BackendNote

var _backends: PackedStringArray = PackedStringArray()


func _ready() -> void:
	_populate_backends()
	_backend_picker.item_selected.connect(_on_backend_selected)

	for demo in DEMOS:
		var button: Button = Button.new()
		button.text = demo["name"]
		button.pressed.connect(_on_demo_pressed.bind(demo["path"]))
		_buttons.add_child(button)

	if _buttons.get_child_count() > 0:
		(_buttons.get_child(0) as Button).grab_focus()


func _populate_backends() -> void:
	var current: String = str(ProjectSettings.get_setting(BACKEND_SETTING, "DEFAULT"))
	for property in ProjectSettings.get_property_list():
		if property["name"] != BACKEND_SETTING:
			continue
		for backend in String(property["hint_string"]).split(","):
			if not HIDDEN_BACKENDS.has(backend):
				_backends.append(backend)
		break

	for i in _backends.size():
		_backend_picker.add_item(_backends[i], i)
		if _backends[i] == current:
			_backend_picker.select(i)

	_backend_note.text = "Switching relaunches: the physics server is built once at startup."
	if OS.has_feature("android"):
		_backend_picker.disabled = true
		_backend_note.text = "Android uses the physics backend selected before export."


# The physics server is created once at startup, so switching means relaunching.
func _on_backend_selected(index: int) -> void:
	var chosen: String = _backends[index]
	if chosen == str(ProjectSettings.get_setting(BACKEND_SETTING, "DEFAULT")):
		return
	ProjectSettings.set_setting(BACKEND_SETTING, chosen)
	ProjectSettings.save()
	OS.set_restart_on_exit(true, ["--path", ProjectSettings.globalize_path("res://")])
	get_tree().quit()


func _on_demo_pressed(path: String) -> void:
	get_tree().change_scene_to_file(path)
