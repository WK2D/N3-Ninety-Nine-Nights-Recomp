#pragma once
#include "math_types.h"

namespace n3::native {
// The caller's scalar is double precision in a guest floating-point register.
// Each product rounds to float32 before addition; do not fuse the operations.
Vec3 AccumulateScaledVector(Vec3 accumulator, Vec3 direction, double amount);
// Returns false without changing output when inputs or the rounded ratio need
// the original exceptional-value path. FP status may change while testing ratio.
bool TryAccumulateVectorRatio(Vec3 accumulator, Vec3 direction,
                              double numerator, double denominator, Vec3& output);

// These engine channels have both current and cached storage. Keep the two
// copies synchronized when native code changes a channel. Semantic names such
// as position/rotation await identification of the surrounding game classes.
struct MirroredVector3 { Vec3 current, cached; };
struct MirroredVector4 { Vec4 current, cached; };
void SetMirroredVector(MirroredVector3& channel, Vec3 value);
void SetMirroredVector(MirroredVector4& channel, Vec4 value);
bool SupportsVectorScalar(double amount);
}
