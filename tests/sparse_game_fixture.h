#pragma once
#include <Windows.h>
#include <rex/ppc.h>
#include <rex/memory/utils.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace n3::test {
// Independent oracle mapping, deliberately separate from the adapter helper.
inline uint64_t PhysicalOffset(uint32_t address) {
  return uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u);
}
struct Region { uint64_t start; size_t size; };
inline constexpr std::array<Region, 4> sparse_regions{{{0, 0x20000},
    {0xdfff0000, 0x40000}, {0xe1000000, 0x40000}, {0xffff0000, 0x20000}}};
class SparseGameFixture {
 public:
  uint8_t* base;
  SparseGameFixture() : base(static_cast<uint8_t*>(VirtualAlloc(nullptr,
      size_t{0x100010000ull}, MEM_RESERVE, PAGE_NOACCESS))) {
    if (!base) throw std::runtime_error("Cannot reserve sparse game fixture");
    for (auto region : sparse_regions)
      if (!VirtualAlloc(base + region.start, region.size, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(base, 0, MEM_RELEASE);
        throw std::runtime_error("Cannot commit sparse game fixture");
      }
  }
  ~SparseGameFixture() { VirtualFree(base, 0, MEM_RELEASE); }
  SparseGameFixture(const SparseGameFixture&) = delete;
  void Fill() {
    for (auto region : sparse_regions) for (size_t i = 0; i < region.size; ++i)
      base[region.start + i] = uint8_t(i * 37 + (region.start >> 12) + 19);
  }
  void Copy(const SparseGameFixture& other) {
    for (auto region : sparse_regions) std::memcpy(base + region.start, other.base + region.start, region.size);
  }
  bool Same(const SparseGameFixture& other) const {
    for (auto region : sparse_regions)
      if (std::memcmp(base + region.start, other.base + region.start, region.size)) return false;
    return true;
  }
  template<class T> T Load(uint32_t at) const {
    return rex::memory::load_and_swap<T>(base + PhysicalOffset(at));
  }
  template<class T> void Store(uint32_t at, T value) {
    rex::memory::store_and_swap<T>(base + PhysicalOffset(at), value);
  }
};
inline void SeedPreserved(PPCContext& ctx, uint32_t sample) {
#define SEED(n) ctx.r##n.u64 = 0x123456789abcde00ull + n; ctx.f##n.u64 = 0x40123456789abc00ull + n;
  SEED(14) SEED(15) SEED(16) SEED(17) SEED(18) SEED(19) SEED(20) SEED(21) SEED(22)
  SEED(23) SEED(24) SEED(25) SEED(26) SEED(27) SEED(28) SEED(29) SEED(30) SEED(31)
#undef SEED
  ctx.lr = 0x82081234; ctx.ctr.u64 = 0x1234567887654321ull;
  auto* vectors = reinterpret_cast<uint8_t*>(&ctx.v0);
  for (size_t i = 0; i < 128 * sizeof(PPCVRegister); ++i) vectors[i] = uint8_t(sample + i * 29);
#define CR(n) ctx.cr##n.set_raw((sample + n) % 16);
  CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
  ctx.xer = {uint8_t(sample & 1), uint8_t((sample >> 1) & 1), uint8_t((sample >> 2) & 1)};
  ctx.vscr_sat = uint8_t(sample & 1); ctx.last_indirect_target = 0x81234567;
}
inline bool SamePreserved(const PPCContext& a, const PPCContext& b) {
#define CHECK(n) if (a.r##n.u64 != b.r##n.u64 || a.f##n.u64 != b.f##n.u64) return false;
  CHECK(14) CHECK(15) CHECK(16) CHECK(17) CHECK(18) CHECK(19) CHECK(20) CHECK(21) CHECK(22)
  CHECK(23) CHECK(24) CHECK(25) CHECK(26) CHECK(27) CHECK(28) CHECK(29) CHECK(30) CHECK(31)
#undef CHECK
#define CR(n) if (a.cr##n.raw() != b.cr##n.raw()) return false;
  CR(0) CR(1) CR(2) CR(3) CR(4) CR(5) CR(6) CR(7)
#undef CR
  return a.r1.u64 == b.r1.u64 && a.lr == b.lr && a.ctr.u64 == b.ctr.u64 &&
      a.xer.ca == b.xer.ca && a.xer.ov == b.xer.ov && a.xer.so == b.xer.so &&
      !std::memcmp(&a.v0, &b.v0, 128 * sizeof(PPCVRegister)) &&
      a.vscr_sat == b.vscr_sat && a.msr == b.msr && a.last_indirect_target == b.last_indirect_target;
}
}
