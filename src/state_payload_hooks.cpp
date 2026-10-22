#include "guest_state_payload.h"
#include "guest_native_support.h"

extern "C" void __imp__sub_8209D358(PPCContext&, uint8_t*);
namespace n3::guest {
thread_local uint32_t state_payload_retained_depth = 0;
}
#ifdef N3_STATE_PAYLOAD_TEST
extern thread_local bool n3_state_payload_native_used;
#endif
namespace {
bool Copy(PPCContext& ctx, uint8_t* base) {
  using namespace n3::native;
  using namespace n3::guest;
  const auto source_address = ctx.r4.u32, destination_address = ctx.r3.u32;
  if (!ScalarExceptionsMasked(ctx) || !FitsLinearGuestRange(source_address, kStatePayloadSize) ||
      !FitsLinearGuestRange(destination_address, kStatePayloadSize) ||
      !DisjointGuestHostRanges(source_address, kStatePayloadSize, destination_address, kStatePayloadSize)) return false;
  const auto source = ReadStatePayload(base, source_address);
  StatePayload destination;
  if (!TryCopyStatePayload(source, destination)) return false;
  ctx.fpscr.disableFlushMode();
  WriteStatePayload(base, destination_address, destination);
  ctx.r5.u64 = source.words56[2]; ctx.r6.u64 = source.words56[1]; ctx.r7.u64 = source.word96;
  ctx.r8.u64 = source.words56[6]; ctx.r9.u64 = source.words56[5];
  ctx.r10.u64 = source.words56[4]; ctx.r11.u64 = source.words56[3];
  ctx.f0.f64 = double(source.floats32[0]); ctx.f13.f64 = double(source.floats32[1]);
  ctx.f12.f64 = double(source.floats32[2]); ctx.f11.f64 = double(source.floats48[0]);
  ctx.f10.f64 = double(source.floats48[1]); ctx.f9.f64 = double(source.floats84[0]);
  ctx.f8.f64 = double(source.floats84[1]); ctx.f7.f64 = double(source.floats84[2]);
#ifdef N3_STATE_PAYLOAD_TEST
  n3_state_payload_native_used = true;
#endif
  return true;
}
}
extern "C" void sub_8209D358(PPCContext& ctx, uint8_t* base) {
  if (n3::guest::PayloadChainRetained() || !Copy(ctx, base)) __imp__sub_8209D358(ctx, base);
}
