#include <Windows.h>
#include "guest_handle_table.h"
#include "guest_state_payload.h"
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

extern "C" void sub_8209D280(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D280(PPCContext&, uint8_t*);
extern "C" void sub_8209DEE0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209DEE0(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_state_relation_native_used = false;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 8> regions{{{0, 0x40000}, {0x82041000, 8192},
    {0x8205a000, 8192}, {0x82b47000, 12288}, {0x82b5f000, 8192},
    {0xdfff0000, 0x20000}, {0xe1000000, 0x50000}, {0xffffa000, 0x6000}}};
// Independent fixture implementation of the generated Windows address rule.
uint64_t HostOffset(uint32_t address) { return uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u); }
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve state relation fixture");
    for (auto region : regions)
      if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit state relation fixture");
      }
  }
  ~Guest() { VirtualFree(base, 0, MEM_RELEASE); }
  Guest(const Guest&) = delete;
  void Copy(const Guest& other) {
    for (auto region : regions) std::memcpy(base + region.start, other.base + region.start, region.size);
  }
  bool Same(const Guest& other) const {
    for (auto region : regions)
      if (std::memcmp(base + region.start, other.base + region.start, region.size)) return false;
    return true;
  }
  void Word(uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(base + HostOffset(at), value); }
  void Byte(uint32_t at, uint8_t value) { base[HostOffset(at)] = value; }
};
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  constexpr uint32_t handle_descriptor = 0x82b4807c, resource_pointer = 0x82b60088;
  constexpr std::array<uint32_t, 8> float_offsets{32, 36, 40, 48, 52, 84, 88, 92};
  constexpr std::array<uint32_t, 6> exceptions{1, 0x80000001u, 0x7f800000u,
      0xff800000u, 0x7fc12345u, 0x7f812345u};
  constexpr std::array<uint32_t, 8> integers{0, 1, 0xffffffffu, 0xfffffffeu,
      0x80000000u, 0x7fffffffu, 0x01000001u, 0xfedcba98u};
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, rounding = 0, address = 0; bool flush = false;
  try {
    using Function = void (*)(PPCContext&, uint8_t*);
    struct Entry { uint32_t address; Function original, replacement; bool apply; };
    constexpr std::array<Entry, 2> entries{{
        {0x8209d280, __imp__sub_8209D280, sub_8209D280, true},
        {0x8209dee0, __imp__sub_8209DEE0, sub_8209DEE0, false}}};
    Guest seed, expected, actual;
    std::mt19937 random(0x52454c41);
    std::mt19937 physical_random(0x50485953);
    for (uint32_t region_index = 0; region_index < regions.size(); ++region_index) {
      const auto region = regions[region_index];
      auto& generator = region_index < 5 ? random : physical_random;
      for (uint32_t i = 0; i < region.size; ++i) seed.base[region.start + i] = uint8_t(generator());
    }
    uint32_t comparisons = 0, native_count = 0, multiplied_count = 0;
    uint32_t positive_wide_products = 0, negative_wide_products = 0, skipped_variants = 0;
    uint32_t native_after_fallback = 0;
    uint32_t physical_native = 0, physical_retained = 0, physical_canaries = 0, physical_products = 0;
    bool previous_fallback = false;
    for (auto entry : entries) {
      address = entry.address;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == entry.address) registered = mapping->host == entry.replacement;
      Require(registered, "State relation entry missing from indirect-call mapping");
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < 338; ++sample) {
          const auto saved_random = random;
          expected.Copy(seed);
          uint32_t object = 0x1000 + sample % 4, source = 0x6000 + (sample / 4) % 4;
          uint32_t vector = sample % 3 ? 0xc000 + sample % 4 : 0;
          uint32_t related = 0x9000 + sample % 4, handles = 0x8000 + sample % 4;
          uint32_t root = 0x4000 + sample % 4, table = 0x10000 + sample % 4;
          uint32_t stack = 0x30000, link = 0x12345678;
          uint32_t key = 0x12340000u | (sample % 16), category = sample % 4;
          uint32_t kind = sample % 5 == 0 ? 5 : sample % 5 == 1 ? 6 : sample % 5 == 2 ? 1000 : 17;
          uint32_t related_kind = sample % 3 == 0 ? 1000 : sample % 3 == 1 ? 6 : 17;
          uint32_t variant = (sample / 3) % 3 == 0 ? 0xffffffffu : (sample / 3) % 3 == 1 ? 0xfffffffeu : 0;
          uint32_t channel = sample % 9, extra = sample % 5;
          bool stale = sample % 4 == 0, must_native = true;
          const bool alias_case = sample >= 224 && sample < 264;
          const bool unmasked = sample == 304 || sample == 305;
          const bool physical_case = sample >= 306;
          if (alias_case) {
            const auto alias = (sample - 224) % 8;
            object = 0x1000; source = 0x6000; vector = 0xc000; related = 0x9000;
            handles = 0x8000; root = 0x4000; table = 0x10000;
            key = 0x12340000; category = 0; kind = 6; related_kind = 1000; variant = 0;
            channel = extra = 0; stale = false; must_native = false;
            if (entry.apply) {
              if (alias == 0) source = object + 336;
              if (alias == 1) source = object + 337;
              if (alias == 2) vector = object + 420;
              if (alias == 3) related = object + 36;
              if (alias == 4) { object = handle_descriptor - 336; handles = 0x1ff40; key = 0x20000; }
              if (alias == 5) handles = object + 336;
              if (alias == 6) vector = object + 412;
              if (alias == 7) { source = stack - 220; kind = 0; vector = 0; }
            } else {
              if (alias == 0) { related = stack - 308; link = 1000; }
              if (alias == 1) { stack = handle_descriptor + 8; link = handles; }
              if (alias == 2) { handles = stack - 8; link = key; }
              if (alias == 3) { root = stack - 8 - 216; related_kind = 17; link = table; }
              if (alias == 4) { table = stack - 8; related_kind = 17; link = 1; }
              if (alias == 5) object = stack - 340;
              if (alias == 6) { stack = resource_pointer + 8; link = root; }
              if (alias == 7) { stack = 0x82042050; link = 0x3f800000; }
            }
          }
          if (physical_case) {
            const auto physical = sample - 306;
            object = 0xe1001000u; source = 0xe1006000u; vector = 0xe100c000u;
            related = 0xe1010000u; handles = 0xe1018000u; root = 0xe1020000u;
            table = 0xe1028000u; stack = 0xe1038000u;
            key = 0xe1230000u; category = 0; kind = 6; related_kind = 1000; variant = 0;
            channel = extra = 0; stale = false;
            switch (physical) {
              case 0: source = 0x6000; vector = 0xc000; related = 0x9000; handles = 0x8000; root = 0x4000; table = 0x10000; stack = 0x30000; break;
              case 1: object = 0x1000; vector = 0xc000; related = 0x9000; handles = 0x8000; root = 0x4000; table = 0x10000; stack = 0x30000; break;
              case 2: object = 0x1000; source = 0x6000; related = 0x9000; handles = 0x8000; root = 0x4000; table = 0x10000; stack = 0x30000; break;
              case 3: object = 0x1000; source = 0x6000; vector = 0xc000; handles = 0x8000; root = 0x4000; table = 0x10000; stack = 0x30000; break;
              case 4: object = 0x1000; source = 0x6000; vector = 0xc000; related = 0x9000; root = 0x4000; table = 0x10000; stack = 0x30000; break;
              case 5: object = 0x1000; source = 0x6000; vector = 0xc000; related = 0x9000; handles = 0x8000; table = 0x10000; stack = 0x30000; break;
              case 6: object = 0x1000; source = 0x6000; vector = 0xc000; related = 0x9000; handles = 0x8000; root = 0x4000; stack = 0x30000; break;
              case 7: object = 0x1000; source = 0x6000; vector = 0xc000; related = 0x9000; handles = 0x8000; root = 0x4000; table = 0x10000; break;
              case 8: break;
              case 9: object += 1; break;
              case 10: source += 3; related += 3; handles += 1; root += 1; table += 1; break;
              case 11: vector += 1; object += 2; break;
              case 12: kind = 5; break;
              case 13: related_kind = 6; break;
              case 14: stale = true; break;
              case 15: variant = 0xffffffffu; break;
              case 16: object = 0xffffb000u; source = 0xffffb800u; vector = 0xffffc000u;
                related = 0xffffc100u; handles = 0xffffd100u; root = 0xffffd300u; table = 0xffffd800u; stack = 0xffffd000u; break;
              case 17: stack = 0xe0000000u; break;
              case 18: stack = 0xe0000010u; must_native = false; break;
              case 19: stack = 0xe00000f0u; break;
              case 20: if (entry.apply) source = 0xdfffffd0u; else related = 0xdfffff00u; must_native = false; break;
              case 21: if (entry.apply) vector = 0xdffffffcu; else object = 0xdffffe00u; must_native = false; break;
              case 22: handles = 0xdffffffcu; must_native = false; break;
              case 23: root = 0xdfffff24u; related_kind = 17; must_native = entry.apply; break;
              case 24: if (entry.apply) related = 0xdfffff00u;
                else { table = 0xdffffe80u; related_kind = 17; } must_native = false; break;
              case 25: if (entry.apply) { source = stack - 220; kind = 0; vector = 0; }
                else { related = stack - 308; link = 1000; } must_native = false; break;
              case 26: if (entry.apply) source = object + 336;
                else { table = stack - 8; related_kind = 17; link = 1; } must_native = false; break;
              case 27: if (entry.apply) vector = object + 420;
                else { root = stack - 8 - 216; related_kind = 17; link = table; } must_native = false; break;
              case 28: if (entry.apply) related = object + 36; else object = stack - 340; must_native = false; break;
              case 29: if (entry.apply) handles = object + 336;
                else { handles = stack - 8; link = key; } must_native = false; break;
              case 30: if (entry.apply) { object = handle_descriptor - 336; handles = 0xe100ff40u; key = 0xe1010000u; }
                else { stack = resource_pointer + 8; link = root; } must_native = false; break;
              case 31: break;
            }
          }
          auto payload = [&](uint32_t at) {
            for (uint32_t i = 0; i < 25; ++i) expected.Word(at + i * 4, random());
            for (uint32_t lane = 0; lane < float_offsets.size(); ++lane) {
              const auto bits = sample < 16 ? ((sample + lane) % 2 ? 0x80000000u : 0)
                  : (random() & 0x807fffffu) | ((120 + random() % 12) << 23);
              expected.Word(at + float_offsets[lane], unmasked ? 0x3f800000u : bits);
            }
            expected.Word(at + 4, key); expected.Word(at + 8, related);
            expected.Word(at + 20, integers[sample % integers.size()]);
            expected.Word(at + 24, integers[(sample / 8 + 3) % integers.size()]);
          };
          expected.Word(object + 300, kind); expected.Byte(object + 969, uint8_t(category));
          expected.Word(object + 304, alias_case || physical_case ? 0 : sample % 16); expected.Word(object + 676, channel);
          expected.Word(object + 780, extra); expected.Word(object + 784, uint32_t(int32_t(random() % 301) - 150));
          expected.Word(related + 300, related_kind); expected.Word(related + 1108, variant);
          expected.Word(handle_descriptor, handles); expected.Word(handle_descriptor + 4, handles + 16 * 12);
          for (uint32_t i = 0; i < 16; ++i) {
            expected.Word(handles + i * 12, 0x12340000u | i);
            expected.Word(handles + i * 12 + 4, related); expected.Word(handles + i * 12 + 8, random());
          }
          const auto index = key & 0xffffu;
          expected.Word(handles + index * 12, stale ? key ^ 0x10000u : key);
          expected.Word(handles + index * 12 + 4, related);
          expected.Word(resource_pointer, root);
          for (uint32_t cat = 0; cat < 4; ++cat) {
            const auto cat_table = table + cat * 16 * 48;
            expected.Word(root + cat * 16 + 216, cat_table);
            expected.Word(root + cat * 16 + 220, cat_table + 16 * 48);
            for (uint32_t i = 0; i < 16 * 12; ++i)
              expected.Word(cat_table + i * 4, unmasked ? 1 : uint32_t(int32_t(random() % 20001) - 10000));
          }
          expected.Word(0x82042048, unmasked ? 0 : 0x3c23d70a);
          expected.Word(0x8205b614, 0x3f800000);
          if (entry.apply) payload(source);
          else { expected.Word(object + 340, key); expected.Word(object + 344, related); }
          if (vector) for (uint32_t lane = 0; lane < 3; ++lane)
            expected.Word(vector + lane * 4, unmasked ? 0x3f800000u : lane % 2 ? 0x80000000u : 0x3f000000u);
          if (!alias_case && entry.apply && sample >= 128 && sample < 176) {
            const auto exceptional = sample - 128;
            expected.Word(source + float_offsets[exceptional / 6], exceptions[exceptional % 6]); must_native = false;
          }
          if (!alias_case && entry.apply && sample >= 176 && sample < 194) {
            if (!vector) vector = 0xc000;
            for (uint32_t lane = 0; lane < 3; ++lane) expected.Word(vector + lane * 4, 0x3f800000);
            const auto exceptional = sample - 176;
            expected.Word(vector + (exceptional / 6) * 4, exceptions[exceptional % 6]); must_native = false;
          }
          if (!entry.apply && sample >= 128 && sample < 152) {
            const auto exceptional = sample - 128;
            expected.Word(exceptional / 6 % 2 ? 0x8205b614 : 0x82042048, exceptions[exceptional % 6]); must_native = false;
          }
          if (!entry.apply && sample >= 176 && sample < 208) {
            expected.Word(object + 784, integers[sample % integers.size()]); expected.Word(0x82042048, 0);
            for (uint32_t cat = 0; cat < 4; ++cat)
              for (uint32_t i = 0; i < 16 * 12; ++i) expected.Word(table + cat * 16 * 48 + i * 4, integers[(sample + 1) % integers.size()]);
          }
          if (!entry.apply && sample >= 208 && sample < 224) {
            expected.Word(object + 784, 100);
            for (uint32_t cat = 0; cat < 4; ++cat)
              for (uint32_t i = 0; i < 16 * 12; ++i) expected.Word(table + cat * 16 * 48 + i * 4, 0x80000000u);
            must_native = false;
          }
          if (!entry.apply && sample >= 264 && sample < 272) {
            expected.Word(object + 300, 17); expected.Word(object + 676, 12 + sample % 4); must_native = false;
          }
          if (alias_case) {
            const auto alias = (sample - 224) % 8;
            if (entry.apply) {
              if (alias == 4) expected.Word(source, handles);
              if (alias == 5) { expected.Word(source, key); expected.Word(handles, key); }
            } else {
              if (alias == 1) expected.Word(handle_descriptor, 0xdead0000u);
              if (alias == 3) {
                expected.Word(root + 216, 0x40000); expected.Word(root + 220, 0x40300);
                expected.Word(object + 304, 0); expected.Word(table, 1);
              }
              if (alias == 6) expected.Word(resource_pointer, 0xdead0000u);
            }
          }
          if (unmasked) { expected.Word(object + 784, 0); must_native = false; }
          if (physical_case) {
            const auto physical = sample - 306;
            expected.Word(object + 784, 25);
            if (entry.apply) {
              if (physical == 29) expected.Word(source, key);
              if (physical == 30) expected.Word(source, handles);
              if (physical == 31) { expected.Word(source + 20, 0x80000000u); expected.Word(source + 24, 0xffffffffu); }
            } else {
              if (physical == 27) {
                expected.Word(root + 216, 0x40000); expected.Word(root + 220, 0x40300); expected.Word(table, 1);
              }
              if (physical == 30) expected.Word(resource_pointer, 0xdead0000u);
            }
          }
          std::array<uint8_t, 100> unbiased_payload;
          const bool check_payload_canary = physical_case && sample - 306 < 20 && object >= 0xe0000000u;
          if (check_payload_canary) std::copy_n(expected.base + object + 336, 100, unbiased_payload.begin());
          actual.Copy(expected);
          std::array<uint8_t, 240> before_stack;
          std::copy_n(expected.base + HostOffset(stack - 240), 240, before_stack.begin());
          PPCContext original{};
          original.r1.u32 = stack; original.r3.u32 = object; original.r4.u32 = source; original.r5.u32 = vector;
          // Nonzero upper halves remain observable in preserved pointers and
          // branch-dependent returns, while guest memory uses their low words.
          if (!alias_case) {
            original.r1.u64 |= 0x1234567800000000ull;
            original.r3.u64 |= 0x8765432100000000ull;
            original.r4.u64 |= 0xabcdef0100000000ull;
            original.r5.u64 |= 0xfedcba9800000000ull;
          }
          original.lr = 0x1234567800000000ull | link; original.ctr.u64 = 0x1234567887654321ull;
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(0) SEED(2) SEED(6) SEED(7) SEED(8) SEED(9) SEED(10) SEED(11) SEED(12) SEED(13)
          SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
          SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
          original.f1.u64 = 0x40123456789abcde; original.f2.u64 = 0xc00123456789abcd;
          original.f3.u64 = 0x40023456789abcde; original.f4.u64 = 0xc01123456789abcd;
          auto* vector_bytes = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
          for (uint32_t i = 0; i < 128 * 16; ++i) vector_bytes[i] = uint8_t(physical_random());
#define CR(n) original.cr##n.set_raw((sample + n) % 16);
          CR(1) CR(2) CR(3) CR(4) CR(5) CR(7)
#undef CR
          original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
          original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
          if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
          if (unmasked) { original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); Platform::setcsr(original.fpscr.csr); }
          const auto initial = original.fpscr.csr; auto replacement = original;
          Platform::setcsr(initial);
          {
            n3::guest::RetainPayloadChain payload_chain; n3::guest::RetainHandleChain handle_chain;
            entry.original(original, expected.base);
          }
          Require(!n3::guest::PayloadChainRetained() && n3::guest::retained_handle_depth == 0,
                  "Retained oracle leaked a nested fallback scope");
          const auto host_controls = Platform::getcsr() & controls;
          Platform::setcsr(initial); n3_state_relation_native_used = false;
          entry.replacement(replacement, actual.base);
          Require(!n3::guest::PayloadChainRetained() && n3::guest::retained_handle_depth == 0,
                  "State relation fallback leaked a nested retained scope");
          const bool native = n3_state_relation_native_used;
          Require(native == must_native, "State relation native/fallback route differs");
          if (native) {
            Require(std::equal(actual.base + HostOffset(stack - 240), actual.base + HostOffset(stack - 240) + 240, before_stack.begin()),
                    "Native state relation wrote below-SP scratch");
            std::copy_n(before_stack.begin(), 240, expected.base + HostOffset(stack - 240)); ++native_count;
            if (physical_case) ++physical_native;
            if (previous_fallback) ++native_after_fallback;
            if (entry.apply && !alias_case && !physical_case && std::bit_cast<int32_t>(kind) >= 6 && !stale && related_kind == 1000) {
              if (variant == 0xffffffffu) ++skipped_variants;
              else {
                const auto first = std::bit_cast<int32_t>(integers[sample % integers.size()]);
                const auto second = std::bit_cast<int32_t>(integers[(sample / 8 + 3) % integers.size()]);
                const auto product = int64_t(first) * int64_t(second);
                Require(replacement.r9.s64 == product &&
                        rex::memory::load_and_swap<uint32_t>(actual.base + HostOffset(object + 356)) == uint32_t(product),
                        "Related multiplier lost the full signed product or stored low word");
                ++multiplied_count;
                if (product > 0xffffffffll) ++positive_wide_products;
                if (product < -0x80000000ll) ++negative_wide_products;
              }
            }
            if (entry.apply && physical_case && sample - 306 == 31) {
              Require(replacement.r9.s64 == 2147483648ll &&
                      rex::memory::load_and_swap<uint32_t>(actual.base + HostOffset(object + 356)) == 0x80000000u,
                      "Physical related multiplier lost its wide signed result"); ++physical_products;
            }
          }
          else if (physical_case) ++physical_retained;
          previous_fallback = !native;
          Require(expected.Same(actual), "State relation whole-fixture bytes differ");
          if (check_payload_canary) {
            Require(std::equal(actual.base + object + 336, actual.base + uint64_t(object) + 436, unbiased_payload.begin()) &&
                    std::equal(expected.base + object + 336, expected.base + uint64_t(object) + 436, unbiased_payload.begin()),
                    "Physical state payload changed its unbiased canary"); ++physical_canaries;
          }
