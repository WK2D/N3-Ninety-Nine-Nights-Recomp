#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

extern "C" void sub_8209D358(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D358(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_state_payload_native_used = false;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, rounding = 0; bool flush = false;
  try {
    bool registered = false;
    for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
      if (mapping->guest == 0x8209d358) registered = mapping->host == sub_8209D358;
    Require(registered, "Payload copy missing from indirect-call mapping");
    constexpr std::array<uint32_t, 8> float_offsets{32, 36, 40, 48, 52, 84, 88, 92};
    constexpr std::array<uint32_t, 6> exceptions{1, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u};
    constexpr std::array<uint32_t, 8> ordinary{0, 0x80000000u, 0x00800000u, 0x80800000u,
        0x7f7fffffu, 0xff7fffffu, 0x3f800000u, 0xbf800000u};
    std::mt19937 random(0x5041594c);
    uint32_t comparisons = 0, native_count = 0;
    for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
      flush = next_flush;
      for (sample = 0; sample < 258; ++sample) {
        std::vector<uint8_t> expected(65536), actual;
        for (auto& byte : expected) byte = uint8_t(random());
        uint32_t source = 16384 + sample % 4, destination = 32768 + (sample / 4) % 4;
        if (sample >= 208 && sample < 240) {
          const auto shift = 1 + ((sample - 208) / 2) * 6;
          destination = sample % 2 ? source - shift : source + shift;
        }
        if (sample >= 240 && sample < 256) destination = sample % 2 ? source + 100 : source;
        auto store = [&](uint32_t offset, uint32_t bits) {
          rex::memory::store_and_swap<uint32_t>(expected.data() + source + offset, bits);
        };
        for (uint32_t i = 0; i < 8; ++i) {
          const auto bits = sample < 32 ? ordinary[(sample + i) % ordinary.size()]
              : (random() & 0x807fffffu) | ((1 + random() % 254) << 23);
          store(float_offsets[i], bits);
        }
        if (sample >= 160 && sample < 208) {
          const auto index = sample - 160;
          store(float_offsets[index / 6], exceptions[index % 6]);
        }
        const bool unmasked = sample >= 256;
        if (unmasked) for (auto offset : float_offsets) store(offset, 0x3f800000);
        actual = expected;
        PPCContext original{};
        original.r1.u64 = 0x123456780000f000ull;
        original.r3.u64 = 0x8765432100000000ull + destination;
        original.r4.u64 = 0xabcdef0100000000ull + source;
        original.lr = 0xfedcba9882081234ull; original.ctr.u64 = 0x1234567887654321ull;
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
        SEED(0) SEED(2) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9) SEED(10) SEED(11) SEED(12) SEED(13)
        SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
        SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
        original.f1.u64 = 0x40123456789abcde; original.f2.u64 = 0xc00123456789abcd;
        original.f3.u64 = 0x40023456789abcde; original.f4.u64 = 0xc01123456789abcd;
        original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
        original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
        original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
        if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
        if (unmasked) { original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); Platform::setcsr(original.fpscr.csr); }
        const auto initial = original.fpscr.csr; auto replacement = original;
        Platform::setcsr(initial); __imp__sub_8209D358(original, expected.data());
        const auto host_controls = Platform::getcsr() & controls;
        Platform::setcsr(initial); n3_state_payload_native_used = false;
        sub_8209D358(replacement, actual.data());
        const bool native_case = sample < 160 || (sample >= 240 && sample < 256 && sample % 2);
        Require(n3_state_payload_native_used == native_case, "Payload copy native/fallback route differs");
        if (native_case) ++native_count;
        Require(actual == expected, "Payload copy whole-fixture bytes differ");
#define GPR(n) Require(original.r##n.u64 == replacement.r##n.u64, "Payload copy GPR result differs");
        GPR(0) GPR(1) GPR(2) GPR(3) GPR(4) GPR(5) GPR(6) GPR(7) GPR(8) GPR(9) GPR(10) GPR(11)
        GPR(12) GPR(13) GPR(14) GPR(15) GPR(16) GPR(17) GPR(18) GPR(19) GPR(20) GPR(21) GPR(22)
        GPR(23) GPR(24) GPR(25) GPR(26) GPR(27) GPR(28) GPR(29) GPR(30) GPR(31)
#undef GPR
#define FPR(n) Require(original.f##n.u64 == replacement.f##n.u64, "Payload copy FPR result differs");
        FPR(0) FPR(1) FPR(2) FPR(3) FPR(4) FPR(5) FPR(6) FPR(7) FPR(8) FPR(9) FPR(10) FPR(11)
        FPR(12) FPR(13) FPR(14) FPR(15) FPR(16) FPR(17) FPR(18) FPR(19) FPR(20) FPR(21) FPR(22)
        FPR(23) FPR(24) FPR(25) FPR(26) FPR(27) FPR(28) FPR(29) FPR(30) FPR(31)
#undef FPR
        Require(original.lr == replacement.lr && original.ctr.u64 == replacement.ctr.u64 &&
                original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so,
                "Payload copy flags or return state differ");
        Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                (Platform::getcsr() & controls) == host_controls, "Payload copy FP controls differ");
        ++comparisons;
      }
    }
    Platform::setcsr(saved);
    std::cout << comparisons << " payload-copy equivalence cases (" << native_count << " native), all fields/registers and overlap fallback passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Payload copy sample " << sample << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n'; return 1;
  }
}
