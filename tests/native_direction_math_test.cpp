#include "native/direction_math.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h>
#endif

#pragma STDC FENV_ACCESS ON

using namespace n3::native;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
uint32_t Bits(float value) { return std::bit_cast<uint32_t>(value); }
constexpr float Float(uint32_t bits) { return std::bit_cast<float>(bits); }
bool Same(Vec3 a, Vec3 b) { return Bits(a.x) == Bits(b.x) && Bits(a.y) == Bits(b.y) && Bits(a.z) == Bits(b.z); }
bool Same(Vec4 a, Vec4 b) {
  return Same(Vec3{a.x, a.y, a.z}, Vec3{b.x, b.y, b.z}) && Bits(a.w) == Bits(b.w);
}
bool Same(const DirectionSquaredLength& a, const DirectionSquaredLength& b) {
  return Same(a.squares, b.squares) && Bits(a.zero_plus_z) == Bits(b.zero_plus_z) &&
      Bits(a.y_plus_x) == Bits(b.y_plus_x) && Bits(a.value) == Bits(b.value);
}
std::array<uint32_t, 9> Stages(const DirectionNormalization& value) {
  return {Bits(value.square_root), Bits(value.estimate), Bits(value.estimate_squared), Bits(value.half_length),
      Bits(value.correction_product), Bits(value.correction_difference), Bits(value.correction),
      Bits(value.estimate_correction), Bits(value.refined_estimate)};
}
bool Same(const DirectionNormalization& a, const DirectionNormalization& b) {
  return Same(a.value, b.value) && Same(a.length, b.length) && a.zeroed == b.zeroed && Stages(a) == Stages(b);
}
DirectionNormalization Sentinel() {
  return {{11, 22, 33, 44}, {{55, 66, 77}, 88, 99, 111}, true,
      120, 121, 122, 123, 124, 125, 126, 127, 128};
}

// Toggle the actual host flush controls without depending on the guest runtime.
// The same fixtures also run on hosts without a direct flush-control accessor.
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
using HostControl = uint32_t;
HostControl ReadControls() { return _mm_getcsr(); }
void WriteControls(HostControl value) { _mm_setcsr(value); }
constexpr HostControl flush_mask = 0x8040;
constexpr std::array<HostControl, 4> flush_modes{0, 0x40, 0x8000, 0x8040};
#elif defined(__aarch64__) && !defined(_MSC_VER)
using HostControl = uint64_t;
HostControl ReadControls() { HostControl value; __asm__ volatile("mrs %0, fpcr" : "=r"(value)); return value; }
void WriteControls(HostControl value) { __asm__ volatile("msr fpcr, %0" : : "r"(value)); }
constexpr HostControl flush_mask = (uint64_t{1} << 24) | (uint64_t{1} << 19);
constexpr std::array<HostControl, 4> flush_modes{0, uint64_t{1} << 19, uint64_t{1} << 24, flush_mask};
#else
using HostControl = uint32_t;
HostControl ReadControls() { return 0; }
void WriteControls(HostControl) {}
constexpr HostControl flush_mask = 0;
constexpr std::array<HostControl, 1> flush_modes{0};
#endif

