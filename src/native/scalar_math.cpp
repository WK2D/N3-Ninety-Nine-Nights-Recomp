#include "scalar_math.h"
#include <bit>
#include <cstdint>
#pragma STDC FENV_ACCESS ON
namespace n3::native {
namespace {
ScalarResult FlushResult(float value) {
  const auto bits = std::bit_cast<uint32_t>(value);
  const bool subnormal = (bits & 0x7fffffffu) != 0 && (bits & 0x7f800000u) == 0;
  return {subnormal ? std::bit_cast<float>(bits & 0x80000000u) : value, subnormal};
}
}
ScalarResult AddScalar(double first, double second) {
  return FlushResult(static_cast<float>(first + second));
}
ScalarResult SubtractScalar(double first, double second) {
  const float narrowed = static_cast<float>(second);
  const float negated = std::bit_cast<float>(std::bit_cast<uint32_t>(narrowed) ^ 0x80000000u);
  return FlushResult(static_cast<float>(double(negated) + first));
}
}
