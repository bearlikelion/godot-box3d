#include "box3d_api_helpers.hpp"

#include "../spaces/box3d_space_3d.hpp"
#include "../spaces/box3d_physics_direct_space_state_3d.hpp"
#include "../objects/box3d_body_impl_3d.hpp"
#include "../joints/box3d_joint_impl_3d.hpp"
#include "../servers/box3d_physics_server_3d.hpp"

// RID lookup helpers for generated code
Box3DSpace3D* api_get_space(RID p_rid) {
	return Box3DPhysicsServer3D::get_singleton()->get_space_owner().get_or_null(p_rid);
}

Box3DBodyImpl3D* api_get_body(RID p_rid) {
	return Box3DPhysicsServer3D::get_singleton()->get_body_owner().get_or_null(p_rid);
}

Box3DJointImpl3D* api_get_joint(RID p_rid) {
	return Box3DPhysicsServer3D::get_singleton()->get_joint_owner().get_or_null(p_rid);
}
