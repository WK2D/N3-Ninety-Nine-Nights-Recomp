#include "native/bounded_value.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#pragma STDC FENV_ACCESS ON

using namespace n3::native;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
bool SameUpdate(const BoundedValueUpdate& first, const BoundedValueUpdate& second) {
  return std::bit_cast<uint32_t>(first.sum) == std::bit_cast<uint32_t>(second.sum) &&
      std::bit_cast<uint32_t>(first.lower_result) == std::bit_cast<uint32_t>(second.lower_result) &&
      std::bit_cast<uint32_t>(first.upper_bound) == std::bit_cast<uint32_t>(second.upper_bound) &&
      first.lower_clamped == second.lower_clamped && first.upper_clamped == second.upper_clamped;
}
bool SameState(const ValueLimitState& first, const ValueLimitState& second) {
  const auto& a = first.parameters; const auto& b = second.parameters;
  return a.base_value == b.base_value && a.percentage_adjustment == b.percentage_adjustment &&
      a.additive_offset == b.additive_offset &&
      std::bit_cast<uint32_t>(a.percentage_unit) == std::bit_cast<uint32_t>(b.percentage_unit) &&
      std::bit_cast<uint32_t>(a.baseline) == std::bit_cast<uint32_t>(b.baseline) &&
      std::bit_cast<uint32_t>(first.value) == std::bit_cast<uint32_t>(second.value);
}
bool SameUpdate(const ValueLimitModifierUpdate& first, const ValueLimitModifierUpdate& second) {
  return first.limit.truncated_bits == second.limit.truncated_bits && first.limit.value == second.limit.value &&
      std::bit_cast<uint32_t>(first.previous_value) == std::bit_cast<uint32_t>(second.previous_value) &&
      std::bit_cast<uint32_t>(first.upper_bound) == std::bit_cast<uint32_t>(second.upper_bound) &&
      first.upper_clamped == second.upper_clamped;
}
bool SameDelta(const PercentageDelta& first, const PercentageDelta& second) {
  return first.truncated_bits == second.truncated_bits &&
      std::bit_cast<uint32_t>(first.fraction) == std::bit_cast<uint32_t>(second.fraction) &&
      std::bit_cast<uint32_t>(first.scaled) == std::bit_cast<uint32_t>(second.scaled);
}
bool SameUpdate(const PercentageValueUpdate& first, const PercentageValueUpdate& second) {
  return first.limit.truncated_bits == second.limit.truncated_bits && first.limit.value == second.limit.value &&
      SameDelta(first.delta, second.delta) && SameUpdate(first.bounded, second.bounded);
}
bool SameResult(const ConditionalPercentageValueResult& first, const ConditionalPercentageValueResult& second) {
  return first.value == second.value && first.applied == second.applied && SameDelta(first.delta, second.delta);
}
void CheckSelector() {
  Require(SelectValueLimitChannel(1, 12, 999) == 12 &&
          SelectValueLimitChannel(0xffffffffu, 0x80000000u, 0x80000000u) == 0x80000000u,
          "Unrecognized kinds must leave the base channel unchanged");
  Require(SelectValueLimitChannel(1000, 3, 5) == 8 && SelectValueLimitChannel(1000, 8, 1) == 8 &&
          SelectValueLimitChannel(1000, 9, 0) == 8 && SelectValueLimitChannel(1000, 0x7fffffffu, 1) == 0x80000000u &&
          SelectValueLimitChannel(1000, 0xffffffffu, 1) == 0 &&
          SelectValueLimitChannel(1000, 0xffffffffu, 0) == 0xffffffffu &&
          SelectValueLimitChannel(1000, 0x80000000u, 0x80000009u) == 8,
          "Channel selection changed signed comparison or wrapping addition");
}
void CheckLimit() {
  ValueLimitParameters parameters{400, 25, 3, std::bit_cast<float>(0x3c23d70au), 1};
  ValueLimitResult result{};
  Require(TryComputeValueLimit(parameters, result) && result.truncated_bits == 500 && result.value == 503,
          "Selected base value, percentage or additive offset changed");
  parameters.base_value = 7;
  Require(TryComputeValueLimit(parameters, result) && result.truncated_bits == 8 && result.value == 11,
          "Positive fractional product must truncate toward zero");
  parameters.base_value = -7;
  Require(TryComputeValueLimit(parameters, result) && result.truncated_bits == 0xfffffff8u && result.value == 0xfffffffbu,
          "Negative product must truncate then zero-extend its bits");
  parameters.additive_offset = 0xffffffffu;
  Require(TryComputeValueLimit(parameters, result) && result.value == 0x1fffffff7ull,
          "Additive offset must retain the 64-bit carry");
  parameters = {std::numeric_limits<int32_t>::max(), 100, 0xffffffffu, std::bit_cast<float>(0x3c23d70au), 1};
  Require(TryComputeValueLimit(parameters, result) && result.truncated_bits == 0x7fffffffu && result.value == 0x17ffffffeull,
          "Positive saturation or zero-extended addition changed");
  parameters = {8, 2, 5, 0.25f, 2};
  Require(TryComputeValueLimit(parameters, result) && result.truncated_bits == 20 && result.value == 25,
          "Explicit scale and baseline edits were ignored");
  const ValueLimitResult sentinel{0x12345678u, 0x123456789abcdef0ull};
  parameters = {std::numeric_limits<int32_t>::min(), 100, 7, std::bit_cast<float>(0x3c23d70au), 1};
  result = sentinel;
  Require(!TryComputeValueLimit(parameters, result) && result.truncated_bits == sentinel.truncated_bits && result.value == sentinel.value,
          "Exceptional negative conversion must reject without publishing");
  parameters = {1, std::numeric_limits<int32_t>::max(), 0, std::numeric_limits<float>::max(), 1};
  Require(!TryComputeValueLimit(parameters, result) && result.truncated_bits == sentinel.truncated_bits && result.value == sentinel.value,
          "Overflowing factor must reject without publishing");
  for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    parameters = {10, 1, 0, std::bit_cast<float>(bits), 1};
    Require(!TryComputeValueLimit(parameters, result) && result.value == sentinel.value,
            "Exceptional percentage unit accepted");
    parameters.percentage_unit = 1; parameters.baseline = std::bit_cast<float>(bits);
    Require(!TryComputeValueLimit(parameters, result) && result.value == sentinel.value,
            "Exceptional baseline accepted");
  }
}
void CheckAdjustment() {
  BoundedValueState state{5}; BoundedValueUpdate update{};
  Require(TryAdjustBoundedValue(state, 3, 10, 0, update) && state.value == 8 && update.sum == 8 &&
          !update.lower_clamped && !update.upper_clamped, "Ordinary bounded addition changed");
  state.value = 9;
  Require(TryAdjustBoundedValue(state, 5, 10, 0, update) && state.value == 10 && update.sum == 14 &&
          update.lower_result == 14 && !update.lower_clamped && update.upper_clamped, "Upper comparison changed");
  state.value = 2;
  Require(TryAdjustBoundedValue(state, -5, 10, 0, update) && state.value == 0 && update.sum == -3 &&
          update.lower_result == 0 && update.lower_clamped && !update.upper_clamped, "Lower comparison changed");
  state.value = -2;
  Require(TryAdjustBoundedValue(state, 0, -5, 0, update) && state.value == -5 && update.sum == -2 &&
          update.lower_result == 0 && update.lower_clamped && update.upper_clamped,
          "Negative upper limit must win after lower clamping");
  state.value = 2;
  Require(TryAdjustBoundedValue(state, 1, 10, 4, update) && state.value == 4 && update.lower_clamped,
          "Explicit lower-bound edits were ignored");
  const BoundedValueUpdate sentinel{1, 2, 3, true, false};
  for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    state.value = std::bit_cast<float>(bits); update = sentinel;
    Require(!TryAdjustBoundedValue(state, 1, 10, 0, update) && std::bit_cast<uint32_t>(state.value) == bits && SameUpdate(update, sentinel),
            "Exceptional current value must reject without changing state or details");
    state.value = 2;
    Require(!TryAdjustBoundedValue(state, 1, 10, std::bit_cast<float>(bits), update) && state.value == 2 && SameUpdate(update, sentinel),
            "Exceptional lower bound must reject without changing state or details");
  }
  state.value = std::numeric_limits<float>::max(); update = sentinel;
  Require(TryAdjustBoundedValue(state, 0, 10, std::numeric_limits<float>::max(), update) && state.value == 10 &&
          update.sum == std::numeric_limits<float>::max() && !update.lower_clamped && update.upper_clamped,
          "Finite extremes must follow lower then upper comparisons");
}
void CheckModifier() {
  ValueLimitState state{{400, 25, 3, std::bit_cast<float>(0x3c23d70au), 1}, 650};
  ValueLimitModifierUpdate update{};
  Require(TrySetValueLimitModifier(state, 50, 0x12340001u, update) &&
          state.parameters.percentage_adjustment == 50 && state.parameters.additive_offset == 3 &&
          update.limit.value == 603 && update.previous_value == 650 && update.upper_bound == 603 &&
          update.upper_clamped && state.value == 603,
          "Percentage replacement must recompute the limit before upper clamping");
  state.value = 700;
  Require(TrySetValueLimitModifier(state, 50, 0x100u, update) &&
          state.parameters.percentage_adjustment == 50 && state.parameters.additive_offset == 50 &&
          update.limit.value == 650 && state.value == 650 && update.upper_clamped,
          "Zero mode low byte must select the offset despite nonzero upper bytes");
  state.value = -8;
  Require(TrySetValueLimitModifier(state, 0xffffff9cu, 0xffu, update) &&
          state.parameters.percentage_adjustment == -100 && state.parameters.additive_offset == 50 &&
          update.limit.value == 50 && state.value == -8 && !update.upper_clamped,
          "Modifier updates must preserve signed replacement bits and omit a lower clamp");
  state = {{-5, 0, 0, 0, 1}, -8};
  Require(TrySetValueLimitModifier(state, 0, 0, update) && update.limit.value == 0xfffffffbu &&
          update.upper_bound == -5 && state.value == -8 && !update.upper_clamped,
          "Values below a negative limit must remain unchanged");
  state.value = -2;
  Require(TrySetValueLimitModifier(state, 0, 0, update) && state.value == -5 && update.upper_clamped,
          "Upper-only modifier update must accept a negative limit");
  state = {{-7, 25, 0, std::bit_cast<float>(0x3c23d70au), 1}, -10};
  Require(TrySetValueLimitModifier(state, 0xffffffffu, 0, update) &&
          state.parameters.additive_offset == 0xffffffffu && update.limit.truncated_bits == 0xfffffff8u &&
          update.limit.value == 0x1fffffff7ull && update.upper_bound == -9 && state.value == -10 && !update.upper_clamped,
          "Modifier update must retain the return carry and clamp against its signed low word");
  state = {{8, 0, 0, 0, 1}, 2};
  Require(TrySetValueLimitModifier(state, 0x80000000u, 1, update) &&
          state.parameters.percentage_adjustment == std::numeric_limits<int32_t>::min() &&
          update.limit.value == 8 && state.value == 2, "Percentage replacement lost high input bits");
  const ValueLimitModifierUpdate sentinel{{0x12345678u, 0x123456789abcdef0ull}, -7, 13, true};
  for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    state = {{10, 0, 3, 0, 1}, std::bit_cast<float>(bits)}; update = sentinel;
    const auto before = state;
    Require(!TrySetValueLimitModifier(state, 7, 1, update) && SameState(state, before) && SameUpdate(update, sentinel),
            "Unsupported current value must not publish a modifier or result");
    state = {{10, 0, 3, std::bit_cast<float>(bits), 1}, 2};
    const auto invalid_parameters = state;
    Require(!TrySetValueLimitModifier(state, 7, 0, update) && SameState(state, invalid_parameters) && SameUpdate(update, sentinel),
            "Unsupported arithmetic must not publish an offset or result");
  }
  state = {{std::numeric_limits<int32_t>::min(), 0, 3, std::bit_cast<float>(0x3c23d70au), 1}, 2};
  const auto negative_before = state; update = sentinel;
  Require(!TrySetValueLimitModifier(state, 100, 1, update) && SameState(state, negative_before) && SameUpdate(update, sentinel),
          "Late exceptional conversion must leave the requested percentage unpublished");
  state = {{1, 0, 3, std::numeric_limits<float>::max(), 1}, 2};
  const auto overflow_before = state;
  Require(!TrySetValueLimitModifier(state, uint32_t(std::numeric_limits<int32_t>::max()), 1, update) &&
          SameState(state, overflow_before) && SameUpdate(update, sentinel),
          "Overflowing requested modifier must reject transactionally");
}
void CheckPercentage() {
  const auto unit = std::bit_cast<float>(0x3c23d70au);
  PercentageDelta delta{};
  Require(TryComputePercentageDelta(400, 25, unit, delta) && delta.truncated_bits == 100 &&
          delta.fraction == 0.25f && delta.scaled == 100, "Percentage delta must omit the limit baseline");
  Require(TryComputePercentageDelta(-7, 25, unit, delta) && delta.truncated_bits == 0xffffffffu && delta.scaled == -1.75f,
          "Negative fractional percentage delta must truncate toward zero");
  Require(TryComputePercentageDelta(12408317, 1976, unit, delta) && delta.truncated_bits == 245188352u &&
          std::bit_cast<uint32_t>(delta.scaled) == 0x4d69d470u,
          "Percentage products must round twice; a single rounding gives 245188336");
  Require(TryComputePercentageDelta(std::numeric_limits<int32_t>::max(), 200, unit, delta) &&
          delta.truncated_bits == 0x7fffffffu, "Positive percentage conversion must saturate");
  const PercentageDelta sentinel{0x12345678u, 7, -3}; delta = sentinel;
  Require(!TryComputePercentageDelta(std::numeric_limits<int32_t>::min(), 200, unit, delta) && SameDelta(delta, sentinel),
          "Negative exceptional percentage conversion must reject transactionally");
  Require(!TryComputePercentageDelta(1, std::numeric_limits<int32_t>::max(), std::numeric_limits<float>::max(), delta) &&
          SameDelta(delta, sentinel), "Overflowing percentage product must reject transactionally");
  for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u})
    Require(!TryComputePercentageDelta(10, 2, std::bit_cast<float>(bits), delta) && SameDelta(delta, sentinel),
            "Unsupported percentage unit accepted");

  ValueLimitState state{{400, 0, 0, unit, 1}, 100};
  PercentageValueUpdate update{};
  const auto parameters_before = state.parameters;
  Require(TryAdjustValueByPercentage(state, 25, 0, update) && state.value == 200 &&
          update.limit.value == 400 && update.delta.truncated_bits == 100 && update.bounded.sum == 200 &&
          !update.bounded.lower_clamped && !update.bounded.upper_clamped &&
          SameState(state, {parameters_before, 200}), "Percentage adjustment must update only the current value");
  state.value = 390;
  Require(TryAdjustValueByPercentage(state, 25, 0, update) && state.value == 400 &&
          update.bounded.sum == 490 && update.bounded.upper_clamped, "Percentage adjustment upper bound changed");
  state.value = 100;
  Require(TryAdjustValueByPercentage(state, -50, 0, update) && state.value == 0 &&
          update.delta.truncated_bits == 0xffffff38u && update.bounded.sum == -100 && update.bounded.lower_clamped,
          "Negative percentage adjustment must follow signed delta and lower comparison");
  state.value = 0;
  Require(TryAdjustValueByPercentage(state, 0, 10, update) && state.value == 10 && update.bounded.lower_clamped,
          "Percentage adjustment ignored an explicit lower-bound edit");
  state = {{-5, 0, 0, unit, 1}, -2};
  Require(TryAdjustValueByPercentage(state, 25, 0, update) && state.value == -5 &&
          update.delta.truncated_bits == 0xffffffffu && update.bounded.lower_clamped && update.bounded.upper_clamped,
          "Percentage adjustment must preserve lower-then-negative-upper behavior");
  state = {{-7, 25, 0xffffffffu, unit, 1}, -5};
  Require(TryAdjustValueByPercentage(state, 100, 0, update) && update.limit.value == 0x1fffffff7ull &&
          update.delta.truncated_bits == 0xfffffff7u && update.bounded.upper_bound == -9 && state.value == -9,
          "Percentage adjustment must use the signed low-word limit while retaining return carry");
  const PercentageValueUpdate update_sentinel{{0x12345678u, 0x123456789abcdef0ull}, sentinel, {1, 2, 3, true, false}};
  for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    state = {{400, 0, 0, unit, 1}, std::bit_cast<float>(bits)}; update = update_sentinel;
    const auto before = state;
    Require(!TryAdjustValueByPercentage(state, 25, 0, update) && SameState(state, before) && SameUpdate(update, update_sentinel),
            "Late unsupported current value published a percentage update");
    state.value = 10; const auto before_lower = state;
    Require(!TryAdjustValueByPercentage(state, 25, std::bit_cast<float>(bits), update) &&
            SameState(state, before_lower) && SameUpdate(update, update_sentinel),
            "Late unsupported lower bound published a percentage update");
  }
  state = {{400, 0, 0, 1, 1}, 10};
  const auto before_conversion = state;
  Require(!TryAdjustValueByPercentage(state, std::numeric_limits<int32_t>::min(), 0, update) &&
          SameState(state, before_conversion) && SameUpdate(update, update_sentinel),
          "Exceptional derived delta published a percentage update");
}
void CheckConditionalPercentage() {
  const auto unit = std::bit_cast<float>(0x3c23d70au);
  ConditionalPercentageValueResult result{};
  Require(TryAddConditionalPercentage(400, 1, 0, 25, unit, result) && result.applied && result.value == 500 &&
          result.delta.truncated_bits == 100, "First percentage-enable flag ignored");
  Require(TryAddConditionalPercentage(400, 0, 255, 25, unit, result) && result.applied && result.value == 500,
          "Second percentage-enable flag must accept any nonzero byte");
  Require(TryAddConditionalPercentage(400, 1, 1, -25, unit, result) && result.value == 0x10000012cull &&
          result.delta.truncated_bits == 0xffffff9cu, "Negative conditional delta must be zero-extended before addition");
  Require(TryAddConditionalPercentage(0xffffffffu, 1, 0, 100, unit, result) && result.value == 0x1fffffffeull,
          "Conditional percentage result lost the 64-bit carry");
  const ConditionalPercentageValueResult sentinel{0x123456789abcdef0ull, true, {0x12345678u, 7, -3}};
  for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    result = sentinel;
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(TryAddConditionalPercentage(0xffffffffu, 0, 0, std::numeric_limits<int32_t>::min(), std::bit_cast<float>(bits), result) &&
            result.value == 0xffffffffu && !result.applied && result.delta.truncated_bits == 0 &&
            std::fetestexcept(FE_ALL_EXCEPT) == 0, "Inactive conditional percentage inspected an unused arithmetic source");
    result = sentinel;
    Require(!TryAddConditionalPercentage(400, 1, 0, 25, std::bit_cast<float>(bits), result) && SameResult(result, sentinel),
            "Active unsupported conditional percentage must reject without publishing");
  }
  result = sentinel;
  Require(!TryAddConditionalPercentage(0x80000000u, 0, 1, 200, unit, result) && SameResult(result, sentinel),
          "Exceptional conditional conversion must reject transactionally");
}
void CheckRounding() {
  for (const auto rounding : {FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD}) {
    Require(std::fesetround(rounding) == 0, "Rounding mode unavailable");
    const auto rounded = rounding == FE_UPWARD ? 16777218u : 16777216u;
    ValueLimitResult result{};
    Require(TryComputeValueLimit({16777217, 0, 0, 0, 1}, result) && result.value == rounded,
            "Base integer must round to float32 before scaling");
    Require(TryComputeValueLimit({1, 16777217, 0, 1, 0}, result) && result.value == rounded,
            "Percentage integer must round before fused scaling");
    BoundedValueState state{-16777216}; BoundedValueUpdate update{};
    Require(TryAdjustBoundedValue(state, 16777217, 100, 0, update) &&
            update.sum == (rounding == FE_UPWARD ? 2.0f : 0.0f) && state.value == update.sum,
            "Delta must round before addition");
    state.value = -0.0f;
    Require(TryAdjustBoundedValue(state, 0, 0, 0, update) && state.value == 0 &&
            std::signbit(state.value) == (rounding == FE_DOWNWARD) && !update.lower_clamped && !update.upper_clamped,
            "Equal zero comparisons must preserve the arithmetic zero sign");
    state.value = 16777218;
    Require(TryAdjustBoundedValue(state, 0, 16777217, 0, update) && update.upper_bound == rounded &&
            state.value == float(rounded) && update.upper_clamped == (rounding != FE_UPWARD),
            "Upper integer conversion must honor caller rounding");
    ValueLimitState modifier_state{{16777217, 0, 0, 0, 1}, 16777218};
    ValueLimitModifierUpdate modifier_update{};
    Require(TrySetValueLimitModifier(modifier_state, 0, 0, modifier_update) &&
            modifier_update.limit.value == rounded && modifier_update.upper_bound == float(rounded) &&
            modifier_state.value == float(rounded) && modifier_update.upper_clamped == (rounding != FE_UPWARD),
            "Modifier recomputation must honor integer and upper-bound rounding");
    modifier_state = {{0, 0, 0, 0, 1}, -0.0f};
    Require(TrySetValueLimitModifier(modifier_state, 0, 0x100u, modifier_update) &&
            std::bit_cast<uint32_t>(modifier_state.value) == 0x80000000u &&
            std::bit_cast<uint32_t>(modifier_update.previous_value) == 0x80000000u && !modifier_update.upper_clamped,
            "Modifier recomputation must preserve negative zero without adding a zero delta");
    PercentageDelta delta{};
    Require(TryComputePercentageDelta(16777217, 1, 1, delta) && delta.truncated_bits == rounded &&
            TryComputePercentageDelta(1, 16777217, 1, delta) && delta.truncated_bits == rounded &&
            TryComputePercentageDelta(3, 1, 0.5f, delta) && delta.truncated_bits == 1,
            "Percentage delta integer rounding or truncation changed");
    ValueLimitState percentage_state{{16777217, 0, 0, 1, 1}, -16777216};
    PercentageValueUpdate percentage_update{};
    Require(TryAdjustValueByPercentage(percentage_state, 1, 0, percentage_update) &&
            percentage_update.limit.value == rounded && percentage_update.delta.truncated_bits == rounded &&
            percentage_state.value == (rounding == FE_UPWARD ? 2.0f : 0.0f),
            "Percentage delta and bounded update must honor each integer rounding stage");
    ConditionalPercentageValueResult conditional{};
    Require(TryAddConditionalPercentage(16777217, 1, 0, 1, 1, conditional) &&
            conditional.value == uint64_t(16777217) + rounded,
            "Conditional percentage must retain original base bits while rounding its signed arithmetic value");
  }
}
int main() {
  const auto saved_rounding = std::fegetround();
  try {
    Require(std::fesetround(FE_TONEAREST) == 0, "Nearest rounding unavailable");
    CheckSelector(); CheckLimit(); CheckAdjustment(); CheckModifier(); CheckPercentage(); CheckConditionalPercentage(); CheckRounding();
    std::fesetround(saved_rounding);
    std::cout << "Editable table/value limits, percentage updates, modifiers, staged rounding, bounds and rejection passed\n";
  } catch (const std::exception& error) {
    std::fesetround(saved_rounding); std::cerr << error.what() << '\n'; return 1;
  }
}
