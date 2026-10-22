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
            buffer[256] == 320.0f * block - 1 && history[(block * 256 - 1) % HistorySize] == 256.0f * block &&
            state.previous_output == buffer[256] && state.cursor == (block * 256) % HistorySize,
            "Feedback history or cached output continuity changed");
  }
  auto wrapped_state = state; wrapped_state.delay += HistorySize;
  auto wrapped_history = history; auto wrapped_buffer = buffer;
  Require(TryProcessFeedbackDelay(state, history, buffer) &&
          TryProcessFeedbackDelay(wrapped_state, wrapped_history, wrapped_buffer) &&
          BitsEqual(history, wrapped_history) && BitsEqual(buffer, wrapped_buffer), "Delay modulo changed");
  state = {0, 1, 0, 42, 0}; history.fill(2); buffer.fill(1);
  constexpr auto first_pass = std::min(HistorySize, size_t{256});
  Require(TryProcessFeedbackDelay(state, history, buffer) && buffer[0] == 42 &&
          std::all_of(buffer.begin() + 1, buffer.begin() + first_pass + 1, [](float value) { return value == 3; }) &&
          std::all_of(buffer.begin() + first_pass + 1, buffer.end(), [](float value) { return value == 2; }),
          "Delay zero must read old history before feedback storage");
  if constexpr (HistorySize == 512) {
    Require(state.cursor == 256 && std::all_of(history.begin() + 256, history.end(),
            [](float value) { return value == 2; }), "First half must leave the second half unchanged");
    buffer.fill(4);
    Require(TryProcessFeedbackDelay(state, history, buffer) && state.cursor == 0 && buffer[0] == 3 &&
            std::all_of(buffer.begin() + 1, buffer.end(), [](float value) { return value == 6; }) &&
            std::all_of(history.begin(), history.begin() + 256, [](float value) { return value == 1; }) &&
            std::all_of(history.begin() + 256, history.end(), [](float value) { return value == 4; }),
            "Second half must preserve the first half and wrap the cursor");
  }
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
void CheckSeparateOutput() {
  FeedbackDelayState state{1, 0.25f, 1, 42, 0};
  std::array<float, 512> history{};
  std::array<float, 257> input;
  std::array<float, 258> guarded_output;
  auto output = std::span<float, 256>(guarded_output.data() + 1, 256);
  for (uint32_t block = 1; block <= 8; ++block) {
    input.fill(1); guarded_output.fill(-77);
    input[256] = std::bit_cast<float>(0x7f812345u); // Unread final-output slot.
    Require(TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256),
                                            output, input[256]), "Separate-output feedback rejected");
    Require(output.front() == (block == 1 ? 42.0f : 320.0f * (block - 1) - 1) &&
            output.back() == 320.0f * block - 2.25f && input[256] == 320.0f * block - 1 &&
            state.previous_output == input[256] && state.cursor == (block * 256) % 512 &&
            history[(block * 256 - 1) % 512] == 256.0f * block,
            "Separate output prefix, final-output slot or history continuity changed");
    Require(guarded_output.front() == -77 && guarded_output.back() == -77 &&
            std::all_of(input.begin(), input.begin() + 256, [](float value) { return value == 1; }),
            "Separate output wrote outside its span or changed input samples");
  }
  auto wrapped_state = state; wrapped_state.delay += 512;
  auto wrapped_history = history; auto wrapped_input = input; auto wrapped_output = guarded_output;
  Require(TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[256]) &&
          TryProcessFeedbackDelaySeparate(wrapped_state, wrapped_history, std::span<const float, 256>(wrapped_input.data(), 256),
              std::span<float, 256>(wrapped_output.data() + 1, 256), wrapped_input[256]) &&
          BitsEqual(history, wrapped_history) && BitsEqual(input, wrapped_input) &&
          BitsEqual(guarded_output, wrapped_output), "Separate-output delay modulo changed");
  state = {0, 1, 0, 42, 0}; history.fill(2); input.fill(1);
  Require(TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[256]) &&
          output[0] == 42 && input[256] == 3 && state.cursor == 256 &&
          std::all_of(output.begin() + 1, output.end(), [](float value) { return value == 3; }) &&
          std::all_of(history.begin(), history.begin() + 256, [](float value) { return value == 1; }) &&
          std::all_of(history.begin() + 256, history.end(), [](float value) { return value == 2; }),
          "Separate-output delay zero must use old history and preserve its unused half");
  state.output_gain = 2; state.feedback_gain = 0.5f; input.fill(4);
  Require(TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[256]) &&
          output[0] == 3 && input[256] == 12 && state.cursor == 0 &&
          std::all_of(output.begin() + 1, output.end(), [](float value) { return value == 12; }) &&
          std::all_of(history.begin(), history.begin() + 256, [](float value) { return value == 1; }) &&
          std::all_of(history.begin() + 256, history.end(), [](float value) { return value == 5; }),
          "Separate-output gain edits or cursor-half wrap changed");
  state = {1, 1, 0.5f, 0, 0}; history.fill(0); input.fill(0); input[0] = 1;
  Require(TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[256]) &&
          output[0] == 0 && output[1] == 1 && output[2] == 1.5f && output[3] == 0.75f &&
          std::bit_cast<uint32_t>(history[149]) == 1 && input[256] == 0,
          "Separate-output impulse or generated subnormal arithmetic changed");
  state = {1, 0.25f, 2, 7, 256}; history.fill(0); input.fill(0); guarded_output.fill(-77);
  input[160] = std::numeric_limits<float>::max(); input[256] = std::bit_cast<float>(0x7f812345u);
  const auto before_state = state; const auto before_history = history;
  const auto before_input = input; const auto before_output = guarded_output;
  Require(!TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[256]) &&
          SameState(state, before_state) && BitsEqual(history, before_history) && BitsEqual(input, before_input) &&
          BitsEqual(guarded_output, before_output), "Separate-output late failure published partial results");
  state.cursor = 1;
  const auto invalid_state = state;
  Require(!TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[256]) &&
          SameState(state, invalid_state) && BitsEqual(history, before_history) && BitsEqual(input, before_input) &&
          BitsEqual(guarded_output, before_output), "Separate-output invalid cursor changed storage");
  state = {1, 1, 0.5f, 0, 0}; history.fill(0); input.fill(0);
  input[3] = std::numeric_limits<float>::infinity();
  Require(!TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[256]),
          "Separate-output exceptional input accepted");
  input[3] = 0; history[300] = std::bit_cast<float>(1u);
  Require(!TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[256]),
          "Separate-output subnormal history accepted");
  history.fill(1); input.fill(1); guarded_output.fill(-77);
  const auto alias_state = state; const auto alias_history = history;
  const auto alias_input = input; const auto alias_output = guarded_output;
  Require(!TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(history.data() + 256, 256), output, input[256]) &&
          !TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256),
              std::span<float, 256>(history.data(), 256), input[256]) &&
          !TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, history[511]) &&
          !TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256),
              std::span<float, 256>(input.data(), 256), input[256]) &&
          !TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, input[255]) &&
          !TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, output.back()) &&
          !TryProcessFeedbackDelaySeparate(state, history, std::span<const float, 256>(input.data(), 256), output, state.output_gain) &&
          SameState(state, alias_state) && BitsEqual(history, alias_history) && BitsEqual(input, alias_input) &&
          BitsEqual(guarded_output, alias_output), "Separate-output alias rejection changed storage");
}
int main() {
  try {
    CheckHistory<128>();
    CheckHistory<256>();
    CheckHistory<512>();
    CheckSeparateOutput();
    std::cout << "Editable feedback/delay, split output, chained history, impulse decay and transactional rejection passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
