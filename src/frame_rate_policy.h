#pragma once
#include <bit>
#include <cstdint>

namespace n3::frames {
inline constexpr uint32_t kPresentIntervalOffset = 13788;

constexpr bool SupportsClock(uint32_t original_fps) {
  return original_fps == 30 || original_fps == 40 || original_fps == 60;
}
constexpr uint32_t StepSecondsBits(uint32_t fps) {
  return std::bit_cast<uint32_t>(1.0f / fps);
}
constexpr uint32_t PresentInterval(uint32_t original, uint32_t target) {
  // Lift an explicit two-vblank cap; retain other modes, including movies.
  return target > 30 && original == 2 ? 1 : original;
}
}  // namespace n3::frames
