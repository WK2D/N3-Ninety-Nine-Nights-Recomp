#include "chance_roll.h"

namespace n3::native {
uint32_t AdvanceRandomSequence(RandomSequenceState& state, RandomSequenceParameters parameters) {
  state.value = state.value * parameters.multiplier + parameters.increment;
  return state.value;
}
ChanceDecision RollChance(RandomSequenceState& state, ChanceThresholds thresholds,
    RandomSequenceParameters parameters) {
  const bool alternate = thresholds.alternate_enabled && thresholds.alternate > 0;
  const auto threshold = alternate ? thresholds.alternate : thresholds.normal;
  const auto sample = AdvanceRandomSequence(state, parameters) % 100;
  return {sample, threshold, alternate, int32_t(sample) < threshold};
}
}
