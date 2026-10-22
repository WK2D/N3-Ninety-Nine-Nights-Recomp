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

#define DECLARE_VECTOR(name) \
  extern "C" void name(PPCContext&, uint8_t*); \
  extern "C" void __imp__##name(PPCContext&, uint8_t*);
DECLARE_VECTOR(sub_8208ECF8)
DECLARE_VECTOR(sub_8209D858)
DECLARE_VECTOR(sub_8209D8A0)
DECLARE_VECTOR(sub_82336D90)
DECLARE_VECTOR(sub_82336DD0)
DECLARE_VECTOR(sub_82336E48)
DECLARE_VECTOR(sub_82336EB0)
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_vector_ops_native_used = false;
struct Entry { uint32_t address; PPCFunc* native; PPCFunc* original; bool setter; };
constexpr std::array<Entry, 7> entries{{
    {0x8208ECF8, sub_8208ECF8, __imp__sub_8208ECF8, false},
    {0x8209D858, sub_8209D858, __imp__sub_8209D858, false},
    {0x8209D8A0, sub_8209D8A0, __imp__sub_8209D8A0, false},
    {0x82336D90, sub_82336D90, __imp__sub_82336D90, true},
    {0x82336DD0, sub_82336DD0, __imp__sub_82336DD0, true},
    {0x82336E48, sub_82336E48, __imp__sub_82336E48, true},
    {0x82336EB0, sub_82336EB0, __imp__sub_82336EB0, true}}};
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
namespace physical {
struct Region { uint64_t at; uint32_t size; };
constexpr std::array<Region, 4> regions{{{0, 0x20000}, {0xdfff0000, 0x20000},
    {0xe1000000, 0x20000}, {0xffffb000, 0x15000}}};
uint64_t Offset(uint32_t at) { return uint64_t(at) + (at >= 0xe0000000u ? 0x1000u : 0u); }
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, (size_t{1} << 32) + 0x10000, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve physical vector fixture");
    for (auto region : regions) if (!VirtualAlloc(base + region.at, region.size, MEM_COMMIT, PAGE_READWRITE)) {
      VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit physical vector fixture");
    }
  }
  ~Guest() { VirtualFree(base, 0, MEM_RELEASE); }
  uint8_t* Ptr(uint32_t at) const { return base + Offset(at); }
  void Word(uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(Ptr(at), value); }
  void Scalar(uint32_t at, float value) { Word(at, std::bit_cast<uint32_t>(value)); }
  void Copy(const Guest& other) { for (auto region : regions) std::memcpy(base + region.at, other.base + region.at, region.size); }
  bool Same(const Guest& other) const {
    for (auto region : regions) if (std::memcmp(base + region.at, other.base + region.at, region.size)) return false;
    return true;
  }
};
void Context(const PPCContext& a, const PPCContext& b, bool native) {
#define CHECK(n) Require(a.r##n.u64 == b.r##n.u64, "Physical vector preserved GPR differs");
  CHECK(0) CHECK(1) CHECK(2) CHECK(3) CHECK(4) CHECK(5) CHECK(6) CHECK(7) CHECK(8) CHECK(9) CHECK(10)
  CHECK(12) CHECK(13) CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21)
  CHECK(22) CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
#undef CHECK
  if (!native) {
    Require(a.r11.u64 == b.r11.u64, "Retained physical vector address register differs");
#define FP(n) Require(a.f##n.u64 == b.f##n.u64, "Retained physical vector FP register differs");
    FP(0) FP(2) FP(3) FP(4) FP(5) FP(6) FP(7) FP(8) FP(9) FP(10) FP(11) FP(12) FP(13)
#undef FP
  }
#define FP(n) Require(a.f##n.u64 == b.f##n.u64, "Physical vector preserved FP register differs");
  FP(1) FP(14) FP(15) FP(16) FP(17) FP(18) FP(19) FP(20) FP(21) FP(22) FP(23)
  FP(24) FP(25) FP(26) FP(27) FP(28) FP(29) FP(30) FP(31)
