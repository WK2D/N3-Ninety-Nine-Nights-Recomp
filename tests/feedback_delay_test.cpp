#include <Windows.h>
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

extern "C" void sub_8253BFA8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253BFA8(PPCContext&, uint8_t*);
extern "C" void sub_8253C310(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C310(PPCContext&, uint8_t*);
extern "C" void sub_8253C480(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C480(PPCContext&, uint8_t*);
extern "C" void sub_8253C5F0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C5F0(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_feedback_delay_native_used = false;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
struct PhysicalRegion { uint64_t start; uint32_t size; };
constexpr std::array<PhysicalRegion, 4> physical_regions{{{0, 0x20000},
    {0xdfff0000, 0x30000}, {0xe1000000, 0x50000}, {0xffff0000, 0x20000}}};
// Independent fixture mapping, including the translated offsets above 4 GiB.
uint64_t PhysicalOffset(uint32_t at) { return uint64_t(at) + (at >= 0xe0000000u ? 0x1000u : 0u); }
struct PhysicalGuest {
  uint8_t* base = static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{0x100010000ull}, MEM_RESERVE, PAGE_NOACCESS));
  PhysicalGuest() {
    Require(base != nullptr, "Cannot reserve physical feedback fixture");
    for (auto region : physical_regions) if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
      VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit physical feedback fixture");
    }
  }
  ~PhysicalGuest() { VirtualFree(base, 0, MEM_RELEASE); }
  PhysicalGuest(const PhysicalGuest&) = delete;
  void Copy(const PhysicalGuest& other) {
    for (auto region : physical_regions) std::memcpy(base + region.start, other.base + region.start, region.size);
  }
  bool Same(const PhysicalGuest& other) const {
    for (auto region : physical_regions) if (std::memcmp(base + region.start, other.base + region.start, region.size)) return false;
    return true;
  }
  void Word(uint32_t at, uint32_t bits) { rex::memory::store_and_swap<uint32_t>(base + PhysicalOffset(at), bits); }
};
uint32_t CheckPhysicalFeedback() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  using Function = void (*)(PPCContext&, uint8_t*);
  struct Entry { uint32_t address, history; Function original, replacement; bool separate; };
  constexpr std::array<Entry, 4> entries{{
      {0x8253bfa8, 128, __imp__sub_8253BFA8, sub_8253BFA8, false},
      {0x8253c310, 256, __imp__sub_8253C310, sub_8253C310, false},
      {0x8253c480, 512, __imp__sub_8253C480, sub_8253C480, false},
      {0x8253c5f0, 512, __imp__sub_8253C5F0, sub_8253C5F0, true}}};
  PhysicalGuest seed, expected, actual;
  for (auto region : physical_regions) for (uint32_t i = 0; i < region.size; ++i)
    seed.base[region.start + i] = uint8_t(i * 29 + (region.start >> 12));
  uint32_t comparisons = 0, native_count = 0, retained_count = 0, canaries = 0, late_native = 0, late_retained = 0;
  for (const auto& entry : entries) for (uint32_t rounding = 0; rounding < 4; ++rounding) for (bool flush : {false, true})
    for (uint32_t sample = 0; sample < 40; ++sample) {
      try {
        expected.Copy(seed);
        const uint32_t object_size = 24 + entry.history * 4, scratch = entry.separate ? 40 : 32;
        uint32_t object = 0x1000, input = 0x8000, output = 0xa000, stack = 0x18000;
        if (sample == 0 || sample >= 4) object = 0xe1001000;
        if (sample == 1 || sample >= 4) input = 0xe1010000;
        if (sample == 2 || sample >= 4) output = 0xe1020000;
        if (sample == 3 || sample >= 4) stack = 0xe1040000;
        bool native = true;
        if (sample >= 5 && sample <= 7) { object += sample - 4; input += sample - 4; output += sample - 4; }
        switch (sample) {
          case 8: object = uint32_t(0u - object_size); break;
          case 9: input = 0xfffffbfc; break;
          case 10: output = entry.separate ? 0xfffffc00 : 0xfffffbfc; if (!entry.separate) input = output; break;
          case 11: stack = 0xfffffff0; break;
          case 12: object = 0xe0000000u - 24 - entry.history * 2; native = false; break;
          case 13: input = 0xdffffe00; native = false; break;
          case 14: if (entry.separate) { output = 0xdffffe00; native = false; } else stack = 0xe0000000; break;
          case 15: stack = 0xe0000008; native = false; break;
          case 16: stack = 0xe0000000; break;
          case 17: stack = 0xe0000030; break;
          case 18: input = object + 24; native = false; break;
          case 19: input = object + 12; native = false; break;
          case 20: if (entry.separate) output = input + 4; else input = object + object_size - 4; native = false; break;
          case 21: stack = object + object_size - 8; native = false; break;
          case 22: input = stack - 1028 + 16; native = false; break;
          case 23: if (entry.separate) output = stack - 1024 + 16; else input = stack - scratch - 1024; native = false; break;
          case 24: object = 0xffffff00; native = false; break;
          case 25: input = 0xfffffe00; native = false; break;
          case 26: if (entry.separate) output = 0xfffffe00; else input = 0xfffffe00; native = false; break;
          case 27: case 28: case 29: case 30: native = false; break;
          case 33: native = rounding == 1 || rounding == 3; break;
          case 34: native = rounding == 1 || rounding == 2; break;
          case 35: if (entry.separate) output = input + 1028; break;
          case 36: input = stack - scratch - 1024; native = false; break;
          case 37: if (entry.separate) output = input; else input = object; native = false; break;
        }
        for (uint32_t i = 0; i < entry.history; ++i) expected.Word(object + 24 + i * 4, 0);
        for (uint32_t i = 0; i < 256; ++i) expected.Word(input + i * 4, 0x3f800000);
        expected.Word(object, 1); expected.Word(object + 4, 0x3f000000); expected.Word(object + 8, 0x3e000000);
        expected.Word(object + 12, 0x40000000); expected.Word(object + 16, entry.history == 512 ? 256 : 0);
        if (sample == 27) expected.Word(input + 1020, 0x7f812345);
        if (sample == 28) expected.Word(object + 16, 1);
        if (sample == 29) expected.Word(object + 4, 0x7f800000);
        if (sample == 30) { expected.Word(object + 4, 0x3f800000); expected.Word(object + 8, 0); }
        if (sample == 33 || sample == 34) {
          expected.Word(object + 4, 0x3f800000); expected.Word(object + 8, 0x40000000); expected.Word(object + 12, 0);
          for (uint32_t i = 0; i < 256; ++i) expected.Word(input + i * 4, 0);
          expected.Word(input + 160 * 4, sample == 34 ? 0xff7fffffu : 0x7f7fffffu);
        }
        if (sample == 38) {
          for (uint32_t i = 0; i < 256; ++i) expected.Word(input + i * 4, 0x80000000);
        }
        actual.Copy(expected);
        struct Canary { uint32_t at, size; std::array<uint8_t, 24> bytes; };
        std::array<Canary, 3> canary{}; uint32_t count = 0;
        for (const auto region : std::array<std::pair<uint32_t, uint32_t>, 3>{{{object, 24}, {input, 16}, {output, 16}}})
          if (region.first >= 0xe0000000u) {
            auto& next = canary[count++]; next.at = region.first; next.size = region.second;
            std::copy_n(actual.base + next.at, next.size, next.bytes.begin());
          }
        PPCContext original{};
        original.r1.u64 = 0x1234567800000000ull | stack; original.r3.u64 = 0x2345678900000000ull | object;
        original.r4.u64 = input; if (entry.separate) original.r5.u64 = output;
        original.r11.u64 = 0xfedc4321; original.lr = 0x82081234;
        original.vscr_sat = uint8_t(sample & 1); original.last_indirect_target = 0x81234567;
        original.f1.u64 = 0x40123456789abcde; original.f2.u64 = 0xc00123456789abcd;
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
        SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
        SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
        auto* vectors = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
        for (uint32_t i = 0; i < 128 * sizeof(PPCVRegister); ++i) vectors[i] = uint8_t(i * 19 + sample);
#define CR(n) original.cr##n.set_raw((sample + n) % 16);
        CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
        original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
        original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
        if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
        if (sample == 30) original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
        const auto initial = original.fpscr.csr; auto replacement = original;
        for (uint32_t block = 0; block < (sample == 31 ? 8u : 1u); ++block) {
          original.r4.u64 = replacement.r4.u64 = input;
          if (entry.separate) original.r5.u64 = replacement.r5.u64 = output;
          original.fpscr.csr = replacement.fpscr.csr = initial;
          std::array<uint8_t, 40> before;
          std::copy_n(actual.base + PhysicalOffset(stack - scratch), scratch, before.begin());
          std::array<uint8_t, 1024> before_input;
          std::array<uint8_t, 4> before_output_tail;
          if (entry.separate && native) {
            std::copy_n(actual.base + PhysicalOffset(input), before_input.size(), before_input.begin());
            std::copy_n(actual.base + PhysicalOffset(output + 1024), before_output_tail.size(), before_output_tail.begin());
          }
          Platform::setcsr(initial); entry.original(original, expected.base);
          const auto host_controls = Platform::getcsr() & controls;
          Platform::setcsr(initial); n3_feedback_delay_native_used = false; entry.replacement(replacement, actual.base);
          Require(n3_feedback_delay_native_used == native, "Physical feedback native/fallback route differs");
          if (native) {
            Require(std::equal(before.begin(), before.begin() + scratch, actual.base + PhysicalOffset(stack - scratch)), "Physical native feedback wrote scratch");
            std::copy_n(before.begin(), scratch, expected.base + PhysicalOffset(stack - scratch)); ++native_count;
            if (entry.separate) {
              Require(std::equal(before_input.begin(), before_input.end(), actual.base + PhysicalOffset(input)),
                      "Physical separate feedback changed its input prefix");
              Require(std::equal(before_output_tail.begin(), before_output_tail.end(), actual.base + PhysicalOffset(output + 1024)),
                      "Physical separate feedback wrote the separate-output tail");
              Require(rex::memory::load_and_swap<uint32_t>(actual.base + PhysicalOffset(input + 1024)) ==
                      rex::memory::load_and_swap<uint32_t>(actual.base + PhysicalOffset(object + 12)),
                      "Physical separate feedback omitted the input-tail output");
            }
          } else ++retained_count;
          if (sample == 33 || sample == 34) { if (native) ++late_native; else ++late_retained; }
          for (uint32_t i = 0; i < count; ++i) {
            const auto& check = canary[i];
            Require(std::equal(check.bytes.begin(), check.bytes.begin() + check.size, actual.base + check.at) &&
                    std::equal(check.bytes.begin(), check.bytes.begin() + check.size, expected.base + check.at), "Physical feedback changed an unbiased canary");
            ++canaries;
          }
          Require(actual.Same(expected), "Physical feedback whole-fixture bytes differ");
#define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Physical feedback nonvolatile state differs");
          CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
          CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
#undef CHECK
          if (!native) {
#define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Retained physical feedback scalar state differs");
            CHECK(0) CHECK(1) CHECK(2) CHECK(3) CHECK(4) CHECK(5) CHECK(6) CHECK(7) CHECK(8) CHECK(9) CHECK(10) CHECK(11) CHECK(12) CHECK(13)
#undef CHECK
          }
          Require(original.r1.u64 == replacement.r1.u64 && original.r3.u64 == replacement.r3.u64 && original.r4.u64 == replacement.r4.u64 &&
                  original.r5.u64 == replacement.r5.u64 && original.f1.u64 == replacement.f1.u64 && original.f2.u64 == replacement.f2.u64 &&
                  original.lr == replacement.lr && original.ctr.u64 == replacement.ctr.u64, "Physical feedback contracted registers differ");
          static_assert(offsetof(PPCContext, v127) + sizeof(PPCVRegister) - offsetof(PPCContext, v0) == 128 * sizeof(PPCVRegister));
          Require(!std::memcmp(&original.v0, &replacement.v0, 128 * sizeof(PPCVRegister)) &&
                  original.msr == replacement.msr && original.vscr_sat == replacement.vscr_sat &&
                  original.last_indirect_target == replacement.last_indirect_target, "Physical feedback preserved vector/control state differs");
#define CR(n) Require(original.cr##n.raw() == replacement.cr##n.raw(), "Physical feedback condition field differs");
          CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
          Require(original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so &&
                  (original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == host_controls, "Physical feedback flags/FP controls differ");
          ++comparisons;
        }
      } catch (const std::exception&) {
        std::cerr << "Physical feedback entry " << std::hex << entry.address << std::dec << " sample " << sample << " rounding " << rounding << " flush " << flush << '\n';
        throw;
      }
    }
  Require(native_count && retained_count && canaries && late_native && late_retained, "Physical feedback coverage missing");
  std::cout << comparisons << " physical feedback calls (" << native_count << " native / " << retained_count << " retained) passed\n";
  return comparisons;
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  constexpr uint32_t stack = 60000;
  constexpr uint64_t marker = 0xfedc4321;
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, rounding = 0, history_size = 0;
  bool flush = false;
  try {
    using Function = void (*)(PPCContext&, uint8_t*);
    struct Entry { uint32_t address, history_size; Function original, replacement; bool separate = false; };
    const std::array<Entry, 4> entries{{
        {0x8253BFA8, 128, __imp__sub_8253BFA8, sub_8253BFA8},
        {0x8253C310, 256, __imp__sub_8253C310, sub_8253C310},
        {0x8253C480, 512, __imp__sub_8253C480, sub_8253C480},
        {0x8253C5F0, 512, __imp__sub_8253C5F0, sub_8253C5F0, true}}};
    std::mt19937 random(0x4644424b);
    uint32_t comparisons = 0;
    for (const auto& entry : entries) {
      history_size = entry.history_size;
      const auto object_size = 24 + history_size * 4;
      const uint32_t scratch = entry.separate ? 40 : 32;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == entry.address) registered = mapping->host == entry.replacement;
      Require(registered, "Feedback entry missing from indirect-call mapping");
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < 258; ++sample) {
          std::vector<uint8_t> expected(65536), actual;
          for (auto& byte : expected) byte = uint8_t(random());
          uint32_t object = 256 + sample % 4, buffer = 16384 + sample % 4;
          uint32_t output = 32768 + sample % 4;
          if (sample >= 128 && sample < 160) {
            if (entry.separate) {
              switch (sample % 8) {
                case 0: output = buffer; break;
                case 1: output = buffer + 4; break;
                case 2: output = object + 24; break;
                case 3: output = object + 12; break;
                case 4: buffer = object + 24; break;
                case 5: buffer = object + 12; break;
                case 6: output = object + object_size - 4; break;
                case 7: object = buffer + 16; break;
              }
            }
            else if (sample % 4 == 0) buffer = object + 24;
            else if (sample % 4 == 1) buffer = object + 12;
            else if (sample % 4 == 2) buffer = object + object_size - 4;
            else object = buffer + 16;
          }
          if (sample >= 160 && sample < 192) {
            if (entry.separate) {
              switch (sample % 4) {
                case 0: object = stack - 16 - object_size; break;
                case 1: buffer = stack - 1028; break;
                case 2: output = stack - 1024; break;
                case 3: buffer = stack - 40 - 1024; break;
              }
            }
            else if (sample % 2) object = stack - 16 - object_size;
            else buffer = stack - 1028;
          }
          auto store = [&](uint32_t location, uint32_t value) {
            rex::memory::store_and_swap<uint32_t>(expected.data() + location, value);
          };
          auto value = [&]() -> uint32_t {
            if (sample >= 192) return sample < 224 || sample >= 256 ? 0x3f800000u : 0;
            if (sample < 8) return sample & 1 ? 0x80000000u : 0;
            return (random() & 0x807fffffu) | ((122 + random() % 9) << 23);
          };
          for (uint32_t index = 0; index < history_size; ++index) store(object + 24 + index * 4, value());
          for (uint32_t index = 0; index < 256; ++index) store(buffer + index * 4, value());
          constexpr std::array<uint32_t, 8> boundaries{0, 1, 2, 3, 127, 128, 255, 256};
          store(object, sample < 8 ? boundaries[sample] : random());
          store(object + 4, 0x3f000000); store(object + 8, 0x3e000000);
          store(object + 12, 0x40e00000); store(object + 16, 0);
          if (history_size == 512 && (sample < 96 || sample >= 192))
            store(object + 16, sample % 2 ? 256 : 0);
          if (sample >= 16 && sample < 32) {
            const std::array<uint32_t, 8> delays{0, 1, 2, 3, history_size - 1, history_size, history_size + 1, 0xffffffffu};
            store(object, delays[sample % delays.size()]);
          }
          if (sample >= 64 && sample < 96) {
            constexpr std::array<uint32_t, 6> exceptions{1, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u};
            store(sample % 3 == 0 ? object + 4 : sample % 3 == 1 ? buffer + 12 : object + 24, exceptions[sample % exceptions.size()]);
          }
          if (sample >= 96 && sample < 128) {
            const std::array<uint32_t, 4> cursors{1, 255, history_size - 1, history_size};
            store(object + 16, cursors[sample % cursors.size()]);
          }
          if (sample >= 192) {
            store(object, 1); store(object + 4, 0x3f800000); store(object + 12, 0);
            store(object + 8, sample < 224 ? 0x40000000 : sample < 256 ? 0x3f000000 : 0);
            for (uint32_t index = 0; index < history_size; ++index) store(object + 24 + index * 4, 0);
            if (sample < 224) {
              // Half the trials fail only after the 128-sample history wraps.
              if (sample & 2) for (uint32_t index = 0; index < 256; ++index) store(buffer + index * 4, 0);
              else if (sample & 1) for (uint32_t index = 0; index < 256; ++index) store(buffer + index * 4, 0xbf800000);
              store(buffer + (sample & 2 ? 160 : 32) * 4, sample & 1 ? 0xff7fffffu : 0x7f7fffffu);
            }
            else if (sample < 256) { store(buffer, sample & 1 ? 0xbf800000u : 0x3f800000u); store(buffer + 1024, 0x7f812345); }
          }
          actual = expected; const auto before = expected;
          PPCContext original{};
          original.r1.u32 = stack; original.r3.u32 = object; original.r4.u32 = buffer;
          if (entry.separate) original.r5.u32 = output;
          original.r11.u64 = marker; original.lr = 0x82081234;
          original.f1.u64 = 0x40123456789abcde; original.f2.u64 = 0xc00123456789abcd;
  #define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
          SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
  #undef SEED
          original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
          original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
          if (flush) original.fpscr.enableFlushModeUnconditional();
          else original.fpscr.disableFlushModeUnconditional();
          if (sample >= 256) { original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); Platform::setcsr(original.fpscr.csr); }
          const auto initial = original.fpscr.csr;
          auto replacement = original;
          for (uint32_t block = 0; block < (sample >= 8 && sample < 16 ? 8u : 1u); ++block) {
            original.r4.u32 = replacement.r4.u32 = buffer;
            if (entry.separate) original.r5.u32 = replacement.r5.u32 = output;
            original.r11.u64 = replacement.r11.u64 = marker;
            original.fpscr.csr = replacement.fpscr.csr = initial;
            Platform::setcsr(initial);
            entry.original(original, expected.data());
            const auto host_controls = Platform::getcsr() & controls;
            Platform::setcsr(initial);
            entry.replacement(replacement, actual.data());
            // Directed overflow can saturate to a finite signed maximum. Those
            // blocks are supported; nonfinite trial results must fall back.
            const bool native_case = sample < 64 || (sample >= 224 && sample < 256) ||
                (sample >= 192 && sample < 224 &&
                 (rounding == 1 || (sample & 1 ? rounding == 2 : rounding == 3)));
            if (native_case) {
              Require(replacement.r11.u64 == marker && original.r11.u64 != marker, "Ordinary feedback did not use native path");
              Require(std::equal(actual.begin() + stack - scratch, actual.begin() + stack, before.begin() + stack - scratch), "Native feedback wrote guest stack scratch");
              std::copy_n(before.begin() + stack - scratch, scratch, expected.begin() + stack - scratch);
              if (entry.separate)
                Require(std::equal(actual.begin() + buffer, actual.begin() + buffer + 1024, before.begin() + buffer),
                        "Separate feedback mutated the input prefix");
            }
            Require(actual == expected, "Native/translated feedback bytes differ");
            Require(original.r1.u64 == replacement.r1.u64 && original.r3.u64 == replacement.r3.u64 &&
                    original.r4.u64 == replacement.r4.u64 && original.r5.u64 == replacement.r5.u64 &&
                    original.f1.u64 == replacement.f1.u64 && original.f2.u64 == replacement.f2.u64 &&
                    original.lr == replacement.lr, "Feedback calling convention differs");
  #define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Feedback nonvolatile state differs");
            CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
            CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
  #undef CHECK
            Require(original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                    original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so,
                    "Feedback condition/carry flags differ");
            Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                    (Platform::getcsr() & controls) == host_controls, "Feedback FP controls differ");
            ++comparisons;
          }
        }
      }
    }
    comparisons += CheckPhysicalFeedback();
    Platform::setcsr(saved);
    std::cout << comparisons << " feedback block equivalence cases, chained history, rollback and FP controls passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Feedback history " << history_size << " sample " << sample << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n';
    return 1;
  }
}
