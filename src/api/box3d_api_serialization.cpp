#include "box3d_api.hpp"
#include "box3d_api_helpers.hpp"
#include "../misc/type_conversions.hpp"
#include "bindings/data_classes/data_classes.gen.hpp"

#include <cstdlib>
#include <cstring>

#include <godot_cpp/templates/vector.hpp>

// Box3D allocates with its default aligned allocator (no b3SetAllocator call in
// this extension), so buffers that b3Destroy*/b3Free will release must be
// allocated with aligned_alloc + the same 16-byte alignment.
#define BOX3D_API_ALIGNMENT 16
#define BOX3D_API_ALIGN_UP(size) ((((size)-1) | (BOX3D_API_ALIGNMENT - 1)) + 1)

Dictionary Box3DAPI::create_mesh(const Ref<Box3DMeshDef> &p_def) {
	ERR_FAIL_NULL_V(p_def, Dictionary());
	const b3MeshDef* def = p_def->ptr();
	ERR_FAIL_NULL_V(def, Dictionary());

	// b3CreateMesh only records a degenerate index while the running count is
	// strictly below capacity, so the out-array needs triangleCount + 1 slots to
	// capture every degenerate triangle.
	int capacity = def->triangleCount + 1;
	godot::Vector<int> degenerate;
	degenerate.resize(capacity);

	b3MeshData* raw = b3CreateMesh(def, degenerate.ptrw(), capacity);
	Dictionary result;
	if (raw == nullptr) {
		return result;
	}

	Ref<Box3DMeshData> mesh;
	mesh.instantiate();
	mesh->take_ownership(raw);
	result["mesh"] = mesh;

	// The mesh records the total degenerate count; the out-array has one spare
	// slot, so every recorded index is present.
	int degenerate_count = raw->degenerateCount;
	PackedInt32Array degen;
	degen.resize(degenerate_count);
	for (int i = 0; i < degenerate_count; i++) {
		degen.set(i, degenerate[i]);
	}
	result["degenerate_triangles"] = degen;
	return result;
}

TypedArray<Box3DSurfaceMaterial> Box3DAPI::get_compound_materials(const Ref<Box3DCompoundData> &p_compound) {
	ERR_FAIL_NULL_V(p_compound, TypedArray<Box3DSurfaceMaterial>());
	const b3CompoundData* compound = p_compound->ptr();
	ERR_FAIL_NULL_V(compound, TypedArray<Box3DSurfaceMaterial>());

	const b3SurfaceMaterial* materials = b3GetCompoundMaterials(compound);
	if (materials == nullptr) {
		return TypedArray<Box3DSurfaceMaterial>();
	}

	TypedArray<Box3DSurfaceMaterial> result;
	for (int i = 0; i < compound->materialCount; i++) {
		Ref<Box3DSurfaceMaterial> ref;
		ref.instantiate();
		ref->from_b3(materials[i]);
		result.append(ref);
	}
	return result;
}

PackedByteArray Box3DAPI::convert_compound_to_bytes(const Ref<Box3DCompoundData> &p_compound) {
	ERR_FAIL_NULL_V(p_compound, PackedByteArray());
	b3CompoundData* compound = p_compound->ptr();
	ERR_FAIL_NULL_V(compound, PackedByteArray());

	int byte_count = compound->byteCount;
	if (byte_count <= 0) {
		return PackedByteArray();
	}

	// b3ConvertCompoundToBytes scrubs the internal tree.nodes pointer so the
	// bytes are deterministic, but it mutates the source compound in place
	// (leaving it unusable). To keep the caller's compound intact, scrub an
	// owned scratch copy instead, following the non-destructive interning
	// pattern used by b3RecInternCompound.
	void* buf = ::aligned_alloc(BOX3D_API_ALIGNMENT, BOX3D_API_ALIGN_UP(byte_count));
	if (buf == nullptr) {
		return PackedByteArray();
	}
	memcpy(buf, compound, byte_count);
	b3ConvertCompoundToBytes((b3CompoundData*)buf);

	PackedByteArray bytes;
	bytes.resize(byte_count);
	memcpy(bytes.ptrw(), buf, byte_count);

	::free(buf);
	return bytes;
}

Ref<Box3DCompoundData> Box3DAPI::convert_bytes_to_compound(const PackedByteArray &p_bytes) {
	if (p_bytes.size() <= 0) {
		return Ref<Box3DCompoundData>();
	}

	// b3ConvertBytesToCompound rewrites its input in place and returns a pointer
	// INTO that buffer, so the wrapper must own a copy. Allocate with the Box3D
	// default alignment so b3DestroyCompound (b3Free) can release it.
	void* buf = ::aligned_alloc(BOX3D_API_ALIGNMENT, BOX3D_API_ALIGN_UP(p_bytes.size()));
	if (buf == nullptr) {
		return Ref<Box3DCompoundData>();
	}
	memcpy(buf, p_bytes.ptr(), p_bytes.size());

	b3CompoundData* compound = b3ConvertBytesToCompound((uint8_t*)buf, p_bytes.size());
	if (compound == nullptr) {
		::free(buf);
		return Ref<Box3DCompoundData>();
	}

	Ref<Box3DCompoundData> ref;
	ref.instantiate();
	ref->take_ownership(compound);
	return ref;
}

Ref<Box3DHeightFieldData> Box3DAPI::shape_get_height_field(uint64_t p_shape_id) {
	const b3HeightFieldData* raw = b3Shape_GetHeightField(b3LoadShapeId(p_shape_id));
	if (raw == nullptr) {
		return Ref<Box3DHeightFieldData>();
	}
	// The shape aliases (not owns) the caller's blob and exposes it as const;
	// there is no b3Shape_SetHeightField, so this is read-back only. Return an
	// owned deep copy (the blob is self-contained at byteCount) so the caller
	// cannot dangle after the shape is destroyed.
	int byte_count = raw->byteCount;
	if (byte_count <= 0) {
		return Ref<Box3DHeightFieldData>();
	}
	void* buf = ::aligned_alloc(BOX3D_API_ALIGNMENT, BOX3D_API_ALIGN_UP(byte_count));
	if (buf == nullptr) {
		return Ref<Box3DHeightFieldData>();
	}
	memcpy(buf, raw, byte_count);

	Ref<Box3DHeightFieldData> ref;
	ref.instantiate();
	ref->take_ownership((b3HeightFieldData*)buf);
	return ref;
}
