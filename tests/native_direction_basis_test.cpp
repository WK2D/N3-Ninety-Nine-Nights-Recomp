#include "native/direction_basis.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

#pragma STDC FENV_ACCESS ON

using namespace n3::native;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
constexpr float Float(uint32_t bits) { return std::bit_cast<float>(bits); }
uint32_t Bits(float value) { return std::bit_cast<uint32_t>(value); }
bool Same(Vec3 a, Vec3 b) { return Bits(a.x) == Bits(b.x) && Bits(a.y) == Bits(b.y) && Bits(a.z) == Bits(b.z); }
bool Equal(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool Near(Vec3 a, Vec3 b) {
  return std::abs(double(a.x) - double(b.x)) < 0.000003 &&
      std::abs(double(a.y) - double(b.y)) < 0.000003 && std::abs(double(a.z) - double(b.z)) < 0.000003;
}
template<typename T> auto Snapshot(const T& value) {
  std::array<std::byte, sizeof(T)> bytes;
  std::memcpy(bytes.data(), &value, sizeof(T)); return bytes;
}
template<typename T> bool Unchanged(const T& value, const std::array<std::byte, sizeof(T)>& before) {
  return std::memcmp(&value, before.data(), sizeof(T)) == 0;
}
DirectionBasisParameters Parameters() {
  return {1, 0, 0, 1, std::numeric_limits<float>::min(), true, {0, Float(0x7fc12345u), 1}, 0, 0, 0};
}
void CheckPhases() {
  const float invalid = Float(0x7fc12345u);
  DirectionBasisState state{{Float(0x7fc54321u), -0.0f, Float(0x80000001u)}};
  const auto before = Snapshot(state);
  auto parameters = Parameters();
  parameters.horizontal = parameters.vertical = 0;
  parameters.unit = invalid; parameters.basis = {invalid, invalid, invalid};
  parameters.input_w = parameters.basis_w = parameters.output_w = invalid;
  DirectionBasisUpdate details{};
  Require(TryPrepareDirectionInput(state, parameters, details) && Unchanged(state, before) &&
          details.branch == DirectionBasisBranch::Unchanged && details.input_normalization.zeroed &&
          details.post_normalization_length.value == 0 && Same(details.previous, state.value),
          "Zero input phase interpreted later operands or changed previous output bits");
  const auto before_details = Snapshot(details);
  parameters.lower_bound = parameters.threshold = invalid;
  Require(TryComposeDirectionBasis(parameters, details) && Unchanged(details, before_details) &&
          TryFinishDirectionBasis(state, details, invalid, invalid) && Unchanged(state, before),
          "Unchanged composition/finish must not inspect dependent scalar or basis operands");
  parameters = Parameters(); parameters.horizontal = parameters.vertical = 0;
  parameters.unit = parameters.basis_w = parameters.output_w = invalid;
  parameters.input_w = invalid; parameters.basis = {invalid, invalid, invalid};
  Require(TryUpdateDirectionBasis(state, parameters, details) && Unchanged(state, before) &&
          details.branch == DirectionBasisBranch::Unchanged,
          "Convenience update changed the no-write branch or required an unused operand");

  parameters = Parameters(); parameters.unit = invalid;
  parameters.basis = {invalid, invalid, invalid}; parameters.basis_w = parameters.output_w = invalid;
  Require(TryPrepareDirectionInput(state, parameters, details) && Unchanged(state, before) &&
          details.branch == DirectionBasisBranch::Default && details.post_normalization_length.value == 1,
          "Active input phase followed a deferred basis or unit value");
  const auto active_input = Snapshot(details);
  Require(!TryComposeDirectionBasis(parameters, details) && Unchanged(details, active_input) && Unchanged(state, before),
          "Rejected composition partially published staged input or output");
  parameters.basis_present = false; parameters.unit = -3;
  Require(TryComposeDirectionBasis(parameters, details) && Unchanged(state, before) &&
          details.branch == DirectionBasisBranch::Default && Equal(details.result, {0, 0, -3}) &&
          TryFinishDirectionBasis(state, details, invalid, invalid) && Equal(state.value, {0, 0, -3}),
          "Default branch must defer state publication and ignore final normalization operands");

  state.value = {7, 8, 9}; parameters = Parameters(); parameters.output_w = invalid;
  Require(TryPrepareDirectionBasis(state, parameters, details) && Same(state.value, {7, 8, 9}) &&
          details.branch == DirectionBasisBranch::Basis && Same(details.result, {7, 8, 9}) && details.combined.x == 1,
          "Basis preparation performed final normalization or prematurely published output");
  const auto prepared = Snapshot(details);
  const auto prepared_state = Snapshot(state);
  Require(!TryFinishDirectionBasis(state, details, parameters.threshold, invalid) &&
          Unchanged(state, prepared_state) && Unchanged(details, prepared),
          "Rejected final raw lane partially published state or exposed normalization details");
  Require(TryFinishDirectionBasis(state, details, parameters.threshold, -4) &&
          Equal(state.value, {1, 0, 0}) && details.output_normalization.value.w == -4,
          "Final raw lane supplied after combined stores was ignored");
  state.value = {7, 8, 9};
  Require(TryPrepareDirectionBasis(state, parameters, details) && TryFinishDirectionBasis(state, details, 2, invalid) &&
          details.output_normalization.zeroed && Same(state.value, {0, 0, 0}),
          "An altered final threshold must zero the result and avoid interpreting an unused raw W");
}
void CheckBasis(int rounding) {
  DirectionBasisState state{{7, 8, 9}}; DirectionBasisUpdate details{};
  auto parameters = Parameters();
  Require(TryUpdateDirectionBasis(state, parameters, details) && details.branch == DirectionBasisBranch::Basis &&
          details.input_normalization.length.value == 1 && details.basis_normalization.length.value == 1 &&
          details.cross_first.x == 1 && details.cross_second.x == 0 && details.cross_difference.x == -1 &&
          details.right.x == 1 && Equal(state.value, {1, 0, 0}) && Same(details.previous, {7, 8, 9}),
          "Basis normalization, up-cross-forward orientation or previous output changed");
  const uint32_t cross_zero = rounding == FE_DOWNWARD ? 0 : 0x80000000u;
  // Downward cross cancellation is negated to +0; the scalar sums therefore
  // add two +0 values. Other modes add opposite signed zeros and also yield +0.
  constexpr uint32_t sum_zero = 0;
  Require(Bits(details.right.y) == cross_zero && Bits(details.right.z) == cross_zero &&
          Bits(details.combined.y) == sum_zero && Bits(details.combined.z) == sum_zero &&
          Bits(state.value.y) == sum_zero && Bits(state.value.z) == sum_zero,
          "Cross sign-bit negation or scalar signed-zero combination order changed");
  constexpr std::array<Vec3, 4> bases{{{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}}};
  constexpr std::array<Vec3, 4> right{{{1, 0, 0}, {0, 0, -1}, {-1, 0, 0}, {0, 0, 1}}};
  for (size_t index = 0; index < bases.size(); ++index) for (bool vertical : {false, true}) {
    parameters = Parameters(); parameters.basis = bases[index];
    parameters.horizontal = vertical ? 0 : 1; parameters.vertical = vertical ? 1 : 0;
    Require(TryUpdateDirectionBasis(state, parameters, details) &&
            Equal(state.value, vertical ? bases[index] : right[index]),
            "Editable input axes did not follow the selected basis orientation");
  }
  parameters = Parameters(); parameters.horizontal = 3; parameters.vertical = 4; parameters.basis = {3, 0, 4};
  Require(TryUpdateDirectionBasis(state, parameters, details) && details.input_normalization.length.value == 25 &&
          details.basis_normalization.length.value == 25 && Near(details.right, {0.8f, 0, -0.6f}) &&
          Near(details.combined, {0.96f, 0, 0.28f}) && Near(state.value, {0.96f, 0, 0.28f}),
          "Two normalized input axes no longer compose lateral and forward basis directions");
  parameters = Parameters(); parameters.horizontal = parameters.vertical = 1; parameters.unit = 2;
  Require(TryUpdateDirectionBasis(state, parameters, details) && Near(state.value, {0.8944272f, 0, 0.4472136f}),
          "Editable up-axis unit did not affect the right basis before final normalization");
  parameters = Parameters(); parameters.unit = 0; parameters.output_w = Float(0x7fc12345u);
  Require(TryUpdateDirectionBasis(state, parameters, details) && details.output_normalization.zeroed &&
          Same(state.value, {0, 0, 0}), "A zero combined vector must use the final zero branch");
  parameters = Parameters(); parameters.basis = {0, Float(0x7f812345u), 0}; parameters.output_w = Float(0x7fc12345u);
  Require(TryUpdateDirectionBasis(state, parameters, details) && details.basis_normalization.zeroed &&
          details.output_normalization.zeroed && Same(state.value, {0, 0, 0}),
          "A zero selected basis must normalize to zero and ignore its overwritten Y value");
  for (uint32_t ignored : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    parameters = Parameters(); parameters.basis.y = Float(ignored);
    Require(TryUpdateDirectionBasis(state, parameters, details) && Equal(state.value, {1, 0, 0}) &&
            Bits(details.basis_input.y) == 0, "Selected basis Y must be overwritten rather than validated");
  }
  parameters = Parameters(); parameters.horizontal = 0; parameters.vertical = 1; parameters.unit = Float(0x80000001u);
  Require(TryUpdateDirectionBasis(state, parameters, details) && Bits(details.cross_first.x) == 0x80000000u &&
          Equal(state.value, {0, 0, 1}), "Vector cross inputs must DAZ-flush signed subnormal unit values");
  parameters = Parameters(); parameters.horizontal = 3; parameters.vertical = 4;
  parameters.unit = std::numeric_limits<float>::min();
  Require(TryUpdateDirectionBasis(state, parameters, details) && Bits(details.lateral_products.x) > 0 &&
          Bits(details.lateral_products.x) < 0x00800000u && Bits(details.combined.x) == Bits(details.lateral_products.x) &&
          Bits(state.value.x) == 0 && Near(state.value, {0, 0, 1}),
          "Scalar generated subnormals must survive composition before final vector DAZ flushing");
  parameters = Parameters(); parameters.basis_present = false; parameters.unit = Float(0x80000001u);
  Require(TryUpdateDirectionBasis(state, parameters, details) && Bits(state.value.z) == 0x80000001u,
          "Default scalar copy must preserve a finite subnormal unit with scalar flush disabled");
  parameters.lower_bound = -0.25f; parameters.unit = 3;
  Require(TryUpdateDirectionBasis(state, parameters, details) && Same(state.value, {-0.25f, -0.25f, 3}),
          "Default output must retain the loaded comparison lower bound and unit");
}
void CheckIntegerRounding(int rounding) {
  DirectionBasisState state{{0, 0, 0}}; DirectionBasisUpdate details{};
  auto parameters = Parameters(); parameters.basis_present = false;
  parameters.horizontal = 16777217; parameters.vertical = -16777217;
  Require(TryUpdateDirectionBasis(state, parameters, details) &&
          details.rounded_input.x == (rounding == FE_UPWARD ? 16777218.0f : 16777216.0f) &&
          details.rounded_input.z == (rounding == FE_DOWNWARD ? -16777218.0f : -16777216.0f),
          "Signed integer inputs must convert through double then round to float under caller rounding");
  parameters.horizontal = std::numeric_limits<int32_t>::max(); parameters.vertical = std::numeric_limits<int32_t>::min();
  Require(TryUpdateDirectionBasis(state, parameters, details) && Bits(details.rounded_input.z) == 0xcf000000u &&
          Bits(details.rounded_input.x) == (rounding == FE_TONEAREST || rounding == FE_UPWARD ? 0x4f000000u : 0x4effffffu),
          "Full signed input range or directed positive boundary rounding changed");
  Require(std::fesetround(FE_UPWARD) == 0, "Initial scalar rounding unavailable");
  volatile int32_t integer = 16777217;
  volatile float rounded = static_cast<float>(double(integer));
  const Vec4 explicit_input{rounded, 0, 0, 0};
  Require(std::fesetround(FE_DOWNWARD) == 0, "Vector rounding unavailable");
  Require(TryPrepareRoundedDirectionInput(state, explicit_input, 0, parameters.threshold, details) &&
          details.rounded_input.x == 16777218.0f && details.branch == DirectionBasisBranch::Default,
          "Already rounded scalar input was converted again under later vector rounding");
  Require(std::fesetround(rounding) == 0, "Caller rounding could not be restored");
}
void CheckRejection() {
  const DirectionBasisState initial{{7, 8, 9}};
  DirectionBasisUpdate sentinel{}; sentinel.previous = {11, 12, 13}; sentinel.result = {14, 15, 16};
  sentinel.cross_first = {17, 18, 19}; sentinel.output_normalization.estimate = 21;
  for (uint32_t bits : {0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    auto details = sentinel;
    const auto old_details = Snapshot(details);
    Require(!TryPrepareRoundedDirectionInput(initial, {1, 0, 0, 0}, Float(bits),
                std::numeric_limits<float>::min(), details) && Unchanged(details, old_details),
            "A nonfinite separate lower bound must reject finite explicit input without publishing details");
  }
  for (uint32_t which = 0; which < 11; ++which) {
    auto parameters = Parameters(); DirectionBasisState state = initial; auto details = sentinel;
    const auto old_state = Snapshot(state);
    const auto old_details = Snapshot(details);
    switch (which) {
      case 0: parameters.input_w = Float(0x7fc12345u); break;
      case 1: parameters.threshold = Float(0x7f800000u); break;
      case 2: parameters.lower_bound = Float(0x7fc12345u); break;
      case 3: parameters.basis.x = Float(0x7f812345u); break;
      case 4: parameters.basis.z = Float(0x7f800000u); break;
      case 5: parameters.basis_w = Float(0x7fc12345u); break;
      case 6: parameters.output_w = Float(0x7fc12345u); break;
      case 7: parameters.unit = Float(0x7fc12345u); break;
      case 8: parameters.basis = {Float(0x1fffffffu), 0, 0}; break;
      case 9: parameters.basis = {3, 0, 4}; parameters.unit = std::numeric_limits<float>::min(); break;
      case 10: parameters.unit = std::numeric_limits<float>::max(); break;
    }
    Require(!TryUpdateDirectionBasis(state, parameters, details) && Unchanged(state, old_state) && Unchanged(details, old_details),
            "An early or late rejected basis update partially published state/details");
  }
}
int main() {
  std::fenv_t saved;
  if (std::feholdexcept(&saved)) { std::cerr << "Cannot save/mask FP environment\n"; return 1; }
#if defined(__x86_64__) || defined(_M_X64)
  const auto controls = _mm_getcsr(); _mm_setcsr(controls & ~0x8040u);
#elif defined(__aarch64__) && !defined(_MSC_VER)
  uint64_t controls; __asm__ volatile("mrs %0, fpcr" : "=r"(controls));
  const uint64_t scalar = controls & ~((uint64_t{1} << 24) | (uint64_t{1} << 19));
  __asm__ volatile("msr fpcr, %0" : : "r"(scalar));
#endif
  uint32_t rounding_index = 0;
  try {
    for (int rounding : {FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD}) {
      Require(std::fesetround(rounding) == 0, "Rounding mode unavailable");
      CheckPhases(); CheckBasis(rounding); CheckIntegerRounding(rounding); CheckRejection();
      Require(std::fegetround() == rounding, "Basis model changed caller rounding"); ++rounding_index;
    }
    std::fesetenv(&saved);
    std::cout << "Phased editable basis direction, cross/combination stages, signed rounding, scalar underflow and rollback passed\n";
  } catch (const std::exception& error) {
    std::fesetenv(&saved);
    std::cerr << "Direction basis rounding case " << rounding_index << ": " << error.what() << '\n'; return 1;
  }
}
