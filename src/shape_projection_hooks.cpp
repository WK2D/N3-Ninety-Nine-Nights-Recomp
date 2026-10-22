#include "native/shape_projection.h"
#include "guest_native_support.h"
#include <rex/memory/utils.h>
#include <bit>

namespace {
using namespace n3::native;
float Read(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(rex::memory::load_and_swap<uint32_t>(base + address));
}
Vec3 Read3(uint8_t* base, uint32_t address) {
  return {Read(base, address), Read(base, address + 4), Read(base, address + 8)};
}
void Write(uint8_t* base, uint32_t address, float value) {
  rex::memory::store_and_swap<uint32_t>(base + address, std::bit_cast<uint32_t>(value));
}
void Write3(uint8_t* base, uint32_t address, Vec3 value) {
  Write(base, address, value.x); Write(base, address + 4, value.y); Write(base, address + 8, value.z);
}
bool Eligible(const PPCContext& ctx, uint32_t size, uint32_t scratch) {
  using namespace n3::guest;
  const auto source = ctx.r3.u32, output = ctx.r4.u32, stack = ctx.r1.u32;
  return ScalarExceptionsMasked(ctx) && FitsGuestRange(source, 304) && FitsGuestRange(output, size) &&
      DisjointGuestRanges(source, 304, output, size) && (!scratch ||
      (stack >= scratch && DisjointGuestRanges(source, 304, stack - scratch, scratch) &&
       DisjointGuestRanges(output, size, stack - scratch, scratch)));
}
}
extern "C" void __imp__sub_82366808(PPCContext&, uint8_t*);
extern "C" void sub_82366808(PPCContext& ctx, uint8_t* base) {
  if (!Eligible(ctx, 24, 0)) return __imp__sub_82366808(ctx, base);
  const auto source = ctx.r3.u32;
  const OrientedCapsule capsule{Read3(base, source + 68),
      {Read(base, source + 36), Read(base, source + 48), Read(base, source + 60)},
      Read(base, source + 288), Read(base, source + 292)};
  if (!SupportsCapsuleProjection(capsule)) return __imp__sub_82366808(ctx, base);
  ctx.fpscr.disableFlushMode();
  const auto projection = ProjectCapsuleBounds(capsule);
  Write3(base, ctx.r4.u32, projection.bounds.minimum);
  Write3(base, ctx.r4.u32 + 12, projection.bounds.maximum);
  ctx.f1.f64 = double(projection.negative_endpoint.minimum.x);
  ctx.cr6.compare(double(projection.negative_endpoint.maximum.z), double(projection.positive_endpoint.maximum.z));
}
extern "C" void __imp__sub_823603B0(PPCContext&, uint8_t*);
extern "C" void sub_823603B0(PPCContext& ctx, uint8_t* base) {
  if (!Eligible(ctx, 16, 96)) return __imp__sub_823603B0(ctx, base);
  const auto center = Read3(base, ctx.r3.u32 + 68), extents = Read3(base, ctx.r3.u32 + 292);
  if (!SupportsShapeValue(center) || !SupportsShapeValue(extents)) return __imp__sub_823603B0(ctx, base);
  ctx.fpscr.disableFlushMode();
  const auto radius = BoxSphereRadius(extents);
  Write3(base, ctx.r4.u32, center);
  Write(base, ctx.r4.u32 + 12, static_cast<float>(radius));
  ctx.f1.f64 = radius;
}
