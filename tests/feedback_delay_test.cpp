#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

extern "C" void sub_8253BFA8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253BFA8(PPCContext&, uint8_t*);
extern "C" void sub_8253C310(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C310(PPCContext&, uint8_t*);
extern "C" void sub_8253C480(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C480(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  constexpr uint32_t stack = 60000;
  constexpr uint64_t marker = 0xfedc4321;
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, rounding = 0, history_size = 0;
  bool flush = false;
  try {
    using Function = void (*)(PPCContext&, uint8_t*);
    struct Entry { uint32_t address, history_size; Function original, replacement; };
    const std::array<Entry, 3> entries{{
        {0x8253BFA8, 128, __imp__sub_8253BFA8, sub_8253BFA8},
        {0x8253C310, 256, __imp__sub_8253C310, sub_8253C310},
        {0x8253C480, 512, __imp__sub_8253C480, sub_8253C480}}};
    std::mt19937 random(0x4644424b);
    uint32_t comparisons = 0;
    for (const auto& entry : entries) {
      history_size = entry.history_size;
      const auto object_size = 24 + history_size * 4;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == entry.address) registered = mapping->host == entry.replacement;
      Require(registered, "Feedback entry missing from indirect-call mapping");
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < 258; ++sample) {
          std::vector<uint8_t> expected(65536), actual;
          for (auto& byte : expected) byte = uint8_t(random());
          uint32_t object = 256 + sample % 4, buffer = 16384 + sample % 4;
          if (sample >= 128 && sample < 160) {
            if (sample % 4 == 0) buffer = object + 24;
            else if (sample % 4 == 1) buffer = object + 12;
            else if (sample % 4 == 2) buffer = object + object_size - 4;
            else object = buffer + 16;
          }
          if (sample >= 160 && sample < 192) {
            if (sample % 2) object = stack - 16 - object_size;
            else buffer = stack - 1028;
          }
          auto store = [&](uint32_t location, uint32_t value) {
            rex::memory::store_and_swap<uint32_t>(expected.data() + location, value);
          };
          auto value = [&]() -> uint32_t {
            if (sample >= 192) return sample < 224 || sample >= 256 ? 0x3f800000u : 0;
            if (sample < 8) return sample & 1 ? 0x80000000u : 0;
            return (random() & 0x807fffffu) | ((122 + random() % 9) << 23);
          };
          for (uint32_t index = 0; index < history_size; ++index) store(object + 24 + index * 4, value());
          for (uint32_t index = 0; index < 256; ++index) store(buffer + index * 4, value());
          constexpr std::array<uint32_t, 8> boundaries{0, 1, 2, 3, 127, 128, 255, 256};
          store(object, sample < 8 ? boundaries[sample] : random());
          store(object + 4, 0x3f000000); store(object + 8, 0x3e000000);
          store(object + 12, 0x40e00000); store(object + 16, 0);
          if (history_size == 512 && (sample < 96 || sample >= 192))
            store(object + 16, sample % 2 ? 256 : 0);
          if (sample >= 16 && sample < 32) {
            const std::array<uint32_t, 8> delays{0, 1, 2, 3, history_size - 1, history_size, history_size + 1, 0xffffffffu};
            store(object, delays[sample % delays.size()]);
          }
          if (sample >= 64 && sample < 96) {
            constexpr std::array<uint32_t, 6> exceptions{1, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u};
            store(sample % 3 == 0 ? object + 4 : sample % 3 == 1 ? buffer + 12 : object + 24, exceptions[sample % exceptions.size()]);
          }
          if (sample >= 96 && sample < 128) {
            const std::array<uint32_t, 4> cursors{1, 255, history_size - 1, history_size};
            store(object + 16, cursors[sample % cursors.size()]);
          }
          if (sample >= 192) {
            store(object, 1); store(object + 4, 0x3f800000); store(object + 12, 0);
            store(object + 8, sample < 224 ? 0x40000000 : sample < 256 ? 0x3f000000 : 0);
            for (uint32_t index = 0; index < history_size; ++index) store(object + 24 + index * 4, 0);
            if (sample < 224) {
              // Half the trials fail only after the 128-sample history wraps.
              if (sample & 2) for (uint32_t index = 0; index < 256; ++index) store(buffer + index * 4, 0);
              else if (sample & 1) for (uint32_t index = 0; index < 256; ++index) store(buffer + index * 4, 0xbf800000);
              store(buffer + (sample & 2 ? 160 : 32) * 4, sample & 1 ? 0xff7fffffu : 0x7f7fffffu);
            }
            else if (sample < 256) { store(buffer, sample & 1 ? 0xbf800000u : 0x3f800000u); store(buffer + 1024, 0x7f812345); }
          }
          actual = expected; const auto before = expected;
          PPCContext original{};
          original.r1.u32 = stack; original.r3.u32 = object; original.r4.u32 = buffer;
          original.r11.u64 = marker; original.lr = 0x82081234;
          original.f1.u64 = 0x40123456789abcde; original.f2.u64 = 0xc00123456789abcd;
  #define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
          SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
  #undef SEED
          original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
          original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
          if (flush) original.fpscr.enableFlushModeUnconditional();
          else original.fpscr.disableFlushModeUnconditional();
          if (sample >= 256) { original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); Platform::setcsr(original.fpscr.csr); }
          const auto initial = original.fpscr.csr;
          auto replacement = original;
          for (uint32_t block = 0; block < (sample >= 8 && sample < 16 ? 8u : 1u); ++block) {
            original.r4.u32 = replacement.r4.u32 = buffer;
            original.r11.u64 = replacement.r11.u64 = marker;
            original.fpscr.csr = replacement.fpscr.csr = initial;
            Platform::setcsr(initial);
            entry.original(original, expected.data());
            const auto host_controls = Platform::getcsr() & controls;
            Platform::setcsr(initial);
            entry.replacement(replacement, actual.data());
            // Directed overflow can saturate to a finite signed maximum. Those
            // blocks are supported; nonfinite trial results must fall back.
            const bool native_case = sample < 64 || (sample >= 224 && sample < 256) ||
                (sample >= 192 && sample < 224 &&
                 (rounding == 1 || (sample & 1 ? rounding == 2 : rounding == 3)));
            if (native_case) {
              Require(replacement.r11.u64 == marker && original.r11.u64 != marker, "Ordinary feedback did not use native path");
              Require(std::equal(actual.begin() + stack - 32, actual.begin() + stack, before.begin() + stack - 32), "Native feedback wrote guest stack scratch");
              std::copy_n(before.begin() + stack - 32, 32, expected.begin() + stack - 32);
            }
            Require(actual == expected, "Native/translated feedback bytes differ");
            Require(original.r1.u64 == replacement.r1.u64 && original.r3.u64 == replacement.r3.u64 &&
                    original.r4.u64 == replacement.r4.u64 && original.r5.u64 == replacement.r5.u64 &&
                    original.f1.u64 == replacement.f1.u64 && original.f2.u64 == replacement.f2.u64 &&
                    original.lr == replacement.lr, "Feedback calling convention differs");
  #define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Feedback nonvolatile state differs");
            CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
            CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
  #undef CHECK
            Require(original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                    original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov && original.xer.so == replacement.xer.so,
                    "Feedback condition/carry flags differ");
            Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                    (Platform::getcsr() & controls) == host_controls, "Feedback FP controls differ");
            ++comparisons;
          }
        }
      }
    }
    Platform::setcsr(saved);
    std::cout << comparisons << " feedback block equivalence cases, chained history, rollback and FP controls passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Feedback history " << history_size << " sample " << sample << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n';
    return 1;
  }
}
