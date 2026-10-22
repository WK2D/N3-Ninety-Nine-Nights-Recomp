#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace n3::native {
inline constexpr uint32_t kDelayBlockSamples = 256;
struct DelayTap { uint32_t delay; float gain; };
struct DelayBlockState {
  std::array<DelayTap, 3> taps;
  float previous_mixed, previous_auxiliary;
  uint32_t cursor;
};
// Supported history lengths are 512 and 2048 samples. Cursor must identify a
// complete contiguous 256-sample block. Tap delays wrap modulo history length;
// delay zero reads old history before the current input is written.
// Outputs include the previous result at index 0, followed by 256 new results.
// Caller supplies valid storage, its rounding mode, disabled scalar flush and
// masked traps. Overlapping storage or invalid values reject without mutation.
bool TryProcessDelayBlock(DelayBlockState& state, std::span<float> history,
    std::span<const float, kDelayBlockSamples> input,
    std::span<float, kDelayBlockSamples + 1> mixed,
    std::span<float, kDelayBlockSamples + 1> auxiliary);

struct DualTapDelayState {
  std::array<DelayTap, 2> taps;
  std::array<float, 2> previous;
  uint32_t cursor;
};
// Two independent delayed products share a 1024-sample input history. Both taps
// read before the current input is stored. The same storage/FP rules apply.
bool TryProcessDualTapDelay(DualTapDelayState& state, std::span<float, 1024> history,
    std::span<const float, kDelayBlockSamples> input,
    std::span<float, kDelayBlockSamples + 1> first,
    std::span<float, kDelayBlockSamples + 1> second);
}