#undef FP
#define CR(n) Require(a.cr##n.raw() == b.cr##n.raw(), "Physical vector condition field differs");
  CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
  Require(std::memcmp(reinterpret_cast<const uint8_t*>(&a) + offsetof(PPCContext, v0),
                      reinterpret_cast<const uint8_t*>(&b) + offsetof(PPCContext, v0), 128 * sizeof(PPCVRegister)) == 0 &&
          a.lr == b.lr && a.ctr.u64 == b.ctr.u64 && a.xer.ca == b.xer.ca && a.xer.ov == b.xer.ov && a.xer.so == b.xer.so &&
          a.msr == b.msr && a.vscr_sat == b.vscr_sat && a.last_indirect_target == b.last_indirect_target,
          "Physical vector preserved VMX/control state differs");
}
void Run() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  Guest seed, expected, actual;
  std::mt19937 random(0x56504859);
  for (auto region : regions) for (uint32_t i = 0; i < region.size; ++i) seed.base[region.at + i] = uint8_t(random());
  uint32_t comparisons = 0, native_count = 0, retained_count = 0, canaries = 0;
  for (auto entry : entries) for (uint32_t rounding = 0; rounding < 4; ++rounding) for (bool flush : {false, true})
    for (uint32_t sample = 0; sample < 32; ++sample) {
      expected.Copy(seed);
      const bool four = entry.address == 0x82336dd0;
      const auto input_size = four ? 16u : 12u;
      uint32_t object = sample < 4 || sample >= 8 ? 0xe1001000u + sample % 4 : 0x1000;
      uint32_t input = sample >= 4 ? 0xe1009000u + sample % 4 : 0x9000;
      bool fallback = false;
      if (sample == 12) object = 0xfffff000u;
      if (sample == 13) input = 0xffffffe0u;
      if (sample == 14) input = object + 60;
      if (sample == 15) input = object + 5;
      if (sample == 16) input = object + 71;
      if (sample == 17) { object = 0xdffffff0u; fallback = true; }
      if (sample == 18) input = 0xdffffff8u;
      if (sample == 19) input = 0xfffffffcu;
      if (sample == 20) { object = 0xffffffd0u; fallback = true; }
      if (sample == 21) input = object;
      if (sample == 22) input = object + 111;
      if (sample == 23) input = object - 4;
      if (sample == 24) object = 0xe0000000u;
      if (sample == 25) object = 0xdffffdc0u;
      if (sample == 26) input = 0xdffffff4u;
      if (sample >= 30) object = 0xfffff800u + sample % 4;
      const bool unmasked = sample == 28 || sample == 29;
      fallback |= unmasked || (!entry.setter && sample == 27);
      if (entry.setter) fallback |= (sample >= 14 && sample <= 16) || sample == 18 || sample == 19 ||
          (sample >= 21 && sample <= 23) || (sample == 26 && input_size == 16);
      for (uint32_t at : {32u, 36u, 40u, 48u, 52u, 56u, 64u, 68u, 72u, 564u, 568u, 572u})
        expected.Scalar(object + at, float(1 + (at / 4) % 4));
      if (entry.setter) for (uint32_t at = 0; at < 16; at += 4) expected.Scalar(input + at, float(1 + at / 4));
      std::array<uint8_t, 112> unbiased{};
      const bool canary = object >= 0xe0000000u && uint64_t(object) + (entry.setter ? 112 : 576) <= 0x100000000ull;
      const auto offset = entry.setter ? 0u : 564u, size = entry.setter ? 112u : 12u;
      if (canary) std::copy_n(expected.base + uint64_t(object) + offset, size, unbiased.begin());
      actual.Copy(expected);
      PPCContext original{};
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
      SEED(0) SEED(1) SEED(2) SEED(3) SEED(4) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9)
      SEED(10) SEED(11) SEED(12) SEED(13) SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19)
      SEED(20) SEED(21) SEED(22) SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
      original.r1.u64 = 0xe1018000u; original.r3.u64 = 0x1234567800000000ull | object;
      original.r4.u64 = 0x8765432100000000ull | input; original.f1.f64 = 0.5;
      if (sample == 27 && !entry.setter) original.f1.u64 = 0x7ff8123456789abcull;
      original.lr = 0x82081234; original.ctr.u64 = 0x12345678;
      auto* vector = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
      for (uint32_t i = 0; i < 128 * sizeof(PPCVRegister); ++i) vector[i] = uint8_t(random());
