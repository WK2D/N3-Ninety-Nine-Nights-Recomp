#include "native/bounded_value.h"
#include "guest_native_support.h"
#include <rex/memory/utils.h>
#include <array>
#include <bit>

extern "C" void __imp__sub_8209DBF0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D718(PPCContext&, uint8_t*);
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
  uint32_t channel, record, count;
};
bool ReadLimit(PPCContext& ctx, uint8_t* base, uint32_t scratch, LimitInputs& input) {
  using namespace n3::guest;
  const auto object = ctx.r3.u32, stack = ctx.r1.u32;
  if (!ScalarExceptionsMasked(ctx) || stack < scratch || !FitsGuestRange(object, 970)) return false;
  const auto channel = SelectValueLimitChannel(Read(base, object + 300), Read(base, object + 676), Read(base, object + 780));
  // A verified record is 48 bytes. Unknown channels retain the original address
  // arithmetic rather than reading a different record through a native index.
  if (channel >= 12) return false;
  const auto root = Read(base, table_pointer);
  const uint64_t metadata = uint64_t(root) + uint32_t(base[object + 969]) * 16 + 88;
  if (metadata + 8 > 0x100000000ull) return false;
  const auto begin = Read(base, uint32_t(metadata)), end = Read(base, uint32_t(metadata) + 4);
  if (!begin || end < begin || end - begin > 0x7fffffffu || (end - begin) % 48) return false;
  const auto count = (end - begin) / 48, record = Read(base, object + 304);
  if (record >= count) return false;
  const uint64_t selected = uint64_t(begin) + uint64_t(record) * 48 + channel * 4;
  if (selected + 4 > 0x100000000ull) return false;
  struct Range { uint32_t start, size; };
  const std::array<Range, 7> sources{{{object, 970}, {table_pointer, 4}, {uint32_t(metadata), 8},
      {uint32_t(selected), 4}, {percentage_unit, 4}, {baseline, 4}, {lower_bound, 4}}};
  for (auto source : sources)
    if (!DisjointGuestRanges(source.start, source.size, stack - scratch, scratch)) return false;
  input = {{std::bit_cast<int32_t>(Read(base, uint32_t(selected))),
            std::bit_cast<int32_t>(Read(base, object + 728)), Read(base, object + 732),
            Float(base, percentage_unit), Float(base, baseline)}, channel, record, count};
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
