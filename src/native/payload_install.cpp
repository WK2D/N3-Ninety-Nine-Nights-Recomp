#include "payload_install.h"
#include <bit>

namespace n3::native {
namespace {
bool Supported(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
}
bool TryInstallStatePayload(StatePayload& state, const StatePayload& source,
                            const Vec3* optional, uint32_t own_kind,
                            PayloadRelation relation, PayloadInstallUpdate& update) {
  StatePayload next;
  if (!TryCopyStatePayload(source, next)) return false;
  if (optional) {
    if (!Supported(optional->x) || !Supported(optional->y) || !Supported(optional->z)) return false;
    next.words56[5] = 1;
    next.floats84 = {optional->x, optional->y, optional->z};
  }
  PayloadInstallUpdate details{};
  if (std::bit_cast<int32_t>(own_kind) >= 6 && relation.key_matches &&
      relation.kind == 1000 && relation.variant != 0xffffffffu) {
    details.multiplied = true;
    details.signed_product = int64_t(std::bit_cast<int32_t>(next.words4[4])) *
        int64_t(std::bit_cast<int32_t>(next.words4[5]));
    next.words4[4] = uint32_t(details.signed_product);
  }
  state = next;
  update = details;
  return true;
}
uint32_t SelectRelatedTableCategory(uint8_t own_category, bool key_matches,
                                    uint32_t related_kind) {
  return key_matches && (related_kind == 6 || related_kind == 1000) ? 2 : own_category;
}
}
