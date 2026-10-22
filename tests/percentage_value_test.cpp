#include <Windows.h>
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

extern "C" void sub_8209D688(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D688(PPCContext&, uint8_t*);
extern "C" void sub_820B4DC8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_820B4DC8(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_percentage_original_callees = false;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 4> regions{{{0, 0x20000}, {0x82041000, 8192},
    {0x8205b000, 4096}, {0x82b5f000, 8192}}};
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve sparse percentage fixture");
    for (auto region : regions) if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
      VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit percentage fixture");
    }
  }
  ~Guest() { VirtualFree(base, 0, MEM_RELEASE); }
  Guest(const Guest&) = delete;
  void Copy(const Guest& other) { for (auto region : regions) std::memcpy(base + region.start, other.base + region.start, region.size); }
  bool Same(const Guest& other) const {
    for (auto region : regions) if (std::memcmp(base + region.start, other.base + region.start, region.size)) return false;
    return true;
  }
  void Word(uint32_t at, uint32_t bits) { rex::memory::store_and_swap<uint32_t>(base + at, bits); }
};
void CheckRetainedRegisters(const PPCContext& original, const PPCContext& actual) {
#define CHECK(n) Require(original.r##n.u64 == actual.r##n.u64 && original.f##n.u64 == actual.f##n.u64, "Retained percentage register differs");
  CHECK(0) CHECK(1) CHECK(2) CHECK(3) CHECK(4) CHECK(5) CHECK(6) CHECK(7) CHECK(8) CHECK(9)
  CHECK(10) CHECK(11) CHECK(12) CHECK(13) CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19)
  CHECK(20) CHECK(21) CHECK(22) CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29)
  CHECK(30) CHECK(31)
