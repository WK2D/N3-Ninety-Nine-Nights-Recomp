#include <Windows.h>
#include "native/shape_bounds.h"
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <array>
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

#define DECLARE_BOUNDS(name) \
  extern "C" void name(PPCContext&, uint8_t*); \
  extern "C" void __imp__##name(PPCContext&, uint8_t*);
DECLARE_BOUNDS(sub_8235FB80)
DECLARE_BOUNDS(sub_8235FBE8)
DECLARE_BOUNDS(sub_82366970)
DECLARE_BOUNDS(sub_82360478)
DECLARE_BOUNDS(sub_82366918)
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_shape_bounds_native_used = false;

struct Entry { uint32_t address; PPCFunc* native; PPCFunc* translated; };
constexpr std::array<Entry, 5> entries{{
    {0x8235FB80, sub_8235FB80, __imp__sub_8235FB80},
    {0x8235FBE8, sub_8235FBE8, __imp__sub_8235FBE8},
    {0x82366970, sub_82366970, __imp__sub_82366970},
    {0x82360478, sub_82360478, __imp__sub_82360478},
    {0x82366918, sub_82366918, __imp__sub_82366918}}};
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
struct Region { uint64_t start; uint32_t size; };
constexpr std::array<Region, 4> physical_regions{{{0, 0x10000}, {0xdfff0000, 0x20000},
    {0xe1000000, 0x20000}, {0xffff0000, 0x20000}}};
