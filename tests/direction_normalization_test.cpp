#include <Windows.h>
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

extern "C" void sub_823B3C00(PPCContext&, uint8_t*);
extern "C" void __imp__sub_823B3C00(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];
thread_local bool n3_direction_normalization_native_used = false;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Region { uint32_t start, size; };
constexpr std::array<Region, 5> regions{{{0, 0x40000}, {0x82001000, 4096},
    {0xdfff0000, 0x20000}, {0xe1000000, 0x20000}, {0xffffb000, 0x4000}}};
constexpr uint32_t threshold_address = 0x820018ac;
// Independent fixture implementation of the Windows translation's address rule.
uint64_t HostOffset(uint32_t address) { return uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u); }
struct Guest {
  uint8_t* base;
  Guest() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS))) {
    Require(base != nullptr, "Cannot reserve normalization fixture");
    for (auto region : regions) if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
      VirtualFree(base, 0, MEM_RELEASE); throw std::runtime_error("Cannot commit normalization fixture");
    }
  }
  ~Guest() { VirtualFree(base, 0, MEM_RELEASE); }
  Guest(const Guest&) = delete;
  void Copy(const Guest& other) { for (auto region : regions) std::memcpy(base + region.start, other.base + region.start, region.size); }
  bool Same(const Guest& other) const {
    for (auto region : regions) if (std::memcmp(base + region.start, other.base + region.start, region.size)) return false;
    return true;
  }
  void Word(uint32_t at, uint32_t bits) { rex::memory::store_and_swap<uint32_t>(base + HostOffset(at), bits); }
};
void CheckContext(const PPCContext& expected, const PPCContext& actual) {
#define CHECK(n) if (expected.r##n.u64 != actual.r##n.u64 || expected.f##n.u64 != actual.f##n.u64) { \
    std::cerr << "r/f" #n " expected " << std::hex << expected.r##n.u64 << '/' << expected.f##n.u64 \
              << " actual " << actual.r##n.u64 << '/' << actual.f##n.u64 << std::dec << '\n'; \
    throw std::runtime_error("Normalization GPR/FPR result differs"); }
  CHECK(0) CHECK(1) CHECK(2) CHECK(3) CHECK(4) CHECK(5) CHECK(6) CHECK(7) CHECK(8) CHECK(9)
  CHECK(10) CHECK(11) CHECK(12) CHECK(13) CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19)
  CHECK(20) CHECK(21) CHECK(22) CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29)
  CHECK(30) CHECK(31)
#undef CHECK
  static_assert(offsetof(PPCContext, v127) + sizeof(PPCVRegister) - offsetof(PPCContext, v0) == 128 * sizeof(PPCVRegister));
  const auto* left = reinterpret_cast<const uint8_t*>(&expected) + offsetof(PPCContext, v0);
  const auto* right = reinterpret_cast<const uint8_t*>(&actual) + offsetof(PPCContext, v0);
  for (uint32_t i = 0; i < 128; ++i) if (std::memcmp(left + i * 16, right + i * 16, 16)) {
    std::cerr << "v" << i << " differs\n"; throw std::runtime_error("Normalization VMX result differs");
  }