#undef CHECK
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, rounding = 0, address = 0;
  bool flush = false;
  try {
    using Function = void (*)(PPCContext&, uint8_t*);
    struct Entry { uint32_t address; Function original, replacement; bool conditional; };
    constexpr std::array<Entry, 2> entries{{{0x8209d688, __imp__sub_8209D688, sub_8209D688, false},
        {0x820b4dc8, __imp__sub_820B4DC8, sub_820B4DC8, true}}};
    Guest seed, expected, actual, probe;
    std::mt19937 random(0x50455243);
    for (auto region : regions) for (uint32_t i = 0; i < region.size; ++i) seed.base[region.start + i] = uint8_t(random());
    constexpr uint64_t marker = 0xfedcbabe;
    constexpr std::array<uint32_t, 16> floats{0, 0x80000000u, 0x3f800000u, 0xbf800000u,
        1, 0x80000001u, 0x00800000u, 0x007fffffu, 0x7f7fffffu, 0xff7fffffu,
        0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u, 0x3c23d70au, 0x33800000u};
    constexpr std::array<uint32_t, 8> integers{0, 1, 0xffffffffu, 0x80000000u,
        0x7fffffffu, 0x01000001u, 0x01000003u, 100};
    uint32_t comparisons = 0, native_count = 0, retained_count = 0, probes = 0;
    for (auto entry : entries) {
      address = entry.address;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == address) registered = mapping->host == entry.replacement;
      Require(registered, "Percentage entry missing from indirect mapping");
      PPCContext probe_context{};
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < 522; ++sample) {
          expected.Copy(seed);
          uint32_t stack = 0x18000, object = 0x1000 + sample % 4;
          uint32_t root = 0x4000 + sample % 4, table = 0x9000 + sample % 4;
          const auto descriptor = entry.conditional ? 8u : 88u;
          uint32_t link = 0x82081234, argument = sample < 32 ? sample : integers[sample % integers.size()];
          uint32_t channel = sample % 12, kind = sample % 2 ? 1000 : 17, extra = sample % 10;
          if (sample >= 320 && sample < 384) {
            constexpr std::array<uint32_t, 8> channels{12, 13, 0xffffffffu, 0xffffff00u,
                0x40000000u, 0x80000000u, 0xfffffffeu, 11};
            kind = 17; channel = channels[sample % channels.size()];
          }
          if (sample >= 384 && sample < 448) {
            kind = 17; channel = 0; extra = 0;
            if (sample % 4 == 0) object = stack - 970 - 8;
            else if (sample % 4 == 1) table = stack - 16;
            else if (sample % 4 == 2) table = object + 640;
            else table = stack - (entry.conditional ? 128 : 256);
          }
          for (uint32_t i = 0; i < 256; ++i) {
            expected.Word(root + i * 16 + descriptor, table);
            expected.Word(root + i * 16 + descriptor + 4, table + 16 * 48);
          }
          for (uint32_t i = 0; i < 16 * 12; ++i)
            expected.Word(table + i * 4, sample < 32 ? 100 : uint32_t(int32_t(random() % 200001) - 100000));
          expected.Word(0x82b60088, root);
          expected.Word(0x82042048, 0x3c23d70a); expected.Word(0x8205b614, 0x3f800000); expected.Word(0x8205b610, 0);
          expected.Word(object + 300, kind); expected.Word(object + 304, sample % 16);
          expected.Word(object + 676, channel); expected.Word(object + 780, extra);
          expected.base[object + 969] = uint8_t(sample);
          expected.Word(object + 728, sample < 32 ? 10 : uint32_t(int32_t(random() % 401) - 200));
          expected.Word(object + 732, sample < 32 ? 5 : random());
          expected.Word(object + 736, sample < 32 ? 25 : uint32_t(int32_t(random() % 401) - 200));
          expected.Word(object + 664, std::bit_cast<uint32_t>(sample < 32 ? 10.0f : float(int32_t(random() % 2001) - 1000)));
          expected.Word(object + 1376, sample < 32 ? 40 : integers[(sample + 3) % integers.size()]);
          expected.base[object + 1336] = sample % 4 == 1 ? 0xff : sample % 4 == 2 ? 1 : 0;
          expected.base[object + 719] = sample % 4 == 3 ? 0x80 : 0;
          if (sample >= 192 && sample < 256) {
            const auto bits = floats[sample % floats.size()];
            if (sample % 4 == 0) expected.Word(0x82042048, bits);
            else if (sample % 4 == 1) expected.Word(0x8205b614, bits);
            else if (sample % 4 == 2) expected.Word(object + 664, bits);
            else expected.Word(0x8205b610, bits);
          }
          if (sample >= 256 && sample < 320) {
            expected.Word(object + (entry.conditional ? 736 : 728), integers[sample % integers.size()]);
            expected.Word(object + 732, integers[(sample + 3) % integers.size()]);
            for (uint32_t i = 0; i < 16 * 12; ++i) expected.Word(table + i * 4, integers[(sample + 1) % integers.size()]);
          }
          if (sample >= 448 && sample < 512) {
            const auto bits = floats[sample % floats.size()];
            expected.Word(0x82042048, bits); expected.Word(object + 664, floats[(sample + 5) % floats.size()]);
          }
          const bool exact_unmasked = sample >= 520;
          if (sample >= 512) {
            const auto alias = sample - 512;
            object = 0x1000; root = 0x4000; table = 0x9000; argument = 0;
            if (alias == 4 && entry.conditional) object = stack - 124 - 1336;
            if (alias == 5 && entry.conditional) object = stack - 128 - 1376;
            if (alias == 6) object = stack - 720;
            if (alias == 7) object = stack - 32 - 728;
            expected.Word(object + 300, 17); expected.Word(object + 304, 0);
            expected.Word(object + 676, 0); expected.Word(object + 780, 0);
            expected.base[object + 969] = 0;
            expected.Word(object + 728, 0); expected.Word(object + 732, 0); expected.Word(object + 736, 0);
            expected.Word(object + 664, 0x3f800000);
            expected.Word(object + 1376, 100);
            expected.base[object + 1336] = alias == 5 ? 1 : 0; expected.base[object + 719] = 0;
            expected.Word(0x82b60088, root);
            expected.Word(0x82042048, exact_unmasked ? 0 : 0x3c23d70a);
            expected.Word(0x8205b614, 0x3f800000); expected.Word(0x8205b610, 0);
            expected.Word(root + descriptor, table); expected.Word(root + descriptor + 4, table + 16 * 48);
            for (uint32_t i = 0; i < 16 * 12; ++i) expected.Word(table + i * 4, alias == 4 ? 0x01000000 : 1);
            if (alias == 0) { stack = 0x82b60090; link = root; expected.Word(0x82b60088, 0xdead0000); }
            if (alias == 1) {
              stack = root + descriptor + 8; link = table;
              expected.Word(root + descriptor, 0x30000); expected.Word(root + descriptor + 4, 0x30300);
            }
            if (alias == 2) { stack = table + 8; link = 1; }
            if (alias == 3) { stack = object + 736; link = 0; }
          }
          actual.Copy(expected);
          const auto scratch = entry.conditional ? 128u : 256u;
          std::array<uint8_t, 256> before_stack;
          std::copy_n(expected.base + stack - scratch, scratch, before_stack.begin());
          PPCContext original{};
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(0) SEED(1) SEED(2) SEED(3) SEED(4) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9)
          SEED(10) SEED(11) SEED(12) SEED(13) SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19)
          SEED(20) SEED(21) SEED(22) SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29)
          SEED(30) SEED(31)
