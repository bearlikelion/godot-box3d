#include "box3d_concave_polygon_shape_impl_3d.hpp"

#include "../misc/type_conversions.hpp"

#include <box3d/collision.h>
#include <godot_cpp/templates/local_vector.hpp>

Box3DConcavePolygonShapeImpl3D::~Box3DConcavePolygonShapeImpl3D() {
	if (mesh != nullptr) {
		b3DestroyMesh(mesh);
		mesh = nullptr;
	}
}

Variant Box3DConcavePolygonShapeImpl3D::get_data() const {
	Dictionary data;
	data["faces"] = faces;
	data["backface_collision"] = false;
	return data;
}

void Box3DConcavePolygonShapeImpl3D::set_data(const Variant& p_data) {
	if (p_data.get_type() == Variant::DICTIONARY) {
		const Dictionary data = p_data;
		ERR_FAIL_COND(!data.has("faces"));
		faces = data["faces"];
	} else {
		ERR_FAIL_COND(p_data.get_type() != Variant::PACKED_VECTOR3_ARRAY);
		faces = p_data;
	}
	_rebuild_mesh();
}

void Box3DConcavePolygonShapeImpl3D::_rebuild_mesh() {
	if (mesh != nullptr) {
		b3DestroyMesh(mesh);
		mesh = nullptr;
	}

	const int face_count = faces.size();
	if (face_count < 3 || face_count % 3 != 0) {
		return;
	}

	const int triangle_count = face_count / 3;

	LocalVector<b3Vec3> vertices;
	vertices.resize(face_count);

	LocalVector<int32_t> indices;
	indices.resize(face_count);

	Vector3 min_point = faces[0];
	Vector3 max_point = faces[0];

	for (int i = 0; i < face_count; i++) {
		vertices[i] = godot_to_b3(faces[i]);
		indices[i] = i;
		min_point = min_point.min(faces[i]);
		max_point = max_point.max(faces[i]);
	}

	aabb = AABB(min_point, max_point - min_point);

	b3MeshDef def = {};
	def.vertices = vertices.ptr();
	def.indices = indices.ptr();
	def.materialIndices = nullptr;
	def.weldTolerance = 0.0f;
	def.vertexCount = face_count;
	def.triangleCount = triangle_count;
	def.weldVertices = false;
	def.useMedianSplit = false;
	def.identifyEdges = false;

	mesh = b3CreateMesh(&def, nullptr, 0);
}
