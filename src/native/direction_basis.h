#pragma once
#include "direction_math.h"
#include <cstdint>

namespace n3::native {
struct DirectionBasisState { Vec3 value; };
struct DirectionBasisParameters {
  int32_t horizontal, vertical;
  float lower_bound, unit, threshold;
  bool basis_present;
  Vec3 basis;
  float input_w, basis_w, output_w;
};
enum class DirectionBasisBranch { Unchanged, Default, Basis };
struct DirectionBasisUpdate {
  Vec3 previous, result;
  Vec4 rounded_input;
  DirectionNormalization input_normalization;
  DirectionSquaredLength post_normalization_length;
  DirectionBasisBranch branch;
  Vec4 basis_input;
  DirectionNormalization basis_normalization;
  Vec3 cross_first, cross_second, cross_difference, right;
  Vec3 lateral_products, forward_products, combined;
  DirectionNormalization output_normalization;
};
// Normalize {rounded signed horizontal, lower_bound, rounded signed vertical},
// then compare its newly computed squared length with the scalar lower_bound.
// The unchanged branch preserves state and does not inspect unit/basis/basis_w/
// output_w. A missing basis writes {lower_bound,lower_bound,unit}. Otherwise
// normalize {basis.x,lower_bound,basis.z}, derive right=up cross basis with
// up={lower_bound,unit,lower_bound}, combine the two input axes and normalize
// the result. basis.y is never interpreted. Fourth lanes are passed only to
// the corresponding normalization; they do not contribute to lengths or cross.
//
// Vector stages explicitly DAZ-flush signed subnormals and reject exceptional
// or mathematically tiny nonzero trial stages like direction_math. Cross uses
// separately rounded products, second-first subtraction, then a sign-bit flip.
// Scalar products and sums use double intermediates rounded separately to
// float, preserve generated subnormals, and retain the original operand order.
// Caller supplies rounding, disabled scalar flush and masked FP exceptions.
// State and details publish only on success; rejected trials may change FP status.
bool TryUpdateDirectionBasis(DirectionBasisState& state,
    const DirectionBasisParameters& parameters, DirectionBasisUpdate& update);

// Prepare only the normalized integer input and its length comparison. The
// branch is Unchanged or Default (an active marker awaiting composition). No
// unit, basis_present, basis, basis_w or output_w operand is inspected here, so
// guest adapters can defer dependent pointer/constant loads until it is active.
bool TryPrepareDirectionInput(const DirectionBasisState& state,
    const DirectionBasisParameters& parameters, DirectionBasisUpdate& preparation);
// Identical input phase with integer conversions already rounded by the caller.
// Guest adapters use this when those scalar conversions observe initial host
// rounding before normalization installs the guest vector rounding controls.
bool TryPrepareRoundedDirectionInput(const DirectionBasisState& state,
    Vec4 rounded_input, float lower_bound, float threshold, DirectionBasisUpdate& preparation);
// Compose a successful input preparation through Default or the Basis combined
// vector. Unchanged ignores all parameters. The caller retains the initial
// lower_bound/threshold and supplies the selected basis/unit inputs only now.
// Rejection leaves preparation unchanged.
bool TryComposeDirectionBasis(const DirectionBasisParameters& parameters,
    DirectionBasisUpdate& preparation);

// Prepare every stage through the combined vector without changing state or
// doing final normalization. On the Basis branch result retains previous until
// Finish succeeds. An adapter can then reconstruct the fourth lane from the
// combined stores and aligned source loads, including an unaligned source's
// wrapped lane. input_w/basis_w are consumed here; output_w is consumed only by
// the convenience Update operation above. Rejection leaves preparation intact.
bool TryPrepareDirectionBasis(const DirectionBasisState& state,
    const DirectionBasisParameters& parameters, DirectionBasisUpdate& preparation);
// Complete a successful preparation. Unchanged preserves state and Default
// publishes its prepared result without interpreting threshold/output_w. Basis
// normalizes combined using the supplied final raw lane. A rejected finish
// changes neither state nor preparation, including any exposed trial details.
bool TryFinishDirectionBasis(DirectionBasisState& state,
    DirectionBasisUpdate& preparation, float threshold, float output_w);
}
