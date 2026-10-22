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
  }
}
int main() {
  const auto saved_rounding = std::fegetround();
  try {
    Require(std::fesetround(FE_TONEAREST) == 0, "Nearest rounding unavailable");
    CheckSelector(); CheckLimit(); CheckAdjustment(); CheckRounding();
    std::fesetround(saved_rounding);
    std::cout << "Editable table/value limits, staged integer rounding, ordered bounds and rejection passed\n";
  } catch (const std::exception& error) {
    std::fesetround(saved_rounding); std::cerr << error.what() << '\n'; return 1;
  }
}
