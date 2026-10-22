#include "native/scalar_math.h"
#include "native/frame_countdown.h"
#include "native/bit_window.h"
#include "native/vector_ops.h"
#include "guest_native_support.h"
#include <rex/memory/utils.h>
#include <bit>

namespace {
using namespace n3::native;
uint32_t Read(uint8_t* base, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(base + address);
}
void Write(uint8_t* base, uint32_t address, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(base + address, value);
}
Vec3 Read3(uint8_t* base, uint32_t address) {
  return {std::bit_cast<float>(Read(base, address)), std::bit_cast<float>(Read(base, address + 4)),
          std::bit_cast<float>(Read(base, address + 8))};
}
bool Scalar(PPCContext& ctx, bool subtract) {
  if (!n3::guest::ScalarExceptionsMasked(ctx) || !SupportsVectorScalar(ctx.f1.f64) ||
      !SupportsVectorScalar(ctx.f2.f64)) return false;
  ctx.fpscr.disableFlushMode();
  const auto result = subtract ? SubtractScalar(ctx.f1.f64, ctx.f2.f64) : AddScalar(ctx.f1.f64, ctx.f2.f64);
  ctx.f1.f64 = double(result.value);
  ctx.cr0.compare<int32_t>(result.flushed_subnormal ? 0 : 1, 0, ctx.xer);
  return true;
}
}

extern "C" void __imp__sub_824A0C50(PPCContext&, uint8_t*);
extern "C" void sub_824A0C50(PPCContext& ctx, uint8_t* base) {
  if (!Scalar(ctx, false)) __imp__sub_824A0C50(ctx, base);
}
extern "C" void __imp__sub_824E7AD0(PPCContext&, uint8_t*);
extern "C" void sub_824E7AD0(PPCContext& ctx, uint8_t* base) {
  if (!Scalar(ctx, true)) __imp__sub_824E7AD0(ctx, base);
}

extern "C" void __imp__sub_820801B0(PPCContext&, uint8_t*);
extern "C" void sub_820801B0(PPCContext& ctx, uint8_t* base) {
  const auto object = ctx.r3.u32;
  if (!n3::guest::FitsGuestRange(object, 228)) return __imp__sub_820801B0(ctx, base);
  const auto id = Read(base, object + 208);
  ctx.cr6.compare<int32_t>(std::bit_cast<int32_t>(id), -1, ctx.xer);
  if (id == 0xffffffffu) return;
  FrameCountdown state{id, Read(base, object + 224), Read(base, object + 220), Read(base, object + 216)};
  if (CountdownNeedsProgress(state)) {
    if (!n3::guest::ScalarExceptionsMasked(ctx)) return __imp__sub_820801B0(ctx, base);
    ctx.fpscr.disableFlushMode();
  }
  const auto previous = state.frames_remaining;
  const auto outcome = AdvanceFrameCountdown(state);
  ctx.xer.ca = previous > 0;
  ctx.cr0.compare<int32_t>(std::bit_cast<int32_t>(state.frames_remaining), 0, ctx.xer);
  Write(base, object + 224, state.frames_remaining);
  if (outcome == CountdownOutcome::Expired) Write(base, object + 208, state.active_id);
  else Write(base, object + 216, state.progress_bits);
}

extern "C" void __imp__sub_8246D5F8(PPCContext&, uint8_t*);
extern "C" void sub_8246D5F8(PPCContext& ctx, uint8_t* base) {
  const auto position = ctx.r4.u32, width = ctx.r5.u32;
  if (!position || !width || width > 32 || ((position - 1) & 7) + width > 32)
    return __imp__sub_8246D5F8(ctx, base);
  const uint64_t window = uint64_t(ctx.r3.u32) + ((position - 1) >> 3);
  if (window + 4 > 0x100000000ull) return __imp__sub_8246D5F8(ctx, base);
  const auto value = ReadBitWindow(std::span<const uint8_t, 4>(base + window, 4), (position - 1) & 7, width);
  ctx.r3.u64 = *value;
  ctx.xer.ca = true;
}

extern "C" void __imp__sub_82324CD0(PPCContext&, uint8_t*);
extern "C" void sub_82324CD0(PPCContext& ctx, uint8_t* base) {
  const auto object = ctx.r3.u32, input = ctx.r5.u32;
  if (!n3::guest::FitsGuestRange(object, 356) || !n3::guest::FitsGuestRange(input, 12) ||
      !n3::guest::DisjointGuestRanges(object, 356, input, 12) || !n3::guest::ScalarExceptionsMasked(ctx))
    return __imp__sub_82324CD0(ctx, base);
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr();
  const auto saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  Vec3 result{};
  if (!TryAccumulateVectorRatio(Read3(base, object + 344), Read3(base, input), ctx.f1.f64, ctx.f2.f64, result)) {
    ctx.fpscr.csr = saved_guest;
    rex::platform::FPSCRPlatform::setcsr(saved_host);
    return __imp__sub_82324CD0(ctx, base);
  }
  Write(base, object + 344, std::bit_cast<uint32_t>(result.x));
  Write(base, object + 348, std::bit_cast<uint32_t>(result.y));
  Write(base, object + 352, std::bit_cast<uint32_t>(result.z));
}
