#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/rid.hpp>

using namespace godot;

class Box3DSpace3D;

// Base class for anything the server exposes as a physics object with an RID: bodies and
// areas. Mirrors JoltObjectImpl3D's minimal responsibility: RID, ObjectID (for
// _get_instance_id round-tripping to Godot nodes), collision layer/mask, and owning space.
class Box3DObjectImpl3D {
public:
	Box3DObjectImpl3D() = default;

	virtual ~Box3DObjectImpl3D() = default;

	RID get_rid() const { return rid; }

	void set_rid(const RID& p_rid) { rid = p_rid; }

	uint64_t get_instance_id() const { return instance_id; }

	void set_instance_id(uint64_t p_id) { instance_id = p_id; }

	// Native result structs expect the engine's raw Object pointer, not a godot-cpp
	// instance binding. This pointer must only be passed back to Godot, never dereferenced.
	Object* get_instance_unsafe() const {
		GodotObject* instance = internal::gdextension_interface_object_get_instance_from_id(instance_id);
		return reinterpret_cast<Object*>(instance);
	}

	uint32_t get_collision_layer() const { return collision_layer; }

	virtual void set_collision_layer(uint32_t p_layer) { collision_layer = p_layer; }

	uint32_t get_collision_mask() const { return collision_mask; }

	virtual void set_collision_mask(uint32_t p_mask) { collision_mask = p_mask; }

	bool is_ray_pickable() const { return ray_pickable; }

	void set_ray_pickable(bool p_enabled) { ray_pickable = p_enabled; }

	Box3DSpace3D* get_space() const { return space; }

	virtual void set_space(Box3DSpace3D* p_space) { space = p_space; }

protected:
	RID rid;
	uint64_t instance_id = 0;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	bool ray_pickable = true;
	Box3DSpace3D* space = nullptr;
};
