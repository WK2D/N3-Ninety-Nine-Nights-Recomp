#include "direction_accumulation.h"
#include "vector_ops.h"
#include <bit>
#include <cmath>
#include <cstdint>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
bool Supported(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
bool Supported(Vec3 value) { return Supported(value.x) && Supported(value.y) && Supported(value.z); }
bool Finite(Vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
}
bool TryAccumulateDirection(DirectionAccumulationState& state,
    const DirectionAccumulationParameters& parameters, DirectionAccumulationUpdate& update) {
  if (!Supported(state.value) || !SupportsVectorScalar(parameters.amount) || !Supported(parameters.lower_bound)) return false;
  const bool selected = std::isnan(parameters.squared_override_length) ||
      double(parameters.squared_override_length) > double(parameters.lower_bound);
  const auto direction = selected ? parameters.override_direction : parameters.default_direction;
  if (!Supported(direction)) return false;
  const Vec3 products{
      static_cast<float>(double(direction.x) * parameters.amount),
      static_cast<float>(double(direction.y) * parameters.amount),
      static_cast<float>(double(direction.z) * parameters.amount)};
  const auto previous = state.value;
  const Vec3 result = selected
      ? Vec3{static_cast<float>(double(products.x) + double(previous.x)),
             static_cast<float>(double(previous.y) + double(products.y)),
             static_cast<float>(double(previous.z) + double(products.z))}
      : Vec3{static_cast<float>(double(previous.x) + double(products.x)),
             static_cast<float>(double(previous.y) + double(products.y)),
             static_cast<float>(double(products.z) + double(previous.z))};
  if (!Finite(products) || !Finite(result)) return false;
  update = {previous, direction, products, result, selected};
  state.value = result;
  return true;
}
}
