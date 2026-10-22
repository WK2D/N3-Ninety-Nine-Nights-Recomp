#include "native/direction_math.h"
#include "guest_native_support.h"
#include "guest_memory_address.h"
#include <rex/memory/utils.h>
#include <array>
#include <bit>
#include <cstring>

#pragma STDC FENV_ACCESS ON

extern "C" void __imp__sub_823B3C00(PPCContext&, uint8_t*);
#ifdef N3_DIRECTION_BASIS_TEST
extern thread_local bool n3_direction_basis_original_norm;
#endif
#ifdef N3_DIRECTION_NORMALIZATION_TEST
extern thread_local bool n3_direction_normalization_native_used;
#endif
namespace {
constexpr uint32_t threshold_address = 0x820018ac;
bool Masked(uint32_t controls) {
  using Platform = rex::platform::FPSCRPlatform;
#if defined(__x86_64__) || defined(_M_X64)
  return (controls & Platform::ExceptionMask) == Platform::ExceptionMask;
#else
  return (controls & Platform::ExceptionMask) == 0;
#endif
}
bool Normalize(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  using namespace n3::native;
  using Platform = rex::platform::FPSCRPlatform;
  const auto source = ctx.r4.u32, destination = ctx.r3.u32, stack = ctx.r1.u32;
  const auto saved_host = Platform::getcsr(), saved_guest = ctx.fpscr.csr;
  if (stack < 16 || stack % 16 || !FitsLinearGuestRange(stack - 16, 40) || !FitsLinearGuestRange(source, 12) ||
      !Masked(saved_host) || !Masked(saved_guest)) return false;
  const auto first = source & ~15u, second = (source + 11) & ~15u;
  const auto output = destination & ~3u;
  if (!FitsLinearGuestRange(first, 16) || !FitsLinearGuestRange(second, 16) || !FitsLinearGuestRange(output, 12)) return false;
  auto outside_scratch = [&](uint32_t at, uint32_t size) {
    return DisjointGuestHostRanges(at, size, stack - 16, 16) &&
        DisjointGuestHostRanges(at, size, stack + 20, 4);
  };
  // The original writes the output home and threshold scratch between its two
  // raw input loads. Exclude those dependencies before reading old source bits.
  if (!outside_scratch(first, 16) || !outside_scratch(second, 16) ||
      !outside_scratch(threshold_address, 4) || !outside_scratch(output, 12)) return false;
  const auto threshold_bits = rex::memory::load_and_swap<uint32_t>(GuestHostAddress(base, threshold_address));
  const auto threshold = std::bit_cast<float>(threshold_bits);
  alignas(16) std::array<uint32_t, 4> raw_input{};
  alignas(16) std::array<uint8_t, 16> shift{};
  // These intrinsics only reconstruct the original byte layout. Arithmetic is
  // performed by the typed model, including the raw fourth lane's wraparound.
  const auto first_raw = simde_mm_shuffle_epi8(
      simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(GuestHostAddress(base, first))),
      simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(VectorMaskL)));
  const auto second_raw = simde_mm_shuffle_epi8(
      simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(GuestHostAddress(base, second))),
      simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(VectorMaskL)));
  const auto mask = simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(VectorShiftTableL + (source & 15) * 16));
  simde_mm_store_si128(reinterpret_cast<simde__m128i*>(raw_input.data()),
                      rex::ppc::simde_mm_perm_epi8_(first_raw, second_raw, mask));
  simde_mm_store_si128(reinterpret_cast<simde__m128i*>(shift.data()), mask);
  const Vec4 input{std::bit_cast<float>(raw_input[3]), std::bit_cast<float>(raw_input[2]),
                  std::bit_cast<float>(raw_input[1]), std::bit_cast<float>(raw_input[0])};
  ctx.fpscr.disableFlushMode();
  const double loaded_threshold = double(threshold);
  volatile float stored_threshold = float(loaded_threshold);
  const auto scratch_threshold = stored_threshold;
  const auto scratch_threshold_bits = std::bit_cast<uint32_t>(scratch_threshold);
  ctx.fpscr.enableFlushModeUnconditional();
  DirectionNormalization result;
  if (!TryNormalizeDirection(input, scratch_threshold, result)) {
    ctx.fpscr.csr = saved_guest; Platform::setcsr(saved_host); return false;
  }
  auto write = [&](uint32_t at, float value) {
    rex::memory::store_and_swap<uint32_t>(GuestHostAddress(base, at), std::bit_cast<uint32_t>(value));
  };
  rex::memory::store_and_swap<uint32_t>(GuestHostAddress(base, stack + 20), destination);
  for (uint32_t i = 0; i < 4; ++i)
    rex::memory::store_and_swap<uint32_t>(GuestHostAddress(base, stack - 16 + i * 4),
        result.zeroed ? scratch_threshold_bits : std::bit_cast<uint32_t>(result.refined_estimate));
  write(output, result.value.x); write(output + 4, result.value.y); write(output + 8, result.value.z);
  std::memcpy(ctx.v11.u32, raw_input.data(), 16); std::memcpy(ctx.v7.u8, shift.data(), 16);
  for (auto& lane : ctx.v0.u32) lane = std::bit_cast<uint32_t>(result.value.z);
  for (auto& lane : ctx.v12.u32) lane = std::bit_cast<uint32_t>(result.value.y);
  for (auto& lane : ctx.v13.u32) lane = std::bit_cast<uint32_t>(result.value.x);
  for (auto& lane : ctx.v10.u32) lane = result.zeroed ? scratch_threshold_bits : std::bit_cast<uint32_t>(result.estimate_squared);
  ctx.r3.u64 = destination; ctx.r9.u64 = destination; ctx.r10.u64 = 8; ctx.r11.u64 = destination;
  ctx.f0.f64 = loaded_threshold;
  ctx.cr6.set_raw(result.zeroed ? 8 : 2);
  ctx.cr0.compare<int32_t>(result.zeroed ? 0x80 : 0, 0, ctx.xer);
#ifdef N3_DIRECTION_NORMALIZATION_TEST
  n3_direction_normalization_native_used = true;
#endif
  return true;
}
}
extern "C" void sub_823B3C00(PPCContext& ctx, uint8_t* base) {
#ifdef N3_DIRECTION_BASIS_TEST
  if (n3_direction_basis_original_norm) { __imp__sub_823B3C00(ctx, base); return; }
#endif
  if (!Normalize(ctx, base)) __imp__sub_823B3C00(ctx, base);
}
