#include "native/capped_accumulator.h"
#include <array>
#include <bit>
#include <cfenv>
#include <iostream>
#include <limits>
#include <stdexcept>

#pragma STDC FENV_ACCESS ON

using namespace n3::native;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool Same(CappedAccumulatorState a, CappedAccumulatorState b) {
  return a.value == b.value && a.tracked_total == b.tracked_total && a.tracked_count == b.tracked_count;
}
bool Same(const CappedAccumulatorUpdate& a, const CappedAccumulatorUpdate& b) {
  return std::bit_cast<uint32_t>(a.rounded_delta) == std::bit_cast<uint32_t>(b.rounded_delta) &&
      std::bit_cast<uint32_t>(a.rounded_percentage) == std::bit_cast<uint32_t>(b.rounded_percentage) &&
      std::bit_cast<uint32_t>(a.factor) == std::bit_cast<uint32_t>(b.factor) &&
      std::bit_cast<uint32_t>(a.scaled_delta) == std::bit_cast<uint32_t>(b.scaled_delta) &&
      a.scaled_delta_bits == b.scaled_delta_bits && a.value_sum == b.value_sum &&
      a.tracked_total_sum == b.tracked_total_sum && a.tracked_count_sum == b.tracked_count_sum &&
      a.upper_clamped == b.upper_clamped;
}
void CheckEdits() {
  CappedAccumulatorParameters parameters{25, std::bit_cast<float>(0x3c23d70au), 1};
  CappedAccumulatorState state{100, 30, 2}; CappedAccumulatorUpdate update{};
  Require(TryAccumulateCappedValue(state, parameters, 8, true, update) &&
          Same(state, {110, 40, 3}) && update.factor == 1.25f && update.scaled_delta_bits == 10 &&
          update.value_sum == 110 && !update.upper_clamped, "Percentage scaling or accounting changed");
  parameters.percentage_adjustment = -50;
  Require(TryAccumulateCappedValue(state, parameters, 20, false, update) &&
          Same(state, {120, 40, 3}) && update.factor == 0.5f,
          "Editable percentage or disabled accounting was ignored");
  state.value = 999995;
  parameters = {0, 0, 1};
  Require(TryAccumulateCappedValue(state, parameters, 10, false, update) &&
          state.value == 999999 && update.value_sum == 1000005 && update.upper_clamped,
          "Signed upper cap changed");
  parameters.upper_cap = 12; state.value = 11;
  Require(TryAccumulateCappedValue(state, parameters, 2, false, update) && state.value == 12 && update.upper_clamped,
          "Explicit cap edit was ignored");
  parameters.upper_cap = -5; state.value = 0;
  Require(TryAccumulateCappedValue(state, parameters, 0, false, update) && state.value == 0xfffffffbu,
          "Negative upper caps must retain their signed comparison");
  parameters = {0, 0, 1}; state = {0, 1, 0xffffffffu};
  Require(TryAccumulateCappedValue(state, parameters, -1, true, update) &&
          Same(state, {0xffffffffu, 0, 0}) && update.scaled_delta_bits == 0xffffffffu &&
          update.tracked_total_sum == 0x100000000ull && update.tracked_count_sum == 0x100000000ull &&
          !update.upper_clamped, "Negative delta, absent lower clamp or counter carry changed");
  state = {0xffffffffu, 0xffffffffu, 0xffffffffu};
  Require(TryAccumulateCappedValue(state, parameters, 1, true, update) && Same(state, {0, 0, 0}) &&
          update.value_sum == 0x100000000ull && update.tracked_total_sum == 0x100000000ull,
          "Value addition must wrap before comparing and accounting must retain carry");
  state.value = 0x7fffffffu;
  Require(TryAccumulateCappedValue(state, parameters, 1, false, update) && state.value == 0x80000000u &&
          !update.upper_clamped, "Signed wrapped-negative sum must not receive the positive cap");
  parameters = {-50, std::bit_cast<float>(0x3c23d70au), 1}; state.value = 0;
  Require(TryAccumulateCappedValue(state, parameters, 3, false, update) && state.value == 1,
          "Positive fraction must truncate toward zero");
  Require(TryAccumulateCappedValue(state, parameters, -3, false, update) && state.value == 0,
          "Negative fraction must truncate before wrapping addition");
  parameters = {0, 0, 1}; state.value = 0;
  Require(TryAccumulateCappedValue(state, parameters, std::numeric_limits<int32_t>::max(), false, update) &&
          update.scaled_delta_bits == 0x7fffffffu, "Positive conversion saturation changed");
}
void CheckRejection() {
  const CappedAccumulatorState before{17, 29, 43};
  const CappedAccumulatorUpdate sentinel{1, 2, 3, 4, 5, 6, 7, 8, true};
  CappedAccumulatorState state = before; CappedAccumulatorUpdate update = sentinel;
  CappedAccumulatorParameters parameters{0, 0, 1};
  for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    parameters.percentage_unit = std::bit_cast<float>(bits);
    Require(!TryAccumulateCappedValue(state, parameters, 1, true, update) && Same(state, before) && Same(update, sentinel),
            "Invalid unit partially published state or details");
    parameters.percentage_unit = 0; parameters.baseline = std::bit_cast<float>(bits);
    Require(!TryAccumulateCappedValue(state, parameters, 1, false, update) && Same(state, before) && Same(update, sentinel),
            "Invalid baseline partially published state or details");
    parameters.baseline = 1;
  }
  parameters = {0, 0, 2};
  Require(!TryAccumulateCappedValue(state, parameters, std::numeric_limits<int32_t>::min(), true, update) &&
          Same(state, before) && Same(update, sentinel), "Negative conversion overflow must reject transactionally");
  parameters = {std::numeric_limits<int32_t>::max(), std::numeric_limits<float>::max(), 1};
  Require(!TryAccumulateCappedValue(state, parameters, 1, true, update) && Same(state, before) && Same(update, sentinel),
          "Nonfinite trial factor must reject transactionally");
}
void CheckRounding() {
  for (auto rounding : {FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD}) {
    Require(std::fesetround(rounding) == 0, "Rounding mode unavailable");
    const auto rounded = rounding == FE_UPWARD ? 16777218u : 16777216u;
    CappedAccumulatorState state{}; CappedAccumulatorUpdate update{};
    CappedAccumulatorParameters parameters{0, 0, 1, std::numeric_limits<int32_t>::max()};
    Require(TryAccumulateCappedValue(state, parameters, 16777217, false, update) &&
            update.scaled_delta_bits == rounded && state.value == rounded,
            "Delta must round before multiplication");
    state.value = 0; parameters = {16777217, 1, 0, std::numeric_limits<int32_t>::max()};
    Require(TryAccumulateCappedValue(state, parameters, 1, false, update) &&
            update.scaled_delta_bits == rounded && state.value == rounded,
            "Percentage integer must round before the fused factor stage");
  }
}
int main() {
  const auto saved = std::fegetround();
  try {
    Require(std::fesetround(FE_TONEAREST) == 0, "Nearest rounding unavailable");
    CheckEdits(); CheckRejection(); CheckRounding();
    std::fesetround(saved);
    std::cout << "Editable scaled accumulation, signed wrap/cap, accounting carry and rejection passed\n";
  } catch (const std::exception& error) {
    std::fesetround(saved); std::cerr << error.what() << '\n'; return 1;
  }
}
