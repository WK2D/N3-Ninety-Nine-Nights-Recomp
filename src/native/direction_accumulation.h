#pragma once
#include "math_types.h"

namespace n3::native {
struct DirectionAccumulationState { Vec3 value; };
struct DirectionAccumulationParameters {
  Vec3 default_direction, override_direction;
  double amount;
  float squared_override_length, lower_bound;
};
struct DirectionAccumulationUpdate {
  Vec3 previous, direction, products, result;
  bool used_override;
};
// A caller computes the override's squared length using the separate vector
// flush/rounding contract. Choose the override when the comparison is greater
// or unordered; otherwise choose the default direction. The chosen components
// multiply the double scalar and round to float32 before separate additions.
// Preserve the original addition operand order in each branch. Caller supplies
// scalar rounding, disabled scalar flush and masked traps. Normal/zero chosen
// inputs and finite trial products/results are supported; an unselected direction
// is not inspected. A computed positive-infinite squared length is valid.
// Rejection changes neither state nor details, although trial FP status may change.
bool TryAccumulateDirection(DirectionAccumulationState& state,
    const DirectionAccumulationParameters& parameters, DirectionAccumulationUpdate& update);
}
