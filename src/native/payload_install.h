#pragma once
#include "math_types.h"
#include "state_payload.h"
#include <cstdint>

namespace n3::native {
struct PayloadRelation {
  bool key_matches;
  uint32_t kind, variant;
};
struct PayloadInstallUpdate {
  bool multiplied;
  int64_t signed_product;
};
// Replace the complete payload, optionally enable and replace its vector, then
// apply the related-object multiplier for signed own_kind >= 6, a matching key,
// related kind 1000 and a variant other than 0xffffffff. The integer product is
// available in full, while the payload retains only its low 32 bits.
// Source and optional vector floats support normal values and signed zeros.
// State and details publish together only on success. Guest adapters must retain
// the original ordered copy/vector reads when they alias preceding guest writes.
bool TryInstallStatePayload(StatePayload& state, const StatePayload& source,
                            const Vec3* optional, uint32_t own_kind,
                            PayloadRelation relation, PayloadInstallUpdate& update);

// A matching related object of kind 6 or 1000 selects category 2; otherwise the
// object's original byte category is retained. No other relation field matters.
uint32_t SelectRelatedTableCategory(uint8_t own_category, bool key_matches,
                                    uint32_t related_kind);
}
