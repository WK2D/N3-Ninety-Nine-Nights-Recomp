#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include "geometry_physical_test.h"

thread_local bool n3_shape_projection_native_used = false;

extern "C" void sub_82366808(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82366808(PPCContext&, uint8_t*);
extern "C" void sub_823603B0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_823603B0(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
struct Entry { uint32_t address; PPCFunc* native; PPCFunc* original; bool capsule; };
constexpr std::array<Entry, 2> entries{{
    {0x82366808, sub_82366808, __imp__sub_82366808, true},
    {0x823603B0, sub_823603B0, __imp__sub_823603B0, false}}};
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
    std::mt19937 random(0x50524f4a);
    constexpr uint32_t stack = 3584;
    constexpr uint64_t marker = 0x76543210;
    uint32_t comparisons = 0;
    constexpr std::array<uint32_t, 18> edges{0, 0x80000000u, 0x3f800000u, 0xbf800000u,
        1, 0x80000001u, 0x00800000u, 0x007fffffu, 0x7f7fffffu, 0xff7fffffu,
        0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u, 0xffc12345u,
        0x3f800001u, 0x33800000u, 0x4b800001u};
    for (const auto& entry : entries) {
      address = entry.address;
      bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == address) registered = mapping->host == entry.native;
      Require(registered, "Projection entry missing from indirect-call mapping");
      for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
        flush = next_flush;
        for (sample = 0; sample < 1026; ++sample) {
          std::array<uint8_t, 4096> expected{}, actual{};
          for (auto& byte : expected) byte = uint8_t(random());
          const bool source_stack_alias = sample >= 32 && sample < 1024 && sample % 17 == 0;
          const uint32_t object = source_stack_alias ? stack - 320 : 256 + sample % 4;
          const bool object_alias = sample >= 32 && sample % 5 == 0;
          const bool stack_alias = sample >= 32 && sample % 13 == 0;
          const auto output = stack_alias ? stack - 32 : object_alias ? object + (sample % 76) * 4 : 2048 + sample % 4;
          uint32_t index = 0;
          for (uint32_t offset : {36u, 48u, 60u, 68u, 72u, 76u, 288u, 292u, 296u, 300u}) {
            uint32_t bits = random();
            if (sample < 32 || sample >= 1024) {
              // Exact 3/4/0 extents give radius 5, allowing unmasked cases
              // without deliberately raising an inexact square-root trap.
              if (offset == 292) bits = 0x40400000;
              else if (offset == 296) bits = 0x40800000;
              else if (offset == 300) bits = 0;
              else bits = 0x3f800000;
              if (entry.capsule && sample >= 8 && sample < 16) {
                if (offset == 68 || offset == 72 || offset == 76) bits = 0x80000000u;
                else if (offset == 292) bits = 0x3f800000u;
                else if (offset == 288) bits = sample & 1 ? 0x80000000u : 0;
                else bits = 0;
              }
            } else if (sample < 128) bits = (bits & 0x807fffffu) | ((126 + random() % 3) << 23);
            else if (sample < 256) bits = edges[(sample + index) % edges.size()];
            else if (sample % 2) bits = (bits & 0x807fffffu) | ((1 + random() % 254) << 23);
            rex::memory::store_and_swap<uint32_t>(expected.data() + object + offset, bits);
            ++index;
          }
          actual = expected;
          const auto before = expected;
          PPCContext original{};
          original.r1.u32 = stack; original.r3.u32 = object; original.r4.u32 = output;
          original.r11.u64 = marker; original.lr = 0x82081234;
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
          if (sample < 8) {
            if (entry.capsule) {
              Require(replacement.r11.u64 == marker && original.r11.u64 != marker,
                      "Ordinary capsule did not use native path");
            } else {
              Require(std::equal(actual.begin() + stack - 96, actual.begin() + stack, before.begin() + stack - 96) &&
                      !std::equal(expected.begin() + stack - 96, expected.begin() + stack, before.begin() + stack - 96),
                      "Ordinary box sphere did not use native path");
            }
          }
          if (!entry.capsule && sample < 1024 && !object_alias && !stack_alias && !source_stack_alias) {
            std::copy_n(before.begin() + stack - 96, 96, expected.begin() + stack - 96);
            std::copy_n(before.begin() + stack - 96, 96, actual.begin() + stack - 96);
          }
          Require(actual == expected, "Native/translated projection bytes differ");
          Require(original.r1.u64 == replacement.r1.u64 && original.r3.u64 == replacement.r3.u64 &&
                  original.r4.u64 == replacement.r4.u64 && original.f1.u64 == replacement.f1.u64 &&
                  original.lr == replacement.lr, "Projection calling convention or FP result differs");
#define CHECK(n) Require(original.r##n.u64 == replacement.r##n.u64 && original.f##n.u64 == replacement.f##n.u64, "Projection nonvolatile state differs");
          CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
          CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
#undef CHECK
          Require(original.cr0.raw() == replacement.cr0.raw() && original.cr6.raw() == replacement.cr6.raw() &&
                  original.xer.ca == replacement.xer.ca && original.xer.ov == replacement.xer.ov &&
                  original.xer.so == replacement.xer.so, "Projection condition/carry flags differ");
          Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                  (Platform::getcsr() & controls) == host_controls, "Projection FP controls differ");
          ++comparisons;
        }
      }
    }
    uint32_t physical = 0, physical_native = 0;
    for (const auto& entry : entries)
      n3::test::CheckGeometryPhysical(entry.native, entry.original,
          entry.capsule ? n3::test::GeometryKind::Capsule : n3::test::GeometryKind::Sphere,
          n3_shape_projection_native_used, physical, physical_native);
    std::cout << physical << " physical projection cases (" << physical_native << " native) passed\n";
    Platform::setcsr(saved);
    std::cout << comparisons << " shape projection equivalence cases, aliases, FP results/controls and registrations passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Projection 0x" << std::hex << address << std::dec << " sample " << sample
              << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n';
    return 1;
  }
}
