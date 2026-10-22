#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
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
    Platform::setcsr(saved);
    std::cout << comparisons << " primitive equivalence cases, all rounding/flush modes, flags and registrations passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Primitive 0x" << std::hex << address << std::dec << " sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n';
    return 1;
  }
}
