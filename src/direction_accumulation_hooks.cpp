#include "native/direction_accumulation.h"
#include "native/direction_math.h"
#include "guest_native_support.h"
#include "guest_memory_address.h"
#include <rex/memory/utils.h>
#include <array>
#include <bit>
#include <cstring>

#pragma STDC FENV_ACCESS ON

extern "C" void __imp__sub_8209D8E8(PPCContext&, uint8_t*);
#ifdef N3_DIRECTION_ACCUMULATION_TEST
extern thread_local bool n3_direction_accumulation_native_used;
#endif
namespace {
using namespace n3::native;
constexpr uint32_t lower_address = 0x8205b610;
float Read(uint8_t* base, uint32_t at) {
  return std::bit_cast<float>(rex::memory::load_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, at)));
}
Vec3 Read3(uint8_t* base, uint32_t at) { return {Read(base, at), Read(base, at + 4), Read(base, at + 8)}; }
void Write3(uint8_t* base, uint32_t at, Vec3 value) {
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, at), std::bit_cast<uint32_t>(value.x));
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, at + 4), std::bit_cast<uint32_t>(value.y));
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, at + 8), std::bit_cast<uint32_t>(value.z));
}
bool Masked(uint32_t controls) {
  using Platform = rex::platform::FPSCRPlatform;
#if defined(__x86_64__) || defined(_M_X64)
  return (controls & Platform::ExceptionMask) == Platform::ExceptionMask;
#else
  return (controls & Platform::ExceptionMask) == 0;
#endif
}
bool Process(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  using Platform = rex::platform::FPSCRPlatform;
  const auto object = ctx.r3.u32, stack = ctx.r1.u32;
  const auto host = Platform::getcsr(), guest = ctx.fpscr.csr;
  // enableFlushMode can leave the host word untouched, while the scalar phase
  // always reloads the guest word. Both effective exception policies must mask.
  const auto vector_controls = (guest & Platform::FlushMask) == Platform::FlushMask ? host : guest;
  if (!Masked(vector_controls) || !Masked(guest) ||
      ((guest & Platform::FlushMask) == Platform::FlushMask &&
       (host & Platform::FlushMask) != Platform::FlushMask) || stack < 16 || stack % 16 ||
      !FitsLinearGuestRange(stack - 16, 16) || !FitsLinearGuestRange(object, 592)) return false;
  const uint32_t raw_first = (object + 576) & ~15u, raw_second = (object + 587) & ~15u;
  if (!FitsLinearGuestRange(raw_first, 16) || !FitsLinearGuestRange(raw_second, 16) ||
      !DisjointGuestHostRanges(object, 592, stack - 16, 16) ||
      !DisjointGuestHostRanges(raw_first, 16, stack - 16, 16) ||
      !DisjointGuestHostRanges(raw_second, 16, stack - 16, 16) ||
      !DisjointGuestHostRanges(lower_address, 4, stack - 16, 16)) return false;
  const auto previous = Read3(base, object + 564), defaults = Read3(base, object + 64);
  const auto override_direction = Read3(base, object + 576);
  const auto lower = Read(base, lower_address);
  std::array<uint8_t, 16> last_raw{}, mask{};
  // These are layout-only VMX operations: retain the original reversed-byte
  // aligned load and lvsl mask without guest arithmetic in the adapter.
  simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(last_raw.data()),
      simde_mm_shuffle_epi8(simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(GuestHostAddress(base, raw_second))),
                           simde_mm_load_si128(reinterpret_cast<const simde__m128i*>(VectorMaskL))));
  std::memcpy(mask.data(), VectorShiftTableL + ((object + 576) & 15) * 16, 16);
  const auto saved_host = host, saved_guest = guest;
  ctx.fpscr.enableFlushMode();
  DirectionSquaredLength length;
  if (!TryDirectionSquaredLength(override_direction, length)) {
    ctx.fpscr.csr = saved_guest; Platform::setcsr(saved_host); return false;
  }
  ctx.fpscr.disableFlushModeUnconditional();
  DirectionAccumulationState state{previous};
  DirectionAccumulationUpdate update;
  if (!TryAccumulateDirection(state, {defaults, override_direction, ctx.f1.f64, length.value, lower}, update)) {
    ctx.fpscr.csr = saved_guest; Platform::setcsr(saved_host); return false;
  }
  for (uint32_t i = 0; i < 4; ++i)
    rex::memory::store_and_swap<uint32_t>(GuestHostAddress(base, stack - 16 + i * 4), std::bit_cast<uint32_t>(length.value));
  Write3(base, object + 564, state.value);
  ctx.r10.u64 = ctx.r3.u64 + 576; ctx.r9.u64 = ctx.r1.u64 - 16; ctx.r11.u64 = ctx.r3.u64 + 564;
  ctx.f0.f64 = double(update.products.x); ctx.f13.f64 = double(update.products.y); ctx.f12.f64 = double(update.products.z);
  if (update.used_override) {
    ctx.f2.f64 = double(previous.x); ctx.f10.f64 = double(previous.y); ctx.f9.f64 = double(previous.z);
    ctx.f11.f64 = double(state.value.x); ctx.f8.f64 = double(state.value.y); ctx.f7.f64 = double(state.value.z);
  } else {
    ctx.f10.f64 = double(defaults.x); ctx.f9.f64 = double(defaults.y); ctx.f8.f64 = double(defaults.z);
    ctx.f11.f64 = double(previous.x); ctx.f6.f64 = double(previous.y); ctx.f5.f64 = double(previous.z);
    ctx.f7.f64 = double(state.value.x); ctx.f4.f64 = double(state.value.y); ctx.f3.f64 = double(state.value.z);
  }
  std::memcpy(ctx.v13.u8, last_raw.data(), 16); std::memcpy(ctx.v7.u8, mask.data(), 16);
  for (auto& lane : ctx.v0.u32) lane = std::bit_cast<uint32_t>(length.value);
  ctx.cr6.compare(double(length.value), double(lower));
#ifdef N3_DIRECTION_ACCUMULATION_TEST
  n3_direction_accumulation_native_used = true;
#endif
  return true;
}
}
extern "C" void sub_8209D8E8(PPCContext& ctx, uint8_t* base) {
  if (!Process(ctx, base)) __imp__sub_8209D8E8(ctx, base);
}
