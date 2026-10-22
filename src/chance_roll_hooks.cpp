#include "native/chance_roll.h"
#include "guest_memory_address.h"
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <bit>

extern "C" void __imp__sub_8209E230(PPCContext&, uint8_t*);
#ifdef N3_CHANCE_ROLL_TEST
extern thread_local bool n3_chance_roll_native_used;
#endif
namespace {
constexpr uint32_t seed_address = 0x825d9d74;
uint32_t Read(uint8_t* base, uint32_t at) {
  return rex::memory::load_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, at));
}
bool Process(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  const auto object = ctx.r3.u32, owner = ctx.r4.u32;
  const auto homes = ctx.r1.u32 + 32, enabled_address = owner + 812;
  // The original publishes r4/r5 argument homes before its first source read.
  // Reject those aliases before following any potentially overwritten value.
  if (!FitsLinearGuestRange(homes, 16) || !FitsLinearGuestRange(object, 772) ||
      !DisjointGuestHostRanges(homes, 16, object, 772) ||
      !DisjointGuestHostRanges(homes, 16, enabled_address, 1) ||
      !DisjointGuestHostRanges(homes, 16, seed_address, 4)) return false;
  const bool enabled = *GuestHostAddress(base, enabled_address) != 0;
  const auto alternate = enabled ? std::bit_cast<int32_t>(Read(base, object + 768)) : 0;
  const auto normal = enabled && alternate > 0 ? 0 : std::bit_cast<int32_t>(Read(base, object + 764));
  const auto previous = Read(base, seed_address);
  n3::native::RandomSequenceState state{previous};
  const auto decision = n3::native::RollChance(state, {normal, alternate, enabled});
  rex::memory::store_and_swap<uint64_t>(GuestHostAddress(base, homes), ctx.r4.u64);
  rex::memory::store_and_swap<uint64_t>(GuestHostAddress(base, homes + 8), ctx.r5.u64);
  rex::memory::store_and_swap<uint32_t>(GuestHostAddress(base, seed_address), state.value);
  // Retain the translated caller-saved integer results, including the signed
  // wide product that survives the word-sized seed store. No FP state changes.
  const auto wide = int64_t(std::bit_cast<int32_t>(previous)) * 134775813 + 1;
  const auto high = (uint64_t(state.value) * 0x51eb851fu) >> 32;
  ctx.r4.u64 = high;
  ctx.r5.u64 = 0x51eb851f;
  ctx.r6.u64 = 0x51eb0000;
  ctx.r7.u64 = 134775813;
  ctx.r9.u64 = std::bit_cast<uint32_t>(decision.threshold);
  ctx.r10.u64 = uint64_t(state.value / 100) * 100;
  ctx.r11.s64 = wide;
  ctx.r8.u64 = uint64_t(wide) - ctx.r10.u64;
  ctx.r3.u64 = decision.accepted ? 1 : 0;
  ctx.cr6.compare<int32_t>(int32_t(decision.sample), decision.threshold, ctx.xer);
#ifdef N3_CHANCE_ROLL_TEST
  n3_chance_roll_native_used = true;
#endif
  return true;
}
}
extern "C" void sub_8209E230(PPCContext& ctx, uint8_t* base) {
  if (!Process(ctx, base)) __imp__sub_8209E230(ctx, base);
}
