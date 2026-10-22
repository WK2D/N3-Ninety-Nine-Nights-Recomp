#include "native/direction_accumulation.h"
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

#pragma STDC FENV_ACCESS ON

using namespace n3::native;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool Same(Vec3 a, Vec3 b) {
  return std::bit_cast<uint32_t>(a.x) == std::bit_cast<uint32_t>(b.x) &&
      std::bit_cast<uint32_t>(a.y) == std::bit_cast<uint32_t>(b.y) &&
      std::bit_cast<uint32_t>(a.z) == std::bit_cast<uint32_t>(b.z);
}
bool Same(const DirectionAccumulationUpdate& a, const DirectionAccumulationUpdate& b) {
  return Same(a.previous, b.previous) && Same(a.direction, b.direction) && Same(a.products, b.products) &&
      Same(a.result, b.result) && a.used_override == b.used_override;
}
void CheckEdits() {
  DirectionAccumulationState state{{1, 2, 3}}; DirectionAccumulationUpdate update{};
  DirectionAccumulationParameters parameters{{4, 8, -12}, {-8, 4, 2}, 0.5, 0, 0};
  Require(TryAccumulateDirection(state, parameters, update) && Same(state.value, {3, 6, -3}) &&
          Same(update.products, {2, 4, -6}) && !update.used_override && Same(update.previous, {1, 2, 3}),
          "Default direction, separately rounded products or update details changed");
  parameters.squared_override_length = 1;
  Require(TryAccumulateDirection(state, parameters, update) && Same(state.value, {-1, 8, -2}) && update.used_override,
          "Positive override length must select its editable direction");
  parameters.lower_bound = 2;
  Require(TryAccumulateDirection(state, parameters, update) && !update.used_override && Same(state.value, {1, 12, -8}),
          "Editable lower comparison was ignored");
  parameters.squared_override_length = std::numeric_limits<float>::quiet_NaN();
  Require(TryAccumulateDirection(state, parameters, update) && update.used_override,
          "Unordered squared-length comparison must select the override");
  parameters.squared_override_length = std::numeric_limits<float>::infinity();
  parameters.default_direction = {std::numeric_limits<float>::quiet_NaN(), 0, 0};
  Require(TryAccumulateDirection(state, parameters, update) && update.used_override,
          "Infinite computed dot is valid and an unused default direction must not be inspected");
  parameters.default_direction = {0, 0, 0}; parameters.override_direction = {std::numeric_limits<float>::infinity(), 0, 0};
  parameters.squared_override_length = 0;
  Require(TryAccumulateDirection(state, parameters, update) && !update.used_override,
          "Unused override scalar sources must not be inspected by the scalar phase");
}
void CheckRejection() {
  const DirectionAccumulationState before{{1, 2, 3}};
  const DirectionAccumulationUpdate sentinel{{4, 5, 6}, {7, 8, 9}, {10, 11, 12}, {13, 14, 15}, true};
  DirectionAccumulationState state = before; DirectionAccumulationUpdate update = sentinel;
  DirectionAccumulationParameters parameters{{1, 1, 1}, {2, 2, 2}, 1, 1, 0};
  for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    parameters.override_direction.x = std::bit_cast<float>(bits);
    Require(!TryAccumulateDirection(state, parameters, update) && Same(state.value, before.value) && Same(update, sentinel),
            "Rejected selected direction partially published state or details");
    parameters.override_direction.x = 2; parameters.lower_bound = std::bit_cast<float>(bits);
    Require(!TryAccumulateDirection(state, parameters, update) && Same(state.value, before.value) && Same(update, sentinel),
            "Rejected comparison source partially published state or details");
    parameters.lower_bound = 0;
  }
  parameters.amount = std::numeric_limits<double>::infinity();
  Require(!TryAccumulateDirection(state, parameters, update) && Same(state.value, before.value) && Same(update, sentinel),
          "Exceptional scalar accepted");
  parameters.amount = std::numeric_limits<double>::max();
  Require(!TryAccumulateDirection(state, parameters, update) && Same(state.value, before.value) && Same(update, sentinel),
          "Nonfinite trial products must reject transactionally");
  parameters.amount = 1; parameters.override_direction = {std::numeric_limits<float>::max(), 2, 2};
  state.value.x = std::numeric_limits<float>::max(); const auto huge_before = state;
  Require(!TryAccumulateDirection(state, parameters, update) && Same(state.value, huge_before.value) && Same(update, sentinel),
          "Late result overflow must reject transactionally");
}
void CheckRounding() {
  for (auto rounding : {FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD}) {
    Require(std::fesetround(rounding) == 0, "Rounding mode unavailable");
    DirectionAccumulationState state{{-16777216, 0, 0}}; DirectionAccumulationUpdate update{};
    DirectionAccumulationParameters parameters{{1, 0, 0}, {1, 0, 0}, 16777217.0, 0, 0};
    Require(TryAccumulateDirection(state, parameters, update) &&
            update.products.x == (rounding == FE_UPWARD ? 16777218.0f : 16777216.0f) &&
            state.value.x == (rounding == FE_UPWARD ? 2.0f : 0.0f),
            "Product must round to float before addition");
    for (bool selected : {false, true}) {
      state.value = {-0.0f, -0.0f, -0.0f};
      parameters = {{0, 0, 0}, {0, 0, 0}, 1, selected ? 1.0f : 0.0f, 0};
      Require(TryAccumulateDirection(state, parameters, update) &&
              std::signbit(state.value.x) == (rounding == FE_DOWNWARD) &&
              std::signbit(state.value.y) == (rounding == FE_DOWNWARD) &&
              std::signbit(state.value.z) == (rounding == FE_DOWNWARD),
              "Signed-zero addition changed between direction branches");
    }
    state.value = {0, 0, 0}; parameters = {{1, 1, 1}, {1, 1, 1}, 0x1p-149, 1, 0};
    Require(TryAccumulateDirection(state, parameters, update) &&
            std::bit_cast<uint32_t>(state.value.x) == 1 && std::bit_cast<uint32_t>(state.value.z) == 1,
            "Generated scalar subnormals must survive with scalar flush disabled");
  }
}
int main() {
  const auto saved = std::fegetround();
  try {
    Require(std::fesetround(FE_TONEAREST) == 0, "Nearest rounding unavailable");
    CheckEdits(); CheckRejection(); CheckRounding();
    std::fesetround(saved);
    std::cout << "Editable selected directions, staged products, scalar underflow and transactional rejection passed\n";
  } catch (const std::exception& error) {
    std::fesetround(saved); std::cerr << error.what() << '\n'; return 1;
  }
}
