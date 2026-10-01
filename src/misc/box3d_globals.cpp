#include "box3d_globals.hpp"

#include "box3d_cpu_topology.hpp"

#include <box3d/constants.h>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <algorithm>

using namespace godot;

namespace {
const char *WORKER_COUNT_SETTING = "physics/box3d/worker_count";
} // namespace

void box3d_initialize() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	if (!settings->has_setting(WORKER_COUNT_SETTING)) {
		settings->set_setting(WORKER_COUNT_SETTING, 0);
	}
	settings->set_initial_value(WORKER_COUNT_SETTING, 0);
	// Spaces capture the count at creation, so a change only applies after a restart.
	settings->set_restart_if_changed(WORKER_COUNT_SETTING, true);
	Dictionary info;
	info["name"] = WORKER_COUNT_SETTING;
	info["type"] = Variant::INT;
	info["hint"] = PROPERTY_HINT_RANGE;
	info["hint_string"] = "0,32,1";
	settings->add_property_info(info);
}

void box3d_deinitialize() {
	// No global Box3D shutdown is required.
}

int box3d_worker_count() {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
	return 1;
#else
	static const int count = []() {
		const int setting = (int)ProjectSettings::get_singleton()->get_setting_with_override(
				WORKER_COUNT_SETTING);
		return setting > 0 ? std::clamp(setting, 1, B3_MAX_WORKERS) : box3d_default_worker_count();
	}();
	return count;
#endif
}