#define CR(n) original.cr##n.set_raw((sample + n) % 16);
      CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
      original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
      original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
      if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
      if (unmasked) original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
      const auto initial = original.fpscr.csr; auto replacement = original;
      Platform::setcsr(initial); entry.original(original, expected.base);
      const auto expected_controls = Platform::getcsr() & controls;
      Platform::setcsr(initial); n3_vector_ops_native_used = false; entry.native(replacement, actual.base);
      const bool native = n3_vector_ops_native_used;
      Require(native != fallback, "Physical vector routing did not preserve the supported/retained boundary");
      Require(expected.Same(actual), "Physical vector complete fixture differs"); Context(original, replacement, native);
      Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
              (Platform::getcsr() & controls) == expected_controls, "Physical vector FP controls differ");
      if (canary) {
        Require(std::equal(unbiased.begin(), unbiased.begin() + size, actual.base + uint64_t(object) + offset) &&
                std::equal(unbiased.begin(), unbiased.begin() + size, expected.base + uint64_t(object) + offset),
                "Physical vector changed unbiased storage"); ++canaries;
      }
      if (native) ++native_count; else ++retained_count; ++comparisons;
    }
  Require(native_count && retained_count && canaries, "Physical vector coverage was empty");
  std::cout << comparisons << " physical vector cases (" << native_count << " native, " << retained_count
            << " retained), including boundary/wrap, alias and unbiased canaries passed\n";
}
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  const auto saved = Platform::getcsr();
  try {
    std::mt19937 random(0x56454333);
    uint32_t comparisons = 0;
    for (const auto& entry : entries) {
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == entry.address) registered = mapping->host == entry.native;
      Require(registered, "Vector hook absent from indirect-call mapping");
      for (uint32_t rounding = 0; rounding < 4; ++rounding) {
        for (bool flush : {false, true}) {
          for (uint32_t sample = 0; sample < 256; ++sample) {
            std::array<uint8_t, 2048> expected{}, actual{};
            for (auto& byte : expected) byte = uint8_t(random());
            const uint32_t object = 256 + sample % 4;
            const uint32_t input = sample % 5 == 0 ? object + sample % 112 : 1024 + sample % 4;
            constexpr std::array<uint32_t, 16> edges{0, 0x80000000u, 0x3f800000u, 0xbf800000u,
                1, 0x80000001u, 0x00800000u, 0x007fffffu, 0x7f7fffffu, 0xff7fffffu,
                0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u, 0x3f800001u, 0x33800000u};
            auto bits = [&](uint32_t index) -> uint32_t {
              if (sample < 32) return edges[(sample + index) % edges.size()];
              auto value = random();
              if (sample % 2) value = (value & 0x807fffffu) | ((1 + random() % 254) << 23);
              return value;
            };
            for (uint32_t offset : {32u, 36u, 40u, 48u, 52u, 56u, 64u, 68u, 72u, 564u, 568u, 572u})
              rex::memory::store_and_swap<uint32_t>(expected.data() + object + offset, bits(offset / 4));
            if (entry.setter)
              for (uint32_t offset = 0; offset < 16; offset += 4)
                rex::memory::store_and_swap<uint32_t>(expected.data() + input + offset, bits(offset / 4));
            actual = expected;
            constexpr std::array<uint64_t, 16> scalars{0, 0x8000000000000000ull, 0x3ff0000000000000ull,
                0xbff0000000000000ull, 0x3fe0000000000000ull, 0x3ff0000000000001ull,
                1, 0x8000000000000001ull, 0x0010000000000000ull, 0x7fefffffffffffffull,
                0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8123456789abcull,
                0x7ff0123456789abcull, 0x3ff0000010000000ull, 0x3800000000000000ull};
            uint64_t amount = sample < 32 ? scalars[sample % scalars.size()] : (uint64_t(random()) << 32) | random();
            if (sample >= 32 && sample % 2)
              amount = (amount & 0x800fffffffffffffull) | (uint64_t(1 + random() % 2046) << 52);
            PPCContext original{};
            original.r1.u32 = 1792; original.r3.u32 = object; original.r4.u32 = input;
            original.f1.u64 = amount; original.lr = 0x82081234;
            original.r14.u64 = 0x123456789abcdef0; original.r31.u64 = 0xfedcba9876543210;
            original.f14.u64 = 0x40123456789abcde;
            original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
            if (flush) original.fpscr.enableFlushModeUnconditional();
            else original.fpscr.disableFlushModeUnconditional();
            const auto initial = original.fpscr.csr;
            auto replacement = original;
            entry.original(original, expected.data());
            const auto host_controls = Platform::getcsr() & PPCFPSCRRegister::GuestMask;
            Platform::setcsr(initial);
            entry.native(replacement, actual.data());
            if (actual != expected) {
              std::cerr << "Vector mismatch at 0x" << std::hex << entry.address << std::dec
                        << " sample " << sample << " rounding " << rounding << " flush " << flush << '\n';
              throw std::runtime_error("Native/translated vector bytes differ");
            }
            Require(replacement.r1.u64 == original.r1.u64 && replacement.r3.u64 == original.r3.u64 &&
                    replacement.r4.u64 == original.r4.u64 && replacement.r14.u64 == original.r14.u64 &&
                    replacement.r31.u64 == original.r31.u64 && replacement.f1.u64 == original.f1.u64 &&
                    replacement.f14.u64 == original.f14.u64 && replacement.lr == original.lr,
                    "Vector hook broke its calling convention");
            Require((replacement.fpscr.csr & PPCFPSCRRegister::GuestMask) ==
                    (original.fpscr.csr & PPCFPSCRRegister::GuestMask) &&
                    (Platform::getcsr() & PPCFPSCRRegister::GuestMask) == host_controls,
                    "Vector hook changed rounding/flush controls");
            ++comparisons;
          }
        }
      }
    }
    physical::Run();
    Platform::setcsr(saved);
    std::cout << comparisons << " vector equivalence cases, double scalars, aliases, FP controls and registrations passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved); std::cerr << error.what() << '\n'; return 1;
  }
}
