#pragma once
#include "sparse_game_fixture.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace n3::test {
enum class GeometryKind { Bounds, Sample, Capsule, Sphere };
inline void CheckGeometryPhysical(PPCFunc* native, PPCFunc* original, GeometryKind kind,
    bool& native_used, uint32_t& comparisons, uint32_t& native_count) {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  const auto saved = Platform::getcsr();
  const bool capsule = kind == GeometryKind::Capsule, sphere = kind == GeometryKind::Sphere;
  const bool corner = kind == GeometryKind::Sample;
  const uint32_t scratch = capsule ? 0 : sphere ? 96 : corner ? 64 : 24;
  const uint32_t output_size = sphere ? 16 : corner ? 12 : 24;
  SparseGameFixture seed, expected, actual;
  seed.Fill();
  for (uint32_t rounding = 0; rounding < 4; ++rounding) for (bool flush : {false, true})
    for (uint32_t sample = 0; sample < 48; ++sample) {
      try {
        expected.Copy(seed);
        uint32_t source = 0xe1004000u + sample % 4, output = 0x8000u + sample % 4, stack = 0xf000;
        bool use_native = true;
        if (sample >= 4 && sample < 8) { source = 0x4000 + sample % 4; output = 0xe1010000u + sample % 4; }
        if (sample >= 8 && sample < 12) output = 0xe1010000u + sample % 4;
        if (sample >= 12 && sample < 16) { source = 0x4000 + sample % 4; stack = 0xe1020000; }
        if (sample >= 16) { output = 0xe1010000u + sample % 4; stack = 0xe1020000; }
        if (sample == 20) { source = 0xe0000000; output = 0xe0008000; stack = 0xe0010000; }
        if (sample == 21) { source = 0xffffff00; use_native = false; }
        if (sample == 22) { output = 0xfffffff8; use_native = false; }
        if (sample == 23) { source = 0xdfffff80; use_native = false; }
        if (sample == 24) { output = 0xdffffff8; use_native = false; }
        if (sample == 25) { stack = 0xe0000020; use_native = scratch <= 32; }
        if (sample == 26) { output = source; use_native = false; }
        if (sample == 27) { output = source + 292; use_native = false; }
        if (sample == 28) { source = stack - 320; use_native = !scratch; }
        if (sample == 29) { output = stack - 16; use_native = !scratch; }
        if (sample == 30) { source = 0xfffffec0; output = 0xfffff800; stack = 0xfffff000; }
        if (sample == 31) { source = 0xfffffed0; output = 0xfffff800; stack = 0xfffff000; }
        if (sample == 32) { output = 0xffffffe8; }
        if (sample == 33) { stack = 0; use_native = !scratch; }
        if (sample >= 34 && sample < 40) use_native = false;
        if (sample == 40 || sample == 42 || sample == 44 || sample == 45) use_native = false;
        if (sample == 46) { source = 0xfffffed1; use_native = false; }
        if (sample == 47) { output = source - output_size + 1; use_native = false; }
        for (uint32_t index = 0; index < 12; ++index)
          expected.Store<uint32_t>(source + 32 + index * 4, index < 9 ? (index % 4 == 0 ? 0x3f800000u : 0u) : 0x41200000u);
        expected.Store<uint32_t>(source + 288, 0x3f800000);
        expected.Store<uint32_t>(source + 292, 0x40400000);
        expected.Store<uint32_t>(source + 296, 0x40800000);
        expected.Store<uint32_t>(source + 300, 0);
        if (sample >= 34 && sample < 40) {
          constexpr uint32_t exceptional[]{1, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u};
          expected.Store<uint32_t>(source + 68, exceptional[sample - 34]);
        }
        if (sample == 40 || sample == 41) expected.Store<uint32_t>(source + 68, 0x00800000);
        actual.Copy(expected);
        std::array<uint8_t, 96> before_scratch{};
        if (use_native && scratch) std::copy_n(actual.base + PhysicalOffset(stack - scratch), scratch, before_scratch.begin());
        std::array<uint8_t, 24> unbiased{};
        if (output >= 0xe0000000u) std::copy_n(actual.base + output, output_size, unbiased.begin());
        PPCContext translated{};
        SeedPreserved(translated, sample);
        translated.lr |= 0x1234567800000000ull;
        translated.r1.u32 = stack; translated.r3.u32 = source;
        translated.r4.s64 = sample & 1 ? 1 : -1;
        translated.r5.s64 = sample & 2 ? 1 : -1;
        translated.r6.s64 = sample & 4 ? 1 : -1;
        translated.r7.u32 = output;
        if (!corner) translated.r4.u32 = output;
        translated.fpscr.InitHost(); translated.fpscr.storeFromGuest(rounding);
        if (flush) translated.fpscr.enableFlushModeUnconditional(); else translated.fpscr.disableFlushModeUnconditional();
        if (sample == 40) translated.fpscr.disableFlushModeUnconditional();
        if (sample == 41 || sample == 43) translated.fpscr.enableFlushModeUnconditional();
        if (sample == 42) translated.fpscr.disableFlushModeUnconditional();
        if (sample == 44 || sample == 45) {
          translated.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
          Platform::setcsr(translated.fpscr.csr);
        }
        const auto guest_initial = translated.fpscr.csr;
        auto host_initial = guest_initial;
        if (sample == 40) host_initial |= Platform::FlushMask;
        if (sample == 41) host_initial &= ~Platform::FlushMask;
        if (sample == 42 || sample == 43) host_initial &= ~(Platform::ExceptionMask | 0x3fu);
        auto replacement = translated;
        Platform::setcsr(host_initial); original(translated, expected.base);
        const auto host_expected = Platform::getcsr() & controls;
        Platform::setcsr(host_initial); native_used = false; native(replacement, actual.base);
        if (native_used != use_native) throw std::runtime_error("Physical geometry native/fallback route differs");
        if (use_native && scratch) {
          if (!std::equal(before_scratch.begin(), before_scratch.begin() + scratch, actual.base + PhysicalOffset(stack - scratch)))
            throw std::runtime_error("Native geometry changed dead scratch");
          std::copy_n(before_scratch.begin(), scratch, expected.base + PhysicalOffset(stack - scratch));
        }
        if (output >= 0xe0000000u && use_native &&
            (!std::equal(unbiased.begin(), unbiased.begin() + output_size, actual.base + output) ||
             !std::equal(unbiased.begin(), unbiased.begin() + output_size, expected.base + output)))
          throw std::runtime_error("Physical geometry changed unbiased output");
        if (!actual.Same(expected)) throw std::runtime_error("Physical geometry whole fixture differs");
        if (!SamePreserved(translated, replacement) || translated.r3.u64 != replacement.r3.u64 ||
            translated.r4.u64 != replacement.r4.u64 || translated.r5.u64 != replacement.r5.u64 ||
            translated.r6.u64 != replacement.r6.u64 || translated.r7.u64 != replacement.r7.u64 ||
            ((capsule || sphere) && translated.f1.u64 != replacement.f1.u64))
          throw std::runtime_error("Physical geometry ABI/result differs");
        if ((translated.fpscr.csr & controls) != (replacement.fpscr.csr & controls) ||
            (Platform::getcsr() & controls) != host_expected)
          throw std::runtime_error("Physical geometry FP controls differ");
        ++comparisons; if (use_native) ++native_count;
      } catch (const std::exception& error) {
        Platform::setcsr(saved);
        throw std::runtime_error(std::string(error.what()) + " kind " + std::to_string(int(kind)) +
            " sample " + std::to_string(sample) + " rounding " + std::to_string(rounding) + " flush " + std::to_string(flush));
      }
    }
  Platform::setcsr(saved);
}
}
