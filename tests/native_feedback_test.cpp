#include "native/feedback_delay.h"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace n3::native;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template<size_t N> bool BitsEqual(const std::array<float, N>& first, const std::array<float, N>& second) {
  return std::equal(first.begin(), first.end(), second.begin(), [](float a, float b) {
    return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
  });
}
bool SameState(FeedbackDelayState a, FeedbackDelayState b) {
  return a.delay == b.delay && a.cursor == b.cursor &&
      std::bit_cast<uint32_t>(a.output_gain) == std::bit_cast<uint32_t>(b.output_gain) &&
      std::bit_cast<uint32_t>(a.feedback_gain) == std::bit_cast<uint32_t>(b.feedback_gain) &&
      std::bit_cast<uint32_t>(a.previous_output) == std::bit_cast<uint32_t>(b.previous_output);
}
template<size_t HistorySize> void CheckHistory() {
  FeedbackDelayState state{1, 0.25f, 1, 42, 0};
  std::array<float, HistorySize> history{};
  std::array<float, 257> buffer{};
  for (uint32_t block = 1; block <= 8; ++block) {
    buffer.fill(1);
    Require(TryProcessFeedbackDelay(state, history, buffer), "Ordinary feedback block rejected");
    Require(buffer[0] == (block == 1 ? 42.0f : 320.0f * (block - 1) - 1) &&
            buffer[256] == 320.0f * block - 1 && history.back() == 256.0f * block &&
            state.previous_output == buffer[256] && state.cursor == 0,
            "Feedback history or cached output continuity changed");
  }
  auto wrapped_state = state; wrapped_state.delay += HistorySize;
  auto wrapped_history = history; auto wrapped_buffer = buffer;
  Require(TryProcessFeedbackDelay(state, history, buffer) &&
          TryProcessFeedbackDelay(wrapped_state, wrapped_history, wrapped_buffer) &&
          BitsEqual(history, wrapped_history) && BitsEqual(buffer, wrapped_buffer), "Delay modulo changed");
  state = {0, 1, 0, 42, 0}; history.fill(2); buffer.fill(1);
  Require(TryProcessFeedbackDelay(state, history, buffer) && buffer[0] == 42 &&
          std::all_of(buffer.begin() + 1, buffer.begin() + HistorySize + 1, [](float value) { return value == 3; }) &&
          std::all_of(buffer.begin() + HistorySize + 1, buffer.end(), [](float value) { return value == 2; }),
          "Delay zero must read old history before feedback storage");
  state = {1, 1, 0.5f, 0, 0}; history.fill(0); buffer.fill(0); buffer[0] = 1;
  buffer[256] = std::bit_cast<float>(0x7f812345u); // Unread output-only tail.
  Require(TryProcessFeedbackDelay(state, history, buffer) && buffer[1] == 1 && buffer[2] == 1.5f &&
          buffer[3] == 0.75f && buffer[256] == 0 && std::bit_cast<uint32_t>(history[149 % HistorySize]) == 1,
          "Feedback decay, generated subnormals or output-only tail changed");
  state = {1, 0.25f, 2, 7, 0}; history.fill(0); buffer.fill(0);
  buffer[160] = std::numeric_limits<float>::max();
  const auto before_state = state; const auto before_history = history; const auto before_buffer = buffer;
  Require(!TryProcessFeedbackDelay(state, history, buffer) && SameState(state, before_state) &&
          BitsEqual(history, before_history) && BitsEqual(buffer, before_buffer),
          "Late overflow must reject without publishing a partial block");
  state.feedback_gain = 0; state.cursor = 1;
  const auto invalid_state = state;
  Require(!TryProcessFeedbackDelay(state, history, buffer) && SameState(state, invalid_state) && BitsEqual(buffer, before_buffer),
          "Unsupported cursor must reject without mutation");
  state.cursor = 0; state.output_gain = std::numeric_limits<float>::infinity();
  Require(!TryProcessFeedbackDelay(state, history, buffer) && BitsEqual(history, before_history), "Exceptional gain accepted");
  state.output_gain = 1;
  history[0] = std::bit_cast<float>(1u);
  Require(!TryProcessFeedbackDelay(state, history, buffer), "Subnormal source history needs original path");
  std::array<float, 600> aliased{}; aliased.fill(1);
  Require(!TryProcessFeedbackDelay(state, std::span<float, HistorySize>(aliased.data(), HistorySize),
                                   std::span<float, 257>(aliased.data() + HistorySize / 2, 257)) &&
          std::all_of(aliased.begin(), aliased.end(), [](float value) { return value == 1; }), "Aliased storage accepted");
}
int main() {
  try {
    CheckHistory<128>();
    CheckHistory<256>();
    std::cout << "Editable feedback/delay, chained history, impulse decay and transactional rejection passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
