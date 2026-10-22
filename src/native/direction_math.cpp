#include "direction_math.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
uint32_t Magnitude(float value) { return std::bit_cast<uint32_t>(value) & 0x7fffffffu; }
bool Finite(float value) { return Magnitude(value) < 0x7f800000u; }
float FlushInput(float value) {
  const auto bits = std::bit_cast<uint32_t>(value);
  return std::bit_cast<float>((bits & 0x7fffffffu) < 0x00800000u ? bits & 0x80000000u : bits);
}
bool SupportedExact(double value, bool positive_overflow = false) {
  const double magnitude = std::abs(value);
  if (magnitude != 0 && magnitude < double(std::numeric_limits<float>::min())) return false;
  return magnitude <= double(std::numeric_limits<float>::max()) || (positive_overflow && value > 0);
}
bool SupportedRounded(float value, bool positive_overflow = false) {
  const auto magnitude = Magnitude(value);
  return (magnitude == 0 || magnitude >= 0x00800000u) &&
      (magnitude < 0x7f800000u || (positive_overflow && std::bit_cast<uint32_t>(value) == 0x7f800000u));
}
bool Product(float left, float right, float& result, bool positive_overflow = false) {
  if (!SupportedExact(double(left) * double(right), positive_overflow)) return false;
  volatile float rounded = left * right;
  const float value = rounded;
  if (!SupportedRounded(value, positive_overflow)) return false;
  result = value;
  return true;
}
bool Sum(float left, float right, float& result, bool positive_overflow = false) {
  if (!SupportedExact(double(left) + double(right), positive_overflow)) return false;
  volatile float rounded = left + right;
  const float value = rounded;
  if (!SupportedRounded(value, positive_overflow)) return false;
  result = value;
  return true;
}
bool Difference(float left, float right, float& result) {
  if (!SupportedExact(double(left) - double(right))) return false;
  volatile float rounded = left - right;
  const float value = rounded;
  if (!SupportedRounded(value)) return false;
  result = value;
  return true;
}
float Negate(float value) { return std::bit_cast<float>(std::bit_cast<uint32_t>(value) ^ 0x80000000u); }
}
bool TryDirectionSquaredLength(Vec3 input, DirectionSquaredLength& result) {
  if (!Finite(input.x) || !Finite(input.y) || !Finite(input.z)) return false;
  const Vec3 source{FlushInput(input.x), FlushInput(input.y), FlushInput(input.z)};
  DirectionSquaredLength next;
  if (!Product(source.x, source.x, next.squares.x, true) ||
      !Product(source.y, source.y, next.squares.y, true) ||
      !Product(source.z, source.z, next.squares.z, true) ||
      !Sum(0.0f, next.squares.z, next.zero_plus_z, true) ||
      !Sum(next.squares.y, next.squares.x, next.y_plus_x, true) ||
      !Sum(next.zero_plus_z, next.y_plus_x, next.value, true)) return false;
  result = next;
  return true;
}
bool TryNormalizeDirection(Vec4 input, float threshold, DirectionNormalization& result) {
  if (!Finite(threshold)) return false;
  DirectionNormalization next{};
  if (!TryDirectionSquaredLength({input.x, input.y, input.z}, next.length)) return false;
  if (FlushInput(threshold) >= next.length.value) {
    next.zeroed = true;
    next.value = {0, 0, 0, 0};
    result = next;
    return true;
  }
  if (!Finite(next.length.value) || !(next.length.value > 0) || !Finite(input.w)) return false;
  const Vec4 source{FlushInput(input.x), FlushInput(input.y), FlushInput(input.z), FlushInput(input.w)};
  volatile float radicand = next.length.value;
  volatile float square_root = std::sqrt(radicand);
  next.square_root = square_root;
  if (!SupportedRounded(next.square_root) || !(next.square_root > 0) ||
      !SupportedExact(1.0 / double(next.square_root))) return false;
  volatile float estimate = 1.0f / next.square_root;
  next.estimate = estimate;
  if (!SupportedRounded(next.estimate) ||
      !Product(next.estimate, next.estimate, next.estimate_squared) ||
      !Product(next.length.value, 0.5f, next.half_length) ||
      !Product(next.half_length, next.estimate_squared, next.correction_product) ||
      !Difference(next.correction_product, 0.5f, next.correction_difference)) return false;
  next.correction = Negate(next.correction_difference);
  if (!Product(next.estimate, next.correction, next.estimate_correction) ||
      !Sum(next.estimate_correction, next.estimate, next.refined_estimate) ||
      !Product(source.x, next.refined_estimate, next.value.x) ||
      !Product(source.y, next.refined_estimate, next.value.y) ||
      !Product(source.z, next.refined_estimate, next.value.z) ||
      !Product(source.w, next.refined_estimate, next.value.w)) return false;
  result = next;
  return true;
}
}