void CheckDot(int rounding) {
  DirectionSquaredLength result{};
  Require(TryDirectionSquaredLength({3, 4, 0}, result) && Same(result.squares, Vec3{9, 16, 0}) &&
          result.zero_plus_z == 0 && result.y_plus_x == 25 && result.value == 25,
          "Dot changed its XYZ lanes or product/reduction details");
  // Pairing y*x contributions before the large z square preserves both units.
  // A reassociated (z*z + y*y) + x*x loses a unit in some rounding modes.
  Require(TryDirectionSquaredLength({1, 1, 4096}, result) &&
          Bits(result.zero_plus_z) == 0x4b800000u && result.y_plus_x == 2 && Bits(result.value) == 0x4b800001u,
          "Dot no longer uses the DPPS pairwise reduction tree");
  Require(TryDirectionSquaredLength({4097, 1, 1}, result) &&
          Bits(result.squares.x) == (rounding == FE_UPWARD ? 0x4b801001u : 0x4b801000u) &&
          Bits(result.y_plus_x) == (rounding == FE_UPWARD ? 0x4b801002u : 0x4b801000u) &&
          Bits(result.value) == (rounding == FE_UPWARD ? 0x4b801003u : 0x4b801000u),
          "Dot products and additions must round separately under caller rounding");
  Require(TryDirectionSquaredLength({Float(1), Float(0x807fffffu), -0.0f}, result) &&
          Same(result.squares, Vec3{0, 0, 0}) && Bits(result.value) == 0,
          "Signed subnormal dot inputs must be treated as signed zero before multiplication");
  Require(TryDirectionSquaredLength({Float(0x20000000u), 0, 0}, result) && Bits(result.value) == 0x00800000u,
          "An exact FLT_MIN squared length must remain supported");
  Require(TryDirectionSquaredLength({std::numeric_limits<float>::max(), 0, 0}, result),
          "Finite dot-product overflow must remain usable by direction selection");
  const uint32_t overflow = rounding == FE_TONEAREST || rounding == FE_UPWARD ? 0x7f800000u : 0x7f7fffffu;
  Require(Bits(result.squares.x) == overflow && Bits(result.value) == overflow,
          "Dot overflow changed caller-directed infinity versus maximum-finite rounding");
  Require(TryDirectionSquaredLength({Float(0x5f400000u), Float(0x5f400000u), 0}, result) &&
          Bits(result.squares.x) < 0x7f800000u && Bits(result.squares.y) < 0x7f800000u &&
          Bits(result.y_plus_x) == overflow && Bits(result.value) == overflow,
          "Finite squared components must permit positive overflow during dot reduction too");
  const DirectionSquaredLength before{{11, 22, 33}, 44, 55, 66};
  for (uint32_t lane = 0; lane < 3; ++lane)
    for (uint32_t bits : {0x1fffffffu, 0x00800000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
      Vec3 source{1, 2, 3};
      (lane == 0 ? source.x : lane == 1 ? source.y : source.z) = Float(bits);
      result = before;
      Require(!TryDirectionSquaredLength(source, result) && Same(result, before),
              "A tiny product or nonfinite input partially published dot details");
    }
}
void CheckNormalization(int rounding, uint32_t rounding_index) {
  constexpr float threshold = std::numeric_limits<float>::min();
  DirectionNormalization result{};
  Require(TryNormalizeDirection({-4, -0.0f, 0, 8}, threshold, result) && !result.zeroed &&
          Same(result.value, Vec4{-1, -0.0f, 0, 2}) && result.square_root == 4 &&
          result.estimate == 0.25f && result.estimate_squared == 0.0625f && result.half_length == 8 &&
          result.correction_product == 0.5f && result.refined_estimate == 0.25f,
          "Axis normalization changed the signed components, ignored W, or reciprocal stages");
  const uint32_t difference_zero = rounding == FE_DOWNWARD ? 0x80000000u : 0;
  Require(Bits(result.correction_difference) == difference_zero &&
          Bits(result.correction) == (difference_zero ^ 0x80000000u) &&
          Bits(result.estimate_correction) == (difference_zero ^ 0x80000000u),
          "Exact Newton cancellation must preserve subtraction rounding then flip its sign bit");
  for (uint32_t lane = 0; lane < 3; ++lane) for (float sign : {-1.0f, 1.0f}) {
    Vec4 input{0, 0, 0, 8}, expected{0, 0, 0, 2};
    (lane == 0 ? input.x : lane == 1 ? input.y : input.z) = sign * 4;
    (lane == 0 ? expected.x : lane == 1 ? expected.y : expected.z) = sign;
    Require(TryNormalizeDirection(input, threshold, result) && !result.zeroed && Same(result.value, expected),
            "A signed axis or raw fourth-lane normalization changed with caller rounding");
  }

  // Exact binary32 expectations for normalization of (3,4,0), including every
  // separate reciprocal/Newton operation. Refinement is deliberately not
  // replaced by an exact mathematical 1/5 or a fused operation.
  constexpr std::array<std::array<uint32_t, 9>, 4> stages{{
      {0x40a00000u, 0x3e4ccccdu, 0x3d23d70bu, 0x41480000u, 0x3f000001u, 0x33800000u, 0xb3800000u, 0xb24ccccdu, 0x3e4cccccu},
      {0x40a00000u, 0x3e4cccccu, 0x3d23d708u, 0x41480000u, 0x3efffffcu, 0xb4000000u, 0x34000000u, 0x32ccccccu, 0x3e4ccccdu},
      {0x40a00000u, 0x3e4cccccu, 0x3d23d708u, 0x41480000u, 0x3efffffcu, 0xb4000000u, 0x34000000u, 0x32ccccccu, 0x3e4ccccdu},
      {0x40a00000u, 0x3e4ccccdu, 0x3d23d70bu, 0x41480000u, 0x3f000001u, 0x33800000u, 0xb3800000u, 0xb24ccccdu, 0x3e4ccccdu}}};
  constexpr std::array<std::array<uint32_t, 4>, 4> components{{
      {0x3f199999u, 0x3f4cccccu, 0, 0x3f7fffffu},
      {0x3f199999u, 0x3f4ccccdu, 0, 0x3f800000u},
      {0x3f199999u, 0x3f4ccccdu, 0, 0x3f800000u},
      {0x3f19999au, 0x3f4ccccdu, 0, 0x3f800001u}}};
  Require(TryNormalizeDirection({3, 4, 0, 5}, threshold, result) && !result.zeroed && result.length.value == 25 &&
          Stages(result) == stages[rounding_index] &&
          Same(result.value, Vec4{Float(components[rounding_index][0]), Float(components[rounding_index][1]),
              Float(components[rounding_index][2]), Float(components[rounding_index][3])}),
          "3/4/5 normalization changed staged float rounding or raw fourth-lane scaling");
  Require(TryNormalizeDirection({1, 1, 0, 0}, threshold, result) &&
          // 0x3fb504f3 is below sqrt(2); only upward rounding selects its successor.
          Bits(result.square_root) == (rounding == FE_UPWARD ? 0x3fb504f4u : 0x3fb504f3u),
          "Float sqrt must honor caller rounding like the translated SIMD sqrt");
  for (float axis : {Float(0x20800000u), Float(0x5f000000u)})
    Require(TryNormalizeDirection({axis, 0, 0, 0}, threshold, result) && !result.zeroed && result.value.x == 1,
            "Exact finite axis normalization was narrowed unnecessarily near supported magnitude limits");
  Require(TryNormalizeDirection({1, Float(0x807fffffu), Float(1), Float(0x80000001u)}, threshold, result) &&
          Same(result.value, Vec4{1, -0.0f, 0, -0.0f}),
          "Explicit DAZ must preserve the sign of subnormal XYZ and W inputs");
  for (uint32_t raw_w : {0x7fc12345u, 0x7f812345u, 0x7f800000u, 0xff800000u, 0x7f7fffffu}) {
    Require(TryNormalizeDirection({3, 4, 0, Float(raw_w)}, 25, result) && result.zeroed &&
            Same(result.value, Vec4{0, 0, 0, 0}) && Stages(result) == std::array<uint32_t, 9>{},
            "Threshold zero branch must ignore raw W and avoid reciprocal/Newton arithmetic");
  }
  Require(TryNormalizeDirection({-0.0f, Float(0x80000001u), 0, Float(0x7fc12345u)}, Float(0x80000001u), result) &&
          result.zeroed && Same(result.value, Vec4{0, 0, 0, 0}),
          "Subnormal threshold must be DAZ-flushed before the zero-length comparison");
  Require(TryNormalizeDirection({Float(0x20000000u), 0, 0, 0}, threshold, result) && result.zeroed,
          "Threshold equality must zero an exact FLT_MIN squared length");
  Require(TryNormalizeDirection({4, 0, 0, 0}, Float(0x417fffffu), result) && !result.zeroed && result.value.x == 1,
          "A threshold just below the squared length must keep active normalization");

  const auto before = Sentinel();
  constexpr std::array<Vec4, 8> rejected{{
      {0, 0, 0, 0}, {Float(0x1fffffffu), 0, 0, 0}, {Float(0x20000000u), 0, 0, 0},
      {2, 0, 0, std::numeric_limits<float>::min()}, {0.5f, 0, 0, std::numeric_limits<float>::max()},
      {std::numeric_limits<float>::max(), 0, 0, 0}, {1, 0, 0, Float(0x7fc12345u)}, {1, 0, 0, Float(0x7f800000u)}}};
  for (size_t index = 0; index < rejected.size(); ++index) {
    result = before;
    const float lower = index == 0 ? -1.0f : index == 2 ? 0.0f : threshold;
    Require(!TryNormalizeDirection(rejected[index], lower, result) && Same(result, before),
            "Unsupported zero, tiny, overflow or active-W path partially published normalization");
  }
  for (uint32_t lane = 0; lane < 3; ++lane)
    for (uint32_t bits : {0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
      Vec4 input{1, 2, 3, 0};
      (lane == 0 ? input.x : lane == 1 ? input.y : input.z) = Float(bits);
      result = before;
      Require(!TryNormalizeDirection(input, threshold, result) && Same(result, before),
              "A nonfinite XYZ input partially published normalization");
    }
  for (uint32_t bits : {0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
    result = before;
    Require(!TryNormalizeDirection({0, 0, 0, 0}, Float(bits), result) && Same(result, before),
            "An unsupported threshold partially published normalization");
  }
}
int main() {
  std::fenv_t saved_environment;
  const auto saved_controls = ReadControls();
  if (std::feholdexcept(&saved_environment)) { std::cerr << "Cannot save and mask FP environment\n"; return 1; }
  uint32_t rounding_index = 0;
  try {
    for (const int rounding : {FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD}) {
      Require(std::fesetround(rounding) == 0, "Rounding mode unavailable");
      const auto controls = ReadControls();
      for (auto flush : flush_modes) {
        WriteControls((controls & ~flush_mask) | flush);
        CheckDot(rounding); CheckNormalization(rounding, rounding_index);
        Require((ReadControls() & flush_mask) == flush, "Portable direction core changed host flush controls");
        Require(std::fegetround() == rounding, "Portable direction core changed caller rounding");
      }
      ++rounding_index;
    }
    std::fesetenv(&saved_environment); WriteControls(saved_controls);
    std::cout << "Direction dot tree, staged normalization, signed DAZ, directed rounding, host flush independence and rollback passed\n";
  } catch (const std::exception& error) {
    std::fesetenv(&saved_environment); WriteControls(saved_controls);
    std::cerr << "Direction math rounding case " << rounding_index << ": " << error.what() << '\n'; return 1;
  }
}
