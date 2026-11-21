#include "box3d_globals.hpp"

#include "box3d_cpu_topology.hpp"

#include <box3d/constants.h>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <algorithm>

using namespace godot;

namespace {
const char *WORKER_COUNT_SETTING = "physics/box3d/worker_count";
const char *LENGTH_UNITS_SETTING = "physics/box3d/length_units_per_meter";

// Box3D reads these once at startup, so a change only applies after a restart.
void define_restart_setting(const char *p_name, const Variant &p_default, Variant::Type p_type, const char *p_hint) {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	if (!settings->has_setting(p_name)) {
		settings->set_setting(p_name, p_default);
	}
	settings->set_initial_value(p_name, p_default);
	settings->set_restart_if_changed(p_name, true);
	Dictionary info;
	info["name"] = p_name;
	info["type"] = p_type;
	info["hint"] = PROPERTY_HINT_RANGE;
	info["hint_string"] = p_hint;
	settings->add_property_info(info);
}
} // namespace

void box3d_initialize() {
	define_restart_setting(WORKER_COUNT_SETTING, 0, Variant::INT, "0,32,1");
	define_restart_setting(LENGTH_UNITS_SETTING, 1.0, Variant::FLOAT, "0.01,100,0.01,or_greater");
	const float length_units = (float)ProjectSettings::get_singleton()->get_setting_with_override(LENGTH_UNITS_SETTING);
	ERR_FAIL_COND_MSG(length_units <= 0.0f, "physics/box3d/length_units_per_meter must be greater than zero.");
	b3SetLengthUnitsPerMeter(length_units);
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
