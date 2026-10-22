#pragma once
#include <cstdint>
#include <optional>
#include <span>
namespace n3::native {
// The original reader assembles a little-endian four-byte window and extracts
// LSB-first fields. Its external bit position is one-based; this API is zero-based.
inline std::optional<uint32_t> ReadBitWindow(std::span<const uint8_t, 4> bytes,
                                             uint32_t offset, uint32_t width) {
  if (offset >= 8 || width == 0 || width > 32 || offset + width > 32) return std::nullopt;
  const uint32_t word = uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 |
      uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
  const uint32_t mask = width == 32 ? 0xffffffffu : (uint32_t(1) << width) - 1;
  return (word >> offset) & mask;
}
}