#define GPR(n) Require(original.r##n.u64 == replacement.r##n.u64, "State relation GPR result differs");
          GPR(0) GPR(1) GPR(2) GPR(3) GPR(4) GPR(5) GPR(6) GPR(7) GPR(8) GPR(9) GPR(10) GPR(11)
          GPR(14) GPR(15) GPR(16) GPR(17) GPR(18) GPR(19) GPR(20) GPR(21) GPR(22)
          GPR(23) GPR(24) GPR(25) GPR(26) GPR(27) GPR(28) GPR(29) GPR(30) GPR(31)
          GPR(12) GPR(13)
#undef GPR
#define FPR(n) Require(original.f##n.u64 == replacement.f##n.u64, "State relation FPR result differs");
          FPR(0) FPR(1) FPR(2) FPR(3) FPR(4) FPR(13)
          FPR(14) FPR(15) FPR(16) FPR(17) FPR(18) FPR(19) FPR(20) FPR(21) FPR(22)
          FPR(23) FPR(24) FPR(25) FPR(26) FPR(27) FPR(28) FPR(29) FPR(30) FPR(31)
          if (entry.apply || !native) { FPR(5) FPR(6) FPR(7) FPR(8) FPR(9) FPR(10) FPR(11) FPR(12) }
#undef FPR
          const auto* original_vectors = reinterpret_cast<const uint8_t*>(&original) + offsetof(PPCContext, v0);
          const auto* replacement_vectors = reinterpret_cast<const uint8_t*>(&replacement) + offsetof(PPCContext, v0);
          Require(std::memcmp(original_vectors, replacement_vectors, 128 * sizeof(PPCVRegister)) == 0,
                  "State relation changed preserved VMX registers");
