#include <Windows.h>
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

extern "C" void sub_8209D9B0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D9B0(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_direction_basis_native_used = false;
thread_local bool n3_direction_basis_original_norm = false;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 8> regions{{{0, 0x40000}, {0x82001000, 4096},
    {0x8205a000, 8192}, {0x82b47000, 12288}, {0xdfff0000, 0x20000},
    {0xe1000000, 0x40000}, {0xe2b47000, 12288}, {0xffffb000, 0x4000}}};
constexpr uint32_t slots_address = 0x82b4808c, lower_address = 0x8205b610;
constexpr uint32_t baseline_address = 0x8205b614, threshold_address = 0x820018ac;
// Independent implementation of the generated Windows physical-shadow rule.
uint64_t HostOffset(uint32_t address) { return uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u); }
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve direction-basis fixture");
    for (auto region : regions) if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
      VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit direction-basis fixture");
    }
  }
  ~Guest() { VirtualFree(base, 0, MEM_RELEASE); }
  Guest(const Guest&) = delete;
  void Copy(const Guest& other) { for (auto region : regions) std::memcpy(base + region.start, other.base + region.start, region.size); }
  bool Same(const Guest& other) const {
    for (auto region : regions) if (std::memcmp(base + region.start, other.base + region.start, region.size)) {
      for (uint32_t i = 0; i < region.size; ++i) if (base[region.start + i] != other.base[region.start + i]) {
        std::cerr << "First differing guest byte 0x" << std::hex << uint64_t(region.start) + i
                  << " expected/actual " << uint32_t(base[region.start + i]) << '/'
                  << uint32_t(other.base[region.start + i]) << std::dec << '\n';
        break;
      }
      return false;
    }
    return true;
  }
  void Word(uint32_t at, uint32_t bits) { rex::memory::store_and_swap<uint32_t>(base + HostOffset(at), bits); }
  uint32_t Word(uint32_t at) const { return rex::memory::load_and_swap<uint32_t>(base + HostOffset(at)); }
};
void CheckContext(const PPCContext& expected, const PPCContext& actual) {
#define CHECK(n) if (expected.r##n.u64 != actual.r##n.u64 || expected.f##n.u64 != actual.f##n.u64) { \
    std::cerr << "r/f" #n " expected " << std::hex << expected.r##n.u64 << '/' << expected.f##n.u64 \
              << " actual " << actual.r##n.u64 << '/' << actual.f##n.u64 << std::dec << '\n'; \
    throw std::runtime_error("Direction-basis GPR/FPR result differs"); }
  CHECK(0) CHECK(1) CHECK(2) CHECK(3) CHECK(4) CHECK(5) CHECK(6) CHECK(7) CHECK(8) CHECK(9)
  CHECK(10) CHECK(11) CHECK(12) CHECK(13) CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19)
  CHECK(20) CHECK(21) CHECK(22) CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29)
  CHECK(30) CHECK(31)
#undef CHECK
  static_assert(offsetof(PPCContext, v127) + sizeof(PPCVRegister) - offsetof(PPCContext, v0) == 128 * sizeof(PPCVRegister));
  const auto* left = reinterpret_cast<const uint8_t*>(&expected) + offsetof(PPCContext, v0);
  const auto* right = reinterpret_cast<const uint8_t*>(&actual) + offsetof(PPCContext, v0);
  for (uint32_t i = 0; i < 128; ++i) if (std::memcmp(left + i * 16, right + i * 16, 16)) {
    std::cerr << "v" << i << " differs\n"; throw std::runtime_error("Direction-basis VMX result differs");
  }
