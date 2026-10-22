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

#define DECLARE_ENTRY(name) \
  extern "C" void name(PPCContext&, uint8_t*); \
  extern "C" void __imp__##name(PPCContext&, uint8_t*);
DECLARE_ENTRY(sub_824A0C50)
DECLARE_ENTRY(sub_824E7AD0)
DECLARE_ENTRY(sub_820801B0)
DECLARE_ENTRY(sub_8246D5F8)
DECLARE_ENTRY(sub_82324CD0)
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_engine_primitives_native_used = false;
enum class Kind { Add, Subtract, Countdown, Bits, Ratio };
struct Entry { uint32_t address; PPCFunc* native; PPCFunc* original; Kind kind; };
constexpr std::array<Entry, 5> entries{{
    {0x824A0C50, sub_824A0C50, __imp__sub_824A0C50, Kind::Add},
    {0x824E7AD0, sub_824E7AD0, __imp__sub_824E7AD0, Kind::Subtract},
    {0x820801B0, sub_820801B0, __imp__sub_820801B0, Kind::Countdown},
    {0x8246D5F8, sub_8246D5F8, __imp__sub_8246D5F8, Kind::Bits},
    {0x82324CD0, sub_82324CD0, __imp__sub_82324CD0, Kind::Ratio}}};
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
    Require(base != nullptr, "Cannot reserve physical primitive fixture");
    for (auto region : regions) if (!VirtualAlloc(base + region.at, region.size, MEM_COMMIT, PAGE_READWRITE)) {
      VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit physical primitive fixture");
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
void Context(const PPCContext& a, const PPCContext& b, Kind kind, bool native) {
#define GPR(n) Require(a.r##n.u64 == b.r##n.u64, "Physical primitive preserved GPR differs");
  GPR(0) GPR(1) GPR(2) GPR(3) GPR(4) GPR(12) GPR(13) GPR(14) GPR(15) GPR(16) GPR(17)
  GPR(18) GPR(19) GPR(20) GPR(21) GPR(22) GPR(23) GPR(24) GPR(25) GPR(26) GPR(27) GPR(28) GPR(29) GPR(30) GPR(31)
  if (kind != Kind::Countdown || !native) { GPR(5) }
  if (!native) { GPR(6) GPR(7) GPR(8) GPR(9) GPR(10) GPR(11) }
#undef GPR
#define FP(n) Require(a.f##n.u64 == b.f##n.u64, "Physical primitive FP register differs");
  FP(1) FP(2) FP(14) FP(15) FP(16) FP(17) FP(18) FP(19) FP(20) FP(21) FP(22) FP(23)
  FP(24) FP(25) FP(26) FP(27) FP(28) FP(29) FP(30) FP(31)
  if (!native || kind == Kind::Bits) { FP(0) FP(3) FP(4) FP(5) FP(6) FP(7) FP(8) FP(9) FP(10) FP(11) FP(12) FP(13) }
#undef FP
#define CR(n) Require(a.cr##n.raw() == b.cr##n.raw(), "Physical primitive condition field differs");
  CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
  Require(std::memcmp(reinterpret_cast<const uint8_t*>(&a) + offsetof(PPCContext, v0),
                      reinterpret_cast<const uint8_t*>(&b) + offsetof(PPCContext, v0), 128 * sizeof(PPCVRegister)) == 0 &&
          a.lr == b.lr && a.ctr.u64 == b.ctr.u64 && a.xer.ca == b.xer.ca && a.xer.ov == b.xer.ov && a.xer.so == b.xer.so &&
          a.msr == b.msr && a.vscr_sat == b.vscr_sat && a.last_indirect_target == b.last_indirect_target,
          "Physical primitive preserved vector/control state differs");
}
void Run() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  Guest seed, expected, actual, before;
  std::mt19937 random(0x50504859);
  for (auto region : regions) for (uint32_t i = 0; i < region.size; ++i) seed.base[region.at + i] = uint8_t(random());
  uint32_t comparisons = 0, native_count = 0, retained_count = 0, canaries = 0, scratch_aliases = 0;
  for (auto entry : entries) {
    if (entry.kind == Kind::Add || entry.kind == Kind::Subtract) continue;
    for (uint32_t rounding = 0; rounding < 4; ++rounding) for (bool flush : {false, true})
      for (uint32_t sample = 0; sample < 32; ++sample) {
        expected.Copy(seed);
        uint32_t object = sample < 4 || sample >= 8 ? 0xe1001000u + sample % 4 : 0x1000;
        uint32_t input = sample >= 4 ? 0xe1009000u + sample % 4 : 0x9000;
        uint32_t stack = 0xe1018000u, position = 1 + sample % 8, width = 8;
        uint32_t id = 7, frames = 3, total = 3;
        bool fallback = false, progress = false;
        const bool unmasked = sample >= 27 && sample <= 29;
        if (entry.kind == Kind::Countdown) {
          if ((sample >= 8 && sample < 12) || sample == 18 || sample == 28) id = 0xffffffffu;
          if ((sample >= 12 && sample < 16) || sample == 19 || sample == 29) frames = 1;
          if (sample == 12 || sample == 26) object = 0xfffff800u;
          if (sample == 13 || sample == 26) stack = 0xfffff000u;
          if (sample == 16) { object = 0xdffffff0u; fallback = true; }
          if (sample == 17) { object = 0xffffff90u; fallback = true; }
          if (sample == 18 || sample == 19) stack = 0;
          if (sample == 20) { stack = 0xe0000008u; fallback = true; }
          if (sample == 21) { stack = 8; fallback = true; }
          if (sample >= 22 && sample <= 25) { object = stack - (sample == 22 ? 224 : sample == 23 ? 232 : sample == 24 ? 220 : 240); fallback = true; }
          if (sample == 27) fallback = true;
          if (sample == 31) total = 0;
          expected.Word(object + 208, id); expected.Word(object + 224, frames);
          expected.Word(object + 220, total); expected.Word(object + 216, 0x7fc12345);
          progress = id != 0xffffffffu && frames > 1;
        } else if (entry.kind == Kind::Bits) {
          if (sample >= 8 && sample < 12) position += 64;
          if (sample == 12) object = 0xfffff800u;
          if (sample == 13) object = 0xffffffe0u;
          if (sample >= 16 && sample <= 19) { object = 0xdffffffcu + sample - 16; position = 1; width = 32; fallback = sample != 16; }
          if (sample == 20) { object = 0xe0000000u; position = 1; width = 32; }
          if (sample >= 21 && sample <= 24) { object = 0xfffffffcu + sample - 21; position = 1; width = 32; fallback = sample != 21; }
          if (sample == 25) { object = 0xfffffffcu; position = 33; fallback = true; }
          if (sample == 26) { object = 0xe0000001u; position = 0; fallback = true; }
          if (sample == 27) { width = 0; fallback = true; }
          if (sample == 28) { width = 33; fallback = true; }
          if (sample == 29) { position = 8; width = 32; fallback = true; }
          const uint32_t window = object + ((position - 1) >> 3);
          for (uint32_t i = 0; i < 4; ++i) *expected.Ptr(window + i) = uint8_t(0x19 + i * 53 + sample);
        } else {
          if (sample == 12) object = 0xfffff800u;
          if (sample == 13) input = 0xffffffe0u;
          if (sample == 16) { object = 0xdffffff0u; fallback = true; }
          if (sample == 17) { object = 0xffffffa0u; fallback = true; }
          if (sample == 18) { input = 0xdffffff8u; fallback = true; }
          if (sample == 19) { input = 0xfffffffcu; fallback = true; }
          if (sample == 20) input = object + 344;
          if (sample == 21) input = object + 345;
          if (sample == 22) input = object + 352;
          if (sample == 23) input = object - 4;
          if (sample >= 20 && sample <= 23) fallback = true;
          if (sample == 24) object = 0xe0000000u;
          if (sample == 25) object = 0xdffffe9cu;
          if (sample == 26) input = 0xdffffff4u;
          if (sample >= 27 && sample <= 29) fallback = true;
          if (sample >= 30) { object = 0xfffff800u + sample % 4; input = 0xffffffe0u + sample % 4; }
          for (uint32_t i = 0; i < 3; ++i) expected.Scalar(object + 344 + i * 4, 1);
          for (uint32_t i = 0; i < 3; ++i) expected.Scalar(input + i * 4, 1);
        }
        const uint32_t result_offset = entry.kind == Kind::Countdown ? 208 : 344;
        const uint32_t result_size = entry.kind == Kind::Countdown ? 20 : 12;
        std::array<uint8_t, 20> unbiased{};
        const bool canary = entry.kind != Kind::Bits && object >= 0xe0000000u && uint64_t(object) + result_offset + result_size <= 0x100000000ull;
        if (canary) std::copy_n(expected.base + uint64_t(object) + result_offset, result_size, unbiased.begin());
        before.Copy(expected); actual.Copy(expected);
        PPCContext original{};
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
        SEED(0) SEED(1) SEED(2) SEED(3) SEED(4) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9)
        SEED(10) SEED(11) SEED(12) SEED(13) SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19)
        SEED(20) SEED(21) SEED(22) SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
        original.r1.u64 = stack; original.r3.u64 = object; original.r4.u64 = position;
        original.r5.u64 = entry.kind == Kind::Bits ? width : input;
        original.f1.f64 = 2; original.f2.f64 = entry.kind == Kind::Ratio && sample == 27 ? 0 : 1;
        original.lr = 0x82081234; original.ctr.u64 = 0x12345678;
        auto* vectors = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
        for (uint32_t i = 0; i < 128 * sizeof(PPCVRegister); ++i) vectors[i] = uint8_t(random());
#define CR(n) original.cr##n.set_raw((sample + n) % 16);
        CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
        original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
        original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
        if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
        if (unmasked && !(entry.kind == Kind::Ratio && sample == 27)) original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
        const auto initial = original.fpscr.csr; auto replacement = original;
        Platform::setcsr(initial); entry.original(original, expected.base);
        const auto expected_controls = Platform::getcsr() & controls;
        Platform::setcsr(initial); n3_engine_primitives_native_used = false; entry.native(replacement, actual.base);
        const bool native = n3_engine_primitives_native_used;
        Require(native != fallback, "Physical primitive supported/retained routing differs");
        if (native && progress) {
          Require(std::equal(before.Ptr(stack - 16), before.Ptr(stack - 16) + 16, actual.Ptr(stack - 16)),
                  "Native physical countdown wrote dead scratch");
          std::copy_n(before.Ptr(stack - 16), 16, expected.Ptr(stack - 16));
        }
        Require(expected.Same(actual), "Physical primitive whole-fixture bytes differ"); Context(original, replacement, entry.kind, native);
        Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                (Platform::getcsr() & controls) == expected_controls, "Physical primitive FP controls differ");
        if (canary) {
          Require(std::equal(unbiased.begin(), unbiased.begin() + result_size, actual.base + uint64_t(object) + result_offset) &&
                  std::equal(unbiased.begin(), unbiased.begin() + result_size, expected.base + uint64_t(object) + result_offset),
                  "Physical primitive changed an unbiased output canary"); ++canaries;
        }
        if (entry.kind == Kind::Countdown && sample >= 22 && sample <= 25 && !native) ++scratch_aliases;
        if (native) ++native_count; else ++retained_count; ++comparisons;
      }
  }
  Require(native_count && retained_count && canaries && scratch_aliases == 32, "Physical primitive coverage was incomplete");
  std::cout << comparisons << " physical primitive cases (" << native_count << " native, " << retained_count
            << " retained), including countdown scratch aliases, bit windows and unbiased canaries passed\n";
}
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto control_mask = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  const auto saved = Platform::getcsr();
  uint32_t address = 0, sample = 0, rounding = 0;
  bool flush = false;
  try {
    std::mt19937 random(0x5052494d);
    uint32_t comparisons = 0;
    constexpr std::array<uint64_t, 20> doubles{0, 0x8000000000000000ull,
        0x3ff0000000000000ull, 0xbff0000000000000ull, 0x3ff0000010000000ull,
        0x3ff0000000000001ull, 1, 0x8000000000000001ull, 0x0010000000000000ull,
        0x7fefffffffffffffull, 0xffefffffffffffffull, 0x7ff0000000000000ull,
        0xfff0000000000000ull, 0x7ff8123456789abcull, 0x7ff0123456789abcull,
        0x36a0000000000000ull, 0xb6a0000000000000ull, 0x3800000000000000ull,
        0x47efffffe0000000ull, 0xc7efffffe0000000ull};
    constexpr std::array<uint32_t, 16> floats{0, 0x80000000u, 0x3f800000u, 0xbf800000u,
        1, 0x80000001u, 0x00800000u, 0x007fffffu, 0x7f7fffffu, 0xff7fffffu,
        0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u, 0x3f800001u, 0x33800000u};
    constexpr std::array<uint32_t, 12> counts{0, 1, 2, 3, 30, 60, 0xffffffffu, 0x80000000u,
        0x7fffffffu, 0x80000001u, 0x01000001u, 0x01000003u};
    for (const auto& entry : entries) {
      address = entry.address;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == entry.address) registered = mapping->host == entry.native;
      Require(registered, "Native primitive absent from indirect-call mapping");
      for (rounding = 0; rounding < 4; ++rounding) {
        for (bool next_flush : {false, true}) {
          flush = next_flush;
          for (sample = 0; sample < 514; ++sample) {
            std::array<uint8_t, 2048> expected{}, actual{};
            for (auto& byte : expected) byte = uint8_t(random());
            const uint32_t object = 256 + sample % 4;
            const uint32_t input = sample % 5 == 0 ? object + sample % 356 : 1024 + sample % 4;
            auto store = [&](uint32_t offset, uint32_t value) {
              rex::memory::store_and_swap<uint32_t>(expected.data() + offset, value);
            };
            PPCContext original{};
            original.r1.u32 = 1792; original.r3.u32 = object;
            original.r4.u32 = 1 + sample % 256; original.r5.u32 = input;
            auto scalar = [&](uint32_t index) -> uint64_t {
              if (sample < 400) return doubles[index % doubles.size()];
              return (uint64_t(random()) << 32) | random();
            };
            original.f1.u64 = scalar(sample / 20);
            original.f2.u64 = scalar(sample);
            if (entry.kind == Kind::Ratio) {
              for (uint32_t offset : {344u, 348u, 352u})
                store(object + offset, sample < 64 ? floats[(sample + offset / 4) % floats.size()] :
                      (random() & 0x807fffffu) | ((1 + random() % 254) << 23));
              for (uint32_t offset = 0; offset < 12; offset += 4)
                store(input + offset, sample < 64 ? floats[(sample + offset / 4) % floats.size()] :
                      (random() & 0x807fffffu) | ((1 + random() % 254) << 23));
            } else if (entry.kind == Kind::Countdown) {
              store(object + 208, sample % 7 == 0 ? 0xffffffffu : random());
              store(object + 224, sample < 144 ? counts[sample % counts.size()] : random());
              store(object + 220, sample < 144 ? counts[sample / counts.size()] : random());
            } else if (entry.kind == Kind::Bits) {
              // Covers every bit offset/width through 32, plus the original's
              // defined PPC shift behavior for widths 0, 33..63 and crossing windows.
              original.r4.u32 = 1 + (sample % 8) + (sample / 64) * 8;
              original.r5.u32 = sample / 8;
            }
            // Exact arithmetic permits exercising the conservative original
            // path with exceptions unmasked, without deliberately trapping.
            if (sample >= 512) {
              original.f1.f64 = 2; original.f2.f64 = 1;
              if (entry.kind == Kind::Countdown) {
                store(object + 208, 0); store(object + 224, 3); store(object + 220, 3);
              } else if (entry.kind == Kind::Ratio) {
                for (uint32_t offset : {344u, 348u, 352u}) store(object + offset, 0x3f800000);
                for (uint32_t offset = 0; offset < 12; offset += 4) store(input + offset, 0x3f800000);
              }
            }
            actual = expected;
            const auto before = expected;
            original.lr = 0x82081234;
            original.r14.u64 = 0x123456789abcdef0; original.r31.u64 = 0xfedcba9876543210;
            original.f14.u64 = 0x40123456789abcde;
            original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
            original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
            original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
            if (flush) original.fpscr.enableFlushModeUnconditional();
            else original.fpscr.disableFlushModeUnconditional();
            if (sample >= 512) {
#if defined(__x86_64__) || defined(_M_X64)
              original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
#else
              original.fpscr.csr |= Platform::ExceptionMask;
#endif
              Platform::setcsr(original.fpscr.csr);
            }
            const auto initial = original.fpscr.csr;
            auto replacement = original;
            entry.original(original, expected.data());
            const auto host_controls = Platform::getcsr() & control_mask;
            Platform::setcsr(initial);
            entry.native(replacement, actual.data());
            // The original scalar/countdown bodies use the caller's 16-byte
            // volatile stack scratch. Native models need none. All other bytes,
            // including object padding and inactive state, must match exactly.
            if (sample < 512 && (entry.kind == Kind::Add || entry.kind == Kind::Subtract || entry.kind == Kind::Countdown)) {
              std::copy_n(before.begin() + 1776, 16, expected.begin() + 1776);
              std::copy_n(before.begin() + 1776, 16, actual.begin() + 1776);
            }
            Require(actual == expected, "Native/translated primitive bytes differ");
            Require(replacement.r1.u64 == original.r1.u64 && replacement.r3.u64 == original.r3.u64 &&
                    replacement.r4.u64 == original.r4.u64 && replacement.r14.u64 == original.r14.u64 &&
                    replacement.r31.u64 == original.r31.u64 && replacement.f1.u64 == original.f1.u64 &&
                    replacement.f2.u64 == original.f2.u64 && replacement.f14.u64 == original.f14.u64 &&
                    replacement.lr == original.lr, "Primitive broke calling convention or return value");
            Require(replacement.cr0.raw() == original.cr0.raw() && replacement.cr6.raw() == original.cr6.raw() &&
                    replacement.xer.ca == original.xer.ca && replacement.xer.so == original.xer.so &&
                    replacement.xer.ov == original.xer.ov, "Primitive changed condition/carry flags");
            Require((replacement.fpscr.csr & control_mask) == (original.fpscr.csr & control_mask) &&
                    (Platform::getcsr() & control_mask) == host_controls,
                    "Primitive changed rounding, flush or exception controls");
            ++comparisons;
          }
        }
      }
    }
    physical::Run();
    Platform::setcsr(saved);
    std::cout << comparisons << " primitive equivalence cases, all rounding/flush modes, flags and registrations passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Primitive 0x" << std::hex << address << std::dec << " sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n';
    return 1;
  }
}
