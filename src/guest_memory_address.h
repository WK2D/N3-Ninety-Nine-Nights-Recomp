#pragma once
#include <cstdint>

namespace n3::guest {
// Match generated/default/n3_pch.h's physical-memory shadow bias. A guest
// address remains 32 bits; its translated host offset may require 33 bits.
inline constexpr uint64_t GuestHostOffset(uint32_t address) {
#if defined(_WIN32) || (defined(__APPLE__) && (defined(__aarch64__) || defined(_M_ARM64)))
  return uint64_t(address) + (address >= 0xe0000000u ? 0x1000u : 0u);
#else
  return address;
#endif
}
inline uint8_t* GuestHostAddress(uint8_t* base, uint32_t address) {
  return base + GuestHostOffset(address);
}
inline const uint8_t* GuestHostAddress(const uint8_t* base, uint32_t address) {
  return base + GuestHostOffset(address);
}
// This validates address arithmetic, not whether memory is mapped. Callers must
// supply valid guest buffers. A range crossing the physical-shadow boundary is
// rejected because separate original accesses can use different host biases.
inline constexpr bool FitsLinearGuestRange(uint32_t address, uint32_t size) {
  const uint64_t end = uint64_t(address) + size;
  if (end > 0x100000000ull) return false;
#if defined(_WIN32) || (defined(__APPLE__) && (defined(__aarch64__) || defined(_M_ARM64)))
  if (address < 0xe0000000u && end > 0xe0000000ull) return false;
#endif
  return true;
}
// Each input range must satisfy FitsLinearGuestRange before this comparison.
inline constexpr bool DisjointGuestHostRanges(uint32_t a, uint32_t a_size,
    uint32_t b, uint32_t b_size) {
  return GuestHostOffset(a) + a_size <= GuestHostOffset(b) ||
      GuestHostOffset(b) + b_size <= GuestHostOffset(a);
}
}
