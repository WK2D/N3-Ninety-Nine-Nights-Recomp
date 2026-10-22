#pragma once
#include <cstdint>

namespace n3::native {
// Names describe the verified arithmetic and optional accounting, without
// assigning gameplay units or a meaning to the surrounding object class.
struct CappedAccumulatorParameters {
  int32_t percentage_adjustment;
  float percentage_unit, baseline;
  int32_t upper_cap = 999999;
};
struct CappedAccumulatorState {
  uint32_t value, tracked_total, tracked_count;
};
struct CappedAccumulatorUpdate {
  float rounded_delta, rounded_percentage, factor, scaled_delta;
  uint32_t scaled_delta_bits;
  uint64_t value_sum, tracked_total_sum, tracked_count_sum;
  bool upper_clamped;
};
// Round both signed integer sources to float32, compute a rounded double FMA
// for the percentage/baseline factor, then a rounded product. Conversion truncates
// toward zero, saturating positive results at INT_MAX. Negative overflow and
// nonfinite trial results reject. The value sum wraps to 32 bits before a signed
// upper-cap comparison; no lower clamp occurs. Enabled accounting adds the
// zero-extended converted bits and increments the count, retaining both 64-bit
// sums in update while storing their low words in state.
// Caller supplies its rounding mode, disabled scalar flush and masked traps.
// Exceptional/subnormal scalar sources reject. Rejection changes neither state
// nor update, although a trial can change FP status. Guest adapters must retain
// the original read/write ordering for aliases or delegate to the translation.
bool TryAccumulateCappedValue(CappedAccumulatorState& state,
    const CappedAccumulatorParameters& parameters, int32_t delta,
    bool track, CappedAccumulatorUpdate& update);
}
