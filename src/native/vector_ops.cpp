#include "vector_ops.h"
#include "shape_bounds.h"
#include <bit>
#include <cstdint>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
float Component(float current, float direction, double amount) {
  const float product = static_cast<float>(double(direction) * amount);
  return static_cast<float>(double(current) + double(product));
}
}
Vec3 AccumulateScaledVector(Vec3 accumulator, Vec3 direction, double amount) {
  return {Component(accumulator.x, direction.x, amount),
          Component(accumulator.y, direction.y, amount),
          Component(accumulator.z, direction.z, amount)};
}
bool TryAccumulateVectorRatio(Vec3 accumulator, Vec3 direction,
                              double numerator, double denominator, Vec3& output) {
  if (!SupportsShapeValue(accumulator) || !SupportsShapeValue(direction) ||
      !SupportsVectorScalar(numerator) || !SupportsVectorScalar(denominator) ||
      (std::bit_cast<uint64_t>(denominator) & 0x7fffffffffffffffull) == 0) return false;
  const float ratio = static_cast<float>(numerator / denominator);
  if (!SupportsShapeValue(ratio)) return false;
  const float z_product = static_cast<float>(double(direction.z) * double(ratio));
  output = {Component(accumulator.x, direction.x, double(ratio)),
            Component(accumulator.y, direction.y, double(ratio)),
            static_cast<float>(double(z_product) + double(accumulator.z))};
  return true;
}
void SetMirroredVector(MirroredVector3& channel, Vec3 value) {
  channel.cached = value;
  channel.current = value;
}
void SetMirroredVector(MirroredVector4& channel, Vec4 value) {
  channel.cached = value;
  channel.current = value;
}
bool SupportsVectorScalar(double amount) {
  const auto bits = std::bit_cast<uint64_t>(amount) & 0x7fffffffffffffffull;
  return bits < 0x7ff0000000000000ull && (bits == 0 || bits >= 0x0010000000000000ull);
}
}
