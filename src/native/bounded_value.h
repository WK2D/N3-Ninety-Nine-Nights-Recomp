#pragma once
#include <cstdint>

namespace n3::native {
// These names describe the verified table/value operations. Their surrounding
// gameplay class and the meaning of individual table channels are not assumed.
struct ValueLimitParameters {
  int32_t base_value, percentage_adjustment;
  uint32_t additive_offset;
  float percentage_unit, baseline;
};
struct ValueLimitResult {
  uint32_t truncated_bits;
  uint64_t value;
};
// Only kind 1000 uses the additional channel. The sum wraps at 32 bits and its
// signed value is capped at 8 when at least 9; negative/wrapped values survive.
// A caller must separately validate the resulting index against its table.
uint32_t SelectValueLimitChannel(uint32_t kind, uint32_t base_channel, uint32_t extra_channel);

// The caller selects a signed record value and supplies the original constants.
// Integer sources round to float32 before the fused percentage/baseline stage.
// The final product rounds to float32 before truncation toward zero. Positive
// values saturate at INT_MAX; truncated bits and additive_offset are then both
// zero-extended before their 64-bit addition. This deliberately retains carry.
// Normal/zero scalar sources and finite trial results are supported. A negative
// product below INT_MIN retains the guest conversion's exceptional path.
// Caller provides its rounding mode, disabled scalar flush and masked traps.
// Rejection leaves result unchanged, though trial FP status may change.
bool TryComputeValueLimit(const ValueLimitParameters& parameters, ValueLimitResult& result);

struct BoundedValueState { float value; };
struct BoundedValueUpdate {
  float sum, lower_result, upper_bound;
  bool lower_clamped, upper_clamped;
};
// Round the signed integer delta to float32, then add it to the current value
// with another float32 rounding. Apply the lower comparison first, then compare
// against the rounded signed upper limit. A negative upper limit can therefore
// replace the lower-clamped result; this is intentionally not std::clamp.
// The same FP prerequisites apply. Exceptional/subnormal sources or nonfinite
// trial arithmetic reject without changing state or update.
bool TryAdjustBoundedValue(BoundedValueState& state, int32_t delta, int32_t upper_limit,
                           float lower_bound, BoundedValueUpdate& update);
}
