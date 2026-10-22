#include "native/state_queries.h"
#include "guest_memory_address.h"
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <bit>

#ifdef N3_STATE_QUERIES_TEST
extern thread_local bool n3_state_queries_native_used;
#endif
namespace {
using namespace n3::guest;
using namespace n3::native;
constexpr uint32_t flags_address = 0x825d9fa8;
uint32_t Read(uint8_t* base, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(GuestHostAddress(base, address));
}
void Used() {
#ifdef N3_STATE_QUERIES_TEST
  n3_state_queries_native_used = true;
#endif
}
struct KindLookup { uint32_t kind, offset, flags; };
bool Lookup(uint8_t* base, uint32_t object, KindLookup& result) {
  const uint32_t field = object + 496;
  if (!FitsLinearGuestRange(field, 4)) return false;
  const auto kind = Read(base, field), offset = kind * 4u;
  const uint32_t cell = flags_address + offset;
  if (!FitsLinearGuestRange(cell, 4)) return false;
  result = {kind, offset, Read(base, cell)};
  return true;
}
void Publish(PPCContext& ctx, KindLookup lookup, uint32_t mask) {
  const auto decision = QueryKindFlags({lookup.flags}, mask);
  ctx.r3.u64 = decision.enabled;
  ctx.r7.u64 = decision.selected_bits;
  ctx.r8.u64 = lookup.flags; ctx.r9.u64 = lookup.offset; ctx.r10.u64 = lookup.kind;
  ctx.r11.u64 = uint64_t(int64_t(std::bit_cast<int32_t>(flags_address)));
  ctx.cr6.compare<int32_t>(std::bit_cast<int32_t>(decision.selected_bits), 0, ctx.xer);
}
bool Query(PPCContext& ctx, uint8_t* base, uint32_t mask) {
  KindLookup lookup;
  if (!Lookup(base, ctx.r3.u32, lookup)) return false;
  Publish(ctx, lookup, mask); Used(); return true;
}
bool RelatedQuery(PPCContext& ctx, uint8_t* base) {
  const uint32_t object = ctx.r3.u32, homes = ctx.r1.u32 + 32;
  const uint32_t tag_field = object + 164, value_field = object + 168;
  auto outside_homes = [&](uint32_t address, uint32_t size) {
    return FitsLinearGuestRange(address, size) && DisjointGuestHostRanges(homes, 16, address, size);
  };
  if (!FitsLinearGuestRange(homes, 16) || !outside_homes(tag_field, 4)) return false;
  const auto tag = Read(base, tag_field), argument_tag = uint32_t(ctx.r4.u64 >> 32);
  const auto argument_value = ctx.r4.u32;
  uint32_t value = 0;
  const bool argument_tag_matches = tag == argument_tag;
  if (argument_tag_matches) {
    if (!outside_homes(value_field, 4)) return false;
    value = Read(base, value_field);
  }
  const bool argument_matches = argument_tag_matches && SameStateIdentity({tag, value}, {argument_tag, argument_value});
  const uint64_t related_full = uint64_t(argument_value) + 844;
  const auto related = uint32_t(related_full), scratch = ctx.r1.u32 - 8;
  uint32_t related_tag = 0, related_value = 0, related_extra = 0;
  bool related_tag_matches = false, related_matches = false;
  KindLookup lookup{};
  if (!argument_matches) {
    if (!outside_homes(related, 12) || !FitsLinearGuestRange(scratch, 4)) return false;
    related_tag = Read(base, related); related_value = Read(base, related + 4); related_extra = Read(base, related + 8);
    related_tag_matches = related_tag == tag;
    if (related_tag_matches) {
      if (!outside_homes(value_field, 4) || !DisjointGuestHostRanges(scratch, 4, value_field, 4)) return false;
      value = Read(base, value_field);
      related_matches = SameStateIdentity({tag, value}, {related_tag, related_value});
    }
    if (!related_matches) {
      const uint32_t kind_field = object + 496;
      if (!outside_homes(kind_field, 4) || !DisjointGuestHostRanges(scratch, 4, kind_field, 4)) return false;
      lookup.kind = Read(base, kind_field); lookup.offset = lookup.kind * 4u;
      const uint32_t cell = flags_address + lookup.offset;
      if (!outside_homes(cell, 4) || !DisjointGuestHostRanges(scratch, 4, cell, 4)) return false;
      lookup.flags = Read(base, cell);
    }
  }
  rex::memory::store_and_swap<uint64_t>(GuestHostAddress(base, homes), ctx.r4.u64);
  rex::memory::store_and_swap<uint64_t>(GuestHostAddress(base, homes + 8), ctx.r5.u64);
  ctx.r10.u64 = argument_value;
  ctx.r11.u64 = argument_matches;
  if (argument_tag_matches) ctx.r9.u64 = value;
  ctx.r8.u64 = argument_matches;
  ctx.cr6.compare<uint32_t>(argument_matches, 0, ctx.xer);
  ctx.r3.u64 = 0;
  if (!argument_matches) {
    ctx.r7.u64 = related_full; ctx.r5.u64 = tag; ctx.r6.u64 = related_extra;
    ctx.r10.u64 = related_value; ctx.r11.u64 = related_matches;
    if (related_tag_matches) ctx.r4.u64 = value;
    rex::memory::store_and_swap<uint32_t>(GuestHostAddress(base, scratch), related_extra);
    ctx.cr6.compare<uint32_t>(related_matches, 0, ctx.xer);
    if (!related_matches) Publish(ctx, lookup, 8);
  }
  Used(); return true;
}
bool CategoryQuery(PPCContext& ctx, uint8_t* base) {
  const auto object = ctx.r3.u32;
  const uint32_t owner_field = object + 316, row_field = object + 192;
  if (!FitsLinearGuestRange(owner_field, 4) || !FitsLinearGuestRange(row_field, 4)) return false;
  const auto owner = Read(base, owner_field), offset = Read(base, row_field) * 32u;
  const uint32_t begin_field = owner + 444;
  if (!FitsLinearGuestRange(begin_field, 4)) return false;
  const auto begin = Read(base, begin_field);
  const uint64_t record_full = uint64_t(begin) + offset;
  const uint32_t category_field = uint32_t(record_full) + 4;
  if (!FitsLinearGuestRange(category_field, 4)) return false;
  const auto category = Read(base, category_field);
  KindLookup lookup{};
  const bool allowed = CategoryAllowsKindFlags(std::bit_cast<int32_t>(category));
  if (allowed && !Lookup(base, object, lookup)) return false;
  ctx.r11.u64 = begin; ctx.r10.u64 = offset; ctx.r9.u64 = record_full; ctx.r8.u64 = category;
  ctx.r3.u64 = 0; ctx.cr6.compare<int32_t>(std::bit_cast<int32_t>(category), 1024, ctx.xer);
  if (allowed) {
    const auto decision = QueryKindFlags({lookup.flags}, 16);
    ctx.r7.u64 = lookup.kind; ctx.r6.u64 = lookup.offset; ctx.r5.u64 = lookup.flags;
    ctx.r4.u64 = decision.selected_bits; ctx.r3.u64 = decision.enabled;
    ctx.r11.u64 = uint64_t(int64_t(std::bit_cast<int32_t>(flags_address)));
    ctx.cr6.compare<int32_t>(int32_t(decision.selected_bits), 0, ctx.xer);
  }
  Used(); return true;
}
void ByteQuery(PPCContext& ctx, uint8_t* base) {
  const auto object = ctx.r3.u32;
  const auto request = uint8_t(ctx.r4.u32);
  uint8_t guard = 0;
  ctx.r11.u64 = request; ctx.cr6.compare<uint32_t>(request, 0, ctx.xer);
  if (request) {
    guard = *GuestHostAddress(base, object + 160);
    ctx.r10.u64 = guard; ctx.cr6.compare<uint32_t>(guard, 0, ctx.xer);
  }
  const auto stored = request && !guard ? uint8_t{0} : *GuestHostAddress(base, object + 716);
  ctx.r3.u64 = SelectStateByte(request, guard, stored); Used();
}
}
#define DEFINE_QUERY(address, mask) \
  extern "C" void __imp__sub_##address(PPCContext&, uint8_t*); \
  extern "C" void sub_##address(PPCContext& ctx, uint8_t* base) { \
    if (!Query(ctx, base, mask)) __imp__sub_##address(ctx, base); }
