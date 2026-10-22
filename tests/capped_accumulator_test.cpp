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

extern "C" void sub_8209D7A8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D7A8(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_capped_accumulator_native_used = false;
void Require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 7> regions{{{0, 0x20000}, {0x82042000, 4096},
    {0x8205b000, 4096}, {0x82b5f000, 8192}, {0xdfff0000, 0x30000},
    {0xe1000000, 0x50000}, {0xffff0000, 0x10000}}};
// Independent fixture mapping; do not use the adapter helper as the oracle.
uint8_t* HostAt(uint8_t* base, uint32_t address) {
  return base + uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u);
}
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
  void Word(uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(HostAt(base, at), value); }
  void Byte(uint32_t at, uint8_t value) { *HostAt(base, at) = value; }
};
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, rounding = 0; bool flush = false;
  try {
    bool registered = false;
    for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
      if (mapping->guest == 0x8209d7a8) registered = mapping->host == sub_8209D7A8;
    Require(registered, "Capped accumulator missing from indirect-call mapping");
    Guest seed, expected, actual;
    std::mt19937 random(0x43415041);
    std::mt19937 physical_random(0x50484341);
    for (size_t region_index = 0; region_index < regions.size(); ++region_index) {
      const auto region = regions[region_index];
      for (uint32_t i = 0; i < region.size; ++i)
        seed.base[region.start + i] = uint8_t(region_index < 4 ? random() : physical_random());
    }
    constexpr std::array<uint32_t, 8> integers{0, 1, 0xffffffffu, 0x80000000u,
        0x7fffffffu, 0x01000001u, 999999, 1000000};
    constexpr std::array<uint32_t, 6> exceptions{1, 0x80000001u, 0x7f800000u,
        0xff800000u, 0x7fc12345u, 0x7f812345u};
    uint32_t comparisons = 0, native_count = 0, physical_native = 0, physical_retained = 0, physical_canaries = 0;
    for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
      flush = next_flush;
      for (sample = 0; sample < 290; ++sample) {
        expected.Copy(seed);
        uint32_t object = 0x1000 + sample % 4, owner = 0x6000 + sample % 4;
        uint32_t root = 0x4000 + sample % 4, stack = 0x18000;
        uint32_t delta = sample < 16 ? integers[sample % integers.size()] : sample >= 258 ? 1 : uint32_t(int32_t(random() % 200001) - 100000);
        bool must_native = true;
        expected.Word(object + 680, integers[(sample + 3) % integers.size()]);
        expected.Word(object + 760, sample < 16 || sample >= 258 ? 0 : uint32_t(int32_t(random() % 401) - 200));
        expected.Word(object + 824, sample % 3 ? owner : 0);
        expected.Byte(owner + 696, sample % 3 == 1 ? 0 : sample % 2 ? 255 : 1);
        expected.Word(0x82042048, 0x3c23d70a); expected.Word(0x8205b614, 0x3f800000);
        expected.Word(0x82b60080, root);
        expected.Word(root + 4708, integers[(sample + 1) % integers.size()]);
        expected.Word(root + 4712, integers[(sample + 2) % integers.size()]);
        if (sample >= 96 && sample < 128) {
          const auto index = sample - 96;
          expected.Word(index / 6 % 2 ? 0x8205b614 : 0x82042048, exceptions[index % 6]);
          must_native = false;
        }
        if (sample >= 128 && sample < 160) {
          expected.Word(object + 760, integers[sample % integers.size()]);
          expected.Word(0x82042048, 0); delta = integers[(sample + 1) % integers.size()];
        }
        if (sample >= 160 && sample < 224) {
          const auto alias = (sample - 160) % 8;
          object = 0x1000; owner = 0x6000; root = 0x4000; delta = 1;
          if (alias == 0) root = object + 680 - 4708;
          if (alias == 1) root = object + 680 - 4712;
          if (alias == 2) owner = object + 680 - 696;
          if (alias == 3) object = 0x82b60080 - 680;
          if (alias == 4) stack = 0x82042058;
          if (alias == 5) stack = object + 696;
          if (alias == 6) stack = 0x82b60090;
          if (alias == 7) stack = object + 832;
          expected.Word(object + 680, 100); expected.Word(object + 760, 0);
          expected.Word(object + 824, owner); expected.Byte(owner + 696, 1);
          expected.Word(0x82042048, 0); expected.Word(0x8205b614, 0x3f800000);
          expected.Word(0x82b60080, root);
          expected.Word(root + 4708, 17); expected.Word(root + 4712, 23);
          if (alias == 2) { expected.Word(object + 680, 0); delta = 0xffffffffu; }
          if (alias == 3) { expected.Word(0x82b60080, 0xdead0000); delta = 0x21534000; }
          if (alias == 6) {
            expected.Word(0x82b60080, 0xdead0000);
            expected.Word(4708, 17); expected.Word(4712, 23);
          }
          if (alias == 7) expected.Word(object + 824, 0xdead0000);
          must_native = false;
        }
        if (sample >= 224 && sample < 240) {
          const auto alias = (sample - 224) % 4;
          object = 0x1000; owner = 0x6000; root = 0x4000; delta = 10;
          if (alias == 0) root = 0x82042048 - 4708;
          if (alias == 1) root = owner + 696 - 4708;
          if (alias == 2) root = object + 824 - 4708;
          if (alias == 3) root = 0xffffedd0u;
          expected.Word(object + 680, 100); expected.Word(object + 760, 25);
          expected.Word(object + 824, owner); expected.Byte(owner + 696, 1);
          expected.Word(0x82042048, 0x3c23d70a); expected.Word(0x8205b614, 0x3f800000);
          expected.Word(0x82b60080, root);
          expected.Word(root + 4708, alias == 0 ? 0x3c23d70au : alias == 1 ? 0x01000000u : alias == 2 ? owner : 17);
          expected.Word(root + 4712, 23);
          must_native = alias != 3;
        }
        if (sample >= 240 && sample < 248) {
          expected.Word(object + 760, 0); expected.Word(0x82042048, 0);
          expected.Word(0x8205b614, 0x40000000); delta = 0x80000000u; must_native = false;
        }
        if (sample >= 248 && sample < 256) {
          expected.Word(object + 760, 0x7fffffffu); expected.Word(0x82042048, 0x7f7fffffu);
          expected.Word(0x8205b614, 0x3f800000); delta = 0x7fffffffu;
          must_native = rounding == 1 || rounding == 3;
        }
        if (sample >= 258) {
          const auto physical = sample - 258;
          object = 0x1000; owner = 0x6000; root = 0x4000; stack = 0x18000;
          if (physical == 0) object = 0xe1001000;
          if (physical == 1) owner = 0xe1010000;
          if (physical == 2) root = 0xe1020000;
          if (physical == 3) stack = 0xe1038000;
          if (physical >= 4) { object = 0xe1001000; owner = 0xe1010000; root = 0xe1020000; stack = 0xe1038000; }
          switch (physical) {
            case 5: owner = 0; break;
            case 8: object += 1; break;
            case 9: owner += 2; root += 3; break;
            case 10: object = 0xffffd000; break;
            case 11: owner = 0xffffd000; break;
            case 12: root = 0xffffd000; break;
            case 13: stack = 0xffffe000; break;
            case 14: owner = object + 680 - 696; delta = 0xffffffffu; break;
            case 15: root = object + 680 - 4708; break;
            case 16: stack = object + 696; break;
            case 17: stack = owner + 704; break;
            case 18: stack = root + 4724; break;
            case 19: stack = 0x82b60090; break;
            case 20: object = 0xe0000000u - 700; break;
            case 21: owner = 0xe0000000u - 696; break;
            case 22: root = 0xe0000000u - 4710; break;
            case 23: stack = 0xe0000008; break;
            case 24: stack = 0xe0000000; break;
            case 25: stack = 0xe0000010; break;
            case 26: object = 0xe0000100; break;
            case 27: root = 0xffffedd0; break;
            case 29: root = owner + 696 - 4708; break;
            default: break;
          }
          expected.Word(object + 680, physical == 14 ? 0 : 100);
          expected.Word(object + 760, 0); expected.Word(object + 824, owner);
          if (owner) expected.Byte(owner + 696, physical == 6 ? 0 : 1);
          expected.Word(0x82042048, 0); expected.Word(0x8205b614, 0x3f800000);
          expected.Word(0x82b60080, root);
          expected.Word(root + 4708, physical == 29 ? 0x01000000u : physical == 7 ? 0xffffffffu : 17);
          expected.Word(root + 4712, physical == 7 ? 0xffffffffu : 23);
          if (physical == 14) expected.Word(object + 680, 0);
          if (physical == 19) {
            expected.Word(0x82b60080, 0xdead0000); expected.Word(4708, 17); expected.Word(4712, 23);
          }
          if (physical == 28) expected.Word(0x82042048, 0x7fc12345u);
          must_native = !(physical >= 14 && physical <= 23) && physical != 27 && physical != 28 && physical < 30;
        }
        const bool unmasked = (sample >= 256 && sample < 258) || sample >= 288;
        if (unmasked) {
          expected.Word(object + 680, 1); expected.Word(object + 760, 0);
          expected.Word(0x82042048, 0); expected.Word(0x8205b614, 0x3f800000);
          delta = 1; must_native = false;
        }
        actual.Copy(expected);
        std::array<uint8_t, 16> before_stack;
        std::copy_n(HostAt(expected.base, stack - 16), 16, before_stack.begin());
        std::array<uint8_t, 4> before_unbiased_value;
        std::copy_n(expected.base + object + 680, 4, before_unbiased_value.begin());
        const bool unbiased_value_canary = sample >= 258 && object >= 0xe0000000u;
        std::array<uint8_t, 8> before_unbiased_counters;
        const uint32_t counters = root + 4708;
        const bool unbiased_counter_canary = sample >= 258 && counters >= 0xe0000000u;
        if (unbiased_counter_canary)
          std::copy_n(expected.base + counters, 8, before_unbiased_counters.begin());
        PPCContext original{};
        original.r1.u32 = stack; original.r3.u32 = object; original.r4.u32 = delta;
        original.r5.u64 = 0x123456789abcdef0ull; original.lr = 0x1234567882081234ull;
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
        SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
        SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
        original.f1.u64 = 0x40123456789abcde; original.f2.u64 = 0xc00123456789abcd;
        original.f3.u64 = 0x40023456789abcde; original.f4.u64 = 0xc01123456789abcd;
        original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
#define SEED_CR(n) original.cr##n.set_raw((sample + n) % 16);
        SEED_CR(1) SEED_CR(2) SEED_CR(3) SEED_CR(4) SEED_CR(5) SEED_CR(7)
#undef SEED_CR
        auto* vector_bytes = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
        for (uint32_t i = 0; i < 128 * 16; ++i) vector_bytes[i] = uint8_t(sample * 7 + i * 13);
        original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
        original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
        if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
        if (unmasked) { original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); Platform::setcsr(original.fpscr.csr); }
        const auto initial = original.fpscr.csr; auto replacement = original;
        Platform::setcsr(initial); __imp__sub_8209D7A8(original, expected.base);
        const auto host_controls = Platform::getcsr() & controls;
        Platform::setcsr(initial); n3_capped_accumulator_native_used = false;
        sub_8209D7A8(replacement, actual.base);
        const bool native = n3_capped_accumulator_native_used;
        Require(native == must_native, "Accumulator native/fallback route differs from fixture contract");
        if (native) {
          Require(!std::equal(HostAt(expected.base, stack - 16), HostAt(expected.base, stack - 16) + 16, before_stack.begin()),
                  "Original scratch must differ to establish native execution");
          std::copy_n(before_stack.begin(), 16, HostAt(expected.base, stack - 16)); ++native_count;
        }
        if (sample >= 258) { if (native) ++physical_native; else ++physical_retained; }
        if (unbiased_value_canary) {
          Require(std::equal(actual.base + object + 680, actual.base + object + 684, before_unbiased_value.begin()),
                  "Accumulator wrote the unbiased physical value canary");
          ++physical_canaries;
        }
        if (unbiased_counter_canary) {
          Require(std::equal(actual.base + counters, actual.base + counters + 8, before_unbiased_counters.begin()),
                  "Accumulator wrote the unbiased physical counter canaries");
          ++physical_canaries;
        }
        Require(expected.Same(actual), "Accumulator whole-fixture bytes differ");
#define CHECK_GPR(n) Require(original.r##n.u64 == replacement.r##n.u64, "Accumulator GPR result differs");
        CHECK_GPR(0) CHECK_GPR(1) CHECK_GPR(2) CHECK_GPR(3) CHECK_GPR(4) CHECK_GPR(5) CHECK_GPR(6)
        CHECK_GPR(7) CHECK_GPR(8) CHECK_GPR(9) CHECK_GPR(10) CHECK_GPR(11) CHECK_GPR(12) CHECK_GPR(13)
        CHECK_GPR(14) CHECK_GPR(15) CHECK_GPR(16) CHECK_GPR(17) CHECK_GPR(18) CHECK_GPR(19)
        CHECK_GPR(20) CHECK_GPR(21) CHECK_GPR(22) CHECK_GPR(23) CHECK_GPR(24) CHECK_GPR(25)
        CHECK_GPR(26) CHECK_GPR(27) CHECK_GPR(28) CHECK_GPR(29) CHECK_GPR(30) CHECK_GPR(31)
#undef CHECK_GPR
#define CHECK_FPR(n) Require(original.f##n.u64 == replacement.f##n.u64, "Accumulator FPR result differs");
        CHECK_FPR(0) CHECK_FPR(1) CHECK_FPR(2) CHECK_FPR(3) CHECK_FPR(4) CHECK_FPR(5) CHECK_FPR(6)
        CHECK_FPR(7) CHECK_FPR(8) CHECK_FPR(9) CHECK_FPR(10) CHECK_FPR(11) CHECK_FPR(12) CHECK_FPR(13)
        CHECK_FPR(14) CHECK_FPR(15) CHECK_FPR(16) CHECK_FPR(17) CHECK_FPR(18) CHECK_FPR(19)
        CHECK_FPR(20) CHECK_FPR(21) CHECK_FPR(22) CHECK_FPR(23) CHECK_FPR(24) CHECK_FPR(25)
        CHECK_FPR(26) CHECK_FPR(27) CHECK_FPR(28) CHECK_FPR(29) CHECK_FPR(30) CHECK_FPR(31)
#undef CHECK_FPR
        static_assert(offsetof(PPCContext, v127) + sizeof(PPCVRegister) - offsetof(PPCContext, v0) == 128 * sizeof(PPCVRegister));
        Require(std::memcmp(&original.v0, &replacement.v0, 128 * sizeof(PPCVRegister)) == 0,
                "Accumulator changed preserved VMX registers");
#define CHECK_CR(n) Require(original.cr##n.raw() == replacement.cr##n.raw(), "Accumulator condition field differs");
        CHECK_CR(1) CHECK_CR(2) CHECK_CR(3) CHECK_CR(4) CHECK_CR(5) CHECK_CR(7)
#undef CHECK_CR
        Require(original.lr == replacement.lr && original.ctr.u64 == replacement.ctr.u64 &&
                original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so,
                "Accumulator condition/carry or return state differs");
        Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                (Platform::getcsr() & controls) == host_controls, "Accumulator FP controls differ");
        ++comparisons;
      }
    }
    Platform::setcsr(saved);
    Require(physical_native == 144 && physical_retained == 112 && physical_canaries > 0,
            "Physical accumulator route or unbiased-canary coverage differs");
    std::cout << comparisons << " capped accumulator equivalence cases (" << native_count << " native), "
              << physical_native << " native/" << physical_retained << " retained physical routes, aliases and accounting carry passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Capped accumulator sample " << sample << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n'; return 1;
  }
}
