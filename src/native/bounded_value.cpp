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
bool TryTruncate(float value, uint32_t& bits) {
  if (!Finite(value) || double(value) < double(std::numeric_limits<int32_t>::min())) return false;
  const auto truncated = double(value) >= double(std::numeric_limits<int32_t>::max())
      ? std::numeric_limits<int32_t>::max() : static_cast<int32_t>(double(value));
  bits = std::bit_cast<uint32_t>(truncated);
  return true;
}
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
  uint32_t bits;
  if (!Finite(factor) || !TryTruncate(scaled, bits)) return false;
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
bool TrySetValueLimitModifier(ValueLimitState& state, uint32_t replacement_bits,
                              uint32_t mode, ValueLimitModifierUpdate& update) {
  if (!SupportedSource(state.value)) return false;
  auto next = state;
  if ((mode & 0xffu) != 0) next.parameters.percentage_adjustment = std::bit_cast<int32_t>(replacement_bits);
  else next.parameters.additive_offset = replacement_bits;
  ValueLimitResult limit;
  if (!TryComputeValueLimit(next.parameters, limit)) return false;
  const auto upper_bound = RoundSigned(std::bit_cast<int32_t>(uint32_t(limit.value)));
  const bool upper_clamped = double(state.value) > double(upper_bound);
  if (upper_clamped) next.value = upper_bound;
  update = {limit, state.value, upper_bound, upper_clamped};
  state = next;
  return true;
}
bool TryComputePercentageDelta(int32_t base, int32_t percentage, float unit, PercentageDelta& delta) {
  if (!SupportedSource(unit)) return false;
  const auto percentage_value = RoundSigned(percentage);
  const auto base_value = RoundSigned(base);
  const auto fraction = static_cast<float>(double(percentage_value) * double(unit));
  const auto scaled = static_cast<float>(double(fraction) * double(base_value));
  uint32_t bits;
  if (!Finite(fraction) || !TryTruncate(scaled, bits)) return false;
  delta = {bits, fraction, scaled};
  return true;
}
bool TryAdjustValueByPercentage(ValueLimitState& state, int32_t percentage, float lower_bound,
                                PercentageValueUpdate& update) {
  ValueLimitResult first_limit, final_limit;
  PercentageDelta delta;
  BoundedValueState bounded_state{state.value};
  BoundedValueUpdate bounded_update;
  if (!TryComputeValueLimit(state.parameters, first_limit) ||
      !TryComputePercentageDelta(std::bit_cast<int32_t>(uint32_t(first_limit.value)), percentage,
                                 state.parameters.percentage_unit, delta) ||
      !TryComputeValueLimit(state.parameters, final_limit) ||
      !TryAdjustBoundedValue(bounded_state, std::bit_cast<int32_t>(delta.truncated_bits),
                             std::bit_cast<int32_t>(uint32_t(final_limit.value)), lower_bound, bounded_update)) return false;
  update = {final_limit, delta, bounded_update};
  state.value = bounded_state.value;
  return true;
}
bool TryAddConditionalPercentage(uint32_t base_bits, uint8_t first_flag, uint8_t second_flag,
                                 int32_t percentage, float unit, ConditionalPercentageValueResult& result) {
  if (first_flag == 0 && second_flag == 0) {
    result = {uint64_t(base_bits), false, {}};
    return true;
  }
  PercentageDelta delta;
  if (!TryComputePercentageDelta(std::bit_cast<int32_t>(base_bits), percentage, unit, delta)) return false;
  result = {uint64_t(base_bits) + uint64_t(delta.truncated_bits), true, delta};
  return true;
}
}
