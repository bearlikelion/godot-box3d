#pragma once

#include "../precompiled.hpp"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "bindings/data_classes/data_classes.gen.hpp"

// Static GDExtension class that exposes the full Box3D C API directly to Godot,
// independent of the PhysicsServer3D integration. Shares the same RID space so
// objects created by either layer are accessible from both.
class Box3DAPI : public godot::Object {
	GDCLASS(Box3DAPI, Object)

public:
	static void _bind_methods();
	// Auto-generated method declarations
#include "bindings/box3d_api_generated.gen.hpp"

	// Hand-written: user data (stored as Variant on RID owner, not passed to Box3D void*)
	static void world_set_user_data(const godot::RID &p_id, const godot::Variant &p_data);
	static godot::Variant world_get_user_data(const godot::RID &p_id);
	static void body_set_user_data(const godot::RID &p_id, const godot::Variant &p_data);
	static godot::Variant body_get_user_data(const godot::RID &p_id);
	static void joint_set_user_data(const godot::RID &p_id, const godot::Variant &p_data);
	static godot::Variant joint_get_user_data(const godot::RID &p_id);

	// Hand-written: output scalar pointers → Dictionary
	static godot::Dictionary joint_get_constraint_tuning(const godot::RID &p_id);
	static godot::Dictionary get_milliseconds_and_reset();

	// Hand-written: INOUT array (b3SolvePlanes modifies planes in place, returns result)
	static godot::Ref<Box3DPlaneSolverResult> solve_planes(godot::Vector3 p_target_delta, godot::Array r_planes);

	// Hand-written: distance query without the debug simplex array (passes NULL/0)
	static godot::Ref<Box3DDistanceOutput> shape_distance(const godot::Ref<Box3DDistanceInput> &p_input, const godot::Ref<Box3DSimplexCache> &p_cache);

	// Hand-written: dual-output mesh creation -> Dictionary { mesh, degenerate_triangles }
	static godot::Dictionary create_mesh(const godot::Ref<Box3DMeshDef> &p_def);

	// Hand-written: compound materials (const b3SurfaceMaterial* + materialCount) -> TypedArray
	static godot::TypedArray<Box3DSurfaceMaterial> get_compound_materials(const godot::Ref<Box3DCompoundData> &p_compound);

	// Hand-written: compound <-> bytes serialization with ownership/lifetime semantics
	static godot::PackedByteArray convert_compound_to_bytes(const godot::Ref<Box3DCompoundData> &p_compound);
	static godot::Ref<Box3DCompoundData> convert_bytes_to_compound(const godot::PackedByteArray &p_bytes);

	// Hand-written: const blob pointer return (no clone fn) -> non-owning view
	static godot::Ref<Box3DHeightFieldData> shape_get_height_field(uint64_t p_shape_id);

private:
	Box3DAPI();
};

// Forward declaration for generated registration
void bind_generated_methods();
