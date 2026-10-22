#pragma once
#include "shape_bounds.h"

namespace n3::native {
struct OrientedCapsule { Vec3 center, axis; float radius, half_height; };
struct CapsuleProjection {
  Bounds3 negative_endpoint, positive_endpoint, bounds;
};
bool SupportsCapsuleProjection(const OrientedCapsule& capsule);
// Caller rounding, disabled scalar flush, masked traps and supported source
// values are required. Axis/negative dimensions are preserved without clamping.
CapsuleProjection ProjectCapsuleBounds(const OrientedCapsule& capsule);
// Float32 square/FMA stages followed by a double square root. The caller can
// retain this double result or round it to float32 for a stored sphere radius.
// Finite normal/zero extents are required; underflow/overflow are not corrected.
double BoxSphereRadius(Vec3 half_extents);
}
