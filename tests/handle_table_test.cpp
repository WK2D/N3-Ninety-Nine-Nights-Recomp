#include <Windows.h>
#include "guest_handle_table.h"
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

extern "C" void sub_82081138(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82081138(PPCContext&, uint8_t*);
extern "C" void sub_82080780(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82080780(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_handle_table_native_used = false;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 5> regions{{{0, 0x100000}, {0x82b47000, 0x3000},
    {0xdfff0000, 0x30000}, {0xe1000000, 0x30000}, {0xffffb000, 0x4000}}};
constexpr uint32_t descriptor = 0x82b4807c;
// Independent implementation of the generated Windows shadow mapping.
uint64_t HostOffset(uint32_t address) { return uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u); }
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve sparse handle fixture");
    for (const auto region : regions) if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
      VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit sparse handle fixture");
    }
  }
  ~Guest() { VirtualFree(base, 0, MEM_RELEASE); }
  Guest(const Guest&) = delete;
  void Copy(const Guest& other) { for (auto region : regions) std::memcpy(base + region.start, other.base + region.start, region.size); }
  bool Same(const Guest& other) const {
    for (auto region : regions) if (std::memcmp(base + region.start, other.base + region.start, region.size)) return false;
    return true;
  }
  void Word(uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(base + HostOffset(at), value); }
  void Table(uint32_t begin, uint32_t end) { Word(descriptor, begin); Word(descriptor + 4, end); }
};
void CheckRegisters(const PPCContext& expected, const PPCContext& actual) {
#define CHECK(n) if (expected.r##n.u64 != actual.r##n.u64 || expected.f##n.u64 != actual.f##n.u64) { \
    std::cerr << "r/f" #n " expected " << std::hex << expected.r##n.u64 << '/' << expected.f##n.u64 \
              << " actual " << actual.r##n.u64 << '/' << actual.f##n.u64 << std::dec << '\n'; \
    throw std::runtime_error("Handle GPR or FP register differs"); }
  CHECK(0) CHECK(1) CHECK(2) CHECK(3) CHECK(4) CHECK(5) CHECK(6) CHECK(7) CHECK(8) CHECK(9)
  CHECK(10) CHECK(11) CHECK(12) CHECK(13) CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19)
  CHECK(20) CHECK(21) CHECK(22) CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29)
  CHECK(30) CHECK(31)
#undef CHECK
#define CHECK_CR(n) Require(expected.cr##n.raw() == actual.cr##n.raw(), "Handle condition register differs");
  CHECK_CR(0) CHECK_CR(1) CHECK_CR(2) CHECK_CR(3) CHECK_CR(4) CHECK_CR(5) CHECK_CR(6) CHECK_CR(7)
