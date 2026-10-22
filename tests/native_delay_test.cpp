#include "native/delay_block.h"
#include <algorithm>
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace n3::native;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
int main() {
  try {
    for (uint32_t size : {512u, 2048u}) {
      std::vector<float> history(size);
      for (uint32_t index = 0; index < size; ++index) history[index] = float(index);
      std::array<float, 256> input{};
      for (uint32_t index = 0; index < input.size(); ++index) input[index] = 1000.0f + index;
      std::array<float, 257> mixed{}, auxiliary{};
      DelayBlockState state{{DelayTap{1, 0.5f}, DelayTap{2, 0.25f}, DelayTap{0, 2}}, 11, 12, 0};
      Require(TryProcessDelayBlock(state, history, input, mixed, auxiliary), "Ordinary delay block rejected");
      Require(mixed[0] == 11 && auxiliary[0] == 12, "Previous outputs must prefix the block");
      Require(mixed[1] == (size - 1) * 0.5f + (size - 2) * 0.25f &&
              mixed[2] == 500 + (size - 1) * 0.25f, "Tap order or in-block history reads changed");
      Require(auxiliary[1] == 0 && auxiliary[256] == 510, "Delay zero must read old history before writing input");
      Require(state.previous_mixed == mixed[256] && state.previous_auxiliary == auxiliary[256] && state.cursor == 256,
              "Delay state did not advance to the next block");
      for (uint32_t index = 0; index < 256; ++index) Require(history[index] == input[index], "Input history differs");
      for (uint32_t index = 256; index < size; ++index) Require(history[index] == float(index), "Untouched history changed");
      const auto previous = mixed[256];
      for (uint32_t block = 1; block < size / 256; ++block) {
        Require(TryProcessDelayBlock(state, history, input, mixed, auxiliary), "Sequential block rejected");
        if (block == 1) Require(mixed[0] == previous && mixed[1] == 941, "Output continuity changed between blocks");
      }
      Require(state.cursor == 0, "Cursor did not wrap at the history length");
      auto wrapped_state = state;
      auto wrapped_history = history;
      for (auto& tap : wrapped_state.taps) tap.delay += size;
      std::array<float, 257> wrapped_mixed{}, wrapped_auxiliary{};
      Require(TryProcessDelayBlock(state, history, input, mixed, auxiliary) &&
              TryProcessDelayBlock(wrapped_state, wrapped_history, input, wrapped_mixed, wrapped_auxiliary) &&
              mixed == wrapped_mixed && auxiliary == wrapped_auxiliary && history == wrapped_history,
              "Tap delays must wrap modulo history length");
      state.taps[0].gain = state.taps[1].gain = 0;
      Require(TryProcessDelayBlock(state, history, input, mixed, auxiliary) &&
              std::all_of(mixed.begin() + 1, mixed.end(), [](float value) { return value == 0; }),
              "Editable tap gains did not change the mixed output");
      const auto original_state = state;
      const auto original_history = history;
      mixed.fill(99); auxiliary.fill(98);
      auto rejected = [&] {
        const auto before_state = state;
        Require(!TryProcessDelayBlock(state, history, input, mixed, auxiliary), "Invalid delay block accepted");
        Require(history == original_history && std::all_of(mixed.begin(), mixed.end(), [](float value) { return value == 99; }) &&
                std::all_of(auxiliary.begin(), auxiliary.end(), [](float value) { return value == 98; }),
                "Rejected delay block partially mutated buffers");
        Require(state.cursor == before_state.cursor && state.previous_mixed == before_state.previous_mixed &&
                state.previous_auxiliary == before_state.previous_auxiliary, "Rejected delay block mutated state");
        for (uint32_t index = 0; index < 3; ++index)
          Require(state.taps[index].delay == before_state.taps[index].delay &&
                  std::bit_cast<uint32_t>(state.taps[index].gain) == std::bit_cast<uint32_t>(before_state.taps[index].gain),
                  "Rejected delay block mutated taps");
      };
      state.cursor = 1; rejected(); state = original_state;
      state.cursor = size; rejected(); state = original_state;
      input[255] = std::numeric_limits<float>::infinity(); rejected(); input[255] = 1255;
      state.taps[1].gain = std::bit_cast<float>(1u); rejected(); state = original_state;
      Require(!TryProcessDelayBlock(state, history, std::span<const float, 256>(mixed.data(), 256), mixed, auxiliary),
              "Overlapping input/output must reject before mutation");
      Require(state.cursor == original_state.cursor && history == original_history && mixed[0] == 99,
              "Aliased rejection changed state or storage");
      history[0] = std::bit_cast<float>(0x7fc12345u);
      Require(!TryProcessDelayBlock(state, history, input, mixed, auxiliary) && mixed[0] == 99,
              "Exceptional history requires the original path");
    }
    std::cout << "Delay tap edits, old-history reads, output continuity, cursor wrap and rejection passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
