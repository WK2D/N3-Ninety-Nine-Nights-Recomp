#include "state_queries.h"

namespace n3::native {
KindFlagDecision QueryKindFlags(KindFlags flags, uint32_t mask) {
  const auto bits = flags.value & mask;
  return {bits, bits != 0};
}
bool SameStateIdentity(StateIdentity first, StateIdentity second) {
  return first.tag == second.tag && first.value == second.value;
}
bool CategoryAllowsKindFlags(int32_t category) { return category != 1024; }
uint8_t SelectStateByte(uint8_t request, uint8_t guard, uint8_t stored) {
  return request && !guard ? uint8_t{1} : stored;
}
}
