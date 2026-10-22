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

extern "C" void sub_8209D8E8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D8E8(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_direction_accumulation_native_used = false;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 5> regions{{{0, 0x20000}, {0x8205b000, 4096},
    {0xdfff0000, 0x20000}, {0xe1000000, 0x20000}, {0xffffb000, 0x4000}}};
// Independent fixture implementation of the Windows translation's address rule.
uint64_t HostOffset(uint32_t address) { return uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u); }
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve direction fixture");
    for (auto region : regions)
      if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit direction fixture");
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
  void Word(uint32_t at, uint32_t bits) { rex::memory::store_and_swap<uint32_t>(base + HostOffset(at), bits); }
};
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  constexpr std::array<uint32_t, 6> exceptions{1, 0x80000001u, 0x7f800000u,
      0xff800000u, 0x7fc12345u, 0x7f812345u};
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, rounding = 0, divergence = 0; bool flush = false;
  try {
    bool registered = false;
    for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
      if (mapping->guest == 0x8209d8e8) registered = mapping->host == sub_8209D8E8;
    Require(registered, "Direction accumulation missing from indirect-call mapping");
    Guest seed, expected, actual;
    std::mt19937 random(0x44495241);
    for (auto region : regions)
      for (uint32_t i = 0; i < region.size; ++i) seed.base[region.start + i] = uint8_t(random());
    uint32_t comparisons = 0, native_count = 0, native_default = 0, native_override = 0;
    uint32_t physical_native = 0, physical_retained = 0;
    for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
      flush = next_flush;
      for (sample = 0; sample < 320; ++sample) {
        expected.Copy(seed);
        uint32_t object = 0x1000 + sample % 16, stack = 0x18000;
        double amount = sample < 32 ? (sample % 2 ? -0.0 : 0.0) : 0.5;
        bool must_native = true;
        if (sample >= 288) {
          const auto physical_case = sample - 288;
          object = 0xe1001000u + physical_case % 16;
          if (physical_case >= 16) stack = 0xe100a000u;
          if (physical_case == 24) { object = stack - 576; must_native = false; }
          if (physical_case == 25) { object = 0xdffffe00u; must_native = false; }
          if (physical_case == 26) stack = 0xe0000000u;
          if (physical_case == 27) stack = 0xe0000010u;
          if (physical_case == 28) { object = 0xffffb000u; stack = 0xffffd800u; }
          if (physical_case == 29) { object = 0xdffffdc0u; must_native = false; }
          if (physical_case == 30) object = 0xe0000000u;
          if (physical_case == 31) { object = 0xe100100fu; stack = (object + 580) & ~15u; must_native = false; }
        }
        auto vector = [&](uint32_t at, uint32_t x, uint32_t y, uint32_t z) {
          expected.Word(at, x); expected.Word(at + 4, y); expected.Word(at + 8, z);
        };
        vector(object + 64, 0x3f800000, 0xc0000000, 0x40400000);
        vector(object + 564, 0x40800000, 0xc0a00000, 0x40c00000);
        vector(object + 576, sample % 2 ? 0x40000000 : 0, 0, 0);
        expected.Word(object + 588, sample % 2 ? 0x7f812345u : 0xff800000u);
        expected.Word(0x8205b610, 0);
        if (sample >= 32 && sample < 80) {
          for (uint32_t lane = 0; lane < 3; ++lane) {
            const auto normal = [&] { return (random() & 0x807fffffu) | ((124 + random() % 7) << 23); };
            expected.Word(object + 64 + lane * 4, normal());
            expected.Word(object + 564 + lane * 4, normal());
            expected.Word(object + 576 + lane * 4, normal());
          }
        }
        if (sample >= 80 && sample < 104) {
          const auto index = sample - 80;
          vector(object + 576, 0, 0, 0);
          expected.Word(object + 64 + (index / 6 % 3) * 4, exceptions[index % 6]); must_native = false;
        }
        if (sample >= 104 && sample < 128) {
          const auto index = sample - 104;
          vector(object + 576, 0x3f800000, 0, 0);
          expected.Word(object + 564 + (index / 6 % 3) * 4, exceptions[index % 6]); must_native = false;
        }
        if (sample >= 128 && sample < 152) {
          const auto index = sample - 128;
          vector(object + 576, 0, 0, 0);
          expected.Word(object + 576 + (index / 6 % 3) * 4, exceptions[index % 6]);
          must_native = index % 6 < 2; // DAZ flushes subnormal dot inputs; zero dot selects the default.
        }
        if (sample >= 152 && sample < 176) {
          const auto index = sample - 152;
          expected.Word(0x8205b610, exceptions[index % 6]); must_native = false;
        }
        if (sample >= 176 && sample < 192) {
          vector(object + 576, 0x1e800000, 0, 0); must_native = false; // Squared product underflows to tiny output.
        }
        if (sample >= 192 && sample < 208) {
          vector(object + 576, 0x7f7fffff, 0, 0); amount = 0;
        }
        if (sample >= 208 && sample < 224) {
          vector(object + 576, 0x3f800000, 0, 0); vector(object + 564, 0, 0, 0); amount = 0x1p-149;
        }
        if (sample >= 224 && sample < 240) {
          vector(object + 576, 0x3f800000, 0, 0); amount = std::bit_cast<double>(0x7ff0000000000000ull); must_native = false;
        }
        if (sample >= 240 && sample < 256) { stack += 1 + sample % 15; must_native = false; }
        if (sample >= 256 && sample < 272) {
          const auto alias = sample % 4;
          if (alias == 0) stack = (object + 564 + 15) & ~15u;
          if (alias == 1) stack = (object + 576 + 15) & ~15u;
          if (alias == 2) stack = (object + 64 + 15) & ~15u;
          if (alias == 3) stack = 0x8205b620;
          must_native = false;
        }
        if (sample >= 272 && sample < 288) {
          // Exact arithmetic stays safe even when a policy deliberately unmasks
          // every exception. Each divergence case tests actual host/guest controls.
          vector(object + 64, 0x3f800000, 0, 0); vector(object + 564, 0, 0, 0);
          vector(object + 576, sample % 2 ? 0x3f800000 : 0, 0, 0); amount = 1;
        }
        std::array<uint8_t, 12> unbiased_current{};
        const bool physical_current = object + 564 >= 0xe0000000u;
        if (physical_current) std::copy_n(expected.base + object + 564, 12, unbiased_current.begin());
        actual.Copy(expected);
        for (divergence = 0; divergence < (sample >= 272 && sample < 288 ? 5u : 1u); ++divergence) {
          if (divergence) { expected.Copy(seed); actual.Copy(seed); }
          // Extra policies reuse identical, fully initialized input memory.
          if (divergence) {
            vector(object + 64, 0x3f800000, 0, 0); vector(object + 564, 0, 0, 0);
            vector(object + 576, sample % 2 ? 0x3f800000 : 0, 0, 0);
            expected.Word(object + 588, 0x7f812345); expected.Word(0x8205b610, 0); actual.Copy(expected);
          }
          PPCContext original{};
          original.r1.u64 = 0x1234567800000000ull + stack; original.r3.u64 = 0x2345678900000000ull + object;
          original.lr = 0xfedcba9882081234ull; original.ctr.u64 = 0x1234567887654321ull;
          original.vscr_sat = uint8_t(sample & 1); original.last_indirect_target = 0x81234567;
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(0) SEED(2) SEED(4) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9) SEED(10) SEED(11) SEED(12) SEED(13)
          SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
          SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
          original.f1.f64 = amount; original.f2.u64 = 0xc00123456789abcd;
          original.f3.u64 = 0x40023456789abcde;
          std::array<uint8_t, 128 * 16> vector_seed;
          for (auto& byte : vector_seed) byte = uint8_t(random());
          std::memcpy(&original.v0, vector_seed.data(), vector_seed.size());
#define SEED_CR(n) original.cr##n.set_raw((sample + n) % 16);
          SEED_CR(0) SEED_CR(1) SEED_CR(2) SEED_CR(3) SEED_CR(4) SEED_CR(5) SEED_CR(6) SEED_CR(7)
#undef SEED_CR
          original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
          if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
          uint32_t guest = original.fpscr.csr, host = guest;
          bool policy_native = must_native;
          if (divergence == 1) {
            host = (host & ~Platform::RoundMaskVal) | Platform::GuestToHost[(rounding + 1) % 4];
          } else if (divergence == 2) {
            guest |= Platform::FlushMask; host &= ~Platform::FlushMask; policy_native = false;
          } else if (divergence == 3) {
            guest |= Platform::FlushMask; host &= ~(Platform::ExceptionMask | 0x3fu); policy_native = false;
          } else if (divergence == 4) {
            guest &= ~(Platform::ExceptionMask | 0x3fu); host = guest | Platform::ExceptionMask; policy_native = false;
          }
          original.fpscr.csr = guest; auto replacement = original;
          Platform::setcsr(host); __imp__sub_8209D8E8(original, expected.base);
          const auto host_controls = Platform::getcsr() & controls;
          Platform::setcsr(host); n3_direction_accumulation_native_used = false;
          sub_8209D8E8(replacement, actual.base);
          Require(n3_direction_accumulation_native_used == policy_native, "Direction native/fallback route differs");
          if (policy_native) {
            ++native_count;
            if (sample >= 288) ++physical_native;
            if (original.cr6.gt || original.cr6.so) ++native_override; else ++native_default;
          } else if (sample >= 288) ++physical_retained;
          if (physical_current)
            Require(std::equal(actual.base + object + 564, actual.base + object + 576, unbiased_current.begin()) &&
                    std::equal(expected.base + object + 564, expected.base + object + 576, unbiased_current.begin()),
                    "Physical direction accumulation changed the unbiased current-vector canary");
          Require(expected.Same(actual), "Direction accumulation whole-fixture bytes differ");
#define GPR(n) Require(original.r##n.u64 == replacement.r##n.u64, "Direction accumulation GPR result differs");
          GPR(0) GPR(1) GPR(2) GPR(3) GPR(4) GPR(5) GPR(6) GPR(7) GPR(8) GPR(9) GPR(10) GPR(11)
          GPR(12) GPR(13) GPR(14) GPR(15) GPR(16) GPR(17) GPR(18) GPR(19) GPR(20) GPR(21) GPR(22)
          GPR(23) GPR(24) GPR(25) GPR(26) GPR(27) GPR(28) GPR(29) GPR(30) GPR(31)
#undef GPR
#define FPR(n) Require(original.f##n.u64 == replacement.f##n.u64, "Direction accumulation FPR result differs");
          FPR(0) FPR(1) FPR(2) FPR(3) FPR(4) FPR(5) FPR(6) FPR(7) FPR(8) FPR(9) FPR(10) FPR(11)
          FPR(12) FPR(13) FPR(14) FPR(15) FPR(16) FPR(17) FPR(18) FPR(19) FPR(20) FPR(21) FPR(22)
          FPR(23) FPR(24) FPR(25) FPR(26) FPR(27) FPR(28) FPR(29) FPR(30) FPR(31)
#undef FPR
          static_assert(offsetof(PPCContext, v127) + sizeof(PPCVRegister) - offsetof(PPCContext, v0) == 128 * sizeof(PPCVRegister));
          Require(std::memcmp(&original.v0, &replacement.v0, 128 * 16) == 0, "Direction accumulation VMX registers differ");
#define CHECK_CR(n) Require(original.cr##n.raw() == replacement.cr##n.raw(), "Direction accumulation condition field differs");
          CHECK_CR(0) CHECK_CR(1) CHECK_CR(2) CHECK_CR(3) CHECK_CR(4) CHECK_CR(5) CHECK_CR(6) CHECK_CR(7)
#undef CHECK_CR
          Require(original.lr == replacement.lr && original.ctr.u64 == replacement.ctr.u64 &&
                  original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so &&
                  original.msr == replacement.msr && original.vscr_sat == replacement.vscr_sat &&
                  original.last_indirect_target == replacement.last_indirect_target,
                  "Direction accumulation flags or return state differ");
          Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == host_controls, "Direction accumulation FP controls differ");
          ++comparisons;
        }
      }
    }
    Platform::setcsr(saved);
    Require(native_default && native_override, "Both direction choices must be exercised natively");
    Require(physical_native >= 200 && physical_retained == 32, "Physical direction mapping, shadow boundary or alias routes lacked coverage");
    std::cout << comparisons << " direction accumulation equivalence cases (" << native_count
              << " native), full scalar/VMX state, FP divergence and alias fallback passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Direction accumulation sample " << sample << " rounding " << rounding << " flush " << flush
              << " divergence " << divergence << ": " << error.what() << '\n'; return 1;
  }
}
