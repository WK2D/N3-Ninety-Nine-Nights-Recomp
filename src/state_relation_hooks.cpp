#include "native/payload_install.h"
#include "native/bounded_value.h"
#include "guest_state_payload.h"
#include "guest_handle_table.h"
#include "guest_memory_address.h"
#include <bit>

extern "C" void __imp__sub_8209D280(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209DEE0(PPCContext&, uint8_t*);
#ifdef N3_STATE_RELATION_TEST
extern thread_local bool n3_state_relation_native_used;
#endif
namespace {
using namespace n3::native;
using namespace n3::guest;
constexpr uint32_t handle_descriptor = 0x82b4807c;
constexpr uint32_t resource_pointer = 0x82b60088, percentage_unit = 0x82042048, baseline = 0x8205b614;
uint32_t Read(uint8_t* base, uint32_t at) { return rex::memory::load_and_swap<uint32_t>(GuestHostAddress(base, at)); }
float Scalar(uint8_t* base, uint32_t at) { return std::bit_cast<float>(Read(base, at)); }
void Retained(PPCContext& ctx, uint8_t* base, void (*original)(PPCContext&, uint8_t*)) {
  RetainPayloadChain payload;
  RetainHandleChain handle;
  original(ctx, base);
}
void PayloadRegisters(PPCContext& ctx, const StatePayload& payload) {
  ctx.r5.u64 = payload.words56[2]; ctx.r6.u64 = payload.words56[1]; ctx.r7.u64 = payload.word96;
  ctx.r8.u64 = payload.words56[6]; ctx.r9.u64 = payload.words56[5];
  ctx.r10.u64 = payload.words56[4]; ctx.r11.u64 = payload.words56[3];
  ctx.f0.f64 = double(payload.floats32[0]); ctx.f13.f64 = double(payload.floats32[1]);
  ctx.f12.f64 = double(payload.floats32[2]); ctx.f11.f64 = double(payload.floats48[0]);
  ctx.f10.f64 = double(payload.floats48[1]); ctx.f9.f64 = double(payload.floats84[0]);
  ctx.f8.f64 = double(payload.floats84[1]); ctx.f7.f64 = double(payload.floats84[2]);
}
void HandleRegisters(PPCContext& ctx, const HandleQuery& query, uint32_t key) {
  ctx.r3.u64 = query.key_matches ? 1 : 0;
  ctx.r4.u64 = query.record.key; ctx.r5.u64 = ctx.r3.u64;
  ctx.r6.u64 = query.key_matches ? 32 : 31; ctx.r7.u64 = query.key_matches ? 0 : 1;
  ctx.r8.u64 = 0x2aaaaaab; ctx.r9.u64 = key;
  ctx.r10.u64 = query.record.key; ctx.r11.u64 = query.key_matches ? 0 : 1;
  ctx.xer.ca = false;
  ctx.cr6.compare<uint32_t>(uint32_t(ctx.r5.u64), 0, ctx.xer);
}
bool Install(PPCContext& ctx, uint8_t* base) {
  const auto object = ctx.r3.u32, source = ctx.r4.u32, vector = ctx.r5.u32, stack = ctx.r1.u32;
  if (!ScalarExceptionsMasked(ctx) || stack < 240 || !FitsLinearGuestRange(stack - 240, 240) ||
      !FitsLinearGuestRange(object, 436) || !FitsLinearGuestRange(source, kStatePayloadSize)) return false;
  auto outside_scratch = [&](uint32_t at, uint32_t size) {
    return FitsLinearGuestRange(at, size) && DisjointGuestHostRanges(at, size, stack - 240, 240);
  };
  if (!outside_scratch(object, 436) || !outside_scratch(source, kStatePayloadSize) ||
      !DisjointGuestHostRanges(object, 436, source, kStatePayloadSize)) return false;
  if (vector && (!outside_scratch(vector, 12) ||
                 !DisjointGuestHostRanges(vector, 12, object, 436))) return false;
  const auto payload = ReadStatePayload(base, source);
  const auto kind = Read(base, object + 300);
  const Vec3 optional = vector ? Vec3{Scalar(base, vector), Scalar(base, vector + 4), Scalar(base, vector + 8)} : Vec3{};
  PayloadRelation relation{};
  HandleQuery query{};
  if (std::bit_cast<int32_t>(kind) >= 6) {
    // The payload write precedes all handle reads. Reject each possible changed
    // pointer before traversal, and a changed record before interpreting it.
    if (!DisjointGuestHostRanges(object + 336, kStatePayloadSize, handle_descriptor, 8)) return false;
    const uint64_t packed = uint64_t(payload.words4[0]) << 32 | payload.words4[1];
    if (!QueryPackedHandle(base, packed, {stack, 240}, query) ||
        !DisjointGuestHostRanges(object + 336, kStatePayloadSize, query.address, 12)) return false;
    relation.key_matches = query.key_matches;
    if (relation.key_matches) {
      const auto related = payload.words4[1];
      if (!FitsLinearGuestRange(related, 304) || !outside_scratch(related + 300, 4) ||
          !DisjointGuestHostRanges(object + 336, kStatePayloadSize, related + 300, 4)) return false;
      relation.kind = Read(base, related + 300);
      if (relation.kind == 1000) {
        if (!FitsLinearGuestRange(related, 1112) || !outside_scratch(related + 1108, 4) ||
            !DisjointGuestHostRanges(object + 336, kStatePayloadSize, related + 1108, 4)) return false;
        relation.variant = Read(base, related + 1108);
      }
    }
  }
  StatePayload state{};
  PayloadInstallUpdate update;
  if (!TryInstallStatePayload(state, payload, vector ? &optional : nullptr, kind, relation, update)) return false;
  ctx.fpscr.disableFlushMode();
  WriteStatePayload(base, object + 336, state);
  const auto object_register = ctx.r3.u64;
  ctx.r3.u64 = object_register + 336;
  PayloadRegisters(ctx, payload);
  if (vector) {
    ctx.r11.u64 = 1;
    ctx.f0.f64 = double(optional.x); ctx.f13.f64 = double(optional.y); ctx.f12.f64 = double(optional.z);
  }
  ctx.r10.u64 = kind; ctx.cr6.compare<int32_t>(std::bit_cast<int32_t>(kind), 6, ctx.xer);
  if (std::bit_cast<int32_t>(kind) >= 6) {
    HandleRegisters(ctx, query, payload.words4[0]);
    if (relation.key_matches) {
      ctx.r11.u64 = payload.words4[1]; ctx.r4.u64 = relation.kind;
      ctx.cr6.compare<int32_t>(std::bit_cast<int32_t>(relation.kind), 1000, ctx.xer);
      if (relation.kind == 1000) {
        ctx.r3.u64 = relation.variant;
        ctx.cr6.compare<int32_t>(std::bit_cast<int32_t>(relation.variant), -1, ctx.xer);
        if (update.multiplied) {
          ctx.r9.s64 = update.signed_product;
          ctx.r10.u64 = payload.words4[4]; ctx.r11.u64 = payload.words4[5];
        }
      }
    }
  }
  ctx.lr = uint32_t(ctx.lr); ctx.r12.u64 = ctx.lr;
#ifdef N3_STATE_RELATION_TEST
  n3_state_relation_native_used = true;
#endif
  return true;
}
bool RelatedValue(PPCContext& ctx, uint8_t* base) {
  const auto object = ctx.r3.u32, stack = ctx.r1.u32;
  if (!ScalarExceptionsMasked(ctx) || stack < 240 || !FitsLinearGuestRange(stack - 240, 240) ||
      !FitsLinearGuestRange(object, 970)) return false;
  auto outside_scratch = [&](uint32_t at, uint32_t size) {
    return FitsLinearGuestRange(at, size) && DisjointGuestHostRanges(at, size, stack - 240, 240);
  };
  if (!outside_scratch(object, 970) || !outside_scratch(resource_pointer, 4) ||
      !outside_scratch(percentage_unit, 4) || !outside_scratch(baseline, 4)) return false;
  HandleQuery query;
  const uint64_t packed = uint64_t(Read(base, object + 340)) << 32 | Read(base, object + 344);
  if (!QueryPackedHandle(base, packed, {stack, 240}, query)) return false;
  uint32_t related_kind = 0;
  if (query.key_matches) {
    const auto related = uint32_t(packed);
    if (!FitsLinearGuestRange(related, 304) || !outside_scratch(related + 300, 4)) return false;
    related_kind = Read(base, related + 300);
  }
  const auto category = SelectRelatedTableCategory(*GuestHostAddress(base, object + 969), query.key_matches, related_kind);
  const auto channel = SelectValueLimitChannel(Read(base, object + 300), Read(base, object + 676), Read(base, object + 780));
  if (channel >= 12) return false;
  const uint64_t descriptor = uint64_t(Read(base, resource_pointer)) + category * 16 + 216;
  if (descriptor + 8 > 0x100000000ull || !outside_scratch(uint32_t(descriptor), 8)) return false;
  const auto begin = Read(base, uint32_t(descriptor)), end = Read(base, uint32_t(descriptor) + 4);
  if (!begin || end < begin || end - begin > 0x7fffffffu || (end - begin) % 48 ||
      !FitsLinearGuestRange(begin, end - begin)) return false;
  const auto count = (end - begin) / 48, row = Read(base, object + 304);
  if (row >= count) return false;
  const uint64_t record = uint64_t(begin) + uint64_t(row) * 48;
  const uint64_t cell = record + channel * 4;
  if (cell + 4 > 0x100000000ull || !outside_scratch(uint32_t(cell), 4)) return false;
  const auto source = Read(base, uint32_t(cell));
  const ValueLimitParameters parameters{std::bit_cast<int32_t>(source),
      std::bit_cast<int32_t>(Read(base, object + 784)), 0, Scalar(base, percentage_unit), Scalar(base, baseline)};
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  ValueLimitResult result;
  if (!TryComputeValueLimit(parameters, result)) {
    ctx.fpscr.csr = saved_guest; rex::platform::FPSCRPlatform::setcsr(saved_host); return false;
  }
  ctx.r3.u64 = result.truncated_bits; ctx.r4.u64 = channel * 4;
  ctx.r5.s64 = parameters.base_value; ctx.r6.u64 = source; ctx.r7.u64 = uint64_t(row) * 3;
  ctx.r8.u64 = ctx.r1.u64 - 48; ctx.r9.u64 = uint64_t(row) * 2;
  ctx.r10.s64 = int32_t(0x82060000u); ctx.r11.u64 = record;
  ctx.f0.f64 = double(parameters.percentage_unit); ctx.f13.f64 = double(parameters.baseline);
  ctx.xer.ca = false; ctx.cr6.compare<uint32_t>(row, count, ctx.xer);
  ctx.lr = uint32_t(ctx.lr); ctx.r12.u64 = ctx.lr;
#ifdef N3_STATE_RELATION_TEST
  n3_state_relation_native_used = true;
#endif
  return true;
}
}
extern "C" void sub_8209D280(PPCContext& ctx, uint8_t* base) {
  if (!Install(ctx, base)) Retained(ctx, base, __imp__sub_8209D280);
}
extern "C" void sub_8209DEE0(PPCContext& ctx, uint8_t* base) {
  if (!RelatedValue(ctx, base)) Retained(ctx, base, __imp__sub_8209DEE0);
}
