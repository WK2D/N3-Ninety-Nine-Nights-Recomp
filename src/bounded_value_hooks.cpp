#include "native/bounded_value.h"
#include "guest_native_support.h"
#include <rex/memory/utils.h>
#include <bit>

extern "C" void __imp__sub_8209DBF0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D718(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209DCF0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209DDE8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_820A13B0(PPCContext&, uint8_t*);
#ifdef N3_BOUNDED_ORACLE_SWITCH
// The differential target can run the retained caller and its retained callee
// together. This switch is absent from the game build.
extern thread_local bool n3_bounded_original_callee;
#endif
namespace {
using namespace n3::native;
constexpr uint32_t table_pointer = 0x82b60088, percentage_unit = 0x82042048;
constexpr uint32_t baseline = 0x8205b614, lower_bound = 0x8205b610;
uint32_t Read(uint8_t* base, uint32_t at) { return rex::memory::load_and_swap<uint32_t>(base + at); }
float Float(uint8_t* base, uint32_t at) { return std::bit_cast<float>(Read(base, at)); }
struct LimitInputs {
  ValueLimitParameters parameters;
  uint32_t channel, record, count, record_address;
};
struct LimitLayout { uint32_t descriptor, percentage; bool additive; };
constexpr LimitLayout additive_layout{88, 728, true};
bool ReadLimit(PPCContext& ctx, uint8_t* base, uint32_t scratch, LimitInputs& input,
               LimitLayout layout = additive_layout, uint32_t pending_write = 0) {
  using namespace n3::guest;
  const auto object = ctx.r3.u32, stack = ctx.r1.u32;
  if (!ScalarExceptionsMasked(ctx) || stack < scratch || !FitsGuestRange(object, 970)) return false;
  // A retained wrapper writes its frame before reading the object or globals.
  // Reject each source before traversing it: old aliased pointer bits need not
  // designate readable memory even when the original post-prologue bits do.
  auto outside_scratch = [&](uint32_t at, uint32_t size) {
    return DisjointGuestRanges(at, size, stack - scratch, scratch);
  };
  if (!outside_scratch(object, 970) || !outside_scratch(table_pointer, 4) ||
      !outside_scratch(percentage_unit, 4) || !outside_scratch(baseline, 4) ||
      !outside_scratch(lower_bound, 4)) return false;
  // The modifier setter writes before the original table lookup. Reject aliases
  // before following any old pointer that the write would have replaced.
  auto separate_write = [&](uint32_t at, uint32_t size) {
    return !pending_write || DisjointGuestRanges(pending_write, 4, at, size);
  };
  if (!separate_write(table_pointer, 4) || !separate_write(percentage_unit, 4) || !separate_write(baseline, 4)) return false;
  const auto channel = SelectValueLimitChannel(Read(base, object + 300), Read(base, object + 676), Read(base, object + 780));
  // A verified record is 48 bytes. Unknown channels retain the original address
  // arithmetic rather than reading a different record through a native index.
  if (channel >= 12) return false;
  const auto root = Read(base, table_pointer);
  const uint64_t metadata = uint64_t(root) + uint32_t(base[object + 969]) * 16 + layout.descriptor;
  if (metadata + 8 > 0x100000000ull) return false;
  if (!outside_scratch(uint32_t(metadata), 8) || !separate_write(uint32_t(metadata), 8)) return false;
  const auto begin = Read(base, uint32_t(metadata)), end = Read(base, uint32_t(metadata) + 4);
  if (!begin || end < begin || end - begin > 0x7fffffffu || (end - begin) % 48) return false;
  const auto count = (end - begin) / 48, record = Read(base, object + 304);
  if (record >= count) return false;
  const uint64_t selected = uint64_t(begin) + uint64_t(record) * 48 + channel * 4;
  if (selected + 4 > 0x100000000ull) return false;
  if (!outside_scratch(uint32_t(selected), 4) || !separate_write(uint32_t(selected), 4)) return false;
  input = {{std::bit_cast<int32_t>(Read(base, uint32_t(selected))),
            std::bit_cast<int32_t>(Read(base, object + layout.percentage)), layout.additive ? Read(base, object + 732) : 0,
            Float(base, percentage_unit), Float(base, baseline)}, channel, record, count, uint32_t(selected) - channel * 4};
  return true;
}
bool Compute(PPCContext& ctx, uint8_t* base, bool adjust) {
  LimitInputs inputs;
  if (!ReadLimit(ctx, base, adjust ? 128 : 16, inputs)) return false;
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  ValueLimitResult limit;
  BoundedValueState state{adjust ? Float(base, ctx.r3.u32 + 664) : 0};
  BoundedValueUpdate update;
  const auto lower = adjust ? Float(base, lower_bound) : 0;
  if (!TryComputeValueLimit(inputs.parameters, limit) ||
      (adjust && !TryAdjustBoundedValue(state, ctx.r4.s32, std::bit_cast<int32_t>(uint32_t(limit.value)), lower, update))) {
    ctx.fpscr.csr = saved_guest; rex::platform::FPSCRPlatform::setcsr(saved_host); return false;
  }
  const auto object = ctx.r3.u32;
  ctx.r3.u64 = limit.value;
  ctx.r4.u64 = uint64_t(inputs.record) * 3;
  ctx.r7.u64 = inputs.channel * 4;
  ctx.r9.u64 = inputs.parameters.additive_offset;
  ctx.xer.ca = false; // Positive, whole-record table length in the supported path.
  if (adjust) {
    rex::memory::store_and_swap<uint32_t>(base + object + 664, std::bit_cast<uint32_t>(state.value));
    ctx.f0.f64 = double(update.upper_bound);
    ctx.cr6.compare(double(update.lower_clamped ? lower : update.sum), double(update.upper_bound));
    ctx.lr = uint32_t(ctx.lr);
  } else {
    ctx.f0.f64 = double(inputs.parameters.percentage_unit);
    ctx.r11.u64 = limit.truncated_bits;
    ctx.cr6.compare<uint32_t>(inputs.record, inputs.count, ctx.xer);
  }
  return true;
}
bool ComputeGetter(PPCContext& ctx, uint8_t* base, LimitLayout layout) {
  LimitInputs inputs;
  if (!ReadLimit(ctx, base, 16, inputs, layout)) return false;
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  ValueLimitResult limit;
  if (!TryComputeValueLimit(inputs.parameters, limit)) {
    ctx.fpscr.csr = saved_guest; rex::platform::FPSCRPlatform::setcsr(saved_host); return false;
  }
  ctx.r3.u64 = limit.truncated_bits;
  ctx.r4.u64 = std::bit_cast<uint32_t>(inputs.parameters.percentage_adjustment);
  ctx.r5.s64 = inputs.parameters.base_value;
  ctx.r6.u64 = std::bit_cast<uint32_t>(inputs.parameters.base_value);
  ctx.r7.u64 = uint64_t(inputs.record) * 3; ctx.r9.u64 = uint64_t(inputs.record) * 2;
  ctx.r11.u64 = inputs.record_address;
  ctx.f0.f64 = double(inputs.parameters.percentage_unit);
  ctx.xer.ca = false;
  ctx.cr6.compare<uint32_t>(inputs.record, inputs.count, ctx.xer);
  return true;
}
bool SetModifier(PPCContext& ctx, uint8_t* base) {
  if (!n3::guest::FitsGuestRange(ctx.r3.u32, 970)) return false;
  const auto target = ctx.r3.u32 + ((ctx.r5.u32 & 0xff) ? 728 : 732);
  LimitInputs inputs;
  if (!ReadLimit(ctx, base, 128, inputs, additive_layout, target)) return false;
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  ValueLimitState state{inputs.parameters, Float(base, ctx.r3.u32 + 664)};
  ValueLimitModifierUpdate update;
  const auto replacement = ctx.r4.u32;
  if (!TrySetValueLimitModifier(state, replacement, ctx.r5.u32, update)) {
    ctx.fpscr.csr = saved_guest; rex::platform::FPSCRPlatform::setcsr(saved_host); return false;
  }
  rex::memory::store_and_swap<uint32_t>(base + target, replacement);
  if (update.upper_clamped)
    rex::memory::store_and_swap<uint32_t>(base + ctx.r3.u32 + 664, std::bit_cast<uint32_t>(state.value));
  ctx.r3.u64 = update.limit.value; ctx.r4.u64 = uint64_t(inputs.record) * 3;
  ctx.r7.u64 = inputs.channel * 4; ctx.r9.u64 = state.parameters.additive_offset;
  ctx.r11.u64 = update.limit.truncated_bits;
  ctx.f0.f64 = double(update.upper_bound); ctx.f13.f64 = double(update.previous_value);
  ctx.xer.ca = false; ctx.cr6.compare(double(update.previous_value), double(update.upper_bound));
  ctx.lr = uint32_t(ctx.lr);
  return true;
}
}
extern "C" void sub_8209DBF0(PPCContext& ctx, uint8_t* base) {
#ifdef N3_BOUNDED_ORACLE_SWITCH
  if (n3_bounded_original_callee) return __imp__sub_8209DBF0(ctx, base);
#endif
  if (!Compute(ctx, base, false)) __imp__sub_8209DBF0(ctx, base);
}
extern "C" void sub_8209D718(PPCContext& ctx, uint8_t* base) {
  if (!Compute(ctx, base, true)) __imp__sub_8209D718(ctx, base);
}
extern "C" void sub_8209DCF0(PPCContext& ctx, uint8_t* base) {
  if (!ComputeGetter(ctx, base, {8, 736, false})) __imp__sub_8209DCF0(ctx, base);
}
extern "C" void sub_8209DDE8(PPCContext& ctx, uint8_t* base) {
  if (!ComputeGetter(ctx, base, {40, 740, false})) __imp__sub_8209DDE8(ctx, base);
}
extern "C" void sub_820A13B0(PPCContext& ctx, uint8_t* base) {
  if (!SetModifier(ctx, base)) __imp__sub_820A13B0(ctx, base);
}
