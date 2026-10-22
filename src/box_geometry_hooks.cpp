#include "native/box_geometry.h"
#include "guest_native_support.h"
#include "guest_memory_address.h"
#include <rex/memory/utils.h>
#include <bit>

#ifdef N3_BOX_GEOMETRY_TEST
extern thread_local bool n3_box_geometry_native_used;
#endif

namespace {
using namespace n3::native;
float Read(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(rex::memory::load_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address)));
}
Vec3 Read3(uint8_t* base, uint32_t address) {
  return {Read(base, address), Read(base, address + 4), Read(base, address + 8)};
}
TransformedBox ReadBox(uint8_t* base, uint32_t object) {
  return {{{Read3(base, object + 32), Read3(base, object + 44), Read3(base, object + 56)},
           Read3(base, object + 68)}, Read3(base, object + 292)};
}
void Write3(uint8_t* base, uint32_t address, Vec3 value) {
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address), std::bit_cast<uint32_t>(value.x));
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address + 4), std::bit_cast<uint32_t>(value.y));
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address + 8), std::bit_cast<uint32_t>(value.z));
}
bool Eligible(const PPCContext& ctx, uint32_t output, uint32_t size, uint32_t scratch_size) {
  using namespace n3::guest;
  const auto object = ctx.r3.u32, stack = ctx.r1.u32;
  using Platform = rex::platform::FPSCRPlatform;
  if (!(ctx.fpscr.csr & Platform::FlushMask) && (Platform::getcsr() & Platform::FlushMask)) return false;
  return ScalarExceptionsMasked(ctx) && FitsLinearGuestRange(object, 304) && FitsLinearGuestRange(output, size) &&
      DisjointGuestHostRanges(object, 304, output, size) && stack >= scratch_size &&
      FitsLinearGuestRange(stack - scratch_size, scratch_size) &&
      DisjointGuestHostRanges(object, 304, stack - scratch_size, scratch_size) &&
      DisjointGuestHostRanges(output, size, stack - scratch_size, scratch_size);
}
void NativeUsed() {
#ifdef N3_BOX_GEOMETRY_TEST
  n3_box_geometry_native_used = true;
#endif
}
}
extern "C" void __imp__sub_823602C0(PPCContext&, uint8_t*);
extern "C" void sub_823602C0(PPCContext& ctx, uint8_t* base) {
  const auto output = ctx.r4.u32;
  if (!Eligible(ctx, output, 24, 24)) return __imp__sub_823602C0(ctx, base);
  const auto box = ReadBox(base, ctx.r3.u32);
  if (!SupportsBoxGeometry(box)) return __imp__sub_823602C0(ctx, base);
  ctx.fpscr.disableFlushMode();
  const auto bounds = TransformedBoxBounds(box);
  Write3(base, output, bounds.minimum); Write3(base, output + 12, bounds.maximum);
  NativeUsed();
}
extern "C" void __imp__sub_823604B8(PPCContext&, uint8_t*);
extern "C" void sub_823604B8(PPCContext& ctx, uint8_t* base) {
  const auto output = ctx.r7.u32;
  if (!Eligible(ctx, output, 12, 64)) return __imp__sub_823604B8(ctx, base);
  const auto box = ReadBox(base, ctx.r3.u32);
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr();
  const auto saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  Vec3 result{};
  if (!TryTransformBoxSample(box, {ctx.r4.s32, ctx.r5.s32, ctx.r6.s32}, result)) {
    ctx.fpscr.csr = saved_guest;
    rex::platform::FPSCRPlatform::setcsr(saved_host);
    return __imp__sub_823604B8(ctx, base);
  }
  Write3(base, output, result);
  ctx.lr = uint32_t(ctx.lr);
  NativeUsed();
}
