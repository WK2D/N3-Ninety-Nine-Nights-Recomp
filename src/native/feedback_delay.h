#pragma once
#include <cstdint>
#include <span>

namespace n3::native {
struct FeedbackDelayState {
  uint32_t delay;
  float output_gain, feedback_gain, previous_output;
  uint32_t cursor;
};
// Processes 256 inputs in buffer[0..255], replacing buffer with the previous
// output prefix and 256 new outputs. History stores the feedback-stage results.
// Only cursor zero is supported: the original writes a contiguous full history.
// Delay wraps modulo 256; zero delay reads old history before writing feedback.
// Valid storage must be disjoint. Caller provides its rounding mode, disabled
// scalar flush and masked traps. Rejection leaves state/history/buffer unchanged,
// though FP status can change during a rejected trial calculation.
bool TryProcessFeedbackDelay(FeedbackDelayState& state, std::span<float, 256> history,
                            std::span<float, 257> buffer);
}
