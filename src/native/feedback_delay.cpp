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
template<size_t Count> bool PairwiseSeparate(const std::array<Storage, Count>& storage) {
  for (size_t first = 0; first < Count; ++first)
    for (size_t second = first + 1; second < Count; ++second)
      if (!Separate(storage[first], storage[second])) return false;
  return true;
}
float MultiplyAdd(float first, float second, float third) {
  return static_cast<float>(std::fma(double(first), double(second), double(third)));
}
template<size_t HistorySize>
bool Calculate(const FeedbackDelayState& state, std::span<const float, HistorySize> history,
               std::span<const float, 256> input, std::array<float, HistorySize>& next_history,
               std::array<float, 257>& next_buffer) {
  static_assert(HistorySize == 128 || HistorySize == 256 || HistorySize == 512);
  const bool supported_cursor = state.cursor == 0 || (HistorySize == 512 && state.cursor == 256);
  if (!supported_cursor || !SupportedSource(state.output_gain) ||
      !SupportedSource(state.feedback_gain) || !SupportedSource(state.previous_output) ||
      !std::all_of(history.begin(), history.end(), SupportedSource) ||
      !std::all_of(input.begin(), input.end(), SupportedSource)) return false;
  std::copy(history.begin(), history.end(), next_history.begin());
  next_buffer[0] = state.previous_output;
  for (uint32_t index = 0; index < 256; ++index) {
    const auto position = state.cursor + index;
    const auto delayed = next_history[(position - state.delay) & (HistorySize - 1)];
    const auto feedback = MultiplyAdd(state.feedback_gain, delayed, input[index]);
    const auto output = MultiplyAdd(state.output_gain, feedback, delayed);
    // Feedback may overflow or make later arithmetic exceptional. Publish
    // nothing unless the whole block succeeds. Generated subnormals are valid.
    if (!Finite(feedback) || !Finite(output)) return false;
    next_history[position & (HistorySize - 1)] = feedback;
    next_buffer[index + 1] = output;
  }
  return true;
}
template<size_t HistorySize>
bool Process(FeedbackDelayState& state, std::span<float, HistorySize> history,
             std::span<float, 257> buffer) {
  const std::array<Storage, 3> storage{{{reinterpret_cast<uintptr_t>(&state), sizeof(state)},
      {reinterpret_cast<uintptr_t>(history.data()), history.size_bytes()},
      {reinterpret_cast<uintptr_t>(buffer.data()), buffer.size_bytes()}}};
  if (!PairwiseSeparate(storage)) return false;
  std::array<float, HistorySize> next_history;
  std::array<float, 257> next_buffer;
  if (!Calculate<HistorySize>(state, history, buffer.first<256>(), next_history, next_buffer)) return false;
  std::copy(next_history.begin(), next_history.end(), history.begin());
  std::copy(next_buffer.begin(), next_buffer.end(), buffer.begin());
  state.previous_output = next_buffer.back();
  state.cursor = (state.cursor + 256) & (HistorySize - 1);
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
bool TryProcessFeedbackDelay(FeedbackDelayState& state, std::span<float, 512> history,
                            std::span<float, 257> buffer) {
  return Process(state, history, buffer);
}
bool TryProcessFeedbackDelaySeparate(FeedbackDelayState& state, std::span<float, 512> history,
                                    std::span<const float, 256> input,
                                    std::span<float, 256> output, float& final_output) {
  const std::array<Storage, 5> storage{{{reinterpret_cast<uintptr_t>(&state), sizeof(state)},
      {reinterpret_cast<uintptr_t>(history.data()), history.size_bytes()},
      {reinterpret_cast<uintptr_t>(input.data()), input.size_bytes()},
      {reinterpret_cast<uintptr_t>(output.data()), output.size_bytes()},
      {reinterpret_cast<uintptr_t>(&final_output), sizeof(final_output)}}};
  if (!PairwiseSeparate(storage)) return false;
  std::array<float, 512> next_history;
  std::array<float, 257> next_buffer;
  if (!Calculate<512>(state, history, input, next_history, next_buffer)) return false;
  std::copy(next_history.begin(), next_history.end(), history.begin());
  std::copy_n(next_buffer.begin(), output.size(), output.begin());
  final_output = next_buffer.back();
  state.previous_output = final_output;
  state.cursor = (state.cursor + 256) & 511;
  return true;
}
}