#undef CHECK_CR
  Require(expected.lr == actual.lr && expected.ctr.u64 == actual.ctr.u64 &&
          expected.xer.ca == actual.xer.ca && expected.xer.ov == actual.xer.ov && expected.xer.so == actual.xer.so,
          "Handle link/count/carry state differs");
  Require(expected.fpscr.csr == actual.fpscr.csr, "Handle changed guest FP controls/status");
  static_assert(offsetof(PPCContext, v127) + sizeof(PPCVRegister) - offsetof(PPCContext, v0) == 128 * sizeof(PPCVRegister));
  Require(!std::memcmp(&expected.v0, &actual.v0, 128 * sizeof(PPCVRegister)) &&
          expected.msr == actual.msr && expected.vscr_sat == actual.vscr_sat &&
          expected.last_indirect_target == actual.last_indirect_target, "Handle changed preserved vector/control state");
}
bool SameQuery(const n3::guest::HandleQuery& a, const n3::guest::HandleQuery& b) {
  return a.record.key == b.record.key && a.record.object == b.record.object && a.record.auxiliary == b.record.auxiliary &&
      a.index == b.index && a.count == b.count && a.address == b.address && a.key_matches == b.key_matches;
}
void CheckQueryGuards(Guest& fixture) {
  using namespace n3::guest;
  constexpr HandleQuery before{{11, 22, 33}, 44, 55, 66, true};
  HandleQuery result = before;
  fixture.Table(0x10000, 0x10024);
  Require(!QueryHandleRecord(fixture.base, 3, {}, result) && SameQuery(result, before), "Past-end query changed output");
  Require(!QueryPackedHandle(fixture.base, uint64_t(0x12348000u) << 32, {}, result) && SameQuery(result, before),
          "Signed-negative packed index was accepted");
  fixture.Table(0, 12);
  Require(!QueryHandleRecord(fixture.base, 0, {}, result) && SameQuery(result, before), "Null table accepted");
  fixture.Table(0x10000, 0x10000);
  Require(!QueryHandleRecord(fixture.base, 0, {}, result) && SameQuery(result, before), "Empty table accepted");
  fixture.Table(0x10000, 0x10025);
  Require(!QueryHandleRecord(fixture.base, 0, {}, result) && SameQuery(result, before), "Partial record layout accepted");
  fixture.Table(0x10000, 0xffff);
  Require(!QueryHandleRecord(fixture.base, 0, {}, result) && SameQuery(result, before), "Reversed table accepted");
  fixture.Table(0x10000, 0x80010000);
  Require(!QueryHandleRecord(fixture.base, 0, {}, result) && SameQuery(result, before), "Signed-negative table length accepted");
  fixture.Table(0x10000, 0x10024);
  Require(!QueryHandleRecord(fixture.base, 0, {4, 112}, result) && SameQuery(result, before), "Wrapping scratch frame accepted");
  Require(!QueryHandleRecord(fixture.base, 0, {0xfffffff0u, 0, 16, 16}, result) && SameQuery(result, before),
          "Wrapping argument homes accepted");
  // Old pointers are intentionally uncommitted. These guards must reject the
  // descriptor or selected record before dereferencing any overwritten source.
  fixture.Table(0xdead0000, 0xdead0018);
  Require(!QueryHandleRecord(fixture.base, 0, {descriptor + 112, 112}, result) && SameQuery(result, before),
          "Scratch descriptor was followed before rejection");
  Require(!QueryHandleRecord(fixture.base, 0, {descriptor - 16, 112, 16, 16}, result) && SameQuery(result, before),
          "Argument-home descriptor was followed before rejection");
  fixture.Table(0xdead0000, 0xdead0018);
  Require(!QueryHandleRecord(fixture.base, 0, {0xdead0020, 112}, result) && SameQuery(result, before),
          "Scratch record was followed before rejection");
  fixture.Table(0xe1001000, 0xe1001018);
  Require(!QueryHandleRecord(fixture.base, 0, {0xe1001010, 112}, result) && SameQuery(result, before),
          "Physical scratch record was followed before rejection");
  Require(!QueryHandleRecord(fixture.base, 0, {0xdfffffe8u, 112, 16, 16}, result) && SameQuery(result, before),
          "Shadow-boundary argument homes accepted");
  fixture.Table(0xdffffffcu, 0xe0000014u);
  Require(!QueryHandleRecord(fixture.base, 0, {}, result) && SameQuery(result, before),
          "Shadow-boundary table accepted");
  Require(retained_handle_depth == 0, "Handle retain depth started nonzero");
  { RetainHandleChain first; Require(retained_handle_depth == 1, "Retain depth did not increase");
    { RetainHandleChain second; Require(retained_handle_depth == 2, "Nested retain depth did not increase"); }
    Require(retained_handle_depth == 1, "Nested retain depth did not restore"); }
  Require(retained_handle_depth == 0, "Retain depth did not restore");
}
int main() {
  using namespace n3::guest;
  using Platform = rex::platform::FPSCRPlatform;
  const auto saved = Platform::getcsr();
  uint32_t address = 0, rounding = 0, sample = 0;
  bool flush = false, unmasked = false;
  try {
    using Function = void (*)(PPCContext&, uint8_t*);
    struct Entry { uint32_t address; Function original, replacement; bool packed; };
    constexpr std::array<Entry, 2> entries{{{0x82081138, __imp__sub_82081138, sub_82081138, false},
        {0x82080780, __imp__sub_82080780, sub_82080780, true}}};
    Guest seed, expected, actual;
    std::mt19937 virtual_random(0x48414e44), physical_random(0x50485948);
    for (auto region : regions) for (uint32_t i = 0; i < region.size; ++i)
      seed.base[region.start + i] = uint8_t(region.start < 0xe0000000u && region.start != 0xdfff0000u ? virtual_random() : physical_random());
    CheckQueryGuards(actual);
    uint32_t comparisons = 0, native_wrappers = 0, retained_wrappers = 0, physical_native = 0, physical_retained = 0;
    for (const auto entry : entries) {
      address = entry.address;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == address) registered = mapping->host == entry.replacement;
      Require(registered, "Handle entry missing from indirect mapping");
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true})
      for (bool next_unmasked : {false, true}) {
        flush = next_flush; unmasked = next_unmasked;
        for (sample = 0; sample < 144; ++sample) {
          auto& random = sample < 112 ? virtual_random : physical_random;
          expected.Copy(seed);
          uint32_t stack = 0x80000, table = 0x10000 + sample % 4, count = 16;
          uint32_t index = sample % count, output = 0x90000 + (sample / 4) % 4;
          uint32_t key = (uint32_t(random()) & 0xffff0000u) | index;
          uint32_t packed_object = random(), link = 0x82081234;
          uint32_t end = table + count * 12;
          bool physical_expected_native = true;
          if (sample >= 112) {
            const auto physical_case = sample - 112;
            packed_object = 0xe100c000u + physical_case * 4;
            table = 0xe1001000u + physical_case % 4;
            if (physical_case >= 8) output = 0xe1008000u + physical_case % 4;
            if (physical_case >= 16) stack = 0xe1018000u;
            if (physical_case == 20) { table = 0xe0000000u; stack = 0xe0001000u; }
            if (physical_case == 21) stack = 0xe0000000u;
            if (physical_case == 22) { stack = 0xdfffffe8u; physical_expected_native = !entry.packed; }
            if (physical_case == 23) { stack = 0xe0000030u; physical_expected_native = !entry.packed; }
            if (physical_case == 24) { table = 0xdffffffcu; index = 0; physical_expected_native = false; }
            if (physical_case == 25) { output = 0xdffffffcu; physical_expected_native = entry.packed; }
            if (physical_case == 26) { table = 0xdfffffa0u; index = 0; physical_expected_native = false; }
            if (physical_case == 27) { table = 0xffffb000u; stack = 0xffffd800u; output = 0xffffc000u; }
            if (physical_case == 28) { index = 0; output = table + 4; physical_expected_native = entry.packed; }
            if (physical_case == 29) {
              index = 0;
              if (entry.packed) table = stack - 32; else output = table;
              physical_expected_native = false;
            }
            if (physical_case == 30) { output = 0xe0000000u; }
            if (physical_case == 31) { table = 0xe0001000u; output = 0xe0003000u; stack = 0xe0005000u; }
            key = (key & 0xffff0000u) | index;
            end = table + count * 12;
          }
          if (sample == 60) { index = 32767; key = 0x92347fff; count = 32768; end = table + count * 12; }
          if (sample == 61) { index = 0; key = 0x92340000; end = table + 0x7ffffff8u; }
          if (sample >= 64 && sample < 88) {
            index = 0; key &= 0xffff0000u;
            if (entry.packed) { table = stack - 32 + (sample % 12); end = table + 24; }
            else output = table + int32_t(sample - 76);
          }
          if (sample >= 88 && sample < 96) {
            index = 0; key &= 0xffff0000u;
            end = sample % 3 == 0 ? table - 13 : sample % 3 == 1 ? table + 37 : table + 0x80000000u;
          }
          if (sample >= 96 && sample < 104 && entry.packed) {
            index = 0; key &= 0xffff0000u;
            table = stack + 16 + (sample % 2) * 8; end = table + 24;
          }
          expected.Table(table, end);
          uint32_t selected = table + index * 12;
          const uint32_t record_key = sample % 3 ? key : key ^ 0x10000u;
          expected.Word(selected, record_key); expected.Word(selected + 4, random()); expected.Word(selected + 8, random());
          if (sample >= 104 && sample < 112 && entry.packed) {
            index = 0; key &= 0xffff0000u;
            if (sample % 3 == 0) {
              // stwu replaces an unreadable old begin with the original SP.
              stack = descriptor + 112;
              expected.Table(0xdead0000, stack + 24);
            } else if (sample % 3 == 1) {
              // Saving LR replaces an unreadable old begin with a valid table.
              stack = descriptor + 8; link = 0x10000;
              expected.Table(0xdead0000, link + 24);
            } else {
              // The packed argument itself becomes the descriptor before the
              // original lookup: begin=0x1000, end=64096, signed index=4096.
              stack = descriptor - 16; key = 0x1000; index = 4096; packed_object = 64096;
              expected.Table(0xdead0000, 0xdead0018);
            }
          }
          actual.Copy(expected);
          const auto packed = uint64_t(key) << 32 | packed_object;
          HandleQuery query;
          const bool native_wrapper = entry.packed && QueryPackedHandle(actual.base, packed, {stack, 112, 16, 16}, query);
          if (sample >= 112 && entry.packed)
            Require(native_wrapper == physical_expected_native, "Physical packed-query route differs");
          std::array<uint8_t, 12> unbiased_output{};
          const bool canary_output = !entry.packed && output >= 0xe0000000u;
          if (canary_output) std::copy_n(actual.base + output, unbiased_output.size(), unbiased_output.begin());
          std::array<uint8_t, 112> before_frame;
          std::array<uint8_t, 16> before_homes;
          std::copy_n(actual.base + HostOffset(stack - 112), before_frame.size(), before_frame.begin());
          std::copy_n(actual.base + HostOffset(stack + 16), before_homes.size(), before_homes.begin());
          PPCContext original{};
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(0) SEED(1) SEED(2) SEED(3) SEED(4) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9)
          SEED(10) SEED(11) SEED(12) SEED(13) SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19)
          SEED(20) SEED(21) SEED(22) SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29)
          SEED(30) SEED(31)
