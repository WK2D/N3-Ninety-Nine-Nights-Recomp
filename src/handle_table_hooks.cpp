#include "guest_handle_table.h"
#include <rex/memory/utils.h>
#include <bit>

extern "C" void __imp__sub_82081138(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82080780(PPCContext&, uint8_t*);
#ifdef N3_HANDLE_TABLE_TEST
extern thread_local bool n3_handle_table_native_used;
#endif
namespace {
constexpr uint32_t descriptor = 0x82b4807c;
uint32_t Read(uint8_t* base, uint32_t at) { return rex::memory::load_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, at)); }
void Write(uint8_t* base, uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, at), value); }
bool ScratchValid(n3::guest::HandleScratch scratch) {
  using namespace n3::guest;
  const uint64_t above = uint64_t(scratch.stack) + scratch.above_offset;
  return scratch.stack >= scratch.below && FitsLinearGuestRange(scratch.stack - scratch.below, scratch.below) &&
      (!scratch.above_size || (above <= 0xffffffffull && FitsLinearGuestRange(uint32_t(above), scratch.above_size)));
}
bool OutsideScratch(uint32_t at, uint32_t size, n3::guest::HandleScratch scratch) {
  using namespace n3::guest;
  return FitsLinearGuestRange(at, size) && DisjointGuestHostRanges(at, size, scratch.stack - scratch.below, scratch.below) &&
      (!scratch.above_size || DisjointGuestHostRanges(at, size, scratch.stack + scratch.above_offset, scratch.above_size));
}
void SetLookupRegisters(PPCContext& ctx, const n3::guest::HandleQuery& query, uint64_t incoming_index) {
  ctx.r4.u64 = query.record.key;
  ctx.r5.u64 = query.address;
  // The unsigned bounds check consumes only low 32 bits, but the later add
  // retains the incoming register's upper half.
  ctx.r6.u64 = incoming_index + uint64_t(query.index) * 2;
  ctx.r7.s64 = (int64_t(query.count * 12) * int64_t(0x2aaaaaab)) >> 32;
  ctx.r8.u64 = 0x2aaaaaab;
  ctx.r9.u64 = 0;
  ctx.r10.u64 = query.record.auxiliary;
  ctx.r11.u64 = query.record.object;
  ctx.xer.ca = false;
  ctx.cr6.compare<uint32_t>(query.index, query.count, ctx.xer);
}
bool Lookup(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  const auto output = ctx.r3.u32;
  HandleQuery query;
  if (!FitsLinearGuestRange(output, 12) || !QueryHandleRecord(base, ctx.r5.u32, {}, query) ||
      !DisjointGuestHostRanges(output, 12, query.address, 12)) return false;
  Write(base, output, query.record.key);
  Write(base, output + 4, query.record.object);
  Write(base, output + 8, query.record.auxiliary);
  SetLookupRegisters(ctx, query, ctx.r5.u64);
#ifdef N3_HANDLE_TABLE_TEST
  n3_handle_table_native_used = true;
#endif
  return true;
}
bool Validate(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  const auto packed = ctx.r3.u64;
  HandleQuery query;
  if (!QueryPackedHandle(base, packed, {ctx.r1.u32, 112, 16, 16}, query)) return false;
  SetLookupRegisters(ctx, query, query.index);
  const auto key = n3::native::PackedHandleKey(packed);
  ctx.r3.u64 = query.key_matches;
  ctx.r6.u64 = query.key_matches ? 32 : 31;
  ctx.r7.u64 = query.key_matches ? 0 : 1;
  ctx.r9.u64 = key;
  ctx.r10.u64 = query.record.key;
  ctx.r11.u64 = query.key_matches ? 0 : 1;
  ctx.r12.u64 = uint32_t(ctx.lr);
  ctx.lr = uint32_t(ctx.lr);
  ctx.cr6.compare<int32_t>(std::bit_cast<int32_t>(query.record.key), std::bit_cast<int32_t>(key), ctx.xer);
#ifdef N3_HANDLE_TABLE_TEST
  n3_handle_table_native_used = true;
#endif
  return true;
}
}
namespace n3::guest {
bool QueryHandleRecord(uint8_t* base, uint32_t index, HandleScratch scratch, HandleQuery& query) {
  if (!ScratchValid(scratch) || !OutsideScratch(descriptor, 8, scratch)) return false;
  const auto begin = Read(base, descriptor), end = Read(base, descriptor + 4);
  if (!begin || end < begin || end - begin > 0x7fffffffu || (end - begin) % 12 ||
      !FitsLinearGuestRange(begin, end - begin)) return false;
  const auto count = (end - begin) / 12;
  if (index >= count) return false;
  const auto address = begin + index * 12;
  if (!OutsideScratch(address, 12, scratch)) return false;
  const native::HandleRecord record{Read(base, address), Read(base, address + 4), Read(base, address + 8)};
  query = {record, index, count, address, false};
  return true;
}
bool QueryPackedHandle(uint8_t* base, uint64_t packed, HandleScratch scratch, HandleQuery& query) {
  HandleQuery next;
  if (!QueryHandleRecord(base, native::PackedHandleIndex(packed), scratch, next)) return false;
  next.key_matches = native::ValidateHandleKey(next.record, packed);
  query = next;
  return true;
}
}
extern "C" void sub_82081138(PPCContext& ctx, uint8_t* base) {
  if (n3::guest::retained_handle_depth || !Lookup(ctx, base)) __imp__sub_82081138(ctx, base);
}
extern "C" void sub_82080780(PPCContext& ctx, uint8_t* base) {
  if (n3::guest::retained_handle_depth || !Validate(ctx, base)) {
    n3::guest::RetainHandleChain retained;
    __imp__sub_82080780(ctx, base);
  }
}