#define CR(n) Require(expected.cr##n.raw() == actual.cr##n.raw(), "Normalization condition field differs");
  CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
  Require(expected.xer.ca == actual.xer.ca && expected.xer.ov == actual.xer.ov && expected.xer.so == actual.xer.so &&
          expected.lr == actual.lr && expected.ctr.u64 == actual.ctr.u64 && expected.msr == actual.msr &&
          expected.vscr_sat == actual.vscr_sat && expected.last_indirect_target == actual.last_indirect_target,
          "Normalization carry/control or preserved state differs");
}
int main() {
  using Platform = rex::platform::FPSCRPlatform;
  constexpr auto controls = PPCFPSCRRegister::GuestMask | Platform::ExceptionMask;
  constexpr uint32_t sample_count = 580;
  const auto saved_host = Platform::getcsr();
  uint32_t sample = 0, rounding = 0; bool flush = false;
  try {
    bool registered = false;
    for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping)
      if (mapping->guest == 0x823b3c00) registered = mapping->host == sub_823B3C00;
    Require(registered, "Normalization entry missing from indirect mapping");
    Guest seed, expected, actual;
    std::mt19937 random(0x4e4f524d);
    for (auto region : regions) for (uint32_t i = 0; i < region.size; ++i) seed.base[region.start + i] = uint8_t(random());
    constexpr std::array<uint32_t, 24> float_bits{
        0, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x40000000u, 0xc0000000u,
        1, 0x80000001u, 0x007fffffu, 0x00800000u, 0x00800001u, 0x80800000u,
        0x1fffffffu, 0x20000000u, 0x20000001u, 0x3f000000u,
        0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u,
        0x7fc12345u, 0x7f812345u, 0x3f800001u, 0x33800000u};
    uint32_t comparisons = 0, native_count = 0, retained_count = 0, overlap_native = 0, divergent_native = 0;
    uint32_t physical_native = 0, boundary_retained = 0;
    for (rounding = 0; rounding < 4; ++rounding) for (bool next_flush : {false, true}) {
      flush = next_flush;
      for (sample = 0; sample < sample_count; ++sample) {
        expected.Copy(seed);
        uint32_t stack = 0x30000;
        uint32_t source = 0x6000 + sample % 16, destination = 0x1000 + sample % 4;
        uint32_t threshold = 0x00800000u;
        std::array<uint32_t, 4> values{};
        bool must_native = sample < 32, must_fallback = false;
        if (sample < 32) {
          source = 0x6000 + (sample % 4) * 4;
          values[(sample / 4) % 3] = std::bit_cast<uint32_t>(sample % 2 ? -2.0f : 1.0f);
          if (sample == 0) values = {};
        } else if (sample < 192) {
          for (auto& bits : values) bits = std::bit_cast<uint32_t>(float(int32_t(random() % 201) - 100) / 8.0f);
        } else if (sample < 384) {
          for (uint32_t lane = 0; lane < 4; ++lane) values[lane] = float_bits[(sample + lane * 5) % float_bits.size()];
          if (sample % 4 == 0) values = {float_bits[sample % float_bits.size()], 0, 0, 0};
          else if (sample % 4 == 1) values = {0x3f800000u, 0, 0, float_bits[sample % float_bits.size()]};
        } else if (sample < 448) {
          values = {0x3f800000u, 0x40000000u, 0x40400000u, 0};
          threshold = float_bits[sample % float_bits.size()];
        } else if (sample < 512) {
          values = {0x3f800000u, 0, 0, 0};
          if (sample % 4 == 0) destination = source;
          else if (sample % 4 == 1) destination = source + 4;
          else if (sample % 4 == 2) destination = source - 3;
          else destination = source + 8;
        } else if (sample < 544) {
          const auto alias = sample - 512;
          source = 0x6000; destination = 0x1000; values = {};
          switch (alias) {
            case 0: source = stack - 16; must_fallback = true; break;
            case 1: source = stack - 8; must_fallback = true; break;
            case 2: source = stack + 16; must_fallback = true; break;
            case 3: source = stack + 20; must_fallback = true; break;
            case 4: destination = stack + 20; must_fallback = true; break;
            case 5: destination = stack + 16; must_fallback = true; break;
            case 6: destination = stack + 12; must_fallback = true; break;
            case 7: destination = stack - 16; must_fallback = true; break;
            case 8: stack += 1; must_fallback = true; break;
            case 9: stack = threshold_address + 16; must_fallback = true; break;
            case 10: stack = threshold_address - 20; must_fallback = true; break;
            case 11: source = threshold_address; break;
            case 12: destination = threshold_address; must_native = true; break;
            case 13: destination = source; must_native = true; break;
            case 14: destination = source + 4; must_native = true; break;
            case 15: source = stack + 8; must_fallback = true; break;
            case 16: source = stack - 32; must_native = true; break;
            case 17: source = stack - 20; must_fallback = true; break;
            case 18: destination = stack - 20; must_fallback = true; break;
            case 19: destination = stack + 22; must_fallback = true; break;
            case 20: stack += 4; must_fallback = true; break;
            case 21: source = stack - 1; must_fallback = true; break;
            case 22: destination = source - 3; must_native = true; break;
            case 23: source = threshold_address - 12; break;
            case 24: destination = stack + 24; must_native = true; break;
            case 25: source = stack + 24; must_fallback = true; break;
            case 26: stack += 8; must_fallback = true; break;
            case 27: destination = stack - 4; must_fallback = true; break;
            case 28: source = stack + 32; must_native = true; break;
            case 29: source = destination = threshold_address; break;
            case 30: stack = threshold_address + 4; must_fallback = true; break;
            case 31: destination = source - 4; must_native = true; break;
          }
        } else if (sample < 548) {
          source = 0x6000; destination = 0x1000; values = {};
          must_fallback = sample < 547; must_native = sample == 547;
        } else {
          const auto physical_case = sample - 548;
          values = {0x40400000u, 0x40800000u, 0, 0};
          source = 0xe1006000u + physical_case % 16;
          must_native = true;
          if (physical_case >= 16) { destination = 0xe1001000u + physical_case % 4; stack = 0xe100a000u; }
          if (physical_case == 24) destination = source + 4;
          if (physical_case == 25) { source = stack - 16; must_native = false; must_fallback = true; }
          if (physical_case == 26) { destination = stack + 20; values = {}; must_native = false; must_fallback = true; }
          if (physical_case == 27) { source = 0xdffffffcu; must_native = false; must_fallback = true; }
          if (physical_case == 28) { destination = 0xdffffffcu; must_native = false; must_fallback = true; }
          if (physical_case == 29) { stack = 0xe0000000u; must_native = false; must_fallback = true; }
          if (physical_case == 30) stack = 0xe0000010u;
          if (physical_case == 31) { source = 0xffffd004u; destination = 0xffffb000u; stack = 0xffffc800u; }
        }
        std::fill_n(expected.base + HostOffset(source & ~15u), 16, uint8_t{0});
        std::fill_n(expected.base + HostOffset((source + 11) & ~15u), 16, uint8_t{0});
        for (uint32_t lane = 0; lane < values.size(); ++lane) expected.Word(source + lane * 4, values[lane]);
        expected.Word(threshold_address, threshold);
        std::array<uint8_t, 12> unbiased_output{};
        const bool physical_output = destination >= 0xe0000000u;
        if (physical_output) std::copy_n(expected.base + (destination & ~3u), 12, unbiased_output.begin());
        actual.Copy(expected);
        PPCContext original{};
#define SEED(n) original.r##n.u64 = 0x123456789abcde00ull + n; original.f##n.u64 = 0x40123456789abc00ull + n;
        SEED(0) SEED(1) SEED(2) SEED(3) SEED(4) SEED(5) SEED(6) SEED(7) SEED(8) SEED(9)
        SEED(10) SEED(11) SEED(12) SEED(13) SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19)
        SEED(20) SEED(21) SEED(22) SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29)
        SEED(30) SEED(31)
#undef SEED
        auto* vector_bytes = reinterpret_cast<uint8_t*>(&original) + offsetof(PPCContext, v0);
        for (uint32_t i = 0; i < 128 * 16; ++i) vector_bytes[i] = uint8_t(random());
        original.r1.u64 = stack; original.r3.u64 = 0x1234567800000000ull | destination;
        original.r4.u64 = 0xabcdef0100000000ull | source;
        original.lr = 0x8765432182081234ull; original.ctr.u64 = 0x23456789abcdef01ull;
        original.vscr_sat = uint8_t(sample & 1); original.last_indirect_target = 0x81234567;
#define CR(n) original.cr##n.set_raw((sample + n) % 16);
        CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
        original.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
        original.fpscr.InitHost(); original.fpscr.storeFromGuest(rounding);
        if (flush) original.fpscr.enableFlushModeUnconditional(); else original.fpscr.disableFlushModeUnconditional();
        uint32_t initial_host = original.fpscr.csr;
        if (sample == 544) {
          original.fpscr.csr &= ~(Platform::ExceptionMask | 0x3fu); initial_host = original.fpscr.csr;
        } else if (sample == 545) {
          original.fpscr.csr |= Platform::FlushMask;
          initial_host = original.fpscr.csr & ~(Platform::ExceptionMask | 0x3fu);
        } else if (sample == 546) {
          original.fpscr.csr &= ~(Platform::ExceptionMask | Platform::FlushMask | 0x3fu);
          initial_host = original.fpscr.csr | Platform::ExceptionMask;
        } else if (sample == 547) {
          initial_host = (original.fpscr.csr & ~Platform::RoundMaskVal) |
              Platform::GuestToHost[(rounding + 1) % 4];
        }
        auto replacement = original;
        Platform::setcsr(initial_host); __imp__sub_823B3C00(original, expected.base);
        const auto original_host_controls = Platform::getcsr() & controls;
        Platform::setcsr(initial_host); n3_direction_normalization_native_used = false;
        sub_823B3C00(replacement, actual.base);
        const bool native = n3_direction_normalization_native_used;
        Require(!must_native || native, "Ordinary or safe alias normalization did not use native path");
        Require(!must_fallback || !native, "Unsafe alias or unmasked normalization used native path");
        if (native) {
          ++native_count;
          const auto output_start = destination & ~3u;
          if (uint64_t(output_start) < uint64_t(source) + 12 && uint64_t(source) < uint64_t(output_start) + 12) ++overlap_native;
          if (sample == 547) ++divergent_native;
          if (sample >= 548) ++physical_native;
        } else ++retained_count;
        if (sample >= 575 && sample <= 577 && !native) ++boundary_retained;
        if (physical_output)
          Require(std::equal(actual.base + (destination & ~3u), actual.base + (destination & ~3u) + 12, unbiased_output.begin()) &&
                  std::equal(expected.base + (destination & ~3u), expected.base + (destination & ~3u) + 12, unbiased_output.begin()),
                  "Physical normalization changed the unbiased output canary");
        Require(expected.Same(actual), "Normalization whole-fixture bytes differ");
        CheckContext(original, replacement);
        Require((original.fpscr.csr & controls) == (replacement.fpscr.csr & controls) &&
                (Platform::getcsr() & controls) == original_host_controls,
                "Normalization FP control or final flush state differs");
        Require((replacement.fpscr.csr & Platform::FlushMask) == Platform::FlushMask,
                "Normalization did not finish with vector flush enabled");
        ++comparisons;
      }
    }
    Require(overlap_native > 0 && divergent_native == 8, "Safe overlap or divergent masked controls lacked native coverage");
    Require(physical_native >= 200 && boundary_retained == 24, "Physical mapping or shadow-boundary fallback lacked coverage");
    Platform::setcsr(saved_host);
    std::cout << comparisons << " normalization equivalence cases (" << native_count << " native, " << retained_count
              << " retained), all VMX/GPR/FPR results, aliases and FP controls passed\n";
  } catch (const std::exception& error) {
    Platform::setcsr(saved_host);
    std::cerr << "Normalization sample " << sample << " rounding " << rounding << " flush " << flush
              << ": " << error.what() << '\n'; return 1;
  }
}