#undef SEED
          original.r1.u64 = uint64_t{0x12345678} << 32 | stack;
          original.r3.u64 = entry.packed ? packed : uint64_t{0x87654321} << 32 | output;
          original.r5.u64 = uint64_t{0x98765432} << 32 | index;
          original.lr = uint64_t{0x87654321} << 32 | link; original.ctr.u64 = 0xfedcba9876543210ull;
          auto* vector_bytes = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
          for (uint32_t i = 0; i < 128 * sizeof(PPCVRegister); ++i) vector_bytes[i] = uint8_t(sample + i * 29 + address);
          original.vscr_sat = uint8_t(sample & 1); original.last_indirect_target = 0x81234567;
#define SEED_CR(n) original.cr##n.set_raw((sample + n) % 16);
          SEED_CR(0) SEED_CR(1) SEED_CR(2) SEED_CR(3) SEED_CR(4) SEED_CR(5) SEED_CR(6) SEED_CR(7)
#undef SEED_CR
          original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
          if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
          if (unmasked) original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
          const auto initial = original.fpscr.csr; auto replacement = original;
          Platform::setcsr(initial);
          { RetainHandleChain oracle; entry.original(original, expected.base); }
          Require(Platform::getcsr() == initial, "Original integer handle code changed host FP state");
          Platform::setcsr(initial); n3_handle_table_native_used = false;
          entry.replacement(replacement, actual.base);
          if (sample >= 112) {
            Require(n3_handle_table_native_used == physical_expected_native, "Physical handle native/fallback route differs");
            if (physical_expected_native) ++physical_native; else ++physical_retained;
          }
          Require(retained_handle_depth == 0, "Retained handle scope leaked into later calls");
          Require(Platform::getcsr() == initial, "Native integer handle code changed host FP state");
          if (native_wrapper) {
            Require(std::equal(before_frame.begin(), before_frame.end(), actual.base + HostOffset(stack - 112)) &&
                    std::equal(before_homes.begin(), before_homes.end(), actual.base + HostOffset(stack + 16)),
                    "Native packed validation wrote retained stack scratch");
            std::copy_n(before_frame.begin(), before_frame.size(), expected.base + HostOffset(stack - 112));
            std::copy_n(before_homes.begin(), before_homes.size(), expected.base + HostOffset(stack + 16));
            ++native_wrappers;
          } else if (entry.packed) ++retained_wrappers;
          if (canary_output)
            Require(std::equal(unbiased_output.begin(), unbiased_output.end(), actual.base + output) &&
                    std::equal(unbiased_output.begin(), unbiased_output.end(), expected.base + output),
                    "Physical handle output changed the unbiased output canary");
          if (sample >= 112 && !entry.packed && physical_expected_native)
            Require(rex::memory::load_and_swap<uint32_t>(actual.base + HostOffset(output)) == record_key,
                    "Native physical handle lookup did not publish the selected key");
          Require(expected.Same(actual), "Handle whole-fixture bytes differ");
          CheckRegisters(original, replacement); ++comparisons;
        }
      }
    }
    Require(native_wrappers && retained_wrappers, "Packed fixtures did not cover native and retained chains");
    Require(physical_native == 864 && physical_retained == 160, "Physical handle fixtures lacked expected native and retained routes");
    Platform::setcsr(saved);
    std::cout << comparisons << " handle equivalence cases with full scalar registers, all rounding/flush/exception modes, "
              << native_wrappers << " native packed validations and " << retained_wrappers << " retained chains passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Handle 0x" << std::hex << address << std::dec << " sample " << sample << " rounding " << rounding
              << " flush " << flush << " unmasked " << unmasked << ": " << error.what() << '\n'; return 1;
  }
}
