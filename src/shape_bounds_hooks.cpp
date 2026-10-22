#include "native/shape_bounds.h"
#include "guest_native_support.h"
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <bit>
#include <cstdint>

namespace {
using namespace n3::native;

class ShapeView {
 public:
  ShapeView(uint8_t* base, uint32_t address) : memory_(base + address) {}
  float Scalar(uint32_t offset) const {
    return std::bit_cast<float>(rex::memory::load_and_swap<uint32_t>(memory_ + offset));
  }
  Vec3 Vector(uint32_t offset) const {
    return {Scalar(offset), Scalar(offset + 4), Scalar(offset + 8)};
  }
  void Scalar(uint32_t offset, float value) const {
    rex::memory::store_and_swap<uint32_t>(memory_ + offset, std::bit_cast<uint32_t>(value));
  }
  void Vector(uint32_t offset, Vec3 value) const {
    Scalar(offset, value.x); Scalar(offset + 4, value.y); Scalar(offset + 8, value.z);
  }
  void Bounds(Bounds3 bounds) const { Vector(0, bounds.minimum); Vector(12, bounds.maximum); }
 private:
  uint8_t* memory_;
};

bool SeparateOutput(PPCContext& ctx, uint32_t output_size) {
  // Native expressions may evaluate independent components in a different
  // order. If exceptions can trap, preserve the original partial-write order.
  if (!n3::guest::ScalarExceptionsMasked(ctx)) return false;
  // The verified source fields are within the first 304 bytes. Conservative
  // alias rejection preserves the original instruction-level read/write order.
  const uint64_t source = ctx.r3.u32, output = ctx.r4.u32;
  return source + 304 <= 0x100000000ull && output + output_size <= 0x100000000ull &&
      (output + output_size <= source || source + 304 <= output);
}
}

extern "C" void __imp__sub_8235FB80(PPCContext&, uint8_t*);
extern "C" void sub_8235FB80(PPCContext& ctx, uint8_t* base) {
  if (!SeparateOutput(ctx, 24)) return __imp__sub_8235FB80(ctx, base);
  const ShapeView source(base, ctx.r3.u32);
  const auto radius = source.Scalar(288);
  const auto center = source.Vector(68);
  if (!SupportsShapeValue(radius) || !SupportsShapeValue(center))
    return __imp__sub_8235FB80(ctx, base);
  ctx.fpscr.disableFlushMode();
  ShapeView(base, ctx.r4.u32).Bounds(SphereBounds(center, radius));
}

extern "C" void __imp__sub_8235FBE8(PPCContext&, uint8_t*);
extern "C" void sub_8235FBE8(PPCContext& ctx, uint8_t* base) {
  if (!SeparateOutput(ctx, 24)) return __imp__sub_8235FBE8(ctx, base);
  const auto radius = ShapeView(base, ctx.r3.u32).Scalar(288);
  if (!SupportsShapeValue(radius)) return __imp__sub_8235FBE8(ctx, base);
  ctx.fpscr.disableFlushMode();
  ShapeView(base, ctx.r4.u32).Bounds(SphereLocalBounds(radius));
}

extern "C" void __imp__sub_82366970(PPCContext&, uint8_t*);
extern "C" void sub_82366970(PPCContext& ctx, uint8_t* base) {
  if (!SeparateOutput(ctx, 24)) return __imp__sub_82366970(ctx, base);
  const ShapeView source(base, ctx.r3.u32);
  const auto radius = source.Scalar(288), half_height = source.Scalar(292);
  if (!SupportsShapeValue(radius) || !SupportsShapeValue(half_height))
    return __imp__sub_82366970(ctx, base);
  ctx.fpscr.disableFlushMode();
  ShapeView(base, ctx.r4.u32).Bounds(CapsuleLocalBounds(radius, half_height));
}

extern "C" void __imp__sub_82360478(PPCContext&, uint8_t*);
extern "C" void sub_82360478(PPCContext& ctx, uint8_t* base) {
  if (!SeparateOutput(ctx, 24)) return __imp__sub_82360478(ctx, base);
  const auto extents = ShapeView(base, ctx.r3.u32).Vector(292);
  if (!SupportsShapeValue(extents)) return __imp__sub_82360478(ctx, base);
  ctx.fpscr.disableFlushMode();
  ShapeView(base, ctx.r4.u32).Bounds(BoxLocalBounds(extents));
}

extern "C" void __imp__sub_82366918(PPCContext&, uint8_t*);
extern "C" void sub_82366918(PPCContext& ctx, uint8_t* base) {
  if (!SeparateOutput(ctx, 16)) return __imp__sub_82366918(ctx, base);
  const ShapeView source(base, ctx.r3.u32);
  const auto center = source.Vector(68);
  const auto radius = source.Scalar(288), half_height = source.Scalar(292);
  if (!SupportsShapeValue(center) || !SupportsShapeValue(radius) ||
      !SupportsShapeValue(half_height)) return __imp__sub_82366918(ctx, base);
  ctx.fpscr.disableFlushMode();
  const auto envelope = CapsuleSphereEnvelope(center, radius, half_height);
  const ShapeView output(base, ctx.r4.u32);
  output.Vector(0, envelope.center); output.Scalar(12, envelope.radius);
}
