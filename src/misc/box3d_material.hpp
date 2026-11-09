#pragma once

#include <box3d/box3d.h>

#include <algorithm>
#include <cmath>

// Godot encodes rough/absorbent as negative coefficients; Box3D requires nonnegative ones.
inline b3SurfaceMaterial box3d_surface_material(float p_friction, float p_bounce) {
	b3SurfaceMaterial material = b3DefaultSurfaceMaterial();
	material.friction = std::abs(p_friction);
	material.restitution = std::abs(p_bounce);
	material.userMaterialId = (p_friction < 0.0f ? 1u : 0u) | (p_bounce < 0.0f ? 2u : 0u);
	return material;
}

inline float box3d_mix_friction(float p_a, uint64_t p_flags_a, float p_b, uint64_t p_flags_b) {
	const float a = (p_flags_a & 1u) != 0 ? -p_a : p_a;
	const float b = (p_flags_b & 1u) != 0 ? -p_b : p_b;
	return std::abs(std::min(a, b));
}

inline float box3d_mix_restitution(float p_a, uint64_t p_flags_a, float p_b, uint64_t p_flags_b) {
	const float a = (p_flags_a & 2u) != 0 ? -p_a : p_a;
	const float b = (p_flags_b & 2u) != 0 ? -p_b : p_b;
	return std::clamp(a + b, 0.0f, 1.0f);
}
