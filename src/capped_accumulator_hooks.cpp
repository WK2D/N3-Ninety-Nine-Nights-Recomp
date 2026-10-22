#include "native/capped_accumulator.h"
#include "guest_native_support.h"
#include <rex/memory/utils.h>
#include <bit>

extern "C" void __imp__sub_8209D7A8(PPCContext&, uint8_t*);
namespace {
constexpr uint32_t percentage_unit = 0x82042048, baseline = 0x8205b614;
constexpr uint32_t accounting_pointer = 0x82b60080;
uint32_t Read(uint8_t* base, uint32_t at) {
  return rex::memory::load_and_swap<uint32_t>(base + at);
}
void Write(uint8_t* base, uint32_t at, uint32_t bits) {
  rex::memory::store_and_swap<uint32_t>(base + at, bits);
}
bool Process(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  const auto object = ctx.r3.u32, stack = ctx.r1.u32;
  if (!ScalarExceptionsMasked(ctx) || stack < 16 || !FitsGuestRange(object, 828)) return false;
  auto outside_scratch = [&](uint32_t at, uint32_t size) {
    return DisjointGuestRanges(at, size, stack - 16, 16);
  };
  if (!outside_scratch(object, 828) || !outside_scratch(percentage_unit, 4) ||
      !outside_scratch(baseline, 4)) return false;
  const auto value_address = object + 680;
  // Value publication precedes the original owner/flag/global reads. Reject an
  // alias before following a pointer or testing bits that publication changes.
  const auto owner = Read(base, object + 824);
  uint32_t flag = 0, root = 0, total = 0, count = 0;
  if (owner) {
    if (!FitsGuestRange(owner, 697) || !outside_scratch(owner + 696, 1) ||
        !DisjointGuestRanges(value_address, 4, owner + 696, 1)) return false;
    flag = base[owner + 696];
    if (flag) {
      if (!outside_scratch(accounting_pointer, 4) ||
          !DisjointGuestRanges(value_address, 4, accounting_pointer, 4)) return false;
      root = Read(base, accounting_pointer);
      if (!FitsGuestRange(root, 4716) || !outside_scratch(root + 4708, 8) ||
          !DisjointGuestRanges(value_address, 4, root + 4708, 8)) return false;
      count = Read(base, root + 4708); total = Read(base, root + 4712);
    }
  }
  const n3::native::CappedAccumulatorParameters parameters{
      std::bit_cast<int32_t>(Read(base, object + 760)),
      std::bit_cast<float>(Read(base, percentage_unit)), std::bit_cast<float>(Read(base, baseline))};
  n3::native::CappedAccumulatorState state{Read(base, value_address), total, count};
  n3::native::CappedAccumulatorUpdate update;
  const auto delta = ctx.r4.s32;
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  if (!n3::native::TryAccumulateCappedValue(state, parameters, delta, flag != 0, update)) {
    ctx.fpscr.csr = saved_guest; rex::platform::FPSCRPlatform::setcsr(saved_host); return false;
  }
  Write(base, value_address, state.value);
  if (flag) {
    // Both counters were loaded before either original counter write. Aliases
    // with already-consumed sources are consequently preserved by these stores.
    Write(base, root + 4712, state.tracked_total); Write(base, root + 4708, state.tracked_count);
    ctx.r3.u64 = update.tracked_count_sum; ctx.r4.u64 = update.tracked_total_sum;
    ctx.r9.u64 = total; ctx.r10.u64 = count; ctx.r11.u64 = root;
  } else {
    ctx.r9.s64 = -2113536000; ctx.r10.u64 = 999999; ctx.r11.u64 = owner;
  }
  if (owner) ctx.r5.u64 = flag;
  ctx.r6.u64 = 983040; ctx.r7.s64 = ctx.r1.s64 - 16; ctx.r8.u64 = update.scaled_delta_bits;
  ctx.f0.f64 = double(parameters.percentage_unit);
  ctx.f5.s64 = std::bit_cast<int32_t>(update.scaled_delta_bits);
  ctx.f6.f64 = double(update.scaled_delta); ctx.f7.f64 = double(update.factor);
  ctx.f8.f64 = double(update.rounded_percentage); ctx.f9.f64 = double(update.rounded_delta);
  ctx.f10.f64 = double(parameters.percentage_adjustment);
  ctx.f11.s64 = parameters.percentage_adjustment; ctx.f12.f64 = double(delta);
  ctx.f13.f64 = double(parameters.baseline);
  if (owner) ctx.cr6.compare<uint32_t>(flag, 0, ctx.xer);
  else ctx.cr6.compare<uint32_t>(0, 0, ctx.xer);
  return true;
}
}
extern "C" void sub_8209D7A8(PPCContext& ctx, uint8_t* base) {
  if (!Process(ctx, base)) __imp__sub_8209D7A8(ctx, base);
}