DEFINE_QUERY(8209E338, 1)
DEFINE_QUERY(8209E368, 2)
DEFINE_QUERY(8209E398, 4)
DEFINE_QUERY(8209E470, 0x20)
DEFINE_QUERY(8209E4A0, 0x80000)
DEFINE_QUERY(8209E4D0, 0x100000)
DEFINE_QUERY(8209E500, 0x40000)
DEFINE_QUERY(8209E530, 0x20000)
DEFINE_QUERY(8209E560, 0x400000)
DEFINE_QUERY(8209E688, 0x10000)
DEFINE_QUERY(8209E6B8, 0x800000)
#undef DEFINE_QUERY
extern "C" void __imp__sub_8209E3C8(PPCContext&, uint8_t*);
extern "C" void sub_8209E3C8(PPCContext& ctx, uint8_t* base) {
  if (!RelatedQuery(ctx, base)) __imp__sub_8209E3C8(ctx, base);
}
extern "C" void __imp__sub_8209E6E8(PPCContext&, uint8_t*);
extern "C" void sub_8209E6E8(PPCContext& ctx, uint8_t* base) {
  if (!CategoryQuery(ctx, base)) __imp__sub_8209E6E8(ctx, base);
}
extern "C" void sub_8209E590(PPCContext& ctx, uint8_t* base) { ByteQuery(ctx, base); }
