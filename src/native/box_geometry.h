#pragma once
#include "shape_bounds.h"
#include <array>
#include <cstdint>

namespace n3::native {
// Three row-major basis rows followed by an explicit world translation.
// No orthonormality assumption: scale and shear retain the original arithmetic.
struct AffineTransform3 {
  std::array<Vec3, 3> rows;
  Vec3 translation;
};
struct TransformedBox { AffineTransform3 transform; Vec3 half_extents; };
bool SupportsBoxGeometry(const TransformedBox& box);
// Products and each sum round to float32, in Z + Y + X order.
Bounds3 TransformedBoxBounds(const TransformedBox& box);
// Coordinates usually select -1/+1 corners, but the original accepts all signed
// 32-bit integers. Conversion to float32 happens before extent multiplication.
// Returns false without mutating output if inputs/scaled extents need fallback.
// Caller provides its rounding mode, disabled scalar flush and masked FP traps.
bool TryTransformBoxSample(const TransformedBox& box, std::array<int32_t, 3> coordinates, Vec3& output);
}