#define CR(n) Require(expected.cr##n.raw() == actual.cr##n.raw(), "Direction-basis condition field differs");
  CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
  Require(expected.xer.ca == actual.xer.ca && expected.xer.ov == actual.xer.ov && expected.xer.so == actual.xer.so &&
          expected.lr == actual.lr && expected.ctr.u64 == actual.ctr.u64 && expected.msr == actual.msr &&
          expected.vscr_sat == actual.vscr_sat && expected.last_indirect_target == actual.last_indirect_target,
          "Direction-basis carry/control or preserved state differs");
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  constexpr uint32_t sample_count = 584;
  const auto saved_host = Platform::getcsr();
  uint32_t sample = 0, rounding = 0; bool flush = false;
  try {
    bool registered = false;
    for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
      if (mapping->guest == 0x8209d9b0) registered = mapping->host == sub_8209D9B0;
    Require(registered, "Direction-basis entry missing from indirect mapping");
    Guest seed, expected, actual, production;
    std::mt19937 random(0x42415349);
    std::mt19937 physical_random(0x50485953);
    for (uint32_t region_index = 0; region_index < regions.size(); ++region_index) {
      const auto region = regions[region_index];
      auto& generator = region_index < 4 ? random : physical_random;
      for (uint32_t i = 0; i < region.size; ++i) seed.base[region.start + i] = uint8_t(generator());
    }
    constexpr std::array<uint32_t, 8> integers{0, 1, 0xffffffffu, 0x80000000u,
        0x7fffffffu, 0x01000001u, 0x01000003u, 100};
    constexpr std::array<uint32_t, 8> slots{0, 1, 7, 0xffffffffu,
        0x80000000u, 0x40000000u, 0x15555555u, 0x55555556u};
    constexpr std::array<uint32_t, 24> float_bits{
        0, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x40000000u, 0xc0000000u,
        1, 0x80000001u, 0x007fffffu, 0x00800000u, 0x00800001u, 0x80800000u,
        0x1fffffffu, 0x20000000u, 0x20000001u, 0x3f000000u,
        0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u,
        0x7fc12345u, 0x7f812345u, 0x3f800001u, 0x33800000u};
    uint32_t comparisons = 0, native_count = 0, retained_count = 0, compositions = 0;
    uint32_t zero_native = 0, null_native = 0, basis_native = 0, native_after_fallback = 0;
    uint32_t physical_native = 0, physical_retained = 0, physical_canaries = 0, physical_canary_aliases = 0;
    bool prior_fallback = false;
    for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
      flush = next_flush;
      for (sample = 0; sample < sample_count; ++sample) {
        expected.Copy(seed);
        uint32_t stack = 0x30000, object = 0x1000, source = 0x6000 + sample % 4;
        uint32_t basis = 0x9000, slot = slots[sample % slots.size()], link = 0x82081234;
        std::array<uint32_t, 2> input{1, 0};
        std::array<uint32_t, 3> forward{0, 0, 0x3f800000u};
        uint32_t lower = 0, baseline = 0x3f800000u, threshold = 0x00800000u;
        uint32_t first_w = 0, second_w = 0, output_w = 0;
        bool must_native = sample < 32, must_fallback = false, stack_upper = false;
        if (sample < 32) {
          if (sample % 3 == 0) { input = {}; basis = 0xdead0000; }
          else if (sample % 3 == 1) basis = 0;
        } else if (sample < 192) {
          input = {uint32_t(int32_t(random() % 201) - 100), uint32_t(int32_t(random() % 201) - 100)};
          for (auto& bits : forward) bits = std::bit_cast<uint32_t>(float(int32_t(random() % 201) - 100) / 8.0f);
          if (sample % 16 == 0) input = {};
          else if (sample % 16 == 1) basis = 0;
          object += (sample / 4) % 4; basis += basis ? sample % 4 : 0;
        } else if (sample < 256) {
          input = {integers[sample % integers.size()], integers[(sample / 8) % integers.size()]};
          for (uint32_t lane = 0; lane < 3; ++lane) forward[lane] = float_bits[(sample + lane * 7) % float_bits.size()];
          object += sample % 4; basis += (sample / 4) % 4;
        } else if (sample < 320) {
          const auto bits = float_bits[sample % float_bits.size()];
          if (sample % 3 == 0) lower = bits;
          else if (sample % 3 == 1) baseline = bits;
          else threshold = bits;
        } else if (sample < 384) {
          const auto bits = float_bits[sample % float_bits.size()];
          if (sample % 3 == 0) first_w = bits;
          else if (sample % 3 == 1) second_w = bits;
          else output_w = bits;
        } else if (sample < 448) {
          input = {integers[sample % integers.size()], integers[(sample / 8) % integers.size()]};
          stack_upper = (sample & 1) != 0;
          if (sample % 5 == 0) basis = 0;
        } else if (sample < 512) {
          object += sample % 4;
          if (sample % 4 == 0) source = object + 576;
          else if (sample % 4 == 1) basis = object + 512;
          else if (sample % 4 == 2) source = object + 580;
          else basis = object + 508;
        } else if (sample < 544) {
          const auto alias = sample - 512;
          slot = 0; source = 0x6000; object = 0x1000; basis = 0x9000;
          switch (alias) {
            case 0: source = stack - 8; input = {}; link = 0; must_fallback = true; break;
            case 1: source = stack - 24; input = {}; must_fallback = true; break;
            case 2: source = stack - 16; input = {}; must_fallback = true; break;
            case 3: source = stack - 176; input = {}; must_fallback = true; break;
            case 4: source = stack + 20; input = {}; must_fallback = true; break;
            case 5: source = stack + 36; input = {}; must_fallback = true; break;
            case 6: source = stack - 190; input = {}; must_fallback = true; break;
            case 7: object = stack - 592; input = {}; must_fallback = true; break;
            case 8: object = stack + 20 - 576; input = {}; must_fallback = true; break;
            case 9: stack = lower_address + 32; input = {}; must_fallback = true; break;
            case 10: stack = baseline_address + 8; input = {}; link = 0; must_fallback = true; break;
            case 11: stack = threshold_address + 16; input = {}; must_fallback = true; break;
            case 12: stack = slots_address + 8; link = basis; must_fallback = true; break;
            case 13: basis = stack - 72; link = 0x3f800000u; must_fallback = true; break;
            case 14: basis = stack - 240; must_fallback = true; break;
            case 15: basis = stack + 20 - 64; must_fallback = true; break;
            case 16: basis = stack + 36 - 72; must_fallback = true; break;
            case 17: source = lower_address; break;
            case 18: source = threshold_address; break;
            case 19: object = lower_address - 576; break;
            case 20: object = threshold_address - 576; must_fallback = true; break;
            case 21: object = slots_address - 576; break;
            case 22: stack += 1; input = {}; must_fallback = true; break;
            case 23: stack += 4; input = {}; must_fallback = true; break;
            case 24: stack += 8; input = {}; must_fallback = true; break;
            case 25: source = object + 576; break;
            case 26: basis = object + 512; break;
            case 27: basis = threshold_address - 64; break;
            case 28: first_w = 0x7fc12345u; must_fallback = true; break;
            case 29: second_w = 0x7fc12345u; must_fallback = true; break;
            case 30: output_w = 0x7fc12345u; must_fallback = true; break;
            case 31: first_w = second_w = output_w = 1; break;
          }
        } else if (sample < 552) {
          input = {}; basis = 0xdead0000;
          must_fallback = sample == 544 || sample == 546 || sample == 548;
        } else {
          const auto physical = sample - 552;
          must_native = true;
          object = 0xe1001000u; source = 0xe1006000u; basis = 0xe1009000u; stack = 0xe100a000u;
          slot = 0; input = {1, 0}; forward = {0, 0, 0x3f800000u};
          switch (physical) {
            case 0: object = 0x1000; basis = 0x9000; stack = 0x30000; break;
            case 1: source = 0x6000; basis = 0x9000; stack = 0x30000; break;
            case 2: object = 0x1000; source = 0x6000; stack = 0x30000; break;
            case 3: object = 0x1000; source = 0x6000; basis = 0x9000; break;
            case 4: break;
            case 5: basis = 0; break;
            case 6: input = {}; basis = 0xdead0000; break;
            case 7: slot = 0xffffffffu; break;
            case 8: object = 0x1000; source = 0x6000; basis = 0x9000; stack = 0x30000; slot = 0x08000000u; break;
            case 9: slot = 0x08000000u; break;
            case 10: source += 3; break;
            case 11: object += 1; break;
            case 12: object += 2; break;
            case 13: object += 3; break;
            case 14: source = object + 576; break;
            case 15: basis = object + 512; break;
            case 16: source = stack - 8; input = {}; link = 0; must_native = false; must_fallback = true; break;
            case 17: object = stack - 592; input = {}; must_native = false; must_fallback = true; break;
            case 18: basis = stack - 72; link = 0x3f800000u; must_native = false; must_fallback = true; break;
            case 19: source = stack + 20; input = {}; must_native = false; must_fallback = true; break;
            case 20: stack = 0xe0000000u; must_native = false; must_fallback = true; break;
            case 21: stack = 0xe0000010u; must_native = false; must_fallback = true; break;
            case 22: stack = 0xe00000c0u; break;
            case 23: source = 0xdffffffcu; input = {}; must_native = false; must_fallback = true; break;
            case 24: object = 0xdffffdbcu; must_native = false; must_fallback = true; break;
            case 25: basis = 0xdfffffbcu; must_native = false; must_fallback = true; break;
            case 26: object = 0xe0000000u; break;
            case 27: object = 0xffffb000u; source = 0xffffd004u; basis = 0xffffc000u; stack = 0xffffd800u; stack_upper = true; break;
            case 28: slot = 0x08000000u; source = 0xe2b4708cu; break;
            case 29: source = 0xe0000000u; break;
            case 30: stack = object + 576 - 0x1000 + 8; input = {}; must_native = false; must_fallback = true; break;
            case 31: source = stack + 36; input = {}; must_native = false; must_fallback = true; break;
          }
          // Control all bytes consumed by the final two aligned VMX loads,
          // including the fourth lane's wraparound on unaligned outputs.
          const auto raw = (object + 576) & ~15u;
          for (uint32_t word = 0; word < 8; ++word) expected.Word(raw + word * 4, 0);
        }
        // W words are read by full VMX loads even though DPPS masks them out.
        expected.Word(stack - 36, first_w); expected.Word(stack - 52, second_w);
        expected.Word(object + 576, 0x41200000u); expected.Word(object + 580, 0xc1200000u);
        expected.Word(object + 584, 0x41a00000u); expected.Word(object + 588, output_w);
        expected.Word(source, input[0]); expected.Word(source + 4, input[1]);
        if (basis && basis != 0xdead0000) {
          expected.Word(basis + 64, forward[0]); expected.Word(basis + 68, forward[1]); expected.Word(basis + 72, forward[2]);
        }
        const uint32_t cell = slots_address + uint32_t(slot * 12u);
        expected.Word(cell, basis);
        expected.Word(lower_address, lower); expected.Word(baseline_address, baseline); expected.Word(threshold_address, threshold);
        if (sample == 524) expected.Word(cell, 0xdead0000); // Saved LR replaces this pointer before lookup.
        std::array<uint8_t, 12> before_output;
        std::copy_n(expected.base + HostOffset(object + 576), before_output.size(), before_output.begin());
        const bool physical_output = object + 576 >= 0xe0000000u;
        const bool canary_alias = sample == 582;
        std::array<uint8_t, 12> unbiased_output;
        if (physical_output) std::copy_n(expected.base + object + 576, unbiased_output.size(), unbiased_output.begin());
        std::array<uint8_t, 4> unbiased_slot;
        if (cell >= 0xe0000000u) std::copy_n(expected.base + cell, unbiased_slot.size(), unbiased_slot.begin());
        actual.Copy(expected);
        const bool compose = (sample % 16 == 0 && sample < 512) || (sample >= 552 && sample < 568);
        if (compose) production.Copy(expected);
        PPCContext original{};
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
        SEED(0) SEED(1) SEED(2) SEED(3) SEED(4) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9)
        SEED(10) SEED(11) SEED(12) SEED(13) SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19)
        SEED(20) SEED(21) SEED(22) SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29)
        SEED(30) SEED(31)
