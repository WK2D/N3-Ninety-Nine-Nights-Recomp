#include "native/direction_basis.h"
#include "guest_native_support.h"
#include "guest_memory_address.h"
#include <rex/memory/utils.h>
#include <array>
#include <bit>
#include <cstring>

#pragma STDC FENV_ACCESS ON

extern "C" void __imp__sub_8209D9B0(PPCContext&, uint8_t*);
#ifdef N3_DIRECTION_BASIS_TEST
extern thread_local bool n3_direction_basis_native_used;
#endif
namespace {
using namespace n3::guest;
using namespace n3::native;
using Platform = rex::platform::FPSCRPlatform;
constexpr uint32_t lower_address = 0x8205b610, unit_address = lower_address + 4;
constexpr uint32_t threshold_address = 0x820018ac, slots_address = 0x82b4808c;
uint32_t ReadWord(uint8_t* base, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(GuestHostAddress(base, address));
}
float ReadFloat(uint8_t* base, uint32_t address) { return std::bit_cast<float>(ReadWord(base, address)); }
void WriteWord(uint8_t* base, uint32_t address, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(GuestHostAddress(base, address), value);
}
void WriteLong(uint8_t* base, uint32_t address, uint64_t value) {
  rex::memory::store_and_swap<uint64_t>(GuestHostAddress(base, address), value);
}
void WriteFloat(uint8_t* base, uint32_t address, float value) {
  WriteWord(base, address, std::bit_cast<uint32_t>(value));
}
void Write3(uint8_t* base, uint32_t address, Vec3 value) {
  WriteFloat(base, address, value.x); WriteFloat(base, address + 4, value.y); WriteFloat(base, address + 8, value.z);
}
void Broadcast(PPCVRegister& target, float value) {
  for (auto& lane : target.u32) lane = std::bit_cast<uint32_t>(value);
}
void SetRaw(PPCVRegister& target, Vec4 value) {
  target.u32[3] = std::bit_cast<uint32_t>(value.x); target.u32[2] = std::bit_cast<uint32_t>(value.y);
  target.u32[1] = std::bit_cast<uint32_t>(value.z); target.u32[0] = std::bit_cast<uint32_t>(value.w);
}
void PublishNormalization(PPCContext& ctx, const DirectionNormalization& result,
    Vec4 raw, uint32_t source, float threshold, double loaded_threshold) {
  SetRaw(ctx.v11, raw);
  std::memcpy(ctx.v7.u8, VectorShiftTableL + (source & 15) * 16, 16);
  Broadcast(ctx.v0, result.value.z); Broadcast(ctx.v12, result.value.y); Broadcast(ctx.v13, result.value.x);
  Broadcast(ctx.v10, result.zeroed ? threshold : result.estimate_squared);
  ctx.r3.u64 = source; ctx.r9.u64 = source; ctx.r10.u64 = 8; ctx.r11.u64 = source;
  ctx.f0.f64 = loaded_threshold;
  ctx.cr6.set_raw(result.zeroed ? 8 : 2);
  ctx.cr0.compare<int32_t>(result.zeroed ? 0x80 : 0, 0, ctx.xer);
}
bool Masked(uint32_t controls) {
#if defined(__x86_64__) || defined(_M_X64)
  return (controls & Platform::ExceptionMask) == Platform::ExceptionMask;
#else
  return (controls & Platform::ExceptionMask) == 0;
#endif
}
// Reconstruct the final vector load before publishing its unaligned scalar
// inputs. Both original aligned blocks receive the staged stores, even when
// the two loads refer to the same block and the fourth lane wraps into it.
Vec4 ReadStagedOutput(uint8_t* base, uint32_t source, Vec3 combined) {
  alignas(16) std::array<uint8_t, 32> blocks;
  const std::array<uint32_t, 2> addresses{source & ~15u, (source + 11) & ~15u};
  const std::array<float, 3> values{combined.x, combined.y, combined.z};
  for (uint32_t block = 0; block < 2; ++block) {
    std::memcpy(blocks.data() + block * 16, GuestHostAddress(base, addresses[block]), 16);
    for (uint32_t word = 0; word < 3; ++word) for (uint32_t byte = 0; byte < 4; ++byte) {
      const uint64_t at = uint64_t(source) + word * 4 + byte;
      if (at >= addresses[block] && at < uint64_t(addresses[block]) + 16)
        blocks[block * 16 + uint32_t(at - addresses[block])] =
            uint8_t(std::bit_cast<uint32_t>(values[word]) >> ((3 - byte) * 8));
    }
  }
  const auto reversal = simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(VectorMaskL));
  const auto first = simde_mm_shuffle_epi8(simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(blocks.data())), reversal);
  const auto second = simde_mm_shuffle_epi8(simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(blocks.data() + 16)), reversal);
  const auto shift = simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(VectorShiftTableL + (source & 15) * 16));
  alignas(16) std::array<uint32_t, 4> words;
  simde_mm_store_si128(reinterpret_cast<simde__m128i*>(words.data()), rex::ppc::simde_mm_perm_epi8_(first, second, shift));
  return {std::bit_cast<float>(words[3]), std::bit_cast<float>(words[2]),
          std::bit_cast<float>(words[1]), std::bit_cast<float>(words[0])};
}
bool Update(PPCContext& ctx, uint8_t* base) {
  const auto stack = ctx.r1.u32, source = ctx.r4.u32, object = ctx.r3.u32;
  const auto host = Platform::getcsr(), guest = ctx.fpscr.csr;
  const auto scalar_controls = guest & Platform::FlushMask ? guest & ~Platform::FlushMask : host;
  if (!Masked(scalar_controls) || !Masked(guest) || (scalar_controls & Platform::FlushMask) ||
      stack < 192 || stack % 16 || !FitsLinearGuestRange(stack - 192, 232)) return false;
  const uint32_t frame = stack - 176, output = object + 576;
  auto outside_frame = [&](uint32_t at, uint32_t size) {
    return FitsLinearGuestRange(at, size) && DisjointGuestHostRanges(at, size, stack - 192, 192) &&
        DisjointGuestHostRanges(at, size, stack + 20, 4) &&
        DisjointGuestHostRanges(at, size, stack + 36, 4);
  };
  // Check prologue dependencies before following pointers or reading a source
  // that the original saved LR/register homes could replace.
  if (!outside_frame(source, 8) || !outside_frame(lower_address, 4) ||
      !outside_frame(unit_address, 4) || !outside_frame(threshold_address, 4) ||
      !outside_frame(output, 12)) return false;
  const auto horizontal = std::bit_cast<int32_t>(ReadWord(base, source));
  const auto vertical = std::bit_cast<int32_t>(ReadWord(base, source + 4));
  const auto lower = ReadFloat(base, lower_address), threshold = ReadFloat(base, threshold_address);
  const auto first_w = ReadFloat(base, frame + 140);
  ctx.fpscr.disableFlushMode();
  const double loaded_lower = double(lower), loaded_threshold = double(threshold);
  volatile float rounded_horizontal = float(double(horizontal));
  volatile float rounded_vertical = float(double(vertical));
  const Vec4 rounded_input{rounded_horizontal, lower, rounded_vertical, first_w};
  // Initial scalar conversions use the effective incoming host rounding. The
  // first vector phase installs guest rounding; every later scalar phase uses it.
  ctx.fpscr.disableFlushModeUnconditional();
  auto reject = [&] { ctx.fpscr.csr = guest; Platform::setcsr(host); return false; };
  DirectionBasisState state{};
  DirectionBasisUpdate update;
  DirectionBasisParameters parameters{};
  parameters.horizontal = horizontal; parameters.vertical = vertical;
  parameters.lower_bound = lower; parameters.threshold = threshold; parameters.input_w = first_w;
  if (!TryPrepareRoundedDirectionInput(state, rounded_input, lower, threshold, update)) return reject();
  uint32_t related = 0, offset = 0;
  Vec4 output_raw{};
  if (update.branch != DirectionBasisBranch::Unchanged) {
    offset = ctx.r5.u32 * 12u;
    const uint32_t cell = slots_address + offset;
    if (!outside_frame(cell, 4)) return reject();
    related = ReadWord(base, cell);
    parameters.basis_present = related != 0;
    parameters.unit = ReadFloat(base, unit_address);
    if (related) {
      const uint32_t basis_source = related + 64;
      const auto first_block = output & ~15u, second_block = (output + 11) & ~15u;
      if (!FitsLinearGuestRange(related, 76) || !outside_frame(basis_source, 12) ||
          !outside_frame(first_block, 16) || !outside_frame(second_block, 16) ||
          !DisjointGuestHostRanges(output, 12, threshold_address, 4)) return reject();
      parameters.basis = {ReadFloat(base, basis_source), 0, ReadFloat(base, basis_source + 8)};
      parameters.basis_w = ReadFloat(base, frame + 124);
    }
    if (!TryComposeDirectionBasis(parameters, update)) return reject();
    if (related) output_raw = ReadStagedOutput(base, output, update.combined);
    if (!TryFinishDirectionBasis(state, update, threshold, output_raw.w)) return reject();
  }
  const auto frame_full = (ctx.r1.u64 & 0xffffffff00000000ull) | frame;
  const auto original_slot = ctx.r5.u64, original_target = ctx.r3.u64;
  const auto original_lr = uint32_t(ctx.lr);
  WriteWord(base, frame, stack);
  WriteLong(base, stack - 32, ctx.f31.u64); WriteLong(base, stack - 24, ctx.r30.u64);
  WriteLong(base, stack - 16, ctx.r31.u64); WriteWord(base, stack - 8, original_lr);
  WriteWord(base, stack + 20, object); WriteWord(base, stack + 36, uint32_t(original_slot));
  WriteLong(base, frame + 80, uint64_t(int64_t(horizontal)));
  Write3(base, frame + 128, {update.input_normalization.value.x, update.input_normalization.value.y, update.input_normalization.value.z});
  for (uint32_t lane = 0; lane < 4; ++lane) WriteFloat(base, frame + 96 + lane * 4, update.post_normalization_length.value);
  const auto& last_normalization = related ? update.output_normalization : update.input_normalization;
  for (uint32_t lane = 0; lane < 4; ++lane)
    WriteFloat(base, frame - 16 + lane * 4, last_normalization.zeroed ? threshold : last_normalization.refined_estimate);
  WriteWord(base, frame + 20, related ? output : frame + 128);
  PublishNormalization(ctx, update.input_normalization, rounded_input, frame + 128, threshold, loaded_threshold);
  // Complete the common caller state after its second dot and scalar compare.
  ctx.r3.u64 = frame_full + 96; ctx.r4.u64 = frame_full + 139;
  ctx.r5.u64 = frame_full + 128; ctx.r6.u64 = frame_full + 128;
  ctx.r7.u64 = uint64_t(int64_t(vertical)); ctx.r8.u64 = uint64_t(int64_t(horizontal));
  ctx.f8.f64 = double(update.post_normalization_length.value);
  ctx.f9.f64 = double(rounded_input.z); ctx.f10.f64 = double(rounded_input.x);
  ctx.f11.f64 = double(vertical); ctx.f12.f64 = double(horizontal); ctx.f13.u64 = uint64_t(int64_t(vertical));
  Broadcast(ctx.v0, update.post_normalization_length.value);
  SetRaw(ctx.v13, {update.input_normalization.value.x, update.input_normalization.value.y,
                  update.input_normalization.value.z, first_w});
  ctx.cr6.compare(ctx.f8.f64, loaded_lower);
  if (update.branch != DirectionBasisBranch::Unchanged) {
    ctx.r8.u64 = offset;
    ctx.r9.u64 = original_slot + uint32_t(uint32_t(original_slot) * 2u);
    ctx.r10.u64 = uint64_t(int64_t(std::bit_cast<int32_t>(slots_address)));
    ctx.cr6.compare<uint32_t>(related, 0, ctx.xer);
    if (!related) {
      ctx.r11.u64 = uint64_t(int64_t(std::bit_cast<int32_t>(uint32_t{0x82060000})));
      ctx.f0.f64 = double(parameters.unit);
      Write3(base, output, state.value);
    } else {
      WriteWord(base, frame + 80, related + 32);
      Write3(base, frame + 112, {update.basis_normalization.value.x, update.basis_normalization.value.y, update.basis_normalization.value.z});
      Write3(base, frame + 96, update.right);
      Write3(base, output, update.combined);
      Write3(base, output & ~3u, state.value);
      PublishNormalization(ctx, update.output_normalization, output_raw, output, threshold, double(threshold));
      ctx.r4.u64 = original_target + 576; ctx.r5.u64 = frame_full + 96;
      ctx.r6.u64 = 8; ctx.r7.u64 = frame_full + 96; ctx.r8.u64 = 4;
      const auto forward = update.basis_normalization.value;
      ctx.f1.f64 = double(forward.x); ctx.f2.f64 = double(update.right.z);
      ctx.f3.f64 = double(update.right.y); ctx.f4.f64 = double(update.right.x);
      ctx.f5.f64 = double(update.combined.z); ctx.f6.f64 = double(update.combined.y);
      ctx.f7.f64 = double(update.lateral_products.z); ctx.f8.f64 = double(update.lateral_products.y);
      ctx.f9.f64 = double(update.combined.x); ctx.f10.f64 = double(update.lateral_products.x);
      ctx.f11.f64 = double(forward.z); ctx.f12.f64 = double(update.forward_products.y);
      ctx.f13.f64 = double(update.forward_products.x);
      std::memcpy(ctx.v6.u8, VectorShiftTableL, 16);
    }
  }
  ctx.r12.u64 = original_lr; ctx.lr = original_lr;
#ifdef N3_DIRECTION_BASIS_TEST
  n3_direction_basis_native_used = true;
#endif
  return true;
}
}
extern "C" void sub_8209D9B0(PPCContext& ctx, uint8_t* base) {
  if (!Update(ctx, base)) __imp__sub_8209D9B0(ctx, base);
}