// Independent fixture mapping, including the 33-bit translated host offset.
uint64_t HostOffset(uint32_t at) { return uint64_t(at) + (at >= 0xe0000000u ? 0x1000u : 0u); }
struct PhysicalGuest {
  uint8_t* base;
  PhysicalGuest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, (size_t{1} << 32) + 0x10000,
                                                          MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve physical shape fixture");
    for (auto region : physical_regions)
      if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit physical shape fixture");
      }
  }
  ~PhysicalGuest() { VirtualFree(base, 0, MEM_RELEASE); }
  PhysicalGuest(const PhysicalGuest&) = delete;
  void Copy(const PhysicalGuest& source) {
    for (auto region : physical_regions) std::memcpy(base + region.start, source.base + region.start, region.size);
  }
  bool Same(const PhysicalGuest& other) const {
    for (auto region : physical_regions)
      if (std::memcmp(base + region.start, other.base + region.start, region.size)) return false;
    return true;
  }
  void Word(uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(base + HostOffset(at), value); }
};
void CheckPhysicalContext(const PPCContext& original, const PPCContext& replacement, bool retained) {
#define GPR(n) Require(original.r##n.u64 == replacement.r##n.u64, "Physical shape GPR differs");
  GPR(0) GPR(1) GPR(2) GPR(3) GPR(4) GPR(5) GPR(6) GPR(7) GPR(8) GPR(9)
  GPR(10) GPR(11) GPR(12) GPR(13) GPR(14) GPR(15) GPR(16) GPR(17) GPR(18) GPR(19)
  GPR(20) GPR(21) GPR(22) GPR(23) GPR(24) GPR(25) GPR(26) GPR(27) GPR(28) GPR(29) GPR(30) GPR(31)
#undef GPR
#define FPR(n) Require(original.f##n.u64 == replacement.f##n.u64, "Physical shape preserved FPR differs");
  FPR(1) FPR(2) FPR(3) FPR(4) FPR(5) FPR(6) FPR(7)
  FPR(14) FPR(15) FPR(16) FPR(17) FPR(18) FPR(19) FPR(20) FPR(21) FPR(22)
  FPR(23) FPR(24) FPR(25) FPR(26) FPR(27) FPR(28) FPR(29) FPR(30) FPR(31)
  if (retained) { FPR(0) FPR(8) FPR(9) FPR(10) FPR(11) FPR(12) FPR(13) }
#undef FPR
#define CR(n) Require(original.cr##n.raw() == replacement.cr##n.raw(), "Physical shape condition field differs");
  CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
  Require(std::memcmp(reinterpret_cast<const uint8_t*>(&original) + offsetof(PPCContext, v0),
                      reinterpret_cast<const uint8_t*>(&replacement) + offsetof(PPCContext, v0),
                      128 * sizeof(PPCVRegister)) == 0 && original.lr == replacement.lr &&
          original.ctr.u64 == replacement.ctr.u64 && original.xer.ca == replacement.xer.ca &&
          original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so &&
          original.msr == replacement.msr && original.vscr_sat == replacement.vscr_sat &&
          original.last_indirect_target == replacement.last_indirect_target,
          "Physical shape VMX, flags or preserved control state differs");
}
uint32_t RunPhysicalShapes() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  constexpr std::array<uint32_t, 7> offsets{68, 72, 76, 288, 292, 296, 300};
  constexpr std::array<uint32_t, 6> exceptions{1, 0x80000001u, 0x7f800000u,
      0xff800000u, 0x7fc12345u, 0x7f812345u};
  const auto saved_host = Platform::getcsr();
  uint32_t sample = 0, rounding = 0, address = 0; bool flush = false;
  try {
    PhysicalGuest seed, expected, actual;
    std::mt19937 random(0x53485048);
    for (auto region : physical_regions)
      for (uint32_t i = 0; i < region.size; ++i) seed.base[region.start + i] = uint8_t(random());
    uint32_t comparisons = 0, native_count = 0, retained_count = 0, unbiased_checks = 0;
    uint32_t top_native = 0, boundary_retained = 0, overlapping_retained = 0, control_retained = 0;
    for (const auto& entry : entries) {
      address = entry.address;
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < 80; ++sample) {
          expected.Copy(seed);
          uint32_t object = 0xe1006000u, output = 0xe1001000u, stack = 0xe100a000u;
          bool must_native = true;
          const uint32_t output_size = entry.address == 0x82366918 ? 16 : 24;
          if (sample < 32) {
            if (sample < 4) { object += sample; output = 0x1000 + sample; stack = 0x8000; }
            else if (sample < 8) { object = 0x6000 + sample % 4; output += sample % 4; stack = 0x8000; }
            else if (sample < 16) { object += sample % 4; output += sample % 4; }
            else switch (sample) {
              case 16: object = 0xfffffed0u; break;
              case 17: output = 0xffffffe8u; break;
              case 18: output = 0xfffffff4u; must_native = false; break;
              case 19: object = 0xdffffed0u; break;
              case 20: object = 0xdffffed1u; must_native = false; break;
              case 21: output = 0xdffffffcu; must_native = false; break;
              case 22: object = 0xe0000000u; break;
              case 23: output = 0xe0000000u; break;
              case 24: output = object; must_native = false; break;
              case 25: output = object + 68; must_native = false; break;
              case 26: output = object + 288; must_native = false; break;
              case 27: output = object + 303; must_native = false; break;
              case 28: output = object - 12; must_native = false; break;
              case 29: output = object + 304; break;
              case 30: object = 0xfffffffcu; must_native = false; break;
              case 31: object += 1; output = object + 305; break;
            }
          }
          for (uint32_t i = 0; i < offsets.size(); ++i)
            expected.Word(object + offsets[i], std::bit_cast<uint32_t>(float(int32_t(i) - 3) * 1.25f));
          if (sample >= 32 && sample < 74) {
            const auto exceptional = sample - 32;
            const auto at = offsets[exceptional / exceptions.size()];
            expected.Word(object + at, exceptions[exceptional % exceptions.size()]);
            const bool center = at == 68 || at == 72 || at == 76;
            const bool uses_field = entry.address == 0x8235fb80 ? center || at == 288 :
                entry.address == 0x8235fbe8 ? at == 288 :
                entry.address == 0x82366970 ? at == 288 || at == 292 :
                entry.address == 0x82360478 ? at == 292 || at == 296 || at == 300 :
                center || at == 288 || at == 292;
            must_native = !uses_field;
          }
          if (sample >= 74) {
            for (auto at : offsets) expected.Word(object + at, sample == 76 ? 0x00800000u : 0);
            if (sample < 77) must_native = false;
          }
          std::array<uint8_t, 24> unbiased_output;
          const bool unbiased = output >= 0xe0000000u;
          if (unbiased) std::copy_n(expected.base + uint64_t(output), output_size, unbiased_output.begin());
          actual.Copy(expected);
          PPCContext oracle{};
#define SEED(n) oracle.r##n.u64 = 0x123456789abcde00ull + n; oracle.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(0) SEED(1) SEED(2) SEED(3) SEED(4) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9)
          SEED(10) SEED(11) SEED(12) SEED(13) SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19)
          SEED(20) SEED(21) SEED(22) SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
          auto* vector_bytes = reinterpret_cast<uint8_t*>(&oracle) + offsetof(PPCContext, v0);
          for (uint32_t i = 0; i < 128 * sizeof(PPCVRegister); ++i) vector_bytes[i] = uint8_t(random());
#define CR(n) oracle.cr##n.set_raw((sample + n) % 16);
          CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
          oracle.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          oracle.r1.u64 = 0x1234567800000000ull | stack;
          oracle.r3.u64 = 0x8765432100000000ull | object; oracle.r4.u64 = 0xabcdef0100000000ull | output;
          oracle.lr = 0xfedcba9882081234ull; oracle.ctr.u64 = 0x1234567887654321ull;
          oracle.vscr_sat = uint8_t(sample & 1); oracle.last_indirect_target = 0x81234567;
          oracle.fpscr.InitHost(); oracle.fpscr.storeFromGuest(rounding);
          if (flush) oracle.fpscr.enableFlushModeUnconditional(); else oracle.fpscr.disableFlushModeUnconditional();
          auto initial_host = oracle.fpscr.csr;
          if (sample == 74 || sample == 75) {
            oracle.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); initial_host = oracle.fpscr.csr;
          } else if (sample == 76) {
            oracle.fpscr.csr &= ~Platform::FlushMask; initial_host = oracle.fpscr.csr | Platform::FlushMask;
          } else if (sample == 77) {
            oracle.fpscr.csr |= Platform::FlushMask;
            initial_host = oracle.fpscr.csr & ~(Platform::FlushMask | Platform::ExceptionMask | 0x3fu);
          } else if (sample == 78) {
            oracle.fpscr.csr &= ~Platform::FlushMask;
            initial_host = (oracle.fpscr.csr & ~Platform::RoundMaskVal) | Platform::GuestToHost[(rounding + 1) % 4];
          } else if (sample == 79) {
            oracle.fpscr.csr &= ~(Platform::ExceptionMask | Platform::FlushMask | 0x3fu);
            initial_host = oracle.fpscr.csr | Platform::ExceptionMask;
          }
          auto replacement = oracle;
          Platform::setcsr(initial_host); entry.translated(oracle, expected.base);
          const auto original_host_controls = Platform::getcsr() & controls;
          Platform::setcsr(initial_host); n3_shape_bounds_native_used = false;
          entry.native(replacement, actual.base);
          const bool native = n3_shape_bounds_native_used;
          Require(native == must_native, "Physical shape native/fallback route differs");
          Require(expected.Same(actual), "Physical shape whole-fixture bytes differ");
          CheckPhysicalContext(oracle, replacement, !native);
          Require((oracle.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == original_host_controls, "Physical shape FP controls differ");
          if (unbiased) {
            Require(std::equal(actual.base + uint64_t(output), actual.base + uint64_t(output) + output_size, unbiased_output.begin()) &&
                    std::equal(expected.base + uint64_t(output), expected.base + uint64_t(output) + output_size, unbiased_output.begin()),
                    "Physical shape output changed its unbiased canary"); ++unbiased_checks;
          }
          if (native) { ++native_count; if (sample == 16 || sample == 17) ++top_native; }
          else {
            ++retained_count;
            if (sample == 18 || sample == 20 || sample == 21 || sample == 30) ++boundary_retained;
            if (sample >= 24 && sample <= 28) ++overlapping_retained;
            if (sample >= 74 && sample <= 76) ++control_retained;
          }
          ++comparisons;
        }
      }
    }
    Require(top_native == 80 && boundary_retained == 160 && overlapping_retained == 200 &&
            control_retained == 120 && unbiased_checks > 2000,
            "Physical shapes lack top-address, boundary, overlap or control coverage");
    Platform::setcsr(saved_host);
    std::cout << comparisons << " physical shape comparisons (" << native_count << " native/" << retained_count
              << " retained), mapped output canaries and full preserved state passed\n";
    return comparisons;
  } catch (const std::exception& error) {
    Platform::setcsr(saved_host);
    std::cerr << "Shape 0x" << std::hex << address << std::dec << " physical sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n';
    throw;
  }
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  const auto saved = Platform::getcsr();
  try {
    std::mt19937 random(0x4e334e33);
    constexpr uint32_t object = 256;
    uint32_t comparisons = 0;
    for (const auto& entry : entries) {
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == entry.address) registered = mapping->host == entry.native;
      Require(registered, "Indirect shape call does not resolve to native hook");
      for (uint32_t rounding = 0; rounding < 4; ++rounding) {
        for (bool flush : {false, true}) {
          for (uint32_t sample = 0; sample < 320; ++sample) {
            std::array<uint8_t, 2048> expected{}, actual{};
            for (auto& byte : expected) byte = uint8_t(random());
            // Non-overlapping output, unaligned output, and overlapping source
            // cases. Whole-buffer comparisons catch writes beyond the result.
            const auto output = sample % 5 == 0 ? object + (sample % 80) * 4 : 1024 + sample % 4;
            constexpr std::array<uint32_t, 18> edges{0, 0x80000000u, 0x3f800000u, 0xbf800000u,
                0x00800000u, 0x80800000u, 0x007fffffu, 1, 0x80000001u, 0x7f7fffffu,
                0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u,
                0xffc54321u, 0x3f800001u, 0x33800000u};
            for (uint32_t offset : {68u, 72u, 76u, 288u, 292u, 296u, 300u}) {
              // First cases cycle edge values; later cases cover random finite
              // normals as well as arbitrary float bit patterns.
              uint32_t bits = sample < 36 ? edges[(sample + offset / 4) % edges.size()] : random();
              if (sample >= 36 && sample % 2) bits = (bits & 0x807fffffu) | ((1 + random() % 254) << 23);
              rex::memory::store_and_swap<uint32_t>(expected.data() + object + offset, bits);
            }
            actual = expected;
            PPCContext oracle{};
            oracle.r1.u32 = 1792; oracle.r3.u32 = object; oracle.r4.u32 = output;
            oracle.lr = 0x82081234; oracle.r14.u64 = 0x123456789abcdef0;
            oracle.r31.u64 = 0xfedcba9876543210; oracle.f14.u64 = 0x40123456789abcde;
            oracle.fpscr.InitHost(); oracle.fpscr.storeFromGuest(rounding);
            if (flush) oracle.fpscr.enableFlushModeUnconditional();
            else oracle.fpscr.disableFlushModeUnconditional();
            const auto initial_csr = oracle.fpscr.csr;
            PPCContext replacement = oracle;
            entry.translated(oracle, expected.data());
            const auto original_host_controls = Platform::getcsr() & PPCFPSCRRegister::GuestMask;
            Platform::setcsr(initial_csr);
            entry.native(replacement, actual.data());
            if (actual != expected) {
              std::cerr << "Bounds mismatch at 0x" << std::hex << entry.address << std::dec
                        << " sample " << sample << " rounding " << rounding << " flush " << flush << '\n';
              throw std::runtime_error("Native/translated shape bytes differ");
            }
            Require(replacement.r1.u64 == oracle.r1.u64 && replacement.r3.u64 == oracle.r3.u64 &&
                    replacement.r4.u64 == oracle.r4.u64 && replacement.r14.u64 == oracle.r14.u64 &&
                    replacement.r31.u64 == oracle.r31.u64 && replacement.f14.u64 == oracle.f14.u64 &&
                    replacement.lr == oracle.lr, "Shape hook broke guest calling convention");
            Require((replacement.fpscr.csr & PPCFPSCRRegister::GuestMask) ==
                    (oracle.fpscr.csr & PPCFPSCRRegister::GuestMask) &&
                    (Platform::getcsr() & PPCFPSCRRegister::GuestMask) == original_host_controls,
                    "Shape hook changed rounding/flush behavior");
            ++comparisons;
          }
        }
      }
    }
    const auto physical_comparisons = RunPhysicalShapes();
    Platform::setcsr(saved);
    std::cout << comparisons << " native/translated shape comparisons, alias/edge cases, FP controls and registrations passed\n";
    Require(physical_comparisons == 3200, "Physical shape comparison count differs");
  } catch (const std::exception& error) {
    Platform::setcsr(saved); std::cerr << error.what() << '\n'; return 1;
  }
}
