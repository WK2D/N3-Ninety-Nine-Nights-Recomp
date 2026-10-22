#include "direction_basis.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
bool Finite(float value) { return (std::bit_cast<uint32_t>(value) & 0x7fffffffu) < 0x7f800000u; }
bool Finite(Vec3 value) { return Finite(value.x) && Finite(value.y) && Finite(value.z); }
float FlushInput(float value) {
  const auto bits = std::bit_cast<uint32_t>(value);
  return std::bit_cast<float>((bits & 0x7fffffffu) < 0x00800000u ? bits & 0x80000000u : bits);
}
bool SupportedExact(double value) {
  const auto magnitude = std::abs(value);
  return magnitude <= double(std::numeric_limits<float>::max()) &&
      (magnitude == 0 || magnitude >= double(std::numeric_limits<float>::min()));
}
bool SupportedRounded(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
bool VectorProduct(float left, float right, float& output) {
  if (!SupportedExact(double(left) * double(right))) return false;
  volatile float rounded = left * right;
  if (!SupportedRounded(rounded)) return false;
  output = rounded;
  return true;
}
bool VectorDifference(float second, float first, float& difference, float& output) {
  if (!SupportedExact(double(second) - double(first))) return false;
  volatile float rounded = second - first;
  if (!SupportedRounded(rounded)) return false;
  difference = rounded;
  output = std::bit_cast<float>(std::bit_cast<uint32_t>(difference) ^ 0x80000000u);
  return true;
}
bool Cross(Vec3 up, Vec3 forward, DirectionBasisUpdate& update) {
  if (!Finite(up)) return false;
  up = {FlushInput(up.x), FlushInput(up.y), FlushInput(up.z)};
  forward = {FlushInput(forward.x), FlushInput(forward.y), FlushInput(forward.z)};
  return VectorProduct(up.y, forward.z, update.cross_first.x) &&
      VectorProduct(up.z, forward.x, update.cross_first.y) &&
      VectorProduct(up.x, forward.y, update.cross_first.z) &&
      VectorProduct(up.z, forward.y, update.cross_second.x) &&
      VectorProduct(up.x, forward.z, update.cross_second.y) &&
      VectorProduct(up.y, forward.x, update.cross_second.z) &&
      VectorDifference(update.cross_second.x, update.cross_first.x, update.cross_difference.x, update.right.x) &&
      VectorDifference(update.cross_second.y, update.cross_first.y, update.cross_difference.y, update.right.y) &&
      VectorDifference(update.cross_second.z, update.cross_first.z, update.cross_difference.z, update.right.z);
}
float RoundedInteger(int32_t value) {
  volatile float rounded = static_cast<float>(double(value));
  return rounded;
}
float ScalarProduct(float value, float amount) {
  volatile float rounded = static_cast<float>(double(value) * double(amount));
  return rounded;
}
float ScalarSum(float first, float second) {
  volatile float rounded = static_cast<float>(double(first) + double(second));
  return rounded;
}
Vec3 XYZ(Vec4 value) { return {value.x, value.y, value.z}; }
}
bool TryPrepareDirectionInput(const DirectionBasisState& state,
    const DirectionBasisParameters& parameters, DirectionBasisUpdate& update) {
  const Vec4 rounded_input{RoundedInteger(parameters.horizontal), parameters.lower_bound,
      RoundedInteger(parameters.vertical), parameters.input_w};
  return TryPrepareRoundedDirectionInput(state, rounded_input, parameters.lower_bound, parameters.threshold, update);
}
bool TryPrepareRoundedDirectionInput(const DirectionBasisState& state,
    Vec4 rounded_input, float lower_bound, float threshold, DirectionBasisUpdate& update) {
  if (!Finite(lower_bound)) return false;
  DirectionBasisUpdate next{};
  next.previous = state.value;
  next.result = state.value;
  next.rounded_input = rounded_input;
  if (!TryNormalizeDirection(next.rounded_input, threshold, next.input_normalization) ||
      !TryDirectionSquaredLength(XYZ(next.input_normalization.value), next.post_normalization_length)) return false;
  if (!(double(next.post_normalization_length.value) > double(lower_bound))) {
    next.branch = DirectionBasisBranch::Unchanged;
  } else next.branch = DirectionBasisBranch::Default;
  update = next;
  return true;
}
bool TryComposeDirectionBasis(const DirectionBasisParameters& parameters,
    DirectionBasisUpdate& preparation) {
  if (preparation.branch == DirectionBasisBranch::Unchanged) return true;
  if (preparation.branch != DirectionBasisBranch::Default) return false;
  auto next = preparation;
  if (!parameters.basis_present) {
    if (!Finite(parameters.unit)) return false;
    next.branch = DirectionBasisBranch::Default;
    next.result = {parameters.lower_bound, parameters.lower_bound, parameters.unit};
  } else {
    next.branch = DirectionBasisBranch::Basis;
    next.basis_input = {parameters.basis.x, parameters.lower_bound, parameters.basis.z, parameters.basis_w};
    if (!TryNormalizeDirection(next.basis_input, parameters.threshold, next.basis_normalization) ||
        !Cross({parameters.lower_bound, parameters.unit, parameters.lower_bound}, XYZ(next.basis_normalization.value), next)) return false;
    const auto input = next.input_normalization.value;
    const auto forward = next.basis_normalization.value;
    next.lateral_products = {ScalarProduct(next.right.x, input.x), ScalarProduct(next.right.y, input.x),
        ScalarProduct(next.right.z, input.x)};
    next.forward_products = {ScalarProduct(forward.x, input.z), ScalarProduct(forward.y, input.z),
        ScalarProduct(forward.z, input.z)};
    if (!Finite(next.lateral_products) || !Finite(next.forward_products)) return false;
    next.combined = {ScalarSum(next.lateral_products.x, next.forward_products.x),
        ScalarSum(next.forward_products.y, next.lateral_products.y),
        ScalarSum(next.lateral_products.z, next.forward_products.z)};
    if (!Finite(next.combined)) return false;
  }
  preparation = next;
  return true;
}
bool TryPrepareDirectionBasis(const DirectionBasisState& state,
    const DirectionBasisParameters& parameters, DirectionBasisUpdate& update) {
  DirectionBasisUpdate next;
  if (!TryPrepareDirectionInput(state, parameters, next) || !TryComposeDirectionBasis(parameters, next)) return false;
  update = next;
  return true;
}
bool TryFinishDirectionBasis(DirectionBasisState& state,
    DirectionBasisUpdate& preparation, float threshold, float output_w) {
  switch (preparation.branch) {
    case DirectionBasisBranch::Unchanged: return true;
    case DirectionBasisBranch::Default: state.value = preparation.result; return true;
    case DirectionBasisBranch::Basis: {
      auto next = preparation;
      if (!TryNormalizeDirection({next.combined.x, next.combined.y, next.combined.z, output_w},
              threshold, next.output_normalization)) return false;
      next.result = XYZ(next.output_normalization.value);
      preparation = next;
      state.value = next.result;
      return true;
    }
    default: return false;
  }
}
bool TryUpdateDirectionBasis(DirectionBasisState& state,
    const DirectionBasisParameters& parameters, DirectionBasisUpdate& update) {
  DirectionBasisUpdate next;
  auto staged_state = state;
  if (!TryPrepareDirectionBasis(state, parameters, next) ||
      !TryFinishDirectionBasis(staged_state, next, parameters.threshold, parameters.output_w)) return false;
  update = next;
  state = staged_state;
  return true;
}
}
