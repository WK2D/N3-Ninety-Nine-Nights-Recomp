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
// The original writes contiguous history passes. The 128/256 variants require
// cursor zero; the 512 variant accepts cursors zero and 256, alternating halves.
// Delay wraps modulo the history size; zero delay reads history before writing.
// Valid storage must be disjoint. Caller provides its rounding mode, disabled
// scalar flush and masked traps. Rejection leaves state/history/buffer unchanged,
// though FP status can change during a rejected trial calculation.
bool TryProcessFeedbackDelay(FeedbackDelayState& state, std::span<float, 256> history,
                            std::span<float, 257> buffer);
// The 128-sample variant makes two history passes for the same 256-input block.
bool TryProcessFeedbackDelay(FeedbackDelayState& state, std::span<float, 128> history,
                            std::span<float, 257> buffer);
bool TryProcessFeedbackDelay(FeedbackDelayState& state, std::span<float, 512> history,
                            std::span<float, 257> buffer);
// Separate-output form of the 512-sample model. Output contains the cached
// prefix and the first 255 new outputs; final_output receives the 256th new
// output. Input is unchanged. In the guest layout final_output is input[256].
// State, history, input, output and final_output must be pairwise disjoint.
// The same cursor, arithmetic and transactional-rejection contract applies.
bool TryProcessFeedbackDelaySeparate(FeedbackDelayState& state, std::span<float, 512> history,
                                    std::span<const float, 256> input,
                                    std::span<float, 256> output, float& final_output);
}
