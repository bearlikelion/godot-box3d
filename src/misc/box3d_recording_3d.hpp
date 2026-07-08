#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>

#include <box3d/id.h>

using namespace godot;

struct b3Recording;

// Engine singleton ("Box3DRecording") bridging Box3D's recording/replay API to scripts.
// Records the b3World behind a Godot space RID into a caller-invisible b3Recording
// buffer, saves it as a .b3rec file, and validates saved files by deterministic replay.
//
// Script-facing surface (mirrors what a PhysicsBackend-style facade expects):
//
//   start(space: RID, byte_capacity: int = 0) -> bool
//   stop() -> void
//   is_recording() -> bool
//   save(path: String) -> bool
//   discard() -> void
//   validate_file(path: String, worker_count: int = 1) -> bool
//   replay_file_diverged(path: String, worker_count: int = 1) -> bool
//
// Step-boundary note: b3World_StartRecording must be called at a step boundary. Script
// execution in Godot only ever happens between physics steps (Godot never interrupts
// PhysicsServer3D::step mid-flight to run GDScript), so calling start() from any script
// callback satisfies the requirement without deferral.
//
// Lifetime note: one recording session exists at a time, covering exactly one space.
// discard() (or a fresh start(), which resets the buffer) releases it. If the recorded
// space is freed mid-session the world id goes stale; stop()/discard() detect that via
// b3World_IsValid and drop the unusable buffer rather than touching the dead world.
// Prefer stop()+save() (or discard()) before freeing the recorded space.
//
// Threading note: all methods are main-thread only, like the physics server itself.
class Box3DRecording final : public Object {
	GDCLASS(Box3DRecording, Object)

public:
	static Box3DRecording* get_singleton() { return singleton; }

	Box3DRecording();

	~Box3DRecording() override;

	// Begins recording the world behind p_space. Returns false (with an error) if a
	// session is already active or the RID is not a live Box3D space.
	bool start(const RID& p_space, int32_t p_byte_capacity = 0);

	// Ends the active session, finalizing the buffer so it can be saved. Safe no-op when
	// idle. The buffer is kept until save()/discard()/the next start().
	void stop();

	bool is_recording() const { return active; }

	// Saves the finalized buffer to p_path (res:// and user:// are globalized first).
	// Implicitly stop()s an active session, mirroring the "record then save" flow.
	bool save(const String& p_path);

	// Ends any active session and frees the buffer without saving.
	void discard();

	// Loads a .b3rec and re-runs the engine over it, checking every embedded state hash.
	// Returns true when the replay reproduces the recording exactly. p_worker_count is
	// forwarded to the engine (reserved for multithreaded replay; 1 matches a serial
	// recording).
	bool validate_file(const String& p_path, int32_t p_worker_count = 1);

	// Steps a replay player over a .b3rec to completion and reports whether any state
	// hash diverged. Returns true on divergence OR on failure to load/open the file
	// (i.e. true always means "do not trust this replay"). Replaying with
	// p_worker_count > 1 re-partitions the constraint graph, turning the hash check
	// into a cross-thread determinism test.
	bool replay_file_diverged(const String& p_path, int32_t p_worker_count = 1);

protected:
	static void _bind_methods();

private:
	static Box3DRecording* singleton;

	String _globalize(const String& p_path) const;

	b3Recording* recording = nullptr;
	b3WorldId world_id = b3_nullWorldId;
	bool active = false;
	bool finalized = false;
};
