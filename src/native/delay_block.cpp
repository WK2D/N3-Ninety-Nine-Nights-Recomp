#include "delay_block.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
bool Supported(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
struct Storage { uintptr_t start; size_t size; };
bool Separate(Storage a, Storage b) {
  return a.start <= b.start ? b.start - a.start >= a.size : a.start - b.start >= b.size;
}
float Product(float first, float second) { return static_cast<float>(double(first) * double(second)); }
}
bool TryProcessDelayBlock(DelayBlockState& state, std::span<float> history,
    std::span<const float, kDelayBlockSamples> input,
    std::span<float, kDelayBlockSamples + 1> mixed,
    std::span<float, kDelayBlockSamples + 1> auxiliary) {
  if ((history.size() != 512 && history.size() != 2048) || state.cursor % kDelayBlockSamples ||
      state.cursor > history.size() - kDelayBlockSamples) return false;
  const std::array<Storage, 5> storage{{{reinterpret_cast<uintptr_t>(&state), sizeof(state)},
      {reinterpret_cast<uintptr_t>(history.data()), history.size_bytes()},
      {reinterpret_cast<uintptr_t>(input.data()), input.size_bytes()},
      {reinterpret_cast<uintptr_t>(mixed.data()), mixed.size_bytes()},
      {reinterpret_cast<uintptr_t>(auxiliary.data()), auxiliary.size_bytes()}}};
  for (size_t first = 0; first < storage.size(); ++first)
    for (size_t second = first + 1; second < storage.size(); ++second)
      if (!Separate(storage[first], storage[second])) return false;
  if (!Supported(state.previous_mixed) || !Supported(state.previous_auxiliary) ||
      !std::all_of(state.taps.begin(), state.taps.end(), [](auto tap) { return Supported(tap.gain); }) ||
      !std::all_of(history.begin(), history.end(), Supported) ||
      !std::all_of(input.begin(), input.end(), Supported)) return false;
  const uint32_t mask = static_cast<uint32_t>(history.size() - 1);
  mixed[0] = state.previous_mixed; auxiliary[0] = state.previous_auxiliary;
  for (uint32_t sample = 0; sample < kDelayBlockSamples; ++sample) {
    const auto position = state.cursor + sample;
    const auto first = history[(position - state.taps[0].delay) & mask];
    const auto second = history[(position - state.taps[1].delay) & mask];
    const auto third = history[(position - state.taps[2].delay) & mask];
    const auto weighted_second = Product(second, state.taps[1].gain);
    const auto weighted_third = Product(third, state.taps[2].gain);
    history[position] = input[sample];
    auxiliary[sample + 1] = weighted_third;
    mixed[sample + 1] = static_cast<float>(std::fma(double(first), double(state.taps[0].gain), double(weighted_second)));
  }
  state.previous_mixed = mixed[kDelayBlockSamples];
  state.previous_auxiliary = auxiliary[kDelayBlockSamples];
  state.cursor = (state.cursor + kDelayBlockSamples) & mask;
  return true;
}
bool TryProcessDualTapDelay(DualTapDelayState& state, std::span<float, 1024> history,
    std::span<const float, kDelayBlockSamples> input,
    std::span<float, kDelayBlockSamples + 1> first,
    std::span<float, kDelayBlockSamples + 1> second) {
  if (state.cursor % kDelayBlockSamples || state.cursor > 1024 - kDelayBlockSamples) return false;
  const std::array<Storage, 5> storage{{{reinterpret_cast<uintptr_t>(&state), sizeof(state)},
      {reinterpret_cast<uintptr_t>(history.data()), history.size_bytes()},
      {reinterpret_cast<uintptr_t>(input.data()), input.size_bytes()},
      {reinterpret_cast<uintptr_t>(first.data()), first.size_bytes()},
      {reinterpret_cast<uintptr_t>(second.data()), second.size_bytes()}}};
  for (size_t a = 0; a < storage.size(); ++a)
    for (size_t b = a + 1; b < storage.size(); ++b)
      if (!Separate(storage[a], storage[b])) return false;
  if (!std::all_of(state.previous.begin(), state.previous.end(), Supported) ||
      !std::all_of(state.taps.begin(), state.taps.end(), [](auto tap) { return Supported(tap.gain); }) ||
      !std::all_of(history.begin(), history.end(), Supported) ||
      !std::all_of(input.begin(), input.end(), Supported)) return false;
  first[0] = state.previous[0]; second[0] = state.previous[1];
  for (uint32_t sample = 0; sample < kDelayBlockSamples; ++sample) {
    const auto position = state.cursor + sample;
    const auto a = history[(position - state.taps[0].delay) & 1023];
    const auto b = history[(position - state.taps[1].delay) & 1023];
    first[sample + 1] = Product(a, state.taps[0].gain);
    second[sample + 1] = Product(b, state.taps[1].gain);
    history[position] = input[sample];
  }
  state.previous = {first[kDelayBlockSamples], second[kDelayBlockSamples]};
  state.cursor = (state.cursor + kDelayBlockSamples) & 1023;
  return true;
}
}
