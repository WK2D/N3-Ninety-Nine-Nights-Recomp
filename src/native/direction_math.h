#pragma once
#include "math_types.h"

namespace n3::native {
struct DirectionSquaredLength {
  Vec3 squares;
  float zero_plus_z, y_plus_x, value;
};
// Match the translated SSE4.1 DPPS 0xEF lane/reduction order:
// ((+0 + round(z*z)) + (round(y*y) + round(x*x))). Every stage rounds to
// float under the caller's rounding mode. Subnormal inputs become signed zero
// as under DAZ. Positive overflow, including an infinite dot, is supported.
// A mathematically tiny nonzero stage or a rounded subnormal rejects before
// publication, so accepted results do not depend on the host's FTZ/DAZ mode.
// The caller must mask FP exceptions and compile with rounding/contraction
// controls respected. Rejection leaves result unchanged; FP status may change.
bool TryDirectionSquaredLength(Vec3 input, DirectionSquaredLength& result);

struct DirectionNormalization {
  Vec4 value;
  DirectionSquaredLength length;
  bool zeroed;
  // These stages are computed only when zeroed is false. They expose the
  // original separate operations for guest register publication and inspection.
  float square_root, estimate, estimate_squared, half_length;
  float correction_product, correction_difference, correction;
  float estimate_correction, refined_estimate;
};
// Normalize XYZ and the supplied raw fourth lane together. The fourth lane
// does not contribute to the squared length. threshold is a loaded float (the
// original constant is FLT_MIN); its subnormal inputs are DAZ-flushed too.
// threshold >= length returns four positive zeros. Otherwise sqrt(float),
// 1/sqrt, correction products/subtraction/sign-bit negation and refinement all
// round separately; none is fused and no reciprocal-square-root approximation
// is used. Active stages require finite normal values or signed zero, and reject
// mathematically tiny nonzero results, overflow, and nonfinite lengths. The raw
// fourth lane is unrestricted on the zero branch, where it is not multiplied.
// Preconditions and transactional/status behavior match the dot helper above.
bool TryNormalizeDirection(Vec4 input, float threshold, DirectionNormalization& result);
}
