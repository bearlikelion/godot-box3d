#include "box3d_api.hpp"
#include "box3d_api_helpers.hpp"
#include "../misc/type_conversions.hpp"
#include "bindings/data_classes/data_classes.gen.hpp"

#include <box3d/collision.h>

Ref<Box3DPlaneSolverResult> Box3DAPI::solve_planes(Vector3 p_target_delta, Array r_planes) {
	Vector<b3CollisionPlane> _buf;
	_buf.resize(r_planes.size());
	for (int i = 0; i < r_planes.size(); i++) {
		Ref<Box3DCollisionPlane> ref = r_planes[i];
		ERR_FAIL_NULL_V(ref, Ref<Box3DPlaneSolverResult>());
		_buf.ptrw()[i] = ref->to_b3();
	}

	b3PlaneSolverResult result = b3SolvePlanes(godot_to_b3(p_target_delta), _buf.ptrw(), (int)_buf.size());

	// Write back modified planes
	for (int i = 0; i < _buf.size(); i++) {
		Ref<Box3DCollisionPlane> ref = r_planes[i];
		ERR_FAIL_NULL_V(ref, Ref<Box3DPlaneSolverResult>());
		ref->from_b3(_buf[i]);
	}

	return _from_b3_to_ref<b3PlaneSolverResult, Box3DPlaneSolverResult>(result);
}

Ref<Box3DDistanceOutput> Box3DAPI::shape_distance(const Ref<Box3DDistanceInput> &p_input, const Ref<Box3DSimplexCache> &p_cache) {
	ERR_FAIL_NULL_V(p_input, Ref<Box3DDistanceOutput>());
	ERR_FAIL_NULL_V(p_cache, Ref<Box3DDistanceOutput>());
	// Debug simplexes are optional (NULL, 0); the simplex cache is still in/out.
	return _from_b3_to_ref<b3DistanceOutput, Box3DDistanceOutput>(
			b3ShapeDistance(p_input->ptr(), p_cache->ptr(), nullptr, 0));
}
