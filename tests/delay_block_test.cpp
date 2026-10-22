#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

extern "C" void sub_8253BED8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253BED8(PPCContext&, uint8_t*);
extern "C" void sub_8253C240(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C240(PPCContext&, uint8_t*);
extern "C" void sub_8253C758(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C758(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
struct Entry { uint32_t address, size; PPCFunc* native; PPCFunc* original; bool dual = false; };
constexpr std::array<Entry, 3> entries{{
    {0x8253BED8, 512, sub_8253BED8, __imp__sub_8253BED8},
    {0x8253C240, 2048, sub_8253C240, __imp__sub_8253C240},
    {0x8253C758, 1024, sub_8253C758, __imp__sub_8253C758, true}}};
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  constexpr uint32_t stack = 60000;
  constexpr uint64_t marker = 0xfedc4321;
  const auto saved = Platform::getcsr();
  uint32_t address = 0, sample = 0, rounding = 0;
  bool flush = false;
  try {
    std::mt19937 random(0x44454c59);
    uint32_t comparisons = 0;
    for (const auto& entry : entries) {
      address = entry.address;
      const uint32_t history_offset = entry.dual ? 32 : 44, second_offset = entry.dual ? 20 : 32;
      const uint32_t cursor_offset = entry.dual ? 24 : 36, scratch = entry.dual ? 32 : 48;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == address) registered = mapping->host == entry.native;
      Require(registered, "Delay entry missing from indirect-call mapping");
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < 258; ++sample) {
          std::vector<uint8_t> expected(65536), actual;
          for (auto& byte : expected) byte = uint8_t(random());
          uint32_t object = 256 + sample % 4, input = 16384 + sample % 4;
          uint32_t mixed = 20480 + sample % 4, auxiliary = 24576 + sample % 4;
          if (sample >= 160 && sample < 192) {
            if (sample % 2) object = stack - scratch / 2 - (history_offset + entry.size * 4);
            else mixed = stack - 1028;
          }
          if (sample >= 128 && sample < 160) {
            switch (sample % 4) {
              case 0: mixed = input; break;
              case 1: auxiliary = mixed + 4; break;
              case 2: mixed = object + 8; break;
              case 3: input = mixed + 4; break;
            }
          }
          auto store = [&](uint32_t location, uint32_t value) {
            rex::memory::store_and_swap<uint32_t>(expected.data() + location, value);
          };
          auto value = [&]() -> uint32_t {
            if (sample >= 256) return 0x3f800000;
            if (sample < 8) return (sample & 1) ? 0x80000000u : 0;
            const auto bits = random();
            return (bits & 0x807fffffu) | ((sample >= 192 ? 1 + random() % 254 : 122 + random() % 9) << 23);
          };
          for (uint32_t index = 0; index < entry.size; ++index) store(object + history_offset + index * 4, value());
          for (uint32_t index = 0; index < 256; ++index) store(input + index * 4, value());
          for (uint32_t offset : {0u, 12u, 24u}) if (!entry.dual || offset != 24)
            store(object + offset, sample < 8 || sample >= 256 ? offset : random());
          store(object + 4, 0x3f000000); store(object + 16, 0x3e800000);
          if (!entry.dual) store(object + 28, 0xbf000000);
          if (sample >= 192 && sample < 256)
            for (uint32_t offset : {4u, 16u, 28u}) if (!entry.dual || offset != 28) store(object + offset, value());
          store(object + 8, 0x40e00000); store(object + second_offset, 0x41000000);
          uint32_t cursor = (sample % (entry.size / 256)) * 256;
          if (sample >= 96 && sample < 128) cursor = sample % 2 ? entry.size - 1 : entry.size;
          store(object + cursor_offset, cursor);
          if (sample >= 64 && sample < 96) {
            constexpr std::array<uint32_t, 6> exceptions{1, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u};
            const auto location = sample % 3 == 0 ? object + 4 : sample % 3 == 1 ? input + 12 : object + history_offset;
            store(location, exceptions[sample % exceptions.size()]);
          }
          actual = expected;
          const auto before = expected;
          PPCContext original{};
          original.r1.u32 = stack; original.r3.u32 = object; original.r4.u32 = input;
          original.r5.u32 = mixed; original.r6.u32 = auxiliary;
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
          if (sample >= 256) {
            original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
            Platform::setcsr(original.fpscr.csr);
          }
          const auto initial = original.fpscr.csr;
          auto replacement = original;
          // Eight consecutive blocks exercise all history lengths through
          // wraparound, keeping the evolving state/history and output prefix.
          for (uint32_t block = 0; block < (sample >= 8 && sample < 16 ? 8u : 1u); ++block) {
            original.r4.u32 = replacement.r4.u32 = input;
            original.r5.u32 = replacement.r5.u32 = mixed;
            original.r6.u32 = replacement.r6.u32 = auxiliary;
            original.r11.u64 = replacement.r11.u64 = marker;
            original.fpscr.csr = replacement.fpscr.csr = initial;
            Platform::setcsr(initial);
            entry.original(original, expected.data());
            const auto host_controls = Platform::getcsr() & controls;
            Platform::setcsr(initial);
            entry.native(replacement, actual.data());
            const bool native_case = sample < 64 || (sample >= 192 && sample < 256);
            if (native_case) {
              Require(replacement.r11.u64 == marker && original.r11.u64 != marker, "Ordinary delay did not use native path");
              Require(std::equal(actual.begin() + stack - scratch, actual.begin() + stack, before.begin() + stack - scratch),
                      "Native delay unexpectedly wrote guest stack scratch");
              std::copy_n(before.begin() + stack - scratch, scratch, expected.begin() + stack - scratch);
            }
            Require(actual == expected, "Native/translated delay bytes differ");
            Require(original.r1.u64 == replacement.r1.u64 && original.r3.u64 == replacement.r3.u64 &&
                    original.r4.u64 == replacement.r4.u64 && original.r5.u64 == replacement.r5.u64 &&
                    original.r6.u64 == replacement.r6.u64 && original.r7.u64 == replacement.r7.u64 &&
                    original.f1.u64 == replacement.f1.u64 && original.f2.u64 == replacement.f2.u64 &&
                    original.lr == replacement.lr, "Delay calling convention or output registers differ");
            if (entry.dual)
              Require(original.r9.u64 == replacement.r9.u64 && original.f0.u64 == replacement.f0.u64 &&
                      original.f13.u64 == replacement.f13.u64, "Dual tap cursor or product registers differ");
  #define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Delay nonvolatile state differs");
            CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
            CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
  #undef CHECK
            Require(original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                    original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov &&
                    original.xer.so == replacement.xer.so, "Delay condition/carry flags differ");
            Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                    (Platform::getcsr() & controls) == host_controls, "Delay FP controls differ");
            ++comparisons;
          }
        }
      }
    }
    Platform::setcsr(saved);
    std::cout << comparisons << " delay block equivalence cases, history/output/state bytes, FP controls and registrations passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Delay 0x" << std::hex << address << std::dec << " sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n';
    return 1;
  }
}
