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
#include <vector>

extern "C" void sub_8209DBF0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209DBF0(PPCContext&, uint8_t*);
extern "C" void sub_8209D718(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D718(PPCContext&, uint8_t*);
extern "C" void sub_8209DCF0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209DCF0(PPCContext&, uint8_t*);
extern "C" void sub_8209DDE8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209DDE8(PPCContext&, uint8_t*);
extern "C" void sub_820A13B0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_820A13B0(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_bounded_original_callee = false;
void Require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 4> regions{{{0, 0x20000}, {0x82041000, 8192},
    {0x8205b000, 4096}, {0x82b5f000, 8192}}};
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve sparse guest fixture");
    for (auto region : regions)
      if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit sparse guest fixture");
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
  void Word(uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(base + at, value); }
};
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, rounding = 0, address = 0;
  bool flush = false;
  try {
    using Function = void (*)(PPCContext&, uint8_t*);
    struct Entry {
      uint32_t address; Function original, replacement; bool adjust;
      uint32_t descriptor = 88, percentage = 728; bool modifier = false;
      bool Wrapper() const { return adjust || modifier; }
      bool Getter() const { return descriptor != 88; }
    };
    constexpr std::array<Entry, 5> entries{{
        {0x8209dbf0, __imp__sub_8209DBF0, sub_8209DBF0, false},
        {0x8209d718, __imp__sub_8209D718, sub_8209D718, true},
        {0x8209dcf0, __imp__sub_8209DCF0, sub_8209DCF0, false, 8, 736},
        {0x8209dde8, __imp__sub_8209DDE8, sub_8209DDE8, false, 40, 740},
        {0x820a13b0, __imp__sub_820A13B0, sub_820A13B0, false, 88, 728, true}}};
    Guest seed, expected, actual, before, production;
    std::mt19937 random(0x424e4456);
    for (auto region : regions)
      for (uint32_t i = 0; i < region.size; ++i) seed.base[region.start + i] = uint8_t(random());
    constexpr uint32_t ordinary_stack = 0x18000;
    constexpr uint64_t marker = 0xfedc4321;
    constexpr std::array<uint32_t, 16> floats{0, 0x80000000u, 0x3f800000u, 0xbf800000u,
        1, 0x80000001u, 0x00800000u, 0x007fffffu, 0x7f7fffffu, 0xff7fffffu,
        0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u, 0x3f800001u, 0x33800000u};
    constexpr std::array<uint32_t, 8> integers{0, 1, 0xffffffffu, 0x80000000u,
        0x7fffffffu, 0x01000001u, 0x01000003u, 100};
    constexpr std::array<uint32_t, 8> modes{0, 1, 256, 257, 0xffffffffu, 0x80000000u, 0xff00, 0x100ff};
    uint32_t comparisons = 0, native_count = 0, compositions = 0, native_inner = 0;
    for (auto entry : entries) {
      address = entry.address;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == entry.address) registered = mapping->host == entry.replacement;
      Require(registered, "Bounded value missing from indirect-call mapping");
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < (entry.modifier ? 294u : entry.adjust ? 262u : 258u); ++sample) {
          expected.Copy(seed);
          uint32_t stack = ordinary_stack;
          uint32_t object = 0x1000 + sample % 4, table = (entry.Getter() ? 0x9000 : 0x8000) + sample % 4;
          uint32_t root = 0x4000 + sample % 4;
          uint32_t mode = modes[sample % modes.size()];
          uint32_t argument = sample >= 256 ? 1 : sample < 16 ? sample : integers[sample % integers.size()];
          const bool unmasked = sample == 256 || sample == 257;
          const bool write_alias = entry.modifier && sample >= 258 && sample < 290;
          const bool stack_alias = entry.Wrapper() && sample >= (entry.modifier ? 290u : 258u);
          uint32_t link = 0x82081234;
          uint32_t channel = sample % 12, kind = sample % 2 ? 1000 : 17, extra = sample % 10;
          if (sample >= 160 && sample < 192) {
            if (sample % 4 == 0) object = stack - 970 - 8;
            else if (sample % 4 == 1) table = stack - 16;
            else if (sample % 4 == 2) table = object + 640;
            else table = stack - 120;
            kind = 17; channel = 0; extra = 0;
          }
          if (sample >= 192 && sample < 224) {
            constexpr std::array<uint32_t, 8> channels{12, 13, 0xffffffffu, 0xffffff00u,
                0x40000000u, 0x80000000u, 0xfffffffeu, 11};
            channel = channels[sample % channels.size()]; kind = 17;
          }
          for (uint32_t i = 0; i < 256; ++i) {
            expected.Word(root + i * 16 + entry.descriptor, table);
            expected.Word(root + i * 16 + entry.descriptor + 4, table + 16 * 48);
          }
          for (uint32_t i = 0; i < 16 * 12; ++i)
            expected.Word(table + i * 4, sample < 16 ? 80u : uint32_t(int32_t(random() % 200001) - 100000));
          expected.Word(0x82b60088, root);
          expected.Word(0x82042048, 0x3c23d70a); expected.Word(0x8205b614, 0x3f800000); expected.Word(0x8205b610, 0);
          expected.Word(object + 300, kind); expected.Word(object + 304, sample % 16);
          expected.Word(object + 676, channel); expected.Word(object + 780, extra);
          expected.base[object + 969] = uint8_t(sample);
          expected.Word(object + 728, sample < 16 ? 10u : uint32_t(int32_t(random() % 401) - 200));
          expected.Word(object + 732, sample < 16 ? 5u : random());
          expected.Word(object + 736, 25); expected.Word(object + 740, 50);
          if (entry.Getter() && sample >= 16)
            expected.Word(object + entry.percentage, uint32_t(int32_t(random() % 401) - 200));
          expected.Word(object + 664, std::bit_cast<uint32_t>(sample < 16 ? 10.0f : float(int32_t(random() % 2001) - 1000)));
          if (sample >= 96 && sample < 160) {
            const auto bits = floats[sample % floats.size()];
            if (sample % 4 == 0) expected.Word(0x82042048, bits);
            else if (sample % 4 == 1) expected.Word(0x8205b614, bits);
            else if (sample % 4 == 2) expected.Word(object + 664, bits);
            else expected.Word(0x8205b610, bits);
          }
          if (sample >= 224 && sample < 256) {
            expected.Word(object + entry.percentage, integers[sample % integers.size()]);
            expected.Word(object + 732, integers[(sample + 3) % integers.size()]);
            for (uint32_t i = 0; i < 16 * 12; ++i) expected.Word(table + i * 4, integers[(sample + 1) % integers.size()]);
          }
          if (unmasked) {
            expected.Word(object + entry.percentage, 0); expected.Word(object + 732, 0); expected.Word(object + 664, 0x3f800000);
            expected.Word(0x82042048, 0); expected.Word(0x8205b614, 0x3f800000); expected.Word(0x8205b610, 0);
            for (uint32_t i = 0; i < 16 * 12; ++i) expected.Word(table + i * 4, 1);
          }
          if (write_alias) {
            // Each original setter write deliberately replaces a source before
            // its nested lookup. Old pointers in two cases are uncommitted, so
            // reading them before rejecting is a fault, not just a wrong result.
            const auto alias = (sample - 258) % 8;
            mode = ((sample - 258) / 8) % 2 ? 1 : 0;
            const auto modifier_offset = mode ? 728u : 732u;
            root = 0x4000; table = 0x9000;
            const uint32_t target = alias == 0 ? table : alias == 1 ? root + 88 :
                alias == 2 ? 0x82b60088u : alias == 3 ? 0x82042048u :
                alias == 4 ? 0x8205b614u : alias == 5 ? root + 92 :
                alias == 6 ? table + 1 : root + 89;
            object = target - modifier_offset;
            expected.Word(object + 300, 17); expected.Word(object + 304, 0);
            expected.Word(object + 676, 0); expected.Word(object + 780, 0);
            expected.base[object + 969] = 0;
            expected.Word(object + 728, 0); expected.Word(object + 732, 0);
            expected.Word(object + 664, 0x80000000u);
            expected.Word(0x82b60088, root);
            expected.Word(0x82042048, 0); expected.Word(0x8205b614, 0x3f800000);
            expected.Word(root + 88, table); expected.Word(root + 92, table + 16 * 48);
            for (uint32_t i = 0; i < 16 * 12; ++i) expected.Word(table + i * 4, 1);
            argument = 1;
            if (alias == 1) {
              expected.Word(target, 0x30000); expected.Word(root + 92, 0x30300); argument = table;
            }
            if (alias == 2) { expected.Word(target, 0xdead0000); argument = root; }
            if (alias == 3 || alias == 4) argument = 0x3f000000;
            if (alias == 5) argument = table + 16 * 48;
            if (alias == 6) argument = 0x00000001;
            if (alias == 7) {
              // A partial overlap that moves the pointer by one whole record;
              // all original reads stay mapped after the write.
              argument = 0x00903000;
            }
          }
          if (stack_alias) {
            const auto alias = sample - (entry.modifier ? 290u : 258u);
            object = 0x1000; root = 0x4000; table = 0x9000;
            mode = 0; argument = 1;
            expected.Word(object + 300, 17); expected.Word(object + 304, 0);
            expected.Word(object + 676, 0); expected.Word(object + 780, 0);
            expected.base[object + 969] = 0;
            expected.Word(object + 728, 0); expected.Word(object + 732, 0);
            expected.Word(object + 664, 0x80000000u);
            expected.Word(0x82b60088, root);
            expected.Word(0x82042048, 0); expected.Word(0x8205b614, 0x3f800000);
            expected.Word(root + 88, table); expected.Word(root + 92, table + 16 * 48);
            for (uint32_t i = 0; i < 16 * 12; ++i) expected.Word(table + i * 4, 1);
            if (alias == 0) {
              stack = 0x82b60090; link = root; expected.Word(0x82b60088, 0xdead0000);
            } else if (alias == 1) {
              stack = root + 96; link = table;
              expected.Word(root + 88, 0x30000); expected.Word(root + 92, 0x30300);
            } else if (alias == 2) {
              stack = table + 8; link = 1;
            } else {
              stack = object + 736; link = 0;
            }
          }
          actual.Copy(expected);
          if (entry.Wrapper()) before.Copy(expected);
          const uint32_t scratch = entry.Wrapper() ? 128 : 16;
          std::array<uint8_t, 128> before_stack;
          std::copy_n(expected.base + stack - scratch, scratch, before_stack.begin());
          PPCContext original{};
          original.r1.u32 = stack; original.r3.u32 = object;
          original.r4.u32 = argument; original.r5.u32 = mode;
          original.r8.u64 = marker; original.lr = link;
          original.f1.u64 = 0x40123456789abcde; original.f2.u64 = 0xc00123456789abcd;
          original.f3.u64 = 0x40023456789abcde; original.f4.u64 = 0xc01123456789abcd;
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
          SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
          original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
          original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
          if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
          if (unmasked) { original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); Platform::setcsr(original.fpscr.csr); }
          const auto initial = original.fpscr.csr; auto replacement = original;
          const auto input_context = original;
          // Force the retained nested callee in both retained execution and
          // fallback. The ordinary replacement uses the typed model directly.
          n3_bounded_original_callee = entry.Wrapper();
          Platform::setcsr(initial); entry.original(original, expected.base);
          const auto host_controls = Platform::getcsr() & controls;
          Platform::setcsr(initial); entry.replacement(replacement, actual.base);
          n3_bounded_original_callee = false;
          if (replacement.r8.u64 == marker) {
            Require(!unmasked && !write_alias && !stack_alias, "Unmasked or aliased call unexpectedly used native path");
            Require(std::equal(actual.base + stack - scratch, actual.base + stack, before_stack.begin()),
                    "Native bounded value wrote stack scratch");
            std::copy_n(before_stack.begin(), scratch, expected.base + stack - scratch); ++native_count;
          } else Require(sample >= 16, "Ordinary bounded value did not use native path");
          Require(expected.Same(actual), "Bounded value whole-fixture bytes differ");
          Require(original.r1.u64 == replacement.r1.u64 && original.r3.u64 == replacement.r3.u64 &&
                  original.r4.u64 == replacement.r4.u64 && original.r7.u64 == replacement.r7.u64 &&
                  original.r9.u64 == replacement.r9.u64 && original.f0.u64 == replacement.f0.u64 &&
                  original.f1.u64 == replacement.f1.u64 && original.f2.u64 == replacement.f2.u64 &&
                  original.f3.u64 == replacement.f3.u64 && original.f4.u64 == replacement.f4.u64 &&
                  original.lr == replacement.lr, "Bounded value return/calling convention differs");
          if (entry.Getter())
            Require(original.r5.u64 == replacement.r5.u64 && original.r6.u64 == replacement.r6.u64 &&
                    original.r11.u64 == replacement.r11.u64, "Limit getter incidental registers differ");
          if (entry.modifier)
            Require(original.r11.u64 == replacement.r11.u64 && original.f13.u64 == replacement.f13.u64,
                    "Modifier post-write limit/current registers differ");
#define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Bounded value nonvolatile state differs");
          CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
          CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
#undef CHECK
          Require(original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                  original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so,
                  "Bounded value condition/carry flags differ");
          Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == host_controls, "Bounded value FP controls differ");
          if (entry.Wrapper()) {
            // Exercise the actual game composition too: an outer rejection can
            // retain the caller while its nested limit call uses typed C++.
            production.Copy(before); auto composed = input_context;
            Platform::setcsr(initial); entry.replacement(composed, production.base);
            if (replacement.r8.u64 != marker && composed.r8.u64 == marker) {
              Require(std::equal(production.base + stack - 128, production.base + stack - 112, before.base + stack - 128),
                      "Native nested limit unexpectedly wrote scratch");
              std::copy_n(before.base + stack - 128, 16, expected.base + stack - 128); ++native_inner;
            }
            Require(expected.Same(production), "Production fallback/native-callee composition bytes differ");
            Require(original.r1.u64 == composed.r1.u64 && original.r3.u64 == composed.r3.u64 &&
                    original.r4.u64 == composed.r4.u64 && original.r7.u64 == composed.r7.u64 &&
                    original.r9.u64 == composed.r9.u64 && original.f0.u64 == composed.f0.u64 &&
                    original.f1.u64 == composed.f1.u64 && original.f2.u64 == composed.f2.u64 && original.lr == composed.lr,
                    "Production composition calling convention differs");
            Require(original.f3.u64 == composed.f3.u64 && original.f4.u64 == composed.f4.u64,
                    "Production composition changed caller comparison registers");
            if (entry.modifier)
              Require(original.r11.u64 == composed.r11.u64 && original.f13.u64 == composed.f13.u64,
                      "Production modifier post-write registers differ");
#define COMPOSE_CHECK(n) Require(original.r##n.u64 == composed.r##n.u64 && original.f##n.u64 == composed.f##n.u64, "Production composition nonvolatile state differs");
            COMPOSE_CHECK(14) COMPOSE_CHECK(15) COMPOSE_CHECK(16) COMPOSE_CHECK(17) COMPOSE_CHECK(18) COMPOSE_CHECK(19)
            COMPOSE_CHECK(20) COMPOSE_CHECK(21) COMPOSE_CHECK(22) COMPOSE_CHECK(23) COMPOSE_CHECK(24) COMPOSE_CHECK(25)
            COMPOSE_CHECK(26) COMPOSE_CHECK(27) COMPOSE_CHECK(28) COMPOSE_CHECK(29) COMPOSE_CHECK(30) COMPOSE_CHECK(31)
#undef COMPOSE_CHECK
            Require(original.cr0.raw() == composed.cr0.raw() && original.cr6.raw() == composed.cr6.raw() &&
                    original.xer.ca == composed.xer.ca && original.xer.ov == composed.xer.ov && original.xer.so == composed.xer.so &&
                    (original.fpscr.csr & controls) == (composed.fpscr.csr & controls) &&
                    (Platform::getcsr() & controls) == host_controls, "Production composition flags/FP controls differ");
            ++compositions;
          }
          ++comparisons;
        }
      }
    }
    Platform::setcsr(saved);
    Require(native_inner > 0, "Production composition never exercised an outer fallback with a native callee");
    std::cout << comparisons << " bounded value equivalence cases passed (" << native_count << " native paths); "
              << compositions << " production compositions (" << native_inner << " fallback/native-callee) passed\n";
  } catch (const std::exception& error) {
    n3_bounded_original_callee = false; Platform::setcsr(saved);
    std::cerr << "Bounded value 0x" << std::hex << address << std::dec << " sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n'; return 1;
  }
}
