#include <Windows.h>
#include "guest_state_payload.h"
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

extern "C" void sub_8209D358(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8209D358(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_state_payload_native_used = false;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 4> regions{{{0, 65536}, {0xdfff0000, 0x20000},
    {0xe1000000, 0x30000}, {0xffffb000, 0x4000}}};
// Independent implementation of the generated Windows shadow mapping.
uint64_t HostOffset(uint32_t address) { return uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u); }
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve sparse payload fixture");
    for (auto region : regions) if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
      VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit sparse payload fixture");
    }
  }
  ~Guest() { VirtualFree(base, 0, MEM_RELEASE); }
  Guest(const Guest&) = delete;
  void Copy(const Guest& other) { for (auto region : regions) std::memcpy(base + region.start, other.base + region.start, region.size); }
  bool Same(const Guest& other) const {
    for (auto region : regions) if (std::memcmp(base + region.start, other.base + region.start, region.size)) return false;
    return true;
  }
  void Word(uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(base + HostOffset(at), value); }
};
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
    std::mt19937 virtual_random(0x5041594c), physical_random(0x50485950);
    Guest seed, expected, actual;
    for (auto region : regions) for (uint32_t i = 0; i < region.size; ++i) seed.base[region.start + i] = uint8_t(physical_random());
    uint32_t comparisons = 0, native_count = 0, physical_native = 0, physical_retained = 0;
    for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
      flush = next_flush;
      for (sample = 0; sample < 290; ++sample) {
        auto& random = sample < 258 ? virtual_random : physical_random;
        expected.Copy(seed);
        for (uint32_t i = 0; i < 65536; ++i) expected.base[i] = uint8_t(random());
        uint32_t source = 16384 + sample % 4, destination = 32768 + (sample / 4) % 4;
        uint32_t stack = 0xf000;
        bool physical_expected_native = true;
        if (sample >= 258) {
          const auto physical_case = sample - 258;
          source = 0xe1004000u + physical_case % 4;
          if (physical_case >= 8) destination = 0xe1010000u + physical_case % 4;
          if (physical_case >= 16) stack = 0xe1020000u;
          if (physical_case == 20) { source = 0xe0000000u; destination = 0xe0004000u; }
          if (physical_case == 21) { destination = source; physical_expected_native = false; }
          if (physical_case == 22) { destination = source + 3; physical_expected_native = false; }
          if (physical_case == 23) { source = 0xdfffffe0u; physical_expected_native = false; }
          if (physical_case == 24) { destination = 0xdfffffe0u; physical_expected_native = false; }
          if (physical_case == 25) { destination = source - 99; physical_expected_native = false; }
          if (physical_case == 26) { source = 0xffffb000u; destination = 0xffffd000u; stack = 0xffffc000u; }
          if (physical_case == 27) { destination = 0xe0000000u; }
          if (physical_case == 28) { source = 0xe0000001u; destination = source + 100; }
          if (physical_case == 29) { destination = source + 99; physical_expected_native = false; }
          if (physical_case == 30) { source = 0xdfffffa0u; physical_expected_native = false; }
          if (physical_case == 31) { destination = source + 100; }
        }
        if (sample >= 208 && sample < 240) {
          const auto shift = 1 + ((sample - 208) / 2) * 6;
          destination = sample % 2 ? source - shift : source + shift;
        }
        if (sample >= 240 && sample < 256) destination = sample % 2 ? source + 100 : source;
        auto store = [&](uint32_t offset, uint32_t bits) {
          expected.Word(source + offset, bits);
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
        const bool unmasked = sample >= 256 && sample < 258;
        if (unmasked) for (auto offset : float_offsets) store(offset, 0x3f800000);
        std::array<uint8_t, 100> unbiased_destination{};
        const bool physical_output = destination >= 0xe0000000u;
        if (physical_output) std::copy_n(expected.base + destination, unbiased_destination.size(), unbiased_destination.begin());
        actual.Copy(expected);
        PPCContext original{};
        original.r1.u64 = 0x1234567800000000ull | stack;
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
        auto* vector_bytes = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
        for (uint32_t i = 0; i < 128 * sizeof(PPCVRegister); ++i) vector_bytes[i] = uint8_t(sample + i * 29);
        original.vscr_sat = uint8_t(sample & 1); original.last_indirect_target = 0x81234567;
#define SEED_CR(n) original.cr##n.set_raw((sample + n) % 16);
        SEED_CR(0) SEED_CR(1) SEED_CR(2) SEED_CR(3) SEED_CR(4) SEED_CR(5) SEED_CR(6) SEED_CR(7)
#undef SEED_CR
        original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
        original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
        if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
        if (unmasked) { original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); Platform::setcsr(original.fpscr.csr); }
        const auto initial = original.fpscr.csr; auto replacement = original;
        Platform::setcsr(initial);
        { n3::guest::RetainPayloadChain oracle; __imp__sub_8209D358(original, expected.base); }
        const auto host_controls = Platform::getcsr() & controls;
        Platform::setcsr(initial); n3_state_payload_native_used = false;
        sub_8209D358(replacement, actual.base);
        Require(!n3::guest::PayloadChainRetained(), "Payload oracle retain scope leaked");
        const bool native_case = sample >= 258 ? physical_expected_native : sample < 160 || (sample >= 240 && sample < 256 && sample % 2);
        Require(n3_state_payload_native_used == native_case, "Payload copy native/fallback route differs");
        if (native_case) ++native_count;
        if (sample >= 258) { if (native_case) ++physical_native; else ++physical_retained; }
        if (physical_output)
          Require(std::equal(unbiased_destination.begin(), unbiased_destination.end(), actual.base + destination) &&
                  std::equal(unbiased_destination.begin(), unbiased_destination.end(), expected.base + destination),
                  "Physical payload changed the unbiased destination canary");
        Require(actual.Same(expected), "Payload copy whole-fixture bytes differ");
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
        static_assert(offsetof(PPCContext, v127) + sizeof(PPCVRegister) - offsetof(PPCContext, v0) == 128 * sizeof(PPCVRegister));
        Require(!std::memcmp(&original.v0, &replacement.v0, 128 * sizeof(PPCVRegister)) &&
                original.msr == replacement.msr && original.vscr_sat == replacement.vscr_sat &&
                original.last_indirect_target == replacement.last_indirect_target, "Payload changed preserved vector/control state");
#define CHECK_CR(n) Require(original.cr##n.raw() == replacement.cr##n.raw(), "Payload condition field differs");
        CHECK_CR(0) CHECK_CR(1) CHECK_CR(2) CHECK_CR(3) CHECK_CR(4) CHECK_CR(5) CHECK_CR(6) CHECK_CR(7)
#undef CHECK_CR
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
    Require(physical_native == 200 && physical_retained == 56, "Physical payload fixtures lacked expected native and retained coverage");
    std::cout << comparisons << " payload-copy equivalence cases (" << native_count << " native), all fields/registers and overlap fallback passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved);
    std::cerr << "Payload copy sample " << sample << " rounding " << rounding << " flush " << flush << ": " << error.what() << '\n'; return 1;
  }
}
