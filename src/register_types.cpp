#include "register_types.hpp"

#include "misc/box3d_globals.hpp"
#include "misc/box3d_recording_3d.hpp"
#include "objects/box3d_physics_direct_body_state_3d.hpp"
#include "servers/box3d_physics_server_3d.hpp"
#include "spaces/box3d_physics_direct_space_state_3d.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/physics_server3d_manager.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

namespace {

Box3DPhysicsServer3D* _create_box3d_physics_server() {
	return memnew(Box3DPhysicsServer3D);
}

Box3DRecording* recording_singleton = nullptr;

} // namespace

void initialize_box3d_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		box3d_initialize();

		GDREGISTER_VIRTUAL_CLASS(Box3DPhysicsDirectBodyState3D);
		GDREGISTER_VIRTUAL_CLASS(Box3DPhysicsDirectSpaceState3D);
		GDREGISTER_VIRTUAL_CLASS(Box3DPhysicsServer3D);

		PhysicsServer3DManager::get_singleton()->register_server(
				"Box3D Physics (Extension)",
				callable_mp_static(&_create_box3d_physics_server));
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// SCENE rather than SERVERS: the godot::Engine singleton wrapper is not yet
		// retrievable at SERVERS init (Engine::get_singleton() hard-fails there), and
		// scripts can only call the recording bridge once the scene level exists anyway.
		// Registered unconditionally (not only when Box3D is the active server) so
		// scripts can safely Engine.has_singleton("Box3DRecording"); the methods
		// themselves fail gracefully when Box3D isn't the active server.
		GDREGISTER_CLASS(Box3DRecording);
		recording_singleton = memnew(Box3DRecording);
		Engine::get_singleton()->register_singleton("Box3DRecording", recording_singleton);
	}
}

void uninitialize_box3d_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (recording_singleton != nullptr) {
			Engine::get_singleton()->unregister_singleton("Box3DRecording");
			memdelete(recording_singleton);
			recording_singleton = nullptr;
		}
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		box3d_deinitialize();
	}
}

extern "C" {

GDExtensionBool GDE_EXPORT godot_box3d_main(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization* r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_box3d_module);
	init_obj.register_terminator(uninitialize_box3d_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SERVERS);

	return init_obj.init();
}
}
