#include "sparse_game_fixture.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

extern "C" void sub_8209E230(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209E230(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_chance_roll_native_used = false;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
namespace {
using namespace n3::test;
constexpr uint32_t seed_address = 0x825d9d74;
struct Guest : SparseGameFixture {
  Guest() {
    if (!VirtualAlloc(base + 0x825d0000, 0x10000, MEM_COMMIT, PAGE_READWRITE))
      throw std::runtime_error("Cannot commit random-seed fixture");
  }
  void Fill() {
    SparseGameFixture::Fill();
    for (uint32_t i = 0; i < 0x10000; ++i) base[0x825d0000 + i] = uint8_t(i * 19 + 7);
  }
  void Copy(const Guest& other) {
    SparseGameFixture::Copy(other);
    std::memcpy(base + 0x825d0000, other.base + 0x825d0000, 0x10000);
  }
  bool Same(const Guest& other) const {
    return SparseGameFixture::Same(other) && !std::memcmp(base + 0x825d0000, other.base + 0x825d0000, 0x10000);
  }
};
void CompareContext(const PPCContext& a, const PPCContext& b) {
#define GPR(n) Require(a.r##n.u64 == b.r##n.u64, "Chance integer register differs");
  GPR(0) GPR(1) GPR(2) GPR(3) GPR(4) GPR(5) GPR(6) GPR(7) GPR(8) GPR(9) GPR(10)
  GPR(11) GPR(12) GPR(13) GPR(14) GPR(15) GPR(16) GPR(17) GPR(18) GPR(19) GPR(20)
  GPR(21) GPR(22) GPR(23) GPR(24) GPR(25) GPR(26) GPR(27) GPR(28) GPR(29) GPR(30) GPR(31)
#undef GPR
#define FPR(n) Require(a.f##n.u64 == b.f##n.u64, "Chance floating register differs");
  FPR(0) FPR(1) FPR(2) FPR(3) FPR(4) FPR(5) FPR(6) FPR(7) FPR(8) FPR(9) FPR(10)
  FPR(11) FPR(12) FPR(13) FPR(14) FPR(15) FPR(16) FPR(17) FPR(18) FPR(19) FPR(20)
  FPR(21) FPR(22) FPR(23) FPR(24) FPR(25) FPR(26) FPR(27) FPR(28) FPR(29) FPR(30) FPR(31)
#undef FPR
  Require(SamePreserved(a, b) && a.fpscr.csr == b.fpscr.csr,
          "Chance vector, condition, FP or control state differs");
}
struct InaccessiblePage {
  uint8_t* page;
  DWORD previous;
  explicit InaccessiblePage(uint8_t* address) : page(address) {
    Require(VirtualProtect(page, 4096, PAGE_NOACCESS, &previous) != 0,
            "Cannot protect unused threshold page");
  }
  ~InaccessiblePage() { DWORD ignored; VirtualProtect(page, 4096, previous, &ignored); }
};
uint32_t CheckDeferredThresholds(const Guest& seed, Guest& expected, Guest& actual) {
  using Platform = rex::platform::FPSCRPlatform;
  uint32_t count = 0;
  // Default is the last word of a host page; alternate is on the next page.
  constexpr uint32_t object = 0xe1000d00, owner = 0x8000, stack = 0xf000;
  for (uint32_t rounding = 0; rounding < 4; ++rounding) for (bool flush : {false, true})
    for (bool alternate : {false, true}) {
      expected.Copy(seed);
      expected.Store<uint32_t>(object + 764, 37);
      expected.Store<uint32_t>(object + 768, 63);
      expected.Store<uint32_t>(seed_address, 0xffffffff);
      *(expected.base + PhysicalOffset(owner + 812)) = alternate ? 1 : 0;
      actual.Copy(expected);
      PPCContext a{};
      SeedPreserved(a, rounding);
      a.r1.u32 = stack; a.r3.u32 = object; a.r4.u64 = 0x1122334400008000ull;
      a.r5.u64 = 0x123456789abcdef0ull;
      a.fpscr.InitHost(); a.fpscr.storeFromGuest(rounding);
      if (flush) a.fpscr.enableFlushModeUnconditional(); else a.fpscr.disableFlushModeUnconditional();
      const auto initial = a.fpscr.csr; auto b = a;
      const auto unused_page = PhysicalOffset(object + (alternate ? 764 : 768)) & ~uint64_t{4095};
      {
        InaccessiblePage expected_page(expected.base + unused_page), actual_page(actual.base + unused_page);
        Platform::setcsr(initial); __imp__sub_8209E230(a, expected.base);
        Platform::setcsr(initial); n3_chance_roll_native_used = false; sub_8209E230(b, actual.base);
      }
      Require(n3_chance_roll_native_used && expected.Same(actual), "Unused threshold was read or chance output differs");
      CompareContext(a, b);
      Require(Platform::getcsr() == initial, "Deferred threshold changed FP state");
      ++count;
    }
  return count;
}
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  const auto saved = Platform::getcsr();
  uint32_t sample = 0, mode = 0;
  try {
    bool registered = false;
    for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
      if (mapping->guest == 0x8209e230) registered = mapping->host == sub_8209E230;
    Require(registered, "Chance roll indirect registration missing");
    Guest seed, expected, actual;
    seed.Fill();
    std::mt19937 random(0x43484e43);
    constexpr std::array<uint32_t, 12> seeds{0, 1, 2, 0xffffffff, 0x80000000, 0x7fffffff,
        99, 100, 101, 134775813, 0x11223344, 0xdeadbeef};
    constexpr std::array<int32_t, 10> thresholds{(-2147483647 - 1), -100, -1, 0, 1, 17, 50, 99, 100, 2147483647};
    uint32_t comparisons = 0, native_count = 0, canaries = 0, replay_calls = 0;
    for (mode = 0; mode < 16; ++mode) for (sample = 0; sample < 256; ++sample) {
      expected.Copy(seed);
      const auto layout = sample % 64;
      uint32_t object = 0x4000 + sample % 4, owner = 0x8000 + (sample / 4) % 4, stack = 0xf000;
      bool native = true;
      if (layout >= 4 && layout < 8) object = 0xe1004000u + sample % 4;
      if (layout >= 8 && layout < 12) owner = 0xe1010000u + sample % 4;
      if (layout >= 12 && layout < 16) stack = 0xe1020000 + sample % 4;
      if (layout >= 16) { object = 0xe1004000u + sample % 4; owner = 0xe1010000u + sample % 4; stack = 0xe1020000 + sample % 4; }
      if (layout == 20) { object = 0xfffffcfc; owner = 0xfffff000; stack = 0xfffff800; }
      if (layout == 21) object = 0xfffffcfd;
      if (layout == 22) object = 0xfffffff0;
      if (layout == 23) object = 0xdfffffe0;
      if (layout == 24) object = 0xdffffcfc;
      if (layout == 25) { object = 0xe0000000; owner = 0xe0004000; stack = 0xe0010000; }
      if (layout == 26) stack = 0xdfffffd8;
      if (layout == 27) stack = 0xffffffd8;
      if (layout == 28) stack = 0xffffffe0;
      if (layout == 29) stack = 0xfffffff0;
      if (layout == 30) owner = 0xfffffcd3; // Derived flag wraps to zero.
      if (layout == 31) owner = 0xdffffcd4; // Flag is the first physical byte.
      if (layout == 32) object = stack + 32 - 764;
      if (layout == 33) object = stack + 32 - 768;
      if (layout == 34) owner = stack + 32 - 812;
      if (layout == 35) owner = stack + 47 - 812;
      if (layout == 36) stack = seed_address - 32;
      if (layout == 37) stack = seed_address - 39;
      if (layout == 38) object = seed_address - 764;
      if (layout == 39) object = seed_address - 768;
      if (layout == 40) owner = seed_address - 812;
      if (layout == 41) owner = object + 764 - 812;
      if (layout == 42) owner = object + 768 - 812;
      if (layout == 43) owner = object - 812;
      if (layout == 44) object = stack + 32;
      if (layout == 45) object = stack + 47;
      if (layout == 46) object = stack + 48;
      if (layout == 47) owner = stack + 48 - 812;
      if (layout == 48) object = 0xe0000001;
      if (layout == 49) owner = 0xfffffcd2; // Derived flag is the last guest byte.
      if (layout == 50) stack = 0xffffffe1;
      if (layout == 51) stack = 0xdfffffe0;
      if (layout == 52) object = 0xdffffcfd;
      if (layout == 53) owner = seed_address + 3 - 812;
      if (layout == 54) stack = seed_address - 47;
      if (layout == 55) object = stack + 16 - 764;
      if (layout == 56) owner = stack + 31 - 812;
      if (layout == 57) object = stack + 48 - 764;
      if (layout == 58) owner = object + 767 - 812;
      if (layout == 59) object = 0xfffffcfb;
      if (layout == 60) stack = 0;
      if (layout == 61) { object = 0x8000; owner = 0xe1004000; stack = 0xe1020000; }
      if (layout == 62) { object = 0xe1004000; owner = 0x8000; stack = 0xe1020000; }
      if (layout == 63) { object = 0xe1004000; owner = 0xe1010000; stack = 0xf000; }
      if (layout == 21 || layout == 22 || layout == 23 || layout == 26 || layout == 27 ||
          (layout >= 32 && layout <= 37) || layout == 44 || layout == 45 || layout == 52 ||
          layout == 54 || layout == 57) native = false;
      expected.Store<uint32_t>(object + 764, std::bit_cast<uint32_t>(thresholds[(sample + mode) % thresholds.size()]));
      expected.Store<uint32_t>(object + 768, std::bit_cast<uint32_t>(thresholds[(sample / 4 + mode) % thresholds.size()]));
      *(expected.base + PhysicalOffset(owner + 812)) = uint8_t((sample / 3 + mode) % 3 ? 255 : 0);
      expected.Store<uint32_t>(seed_address, sample < seeds.size() ? seeds[sample] : random());
      actual.Copy(expected);
      std::array<uint8_t, 16> unbiased{};
      const auto homes = stack + 32;
      const bool canary = native && homes >= 0xe0000000u;
      if (canary) std::copy_n(actual.base + uint64_t(homes), 16, unbiased.begin());
      PPCContext translated{};
      SeedPreserved(translated, sample);
#define SEED(n) translated.r##n.u64 = 0xabcdef0123456700ull + n; translated.f##n.u64 = 0x7ff0000000000001ull + n;
      SEED(0) SEED(2) SEED(6) SEED(7) SEED(8) SEED(9) SEED(10) SEED(11) SEED(12) SEED(13)
#undef SEED
      translated.r1.u64 = 0x1234567800000000ull | stack;
      translated.r3.u64 = 0x8765432100000000ull | object;
      translated.r4.u64 = 0x1122334400000000ull | owner;
      translated.r5.u64 = 0x91827364a5b6c7d8ull;
      translated.lr |= 0x1234567800000000ull;
      translated.fpscr.InitHost(); translated.fpscr.storeFromGuest(mode % 4);
      if (mode & 4) translated.fpscr.enableFlushModeUnconditional(); else translated.fpscr.disableFlushModeUnconditional();
      if (mode & 8) translated.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu);
      const auto guest_initial = translated.fpscr.csr;
      // Integer-only paths must preserve even independently divergent host controls.
      const auto host_initial = guest_initial ^ (sample & 1 ? Platform::FlushMask : 0);
      auto replacement = translated;
      Platform::setcsr(host_initial); __imp__sub_8209E230(translated, expected.base);
      const auto host_expected = Platform::getcsr();
      Platform::setcsr(host_initial); n3_chance_roll_native_used = false; sub_8209E230(replacement, actual.base);
      Require(n3_chance_roll_native_used == native, "Chance native/fallback route differs");
      Require(expected.Same(actual), "Chance entire fixture or argument homes differ");
      CompareContext(translated, replacement);
      Require(Platform::getcsr() == host_expected && host_expected == host_initial,
              "Chance integer helper changed host FP controls/status");
      if (canary) {
        Require(std::equal(unbiased.begin(), unbiased.end(), actual.base + uint64_t(homes)), "Chance changed unbiased homes");
        ++canaries;
      }
      if (native) ++native_count;
      ++comparisons;
      // Replay keeps the shared seed from each prior invocation; both branches
      // must advance identically, including guaranteed outcomes.
      if (layout == 0 && sample == 0) for (uint32_t turn = 0; turn < 64; ++turn) {
        expected.Store<uint32_t>(object + 764, turn & 1 ? 0 : 100);
        actual.Store<uint32_t>(object + 764, turn & 1 ? 0 : 100);
        auto a = replacement, b = replacement;
        a.r3.u32 = object; b.r3.u32 = object;
        a.r4.u64 = b.r4.u64 = 0x1122334400000000ull | owner;
        a.r5.u64 = b.r5.u64 = 0xaabbccddeeff0011ull;
        Platform::setcsr(host_initial); __imp__sub_8209E230(a, expected.base);
        Platform::setcsr(host_initial); n3_chance_roll_native_used = false; sub_8209E230(b, actual.base);
        Require(n3_chance_roll_native_used && expected.Same(actual), "Chance replay seed or homes drifted");
        CompareContext(a, b); ++replay_calls;
      }
    }
    Require(native_count && native_count < comparisons && canaries, "Chance fixture coverage is empty");
    const auto deferred = CheckDeferredThresholds(seed, expected, actual);
    Platform::setcsr(saved);
    std::cout << comparisons << " chance equivalence calls (" << native_count << " native), " << replay_calls
              << " retained-sequence replay calls, " << deferred << " inaccessible unused-threshold cases, "
              << "whole memory/register/FP state and physical canaries passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Chance sample " << sample << " mode " << mode << ": " << error.what() << '\n';
    return 1;
  }
}
