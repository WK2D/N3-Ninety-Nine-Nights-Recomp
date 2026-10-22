#pragma once
namespace n3::native {
struct ScalarResult { float value; bool flushed_subnormal; };
// Host rounding must match the caller and scalar flush must be disabled.
// The guest adapter retains exceptional/subnormal input and unmasked traps.
// These operations explicitly flush only the rounded result to signed zero.
ScalarResult AddScalar(double first, double second);
// Matches the game: narrow the second operand to float32 before subtraction.
ScalarResult SubtractScalar(double first, double second);
}
