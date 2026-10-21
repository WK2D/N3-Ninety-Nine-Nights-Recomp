#include "shape_bounds.h"
#include <bit>
#include <cstdint>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
// The original scalar instructions round a double intermediate to float32.
float Add(float a, float b) { return static_cast<float>(double(a) + double(b)); }
float Subtract(float a, float b) { return static_cast<float>(double(a) - double(b)); }
float Negate(float value) {
  return std::bit_cast<float>(std::bit_cast<uint32_t>(value) ^ 0x80000000u);
}
}
Bounds3 SphereBounds(Vec3 center, float radius) {
  return {{Subtract(center.x, radius), Subtract(center.y, radius), Subtract(center.z, radius)},
          {Add(center.x, radius), Add(center.y, radius), Add(center.z, radius)}};
}
Bounds3 SphereLocalBounds(float radius) {
  const auto negative = Negate(radius);
  return {{negative, negative, negative}, {radius, radius, radius}};
}
Bounds3 CapsuleLocalBounds(float radius, float half_height) {
  const auto negative = Negate(radius);
  // Keep -half_height - radius separate from -(half_height + radius): directed
  // rounding and signed-zero results need not match after algebraic rewriting.
  return {{negative, Subtract(Negate(half_height), radius), negative},
          {radius, Add(half_height, radius), radius}};
}
Bounds3 BoxLocalBounds(Vec3 half_extents) {
  return {{Negate(half_extents.x), Negate(half_extents.y), Negate(half_extents.z)}, half_extents};
}
SphereEnvelope CapsuleSphereEnvelope(Vec3 center, float radius, float half_height) {
  return {center, Add(half_height, radius)};
}
bool SupportsShapeValue(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
bool SupportsShapeValue(Vec3 value) {
  return SupportsShapeValue(value.x) && SupportsShapeValue(value.y) && SupportsShapeValue(value.z);
}
}
