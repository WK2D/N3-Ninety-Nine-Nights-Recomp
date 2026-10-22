#include "native/chance_roll.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace n3::native;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
  try {
    RandomSequenceState state{0};
    Require(AdvanceRandomSequence(state) == 1 && AdvanceRandomSequence(state) == 134775814,
            "Known sequence prefix changed");
    for (uint32_t seed : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
      for (auto parameters : std::array<RandomSequenceParameters, 4>{{{}, {1, 0}, {0, 99}, {0xffffffff, 17}}}) {
        uint32_t reference = seed; state = {seed};
        for (uint32_t i = 0; i < 4096; ++i) {
          reference = uint32_t(uint64_t(reference) * parameters.multiplier + parameters.increment);
          Require(AdvanceRandomSequence(state, parameters) == reference, "Sequence wrap or editable parameters changed");
        }
      }
    }
    constexpr std::array<int32_t, 10> thresholds{std::numeric_limits<int32_t>::min(), -100, -1, 0, 1, 49, 99, 100, 101,
        std::numeric_limits<int32_t>::max()};
    uint32_t comparisons = 0;
    for (auto normal : thresholds) for (auto alternate : thresholds) for (bool enabled : {false, true})
      for (uint32_t sample = 0; sample < 100; ++sample) {
        state = {0xabcdef01};
        const auto decision = RollChance(state, {normal, alternate, enabled}, {0, sample});
        const bool use_alternate = enabled && alternate > 0;
        const auto selected = use_alternate ? alternate : normal;
        Require(state.value == sample && decision.sample == sample && decision.threshold == selected &&
                decision.used_alternate == use_alternate && decision.accepted == (int32_t(sample) < selected),
                "Threshold selection, signed comparison or explicit draw changed");
        ++comparisons;
      }
    for (auto threshold : thresholds) {
      state = {0xffffffff};
      const auto before = state.value;
      RollChance(state, {threshold, 0, false});
      Require(state.value == uint32_t(uint64_t(before) * 134775813 + 1),
              "Guaranteed chance must still advance its sequence");
    }
    state = {7}; uint32_t replay = 7;
    for (uint32_t i = 0; i < 100000; ++i) {
      replay = uint32_t(uint64_t(replay) * 134775813 + 1);
      const auto decision = RollChance(state, {37, 63, bool(i & 1)});
      Require(state.value == replay && decision.sample == replay % 100 &&
              decision.accepted == (replay % 100 < (i & 1 ? 63u : 37u)), "Long replay drifted");
    }
    std::cout << comparisons << " editable threshold cases, sequence wrap/parameters and 100000-roll replay passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
