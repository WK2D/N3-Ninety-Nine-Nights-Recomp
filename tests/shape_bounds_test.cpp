#include "native/shape_bounds.h"
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <array>
#include <bit>
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
    Platform::setcsr(saved);
    std::cout << comparisons << " native/translated shape comparisons, alias/edge cases, FP controls and registrations passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved); std::cerr << error.what() << '\n'; return 1;
  }
}
