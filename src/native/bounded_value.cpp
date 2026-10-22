#include "bounded_value.h"
#include <bit>
#include <cmath>
#include <limits>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
bool SupportedSource(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
bool Finite(float value) { return (std::bit_cast<uint32_t>(value) & 0x7f800000u) != 0x7f800000u; }
float RoundSigned(int32_t value) { return static_cast<float>(double(value)); }
}
uint32_t SelectValueLimitChannel(uint32_t kind, uint32_t base_channel, uint32_t extra_channel) {
  if (kind != 1000) return base_channel;
  const uint32_t sum = base_channel + extra_channel;
  return std::bit_cast<int32_t>(sum) >= 9 ? 8 : sum;
}
bool TryComputeValueLimit(const ValueLimitParameters& parameters, ValueLimitResult& result) {
  if (!SupportedSource(parameters.percentage_unit) || !SupportedSource(parameters.baseline)) return false;
  const auto percentage = RoundSigned(parameters.percentage_adjustment);
  const auto factor = static_cast<float>(std::fma(double(percentage), double(parameters.percentage_unit),
                                               double(parameters.baseline)));
  const auto base = RoundSigned(parameters.base_value);
  const auto scaled = static_cast<float>(double(factor) * double(base));
  if (!Finite(factor) || !Finite(scaled) || double(scaled) < double(std::numeric_limits<int32_t>::min())) return false;
  const auto truncated = double(scaled) >= double(std::numeric_limits<int32_t>::max())
      ? std::numeric_limits<int32_t>::max() : static_cast<int32_t>(double(scaled));
  const auto bits = std::bit_cast<uint32_t>(truncated);
  result = {bits, uint64_t(bits) + uint64_t(parameters.additive_offset)};
  return true;
}
bool TryAdjustBoundedValue(BoundedValueState& state, int32_t delta, int32_t upper_limit,
                           float lower_bound, BoundedValueUpdate& update) {
  if (!SupportedSource(state.value) || !SupportedSource(lower_bound)) return false;
  const auto delta_value = RoundSigned(delta);
  const auto sum = static_cast<float>(double(delta_value) + double(state.value));
  if (!Finite(sum)) return false;
  const bool lower_clamped = double(sum) < double(lower_bound);
  const auto lower_result = lower_clamped ? lower_bound : sum;
  const auto upper_bound = RoundSigned(upper_limit);
  const bool upper_clamped = double(lower_result) > double(upper_bound);
  const auto next_value = upper_clamped ? upper_bound : lower_result;
  update = {sum, lower_result, upper_bound, lower_clamped, upper_clamped};
  state.value = next_value;
  return true;
}
}
