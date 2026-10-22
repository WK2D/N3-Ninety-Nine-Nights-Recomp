#pragma once
#include <cstdint>

namespace n3::native {
struct RandomSequenceState { uint32_t value; };
struct RandomSequenceParameters {
  uint32_t multiplier = 134775813;
  uint32_t increment = 1;
};
struct ChanceThresholds {
  int32_t normal, alternate;
  bool alternate_enabled;
};
struct ChanceDecision {
  uint32_t sample;
  int32_t threshold;
  bool used_alternate, accepted;
};
// The game's shared sequence wraps at 32 bits. It advances once per roll even
// when the selected threshold guarantees rejection or acceptance. This is a
// deterministic game sequence, not a cryptographic generator or unbiased sampler.
uint32_t AdvanceRandomSequence(RandomSequenceState& state,
    RandomSequenceParameters parameters = {});
// Use an enabled alternate only when it is positive. Keep the signed threshold
// comparison without clamping and the original modulo-100 sampling bias.
ChanceDecision RollChance(RandomSequenceState& state, ChanceThresholds thresholds,
    RandomSequenceParameters parameters = {});
}
