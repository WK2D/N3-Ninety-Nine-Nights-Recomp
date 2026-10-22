#include "capped_accumulator.h"
#include <bit>
#include <cmath>
#include <limits>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
bool Supported(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
bool Finite(float value) {
  return (std::bit_cast<uint32_t>(value) & 0x7f800000u) != 0x7f800000u;
}
}
bool TryAccumulateCappedValue(CappedAccumulatorState& state,
    const CappedAccumulatorParameters& parameters, int32_t delta,
    bool track, CappedAccumulatorUpdate& update) {
  if (!Supported(parameters.percentage_unit) || !Supported(parameters.baseline)) return false;
  const auto rounded_delta = static_cast<float>(double(delta));
  const auto rounded_percentage = static_cast<float>(double(parameters.percentage_adjustment));
  const auto factor = static_cast<float>(std::fma(double(rounded_percentage),
      double(parameters.percentage_unit), double(parameters.baseline)));
  const auto scaled_delta = static_cast<float>(double(factor) * double(rounded_delta));
  if (!Finite(factor) || !Finite(scaled_delta) ||
      double(scaled_delta) < double(std::numeric_limits<int32_t>::min())) return false;
  const auto truncated = double(scaled_delta) >= double(std::numeric_limits<int32_t>::max())
      ? std::numeric_limits<int32_t>::max() : static_cast<int32_t>(double(scaled_delta));
  const auto bits = std::bit_cast<uint32_t>(truncated);
  const uint64_t value_sum = uint64_t(state.value) + bits;
  const auto value_bits = uint32_t(value_sum);
  const bool clamped = std::bit_cast<int32_t>(value_bits) > parameters.upper_cap;
  const uint64_t total_sum = uint64_t(state.tracked_total) + (track ? uint64_t(bits) : 0);
  const uint64_t count_sum = uint64_t(state.tracked_count) + (track ? 1 : 0);
  update = {rounded_delta, rounded_percentage, factor, scaled_delta, bits,
            value_sum, total_sum, count_sum, clamped};
  state.value = clamped ? std::bit_cast<uint32_t>(parameters.upper_cap) : value_bits;
  if (track) {
    state.tracked_total = uint32_t(total_sum);
    state.tracked_count = uint32_t(count_sum);
  }
  return true;
}
}
