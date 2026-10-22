#include "sparse_game_fixture.h"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>

#define DECLARE(address) extern "C" void sub_##address(PPCContext&, uint8_t*); \
  extern "C" void __imp__sub_##address(PPCContext&, uint8_t*);
DECLARE(8209E338) DECLARE(8209E368) DECLARE(8209E398) DECLARE(8209E470)
DECLARE(8209E4A0) DECLARE(8209E4D0) DECLARE(8209E500) DECLARE(8209E530)
DECLARE(8209E560) DECLARE(8209E688) DECLARE(8209E6B8) DECLARE(8209E3C8)
DECLARE(8209E6E8) DECLARE(8209E590)
#undef DECLARE
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_state_queries_native_used = false;
void Require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
namespace {
using namespace n3::test;
constexpr uint32_t flags_address = 0x825d9fa8;
enum class Kind { Flag, Related, Category, Byte };
struct Entry { uint32_t address; PPCFunc* native; PPCFunc* original; uint32_t mask; Kind kind; };
#define ENTRY(address, mask, kind) {0x##address, sub_##address, __imp__sub_##address, mask, Kind::kind}
constexpr std::array<Entry, 14> entries{{
    ENTRY(8209E338, 1, Flag), ENTRY(8209E368, 2, Flag), ENTRY(8209E398, 4, Flag),
    ENTRY(8209E470, 0x20, Flag), ENTRY(8209E4A0, 0x80000, Flag), ENTRY(8209E4D0, 0x100000, Flag),
    ENTRY(8209E500, 0x40000, Flag), ENTRY(8209E530, 0x20000, Flag), ENTRY(8209E560, 0x400000, Flag),
    ENTRY(8209E688, 0x10000, Flag), ENTRY(8209E6B8, 0x800000, Flag),
    ENTRY(8209E3C8, 8, Related), ENTRY(8209E6E8, 16, Category), ENTRY(8209E590, 0, Byte)}};
#undef ENTRY
struct Guest : SparseGameFixture {
  Guest() { Require(VirtualAlloc(base + 0x825d0000, 0x10000, MEM_COMMIT, PAGE_READWRITE) != nullptr, "Cannot commit flag table fixture"); }
  void Fill() {
    SparseGameFixture::Fill();
    for (uint32_t i = 0; i < 0x10000; ++i) base[0x825d0000 + i] = uint8_t(i * 17 + 9);
  }
  void Copy(const Guest& other) {
    SparseGameFixture::Copy(other); std::memcpy(base + 0x825d0000, other.base + 0x825d0000, 0x10000);
  }
  bool Same(const Guest& other) const {
    return SparseGameFixture::Same(other) && !std::memcmp(base + 0x825d0000, other.base + 0x825d0000, 0x10000);
  }
};
void Context(const PPCContext& a, const PPCContext& b) {
#define GPR(n) Require(a.r##n.u64 == b.r##n.u64, "State-query integer register differs");
  GPR(0) GPR(1) GPR(2) GPR(3) GPR(4) GPR(5) GPR(6) GPR(7) GPR(8) GPR(9) GPR(10)
  GPR(11) GPR(12) GPR(13) GPR(14) GPR(15) GPR(16) GPR(17) GPR(18) GPR(19) GPR(20)
  GPR(21) GPR(22) GPR(23) GPR(24) GPR(25) GPR(26) GPR(27) GPR(28) GPR(29) GPR(30) GPR(31)
#undef GPR
#define FPR(n) Require(a.f##n.u64 == b.f##n.u64, "State-query floating register differs");
  FPR(0) FPR(1) FPR(2) FPR(3) FPR(4) FPR(5) FPR(6) FPR(7) FPR(8) FPR(9) FPR(10)
  FPR(11) FPR(12) FPR(13) FPR(14) FPR(15) FPR(16) FPR(17) FPR(18) FPR(19) FPR(20)
  FPR(21) FPR(22) FPR(23) FPR(24) FPR(25) FPR(26) FPR(27) FPR(28) FPR(29) FPR(30) FPR(31)
#undef FPR
  Require(SamePreserved(a, b) && a.fpscr.csr == b.fpscr.csr, "State-query VMX/condition/FP/control state differs");
}
void Seed(PPCContext& ctx, uint32_t sample, uint32_t mode) {
  SeedPreserved(ctx, sample);
#define S(n) ctx.r##n.u64 = 0xabcdef0123456700ull + n; ctx.f##n.u64 = 0x7ff0000000000001ull + n;
  S(0) S(2) S(3) S(4) S(5) S(6) S(7) S(8) S(9) S(10) S(11) S(12) S(13)
#undef S
  ctx.lr |= 0x1234567800000000ull;
  ctx.fpscr.InitHost(); ctx.fpscr.storeFromGuest(mode % 4);
  if (mode & 4) ctx.fpscr.enableFlushModeUnconditional(); else ctx.fpscr.disableFlushModeUnconditional();
  if (mode & 8) ctx.fpscr.csr &= ~(rex::platform::FPSCRPlatform::ExceptionMask | 0x3fu);
}
struct Protected {
  uint8_t* page; DWORD previous;
  explicit Protected(uint8_t* at) : page(at) { Require(VirtualProtect(page, 4096, PAGE_NOACCESS, &previous) != 0, "Cannot protect unused query page"); }
  ~Protected() { DWORD ignored; VirtualProtect(page, 4096, previous, &ignored); }
};
uint32_t Deferred(const Guest& seed, Guest& expected, Guest& actual) {
  using Platform = rex::platform::FPSCRPlatform;
  uint32_t calls = 0;
  for (uint32_t mode = 0; mode < 16; ++mode) for (uint32_t branch = 0; branch < 5; ++branch) {
    expected.Copy(seed);
    uint32_t object = 0xe1000f54, owner = 0xe1010000, stack = 0xf000;
    if (branch == 0) owner = 0xdead0000;
    PPCFunc* original = __imp__sub_8209E3C8; PPCFunc* native = sub_8209E3C8;
    uint64_t argument = (uint64_t{7} << 32) | owner;
    uint64_t protected_page = PhysicalOffset(object + 496) & ~uint64_t{4095};
    expected.Store<uint32_t>(object + 164, 7); expected.Store<uint32_t>(object + 168, owner);
    if (branch != 0) {
      expected.Store<uint32_t>(owner + 844, 7); expected.Store<uint32_t>(owner + 848, owner);
      expected.Store<uint32_t>(owner + 852, 13);
    }
    if (branch == 0) argument = (uint64_t{7} << 32) | owner; // No related/table access.
    if (branch == 1) argument = (uint64_t{2} << 32) | owner; // Related match skips table.
    if (branch == 2) {
      original = __imp__sub_8209E6E8; native = sub_8209E6E8; object = 0xe1000e10;
      expected.Store<uint32_t>(object + 192, 0); expected.Store<uint32_t>(object + 316, 0x8000);
      expected.Store<uint32_t>(0x8000 + 444, 0x10000); expected.Store<uint32_t>(0x10004, 1024);
      protected_page = PhysicalOffset(object + 496) & ~uint64_t{4095};
    }
    if (branch >= 3) {
      original = __imp__sub_8209E590; native = sub_8209E590; object = 0xe1000f5f;
      *(expected.base + PhysicalOffset(object + 160)) = 0;
      *(expected.base + PhysicalOffset(object + 716)) = 255;
      argument = branch == 3 ? 0 : 1;
      protected_page = PhysicalOffset(object + (branch == 3 ? 160 : 716)) & ~uint64_t{4095};
    }
    actual.Copy(expected);
    PPCContext a{}; Seed(a, branch, mode); a.r1.u32 = stack; a.r3.u32 = object; a.r4.u64 = argument;
    auto b = a; const auto initial = a.fpscr.csr;
    {
      Protected left(expected.base + protected_page), right(actual.base + protected_page);
      std::optional<Protected> left_table, right_table;
      if (branch == 2) {
        left_table.emplace(expected.base + (flags_address & ~uint32_t{4095}));
        right_table.emplace(actual.base + (flags_address & ~uint32_t{4095}));
      }
      Platform::setcsr(initial); original(a, expected.base);
      Platform::setcsr(initial); n3_state_queries_native_used = false; native(b, actual.base);
    }
    Require(n3_state_queries_native_used && expected.Same(actual), "Deferred query read unused data or changed memory");
    Context(a, b); Require(Platform::getcsr() == initial, "Deferred query changed FP state"); ++calls;
  }
  return calls;
}
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  const auto saved = Platform::getcsr();
  uint32_t address = 0, sample = 0, mode = 0;
  try {
    Guest seed, expected, actual; seed.Fill();
    std::mt19937 random(0x51554552);
    uint32_t comparisons = 0, native_count = 0;
    for (auto entry : entries) {
      address = entry.address; bool registered = false;
      for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
        if (mapping->guest == address) registered = mapping->host == entry.native;
      Require(registered, "State-query indirect mapping missing");
      for (mode = 0; mode < 16; ++mode) for (sample = 0; sample < 128; ++sample) {
        expected.Copy(seed);
        const auto layout = sample % 32;
        const auto branch = (sample / 32 + mode) % 4;
        uint32_t object = 0x4000 + sample % 4, owner = 0x1000, stack = 0xf000, cell = flags_address + 4 * (sample % 12);
        bool native = true;
        if (layout >= 4) object = 0xe1004000u + sample % 4;
        if (layout >= 8) stack = 0xe1020000u + sample % 4;
        if (layout >= 12) cell = 0xe1008000;
        if (layout == 16) object = 0xffffff00;
        if (layout == 17) object = 0xdffffe0e;
        if (layout == 18) object = 0xfffffe0d;
        if (layout == 19) cell = 0xfffffffc;
        if (layout == 20) cell = 0xe0000000;
        if (layout == 21) cell = 0xdffffffc;
        if (layout == 22) stack = 0xffffffd8;
        if (layout == 23) stack = 0xdfffffd8;
        if (layout == 24) object = stack + 32 - 164;
        if (layout == 25) object = stack + 32 - 496;
        if (layout == 26) owner = stack + 32 - 844;
        if (layout == 27) object = stack - 8 - 168;
        if (layout == 28) object = stack - 8 - 496;
        if (layout == 29) cell = stack - 8;
        if (layout == 30) { object = 0xe0000000; stack = 0; }
        if (layout == 31) { object = 0xfffff800; stack = 0xfffffff0; }
        uint32_t kind = uint32_t(cell - flags_address) / 4;
        if (sample & 32) kind += 0x40000000u;
        const auto flags = sample < 32 ? (sample & 1 ? entry.mask : ~entry.mask) : random();
        expected.Store<uint32_t>(cell, flags);
        expected.Store<uint32_t>(object + 496, kind);
        if (entry.kind == Kind::Flag && (layout == 17 || layout == 18)) native = false;
        PPCContext original{}; Seed(original, sample, mode);
        original.r1.u64 = 0x1234567800000000ull | stack;
        original.r3.u64 = 0x8765432100000000ull | object;
        original.r4.u64 = (uint64_t{2} << 32) | owner;
        original.r5.u64 = 0x0000000900000003ull;
        if (entry.kind == Kind::Related) {
          expected.Store<uint32_t>(object + 164, 7); expected.Store<uint32_t>(object + 168, owner);
          expected.Store<uint32_t>(owner + 844, 3); expected.Store<uint32_t>(owner + 848, owner);
          expected.Store<uint32_t>(owner + 852, 4);
          if (branch == 0) original.r4.u64 = (uint64_t{7} << 32) | owner;
          if (branch == 1) expected.Store<uint32_t>(owner + 844, 7);
          if (layout == 22 || layout == 23 || layout == 24 ||
              ((layout == 17 || layout == 18 || layout == 25 || layout == 28 || layout == 29) && branch >= 2) ||
              (layout == 26 && branch != 0) || (layout == 27 && branch == 1)) native = false;
        } else if (entry.kind == Kind::Category) {
          // Separate the row table from the optional flag table.
          uint32_t root = layout < 8 ? 0x8000 : 0xe1010000u + sample % 4;
          uint32_t rows = layout < 12 ? 0x10000 : 0xe1014000u + sample % 4;
          uint32_t row = sample % 4;
          if (layout == 16) { rows = 0xfffffff8; row = 0; }
          if (layout == 19) { rows = 0xdffffff8; row = 0; }
          if (layout == 20) row = 0xffffffff;
          if (layout == 21) { rows = 0xdffffffa; row = 0; native = false; }
          if (layout == 22) { rows = 0xfffffffa; row = 0; native = false; }
          if (layout == 23) { root = 0xdffffe42; native = false; }
          if (layout == 24) row = 0x08000000;
          if (layout == 25) row = 0x80000000;
          if (layout == 26) row = 0x08000007;
          if (layout == 30) { object = 0xdffffec2; native = false; }
          if (layout == 31) { object = 0xdfffff3e; native = false; }
          original.r3.u64 = 0x8765432100000000ull | object;
          expected.Store<uint32_t>(object + 496, kind);
          expected.Store<uint32_t>(object + 316, root); expected.Store<uint32_t>(object + 192, row);
          expected.Store<uint32_t>(root + 444, rows);
          expected.Store<uint32_t>(rows + row * 32 + 4, branch & 1 ? 1024 : 0xffffffff);
          if ((layout == 17 || layout == 18) && !(branch & 1)) native = false;
        } else if (entry.kind == Kind::Byte) {
          original.r4.u64 = branch == 0 ? 0x100 : branch;
          *(expected.base + PhysicalOffset(object + 160)) = (sample / 4 + mode) & 1 ? 255 : 0;
          *(expected.base + PhysicalOffset(object + 716)) = uint8_t(sample * 29);
        }
        actual.Copy(expected); auto replacement = original;
        const auto host_initial = original.fpscr.csr ^ (sample & 1 ? Platform::FlushMask : 0);
        Platform::setcsr(host_initial); entry.original(original, expected.base);
        const auto host_expected = Platform::getcsr();
        Platform::setcsr(host_initial); n3_state_queries_native_used = false; entry.native(replacement, actual.base);
        Require(n3_state_queries_native_used == native, "State-query native/fallback route differs");
        Require(expected.Same(actual), "State-query entire fixture differs"); Context(original, replacement);
        Require(Platform::getcsr() == host_expected && host_expected == host_initial, "Integer query changed FP controls/status");
        if (native) ++native_count; ++comparisons;
      }
    }
    const auto deferred = Deferred(seed, expected, actual);
    Platform::setcsr(saved);
    std::cout << comparisons << " state-query equivalence calls (" << native_count << " native), " << deferred
              << " inaccessible unused-page calls, full memory/register/FP state passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved); std::cerr << "Query 0x" << std::hex << address << std::dec
        << " sample " << sample << " mode " << mode << ": " << error.what() << '\n'; return 1;
  }
}
