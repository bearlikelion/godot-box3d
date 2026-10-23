#pragma once

#include <box3d/box3d.h>
#include <godot_cpp/templates/local_vector.hpp>

// Read-only contact geometry in world space. The normal points from the target
// toward the query shape; separation is negative for penetration.
struct Box3DRecoveryContact {
	b3Vec3 point;
	b3Vec3 normal;
	float separation;
};

// Live fixtures already contain local shape transforms (including baked meshes).
// Supply the hypothetical query body transform without changing the live body.
void box3d_recovery_contacts(b3ShapeId p_query, b3Transform p_query_transform,
		b3ShapeId p_target, float p_margin, godot::LocalVector<Box3DRecoveryContact>& r_contacts);