#define CR(n) Require(original.cr##n.raw() == replacement.cr##n.raw(), "State relation preserved condition field differs");
          CR(1) CR(2) CR(3) CR(4) CR(5) CR(7)
#undef CR
          Require(original.lr == replacement.lr && original.ctr.u64 == replacement.ctr.u64 &&
                  original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                  original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so,
                  "State relation flags or return state differ");
          Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == host_controls, "State relation FP controls differ");
          ++comparisons;
          if (physical_case) random = saved_random;
        }
      }
    }
    Platform::setcsr(saved);
    Require(multiplied_count > 0 && positive_wide_products > 0 && negative_wide_products > 0 && skipped_variants > 0,
            "Related multiplication, wide signed products or sentinel skip branch was not exercised natively");
    Require(native_after_fallback > 0, "Fallback scope never returned to an ordinary native call");
    Require(physical_native > 0 && physical_retained > 0 && physical_canaries > 0 && physical_products == 8,
            "Physical mapped fields, alias fallback, canaries or signed products lack coverage");
    std::cout << comparisons << " state relation equivalence cases (" << native_count
              << " native), including " << physical_native << " native/" << physical_retained
              << " retained physical cases, payload/vector/table aliases and complete fallback passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "State relation 0x" << std::hex << address << std::dec << " sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n'; return 1;
  }
}
