#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <array>
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
    Platform::setcsr(saved);
    std::cout << comparisons << " vector equivalence cases, double scalars, aliases, FP controls and registrations passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved); std::cerr << error.what() << '\n'; return 1;
  }
}
