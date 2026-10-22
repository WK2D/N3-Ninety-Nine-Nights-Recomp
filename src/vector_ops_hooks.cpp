#include "native/vector_ops.h"
#include "native/shape_bounds.h"
#include "guest_native_support.h"
#include "guest_memory_address.h"
#include <rex/memory/utils.h>
#include <bit>

#ifdef N3_VECTOR_OPS_TEST
extern thread_local bool n3_vector_ops_native_used;
#endif

namespace {
using namespace n3::native;
float Read(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(rex::memory::load_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address)));
}
Vec3 Read3(uint8_t* base, uint32_t address) {
  return {Read(base, address), Read(base, address + 4), Read(base, address + 8)};
}
void Write(uint8_t* base, uint32_t address, float value) {
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address), std::bit_cast<uint32_t>(value));
}
void Write3(uint8_t* base, uint32_t address, Vec3 value) {
  Write(base, address, value.x); Write(base, address + 4, value.y); Write(base, address + 8, value.z);
}
bool Accumulate(PPCContext& ctx, uint8_t* base, uint32_t direction_offset) {
  if (!n3::guest::FitsLinearGuestRange(ctx.r3.u32, 576) || !n3::guest::ScalarExceptionsMasked(ctx) ||
      !SupportsVectorScalar(ctx.f1.f64)) return false;
  const auto direction = Read3(base, ctx.r3.u32 + direction_offset);
  const auto current = Read3(base, ctx.r3.u32 + 564);
  if (!SupportsShapeValue(direction) || !SupportsShapeValue(current)) return false;
  ctx.fpscr.disableFlushMode();
  Write3(base, ctx.r3.u32 + 564, AccumulateScaledVector(current, direction, ctx.f1.f64));
#ifdef N3_VECTOR_OPS_TEST
  n3_vector_ops_native_used = true;
#endif
  return true;
}
bool SetChannel(PPCContext& ctx, uint8_t* base, uint32_t current_offset,
                uint32_t cached_offset, bool four_components) {
  const uint32_t count = four_components ? 16 : 12;
  if (!n3::guest::FitsLinearGuestRange(ctx.r3.u32, 112) || !n3::guest::FitsLinearGuestRange(ctx.r4.u32, count) ||
      !n3::guest::DisjointGuestHostRanges(ctx.r3.u32, 112, ctx.r4.u32, count) ||
      !n3::guest::ScalarExceptionsMasked(ctx)) return false;
  const auto vector = Read3(base, ctx.r4.u32);
  if (!SupportsShapeValue(vector)) return false;
  if (four_components) {
    const auto w = Read(base, ctx.r4.u32 + 12);
    if (!SupportsShapeValue(w)) return false;
    ctx.fpscr.disableFlushMode();
    MirroredVector4 channel{};
    SetMirroredVector(channel, Vec4{vector.x, vector.y, vector.z, w});
    // Preserve the original cache-first publication order.
    Write3(base, ctx.r3.u32 + cached_offset, {channel.cached.x, channel.cached.y, channel.cached.z});
    Write(base, ctx.r3.u32 + cached_offset + 12, channel.cached.w);
    Write3(base, ctx.r3.u32 + current_offset, {channel.current.x, channel.current.y, channel.current.z});
    Write(base, ctx.r3.u32 + current_offset + 12, channel.current.w);
  } else {
    ctx.fpscr.disableFlushMode();
    MirroredVector3 channel{};
    SetMirroredVector(channel, vector);
    Write3(base, ctx.r3.u32 + cached_offset, channel.cached);
    Write3(base, ctx.r3.u32 + current_offset, channel.current);
  }
#ifdef N3_VECTOR_OPS_TEST
  n3_vector_ops_native_used = true;
#endif
  return true;
}
}

extern "C" void __imp__sub_8208ECF8(PPCContext&, uint8_t*);
extern "C" void sub_8208ECF8(PPCContext& ctx, uint8_t* base) {
  if (!Accumulate(ctx, base, 64)) __imp__sub_8208ECF8(ctx, base);
}
extern "C" void __imp__sub_8209D858(PPCContext&, uint8_t*);
extern "C" void sub_8209D858(PPCContext& ctx, uint8_t* base) {
  if (!Accumulate(ctx, base, 32)) __imp__sub_8209D858(ctx, base);
}
extern "C" void __imp__sub_8209D8A0(PPCContext&, uint8_t*);
extern "C" void sub_8209D8A0(PPCContext& ctx, uint8_t* base) {
  if (!Accumulate(ctx, base, 48)) __imp__sub_8209D8A0(ctx, base);
}
extern "C" void __imp__sub_82336D90(PPCContext&, uint8_t*);
extern "C" void sub_82336D90(PPCContext& ctx, uint8_t* base) {
  if (!SetChannel(ctx, base, 4, 60, false)) __imp__sub_82336D90(ctx, base);
}
extern "C" void __imp__sub_82336DD0(PPCContext&, uint8_t*);
extern "C" void sub_82336DD0(PPCContext& ctx, uint8_t* base) {
  if (!SetChannel(ctx, base, 16, 72, true)) __imp__sub_82336DD0(ctx, base);
}
extern "C" void __imp__sub_82336E48(PPCContext&, uint8_t*);
extern "C" void sub_82336E48(PPCContext& ctx, uint8_t* base) {
  if (!SetChannel(ctx, base, 32, 88, false)) __imp__sub_82336E48(ctx, base);
}
extern "C" void __imp__sub_82336EB0(PPCContext&, uint8_t*);
extern "C" void sub_82336EB0(PPCContext& ctx, uint8_t* base) {
  if (!SetChannel(ctx, base, 44, 100, false)) __imp__sub_82336EB0(ctx, base);
}