#undef SEED
        auto* vector_bytes = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
        auto& register_random = sample < 552 ? random : physical_random;
        for (uint32_t i = 0; i < 128 * 16; ++i) vector_bytes[i] = uint8_t(register_random());
        original.r1.u64 = (stack_upper ? 0x13579bdf00000000ull : 0) | stack;
        original.r3.u64 = 0x1234567800000000ull | object;
        original.r4.u64 = 0xabcdef0100000000ull | source;
        original.r5.u64 = 0xfedcba9800000000ull | slot;
        original.lr = 0x8765432100000000ull | link; original.ctr.u64 = 0x23456789abcdef01ull;
        original.vscr_sat = uint8_t(sample & 1); original.last_indirect_target = 0x81234567;
#define CR(n) original.cr##n.set_raw((sample + n) % 16);
        CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
        original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
        original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
        if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
        uint32_t initial_host = original.fpscr.csr;
        if (sample == 544) {
          original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); initial_host = original.fpscr.csr;
        } else if (sample == 545) {
          original.fpscr.csr |= Platform::FlushMask;
          initial_host = original.fpscr.csr & ~(Platform::ExceptionMask | 0x3fu);
        } else if (sample == 546) {
          original.fpscr.csr &= ~(Platform::ExceptionMask | Platform::FlushMask | 0x3fu);
          initial_host = original.fpscr.csr | Platform::ExceptionMask;
        } else if (sample == 547) {
          initial_host = (original.fpscr.csr & ~Platform::RoundMaskVal) |
              Platform::GuestToHost[(rounding + 1) % 4];
        } else if (sample == 548) {
          original.fpscr.csr &= ~Platform::FlushMask;
          initial_host = original.fpscr.csr | Platform::FlushMask;
        } else if (sample == 549) {
          original.fpscr.csr |= Platform::FlushMask;
          initial_host = original.fpscr.csr & ~Platform::FlushMask;
        } else if (sample == 550) {
          original.fpscr.csr |= Platform::FlushMask;
          initial_host = (original.fpscr.csr & ~Platform::RoundMaskVal) |
              Platform::GuestToHost[(rounding + 1) % 4];
        } else if (sample == 551) {
          original.fpscr.csr &= ~Platform::FlushMask;
          initial_host = (original.fpscr.csr & ~Platform::RoundMaskVal) |
              Platform::GuestToHost[(rounding + 1) % 4];
        }
        auto replacement = original, composed = original;
        n3_direction_basis_original_norm = true;
        Platform::setcsr(initial_host); __imp__sub_8209D9B0(original, expected.base);
        const auto original_host_controls = Platform::getcsr() & controls;
        n3_direction_basis_original_norm = false;
        Platform::setcsr(initial_host); n3_direction_basis_native_used = false;
        sub_8209D9B0(replacement, actual.base);
        const bool native = n3_direction_basis_native_used;
        Require(!must_native || native, "Ordinary direction-basis branch did not use native path");
        Require(!must_fallback || !native, "Unsafe direction-basis source or FP context used native path");
        Require(expected.Same(actual), "Direction-basis whole-fixture bytes differ");
        CheckContext(original, replacement);
        Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                (Platform::getcsr() & controls) == original_host_controls,
                "Direction-basis FP controls differ");
        Require((replacement.fpscr.csr & Platform::FlushMask) == 0,
                "Direction-basis did not finish with scalar flush disabled");
        if (physical_output) {
          const auto at = uint64_t(object) + 576;
          if (!canary_alias) {
            Require(std::equal(actual.base + at, actual.base + at + 12, unbiased_output.begin()) &&
                    std::equal(expected.base + at, expected.base + at + 12, unbiased_output.begin()),
                    "Physical basis output changed its unbiased canary"); ++physical_canaries;
          } else {
            Require(!std::equal(expected.base + at, expected.base + at + 12, unbiased_output.begin()),
                    "Physical prologue alias did not reach the unbiased output canary"); ++physical_canary_aliases;
          }
        }
        if (cell >= 0xe0000000u)
          Require(std::equal(actual.base + cell, actual.base + uint64_t(cell) + 4, unbiased_slot.begin()) &&
                  std::equal(expected.base + cell, expected.base + uint64_t(cell) + 4, unbiased_slot.begin()),
                  "Physical wrapped slot lookup changed its unbiased source canary");
        if (native) {
          ++native_count; if (prior_fallback) ++native_after_fallback;
          if (sample >= 552) ++physical_native;
          if (sample < 32) {
            if (sample % 3 == 0) {
              Require(std::equal(actual.base + object + 576, actual.base + object + 588, before_output.begin()),
                      "Zero input unexpectedly changed the destination vector"); ++zero_native;
            } else if (sample % 3 == 1) {
              Require(actual.Word(object + 576) == 0 && actual.Word(object + 580) == 0 &&
                      actual.Word(object + 584) == 0x3f800000u, "Null basis did not install the default vector"); ++null_native;
            } else {
              Require(actual.Word(object + 576) == 0x3f800000u &&
                      (actual.Word(object + 580) & 0x7fffffffu) == 0 &&
                      (actual.Word(object + 584) & 0x7fffffffu) == 0, "Axis basis produced an unexpected direction"); ++basis_native;
            }
          }
        } else { ++retained_count; if (sample >= 552) ++physical_retained; }
        prior_fallback = !native;
        if (compose) {
          Platform::setcsr(initial_host); __imp__sub_8209D9B0(composed, production.base);
          Require(expected.Same(production), "Retained outer/native normalizer composition changed memory");
          CheckContext(original, composed);
          Require((original.fpscr.csr & controls) == (composed.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == original_host_controls,
                  "Retained outer/native normalizer composition changed FP controls");
          ++compositions;
        }
        ++comparisons;
      }
    }
    Require(zero_native > 0 && null_native > 0 && basis_native > 0 && native_after_fallback > 0,
            "Direction-basis branch coverage or recovery from fallback is missing");
    Require(physical_native == 168 && physical_retained == 88 && physical_canaries > 0 && physical_canary_aliases == 8,
            "Physical source/output/basis/stack, wrapped table or shadow-boundary coverage is missing");
    Platform::setcsr(saved_host);
    std::cout << comparisons << " direction-basis equivalence cases (" << native_count << " native, " << retained_count
              << " retained), plus " << compositions << " production nested-normalizer compositions; "
              << physical_native << " native/" << physical_retained << " retained physical-shadow cases passed\n";
  } catch (const std::exception& error) {
    n3_direction_basis_original_norm = false; Platform::setcsr(saved_host);
    std::cerr << "Direction-basis sample " << sample << " rounding " << rounding << " flush " << flush
              << ": " << error.what() << '\n'; return 1;
  }
}
