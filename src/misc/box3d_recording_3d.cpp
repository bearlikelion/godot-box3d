#include "box3d_recording_3d.hpp"

// The server header's register_virtuals instantiation requires the complete type; the
// header itself only receives a forward declaration via physics_direct_body_state3d.hpp.
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>

#include "../servers/box3d_physics_server_3d.hpp"
#include "../spaces/box3d_space_3d.hpp"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <box3d/box3d.h>

Box3DRecording* Box3DRecording::singleton = nullptr;

Box3DRecording::Box3DRecording() {
	singleton = this;
}

Box3DRecording::~Box3DRecording() {
	discard();
	if (singleton == this) {
		singleton = nullptr;
	}
}

void Box3DRecording::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "space", "byte_capacity"), &Box3DRecording::start, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("stop"), &Box3DRecording::stop);
	ClassDB::bind_method(D_METHOD("is_recording"), &Box3DRecording::is_recording);
	ClassDB::bind_method(D_METHOD("save", "path"), &Box3DRecording::save);
	ClassDB::bind_method(D_METHOD("discard"), &Box3DRecording::discard);
	ClassDB::bind_method(D_METHOD("validate_file", "path", "worker_count"), &Box3DRecording::validate_file, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("replay_file_diverged", "path", "worker_count"), &Box3DRecording::replay_file_diverged, DEFVAL(1));
}

String Box3DRecording::_globalize(const String& p_path) const {
	ProjectSettings* settings = ProjectSettings::get_singleton();
	if (settings != nullptr) {
		return settings->globalize_path(p_path);
	}
	return p_path;
}

bool Box3DRecording::start(const RID& p_space, int32_t p_byte_capacity) {
	ERR_FAIL_COND_V_MSG(active, false, "Box3D: a recording session is already active; stop() or discard() it before starting another.");

	Box3DPhysicsServer3D* server = Box3DPhysicsServer3D::get_singleton();
	ERR_FAIL_NULL_V_MSG(server, false, "Box3D: the Box3D physics server is not active; recording requires Box3D to be the selected physics engine.");

	Box3DSpace3D* space = server->get_space(p_space);
	ERR_FAIL_NULL_V_MSG(space, false, "Box3D: the given RID is not a live Box3D space.");

	if (recording == nullptr) {
		recording = b3CreateRecording(MAX(0, (int)p_byte_capacity));
		ERR_FAIL_NULL_V_MSG(recording, false, "Box3D: failed to allocate a recording buffer.");
	}

	// b3World_StartRecording snapshots the world as the seed and resets the buffer, so a
	// retained b3Recording is safely reused across sessions.
	world_id = space->get_world_id();
	b3World_StartRecording(world_id, recording);
	active = true;
	finalized = false;
	return true;
}

void Box3DRecording::stop() {
	if (!active) {
		return;
	}

	active = false;

	if (B3_IS_NON_NULL(world_id) && b3World_IsValid(world_id)) {
		// Writes the trailing geometry registry and backpatches the header; only after
		// this is the buffer a complete, saveable .b3rec.
		b3World_StopRecording(world_id);
		finalized = true;
	} else {
		// The recorded space was freed mid-session: the buffer can never be finalized,
		// so drop it rather than hand out a truncated recording.
		WARN_PRINT("Box3D: the recorded space was freed while recording; the session buffer is unusable and has been discarded.");
		b3DestroyRecording(recording);
		recording = nullptr;
		finalized = false;
	}

	world_id = b3_nullWorldId;
}

bool Box3DRecording::save(const String& p_path) {
	if (active) {
		stop();
	}

	ERR_FAIL_NULL_V_MSG(recording, false, "Box3D: no recording buffer to save; record a session first.");
	ERR_FAIL_COND_V_MSG(!finalized, false, "Box3D: the recording buffer was never finalized; record a session first.");

	const String global_path = _globalize(p_path);
	const CharString utf8 = global_path.utf8();

	const bool saved = b3SaveRecordingToFile(recording, utf8.get_data());
	if (!saved) {
		ERR_PRINT(vformat("Box3D: failed to save recording to '%s'.", global_path));
	}
	return saved;
}

void Box3DRecording::discard() {
	if (active) {
		// Detach the world from the buffer before freeing it; the world holds a pointer
		// to the recording while a session is active.
		stop();
	}

	if (recording != nullptr) {
		b3DestroyRecording(recording);
		recording = nullptr;
	}
	finalized = false;
	world_id = b3_nullWorldId;
}

bool Box3DRecording::validate_file(const String& p_path, int32_t p_worker_count) {
	const String global_path = _globalize(p_path);
	const CharString utf8 = global_path.utf8();

	b3Recording* loaded = b3LoadRecordingFromFile(utf8.get_data());
	ERR_FAIL_NULL_V_MSG(loaded, false, vformat("Box3D: failed to load recording '%s' (missing file or wrong format).", global_path));

	const uint8_t* data = b3Recording_GetData(loaded);
	const int size = b3Recording_GetSize(loaded);

	bool valid = false;
	if (data != nullptr && size > 0) {
		valid = b3ValidateReplay(data, size, MAX(1, (int)p_worker_count));
	}

	b3DestroyRecording(loaded);
	return valid;
}

bool Box3DRecording::replay_file_diverged(const String& p_path, int32_t p_worker_count) {
	const String global_path = _globalize(p_path);
	const CharString utf8 = global_path.utf8();

	b3Recording* loaded = b3LoadRecordingFromFile(utf8.get_data());
	if (loaded == nullptr) {
		ERR_PRINT(vformat("Box3D: failed to load recording '%s' (missing file or wrong format).", global_path));
		return true; // Unloadable == untrustworthy: report as diverged.
	}

	const uint8_t* data = b3Recording_GetData(loaded);
	const int size = b3Recording_GetSize(loaded);

	bool diverged = true;
	if (data != nullptr && size > 0) {
		b3RecPlayer* player = b3RecPlayer_Create(data, size, MAX(1, (int)p_worker_count));
		if (player != nullptr) {
			while (b3RecPlayer_StepFrame(player)) {
				// Step to end-of-recording; divergence accumulates in the player.
			}
			diverged = b3RecPlayer_HasDiverged(player);
			b3RecPlayer_Destroy(player);
		} else {
			ERR_PRINT(vformat("Box3D: failed to open a replay player over '%s'.", global_path));
		}
	}

	b3DestroyRecording(loaded);
	return diverged;
}
