#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <random>
#include <stdexcept>
#include "geometry_physical_test.h"

thread_local bool n3_box_geometry_native_used = false;

extern "C" void sub_823602C0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_823602C0(PPCContext&, uint8_t*);
extern "C" void sub_823604B8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_823604B8(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
struct Entry { uint32_t address; PPCFunc* native; PPCFunc* original; bool corner; };
constexpr std::array<Entry, 2> entries{{
    {0x823602C0, sub_823602C0, __imp__sub_823602C0, false},
    {0x823604B8, sub_823604B8, __imp__sub_823604B8, true}}};
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  const auto saved = Platform::getcsr();
  uint32_t address = 0, sample = 0, rounding = 0;
  bool flush = false;
  try {
    std::mt19937 random(0x424f5833);
    uint32_t comparisons = 0;
    constexpr uint32_t stack = 3584;
    constexpr std::array<uint32_t, 18> edges{0, 0x80000000u, 0x3f800000u, 0xbf800000u,
        1, 0x80000001u, 0x00800000u, 0x007fffffu, 0x7f7fffffu, 0xff7fffffu,
        0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u, 0xffc12345u,
        0x3f800001u, 0x33800000u, 0x4b800001u};
    constexpr std::array<int32_t, 10> integers{-1, 1, 0, 2, -2, 16777217, -16777217,
        2147483647, (-2147483647 - 1), 123456789};
    for (const auto& entry : entries) {
      address = entry.address;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == address) registered = mapping->host == entry.native;
      Require(registered, "Box entry missing from indirect-call mapping");
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < 1026; ++sample) {
          std::array<uint8_t, 4096> expected{}, actual{};
          for (auto& byte : expected) byte = uint8_t(random());
          const bool source_stack_alias = sample >= 32 && sample < 1024 && sample % 17 == 0;
          const uint32_t object = source_stack_alias ? stack - 320 : 256 + sample % 4;
          const bool object_alias = sample >= 32 && sample % 5 == 0;
          const bool stack_alias = sample >= 32 && sample % 13 == 0;
          const uint32_t output = stack_alias ? stack - 32 : object_alias ? object + (sample % 76) * 4 : 2048 + sample % 4;
          auto store = [&](uint32_t offset, uint32_t bits) {
            rex::memory::store_and_swap<uint32_t>(expected.data() + object + offset, bits);
          };
          for (uint32_t index = 0; index < 12; ++index) {
            uint32_t bits = random();
            if (sample < 32 || sample >= 1024)
              bits = index < 9 ? (index % 4 == 0 ? 0x3f800000u : 0) : 0x41200000u;
            else if (sample < 128) bits = (bits & 0x807fffffu) | ((126 + random() % 3) << 23);
            else if (sample < 256) bits = edges[(sample + index) % edges.size()];
            else if (sample % 2) bits = (bits & 0x807fffffu) | ((1 + random() % 254) << 23);
            store(32 + 4 * index, bits);
          }
          for (uint32_t index = 0; index < 3; ++index) {
            uint32_t bits = random();
            if (sample < 32 || sample >= 1024) bits = 0x3f800000u + (index << 23);
            else if (sample < 128) bits = (bits & 0x807fffffu) | (126u << 23);
            else if (sample < 256) bits = edges[(sample + index) % edges.size()];
            else if (sample % 2) bits = (bits & 0x807fffffu) | ((1 + random() % 254) << 23);
            store(292 + index * 4, bits);
          }
          actual = expected;
          const auto before = expected;
          PPCContext original{};
          original.r1.u32 = stack; original.r3.u32 = object; original.r7.u32 = output;
          original.r4.s64 = integers[sample % integers.size()];
          original.r5.s64 = integers[(sample / 10) % integers.size()];
          original.r6.s64 = integers[(sample / 100) % integers.size()];
          if (sample < 8 || sample >= 1024) {
            original.r4.s64 = sample & 1 ? 1 : -1;
            original.r5.s64 = sample & 2 ? 1 : -1;
            original.r6.s64 = sample & 4 ? 1 : -1;
          }
          if (!entry.corner) original.r4.u32 = output;
          original.lr = 0x82081234;
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
          SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
          SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
          original.cr0.set_raw(sample % 16); original.cr6.set_raw((sample + 3) % 16);
          original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
          original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
          if (flush) original.fpscr.enableFlushModeUnconditional();
          else original.fpscr.disableFlushModeUnconditional();
          if (sample >= 1024) {
            original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
            Platform::setcsr(original.fpscr.csr);
          }
          const auto initial = original.fpscr.csr;
          auto replacement = original;
          entry.original(original, expected.data());
          const auto host_controls = Platform::getcsr() & controls;
          Platform::setcsr(initial);
          entry.native(replacement, actual.data());
          const auto scratch = entry.corner ? 64 : 24;
          if (sample < 8) {
            Require(std::equal(actual.begin() + stack - scratch, actual.begin() + stack,
                               before.begin() + stack - scratch), "Ordinary box case did not use native path");
            Require(!std::equal(expected.begin() + stack - scratch, expected.begin() + stack,
                                before.begin() + stack - scratch), "Original fixture did not exercise stack scratch");
          }
          if (sample < 1024 && !object_alias && !stack_alias && !source_stack_alias) {
            // Only dead caller stack scratch is normalized. Aliasing outputs
            // and unmasked exact-arithmetic cases must match that scratch too.
            std::copy_n(before.begin() + stack - scratch, scratch, expected.begin() + stack - scratch);
            std::copy_n(before.begin() + stack - scratch, scratch, actual.begin() + stack - scratch);
          }
          Require(actual == expected, "Native/translated box bytes differ");
          Require(original.r1.u64 == replacement.r1.u64 && original.r3.u64 == replacement.r3.u64 &&
                  original.r4.u64 == replacement.r4.u64 && original.r5.u64 == replacement.r5.u64 &&
                  original.r6.u64 == replacement.r6.u64 && original.r7.u64 == replacement.r7.u64 &&
                  original.lr == replacement.lr, "Box calling convention differs");
#define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Box nonvolatile state differs");
          CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
          CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
#undef CHECK
          Require(original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                  original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov &&
                  original.xer.so == replacement.xer.so, "Box condition/carry flags differ");
          Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == host_controls, "Box FP controls differ");
          ++comparisons;
        }
      }
    }
    uint32_t physical = 0, physical_native = 0;
    for (const auto& entry : entries)
      n3::test::CheckGeometryPhysical(entry.native, entry.original,
          entry.corner ? n3::test::GeometryKind::Sample : n3::test::GeometryKind::Bounds,
          n3_box_geometry_native_used, physical, physical_native);
    std::cout << physical << " physical box cases (" << physical_native << " native) passed\n";
    Platform::setcsr(saved);
    std::cout << comparisons << " box equivalence cases, transforms, integer coordinates, aliases, FP controls and registrations passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Box 0x" << std::hex << address << std::dec << " sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n';
    return 1;
  }
}