#undef SEED
          original.r1.u64 = stack; original.r3.u64 = object; original.r4.u64 = argument;
          original.r12.u64 = marker; original.lr = uint64_t{0x12345678} << 32 | link;
          original.ctr.u64 = 0x87654321;
          original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
          original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
          if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
          if (exact_unmasked) { original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); Platform::setcsr(original.fpscr.csr); }
          const auto initial = original.fpscr.csr; auto replacement = original;
          if (sample == 0) { probe.Copy(expected); probe_context = original; }
          n3_percentage_original_callees = true;
          Platform::setcsr(initial); entry.original(original, expected.base);
          const auto original_controls = Platform::getcsr() & controls;
          if (entry.conditional && sample == 516)
            Require(original.r9.u64 == 1 && original.r3.u64 > 0x01000000,
                    "Scratch flag fixture did not enable the original bonus");
          if (entry.conditional && sample == 517)
            Require(original.r3.u64 == 1, "Scratch percentage fixture did not replace the original percentage");
          n3_percentage_original_callees = false;
          Platform::setcsr(initial); entry.replacement(replacement, actual.base);
          const bool native = replacement.r12.u64 == marker;
          if (native) {
            Require(!exact_unmasked, "Unmasked percentage used native path");
            Require(std::equal(actual.base + stack - scratch, actual.base + stack, before_stack.begin()),
                    "Native percentage wrote original scratch");
            std::copy_n(before_stack.begin(), scratch, expected.base + stack - scratch); ++native_count;
          } else {
            Require(sample >= 32, "Ordinary percentage fixture did not use native path");
            CheckRetainedRegisters(original, replacement); ++retained_count;
          }
          Require(expected.Same(actual), "Percentage whole-fixture bytes differ");
#define DETAIL_GPR(n) if (original.r##n.u64 != replacement.r##n.u64) std::cerr << "r" #n << " expected " << std::hex << original.r##n.u64 << " actual " << replacement.r##n.u64 << std::dec << '\n';
          DETAIL_GPR(1) DETAIL_GPR(3) DETAIL_GPR(4) DETAIL_GPR(7) DETAIL_GPR(8) DETAIL_GPR(9) DETAIL_GPR(10) DETAIL_GPR(11)
#undef DETAIL_GPR
          if (original.f0.u64 != replacement.f0.u64 || original.f13.u64 != replacement.f13.u64)
            std::cerr << "FP expected " << std::hex << original.f0.u64 << "/" << original.f13.u64
                      << " actual " << replacement.f0.u64 << "/" << replacement.f13.u64 << std::dec << '\n';
          Require(original.r1.u64 == replacement.r1.u64 && original.r3.u64 == replacement.r3.u64 &&
                  original.r4.u64 == replacement.r4.u64 && original.r7.u64 == replacement.r7.u64 &&
                  original.r8.u64 == replacement.r8.u64 && original.r9.u64 == replacement.r9.u64 &&
                  original.r10.u64 == replacement.r10.u64 && original.r11.u64 == replacement.r11.u64 &&
                  original.f0.u64 == replacement.f0.u64 && original.f13.u64 == replacement.f13.u64 &&
                  original.lr == replacement.lr && original.ctr.u64 == replacement.ctr.u64,
                  "Percentage result or incidental calling convention differs");
          if (entry.conditional) Require(original.r5.u64 == replacement.r5.u64 && original.r6.u64 == replacement.r6.u64,
                                         "Conditional percentage base/mode registers differ");
#define CHECK_F(n) Require(original.f##n.u64 == replacement.f##n.u64, "Percentage input FP register changed");
          CHECK_F(1) CHECK_F(2) CHECK_F(3) CHECK_F(4)
#undef CHECK_F
#define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Percentage nonvolatile register differs");
          CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
          CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
#undef CHECK
          Require(original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                  original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so,
                  "Percentage condition/carry flags differ");
          Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == original_controls, "Percentage FP controls differ");
          ++comparisons;
        }
        // A complete unsupported chain must release its thread-local override.
        // The next ordinary call must still select native C++.
        actual.Copy(probe); auto context = probe_context;
        Platform::setcsr(context.fpscr.csr); entry.replacement(context, actual.base);
        Require(context.r12.u64 == marker, "Retained-chain depth leaked into a later native call"); ++probes;
      }
    }
    Platform::setcsr(saved);
    std::cout << comparisons << " percentage equivalence cases (" << native_count << " native, " << retained_count
              << " complete retained chains), plus " << probes << " post-fallback native probes passed\n";
  } catch (const std::exception& error) {
    n3_percentage_original_callees = false; Platform::setcsr(saved);
    std::cerr << "Percentage 0x" << std::hex << address << std::dec << " sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n'; return 1;
  }
}
