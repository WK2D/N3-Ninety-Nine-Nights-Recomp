#include "feedback_delay.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
bool SupportedSource(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
bool Finite(float value) { return (std::bit_cast<uint32_t>(value) & 0x7f800000u) != 0x7f800000u; }
struct Storage { uintptr_t start; size_t size; };
bool Separate(Storage a, Storage b) {
  return a.start <= b.start ? b.start - a.start >= a.size : a.start - b.start >= b.size;
}
float MultiplyAdd(float first, float second, float third) {
  return static_cast<float>(std::fma(double(first), double(second), double(third)));
}
template<size_t HistorySize>
bool Process(FeedbackDelayState& state, std::span<float, HistorySize> history,
             std::span<float, 257> buffer) {
  static_assert(HistorySize == 128 || HistorySize == 256);
  const std::array<Storage, 3> storage{{{reinterpret_cast<uintptr_t>(&state), sizeof(state)},
      {reinterpret_cast<uintptr_t>(history.data()), history.size_bytes()},
      {reinterpret_cast<uintptr_t>(buffer.data()), buffer.size_bytes()}}};
  if (state.cursor || !Separate(storage[0], storage[1]) || !Separate(storage[0], storage[2]) ||
      !Separate(storage[1], storage[2]) || !SupportedSource(state.output_gain) ||
      !SupportedSource(state.feedback_gain) || !SupportedSource(state.previous_output) ||
      !std::all_of(history.begin(), history.end(), SupportedSource) ||
      !std::all_of(buffer.begin(), buffer.begin() + 256, SupportedSource)) return false;
  std::array<float, HistorySize> next_history;
  std::array<float, 257> next_buffer;
  std::copy(history.begin(), history.end(), next_history.begin());
  next_buffer[0] = state.previous_output;
  for (uint32_t index = 0; index < 256; ++index) {
    const auto delayed = next_history[(index - state.delay) & (HistorySize - 1)];
    const auto feedback = MultiplyAdd(state.feedback_gain, delayed, buffer[index]);
    const auto output = MultiplyAdd(state.output_gain, feedback, delayed);
    // Feedback may overflow or make later arithmetic exceptional. Publish
    // nothing unless the whole block succeeds. Generated subnormals are valid.
    if (!Finite(feedback) || !Finite(output)) return false;
    next_history[index & (HistorySize - 1)] = feedback;
    next_buffer[index + 1] = output;
  }
  std::copy(next_history.begin(), next_history.end(), history.begin());
  std::copy(next_buffer.begin(), next_buffer.end(), buffer.begin());
  state.previous_output = next_buffer.back();
  state.cursor = 0;
  return true;
}
}
bool TryProcessFeedbackDelay(FeedbackDelayState& state, std::span<float, 256> history,
                            std::span<float, 257> buffer) {
  return Process(state, history, buffer);
}
bool TryProcessFeedbackDelay(FeedbackDelayState& state, std::span<float, 128> history,
                            std::span<float, 257> buffer) {
  return Process(state, history, buffer);
}
}
