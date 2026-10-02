#include "register_types.hpp"

#include "api/box3d_api.hpp"
#include "misc/box3d_globals.hpp"
#include "objects/box3d_physics_direct_body_state_3d.hpp"
#include "servers/box3d_physics_server_3d.hpp"
#include "spaces/box3d_physics_direct_space_state_3d.hpp"

#include "api/bindings/data_classes/data_classes.gen.hpp"

#include <godot_cpp/classes/physics_server3d_manager.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

namespace {

Box3DPhysicsServer3D* _create_box3d_physics_server() {
	return memnew(Box3DPhysicsServer3D);
}

} // namespace

void initialize_box3d_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		box3d_initialize();

		GDREGISTER_VIRTUAL_CLASS(Box3DPhysicsDirectBodyState3D);
		GDREGISTER_VIRTUAL_CLASS(Box3DPhysicsDirectSpaceState3D);
		GDREGISTER_VIRTUAL_CLASS(Box3DPhysicsServer3D);

		ClassDB::register_class<Box3DAPI>();

		register_data_classes();

		PhysicsServer3DManager::get_singleton()->register_server(
				"Box3D Physics",
				callable_mp_static(&_create_box3d_physics_server));
	}
}

void uninitialize_box3d_module(ModuleInitializationLevel p_level) {
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
