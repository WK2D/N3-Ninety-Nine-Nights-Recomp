#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace n3::native {
inline constexpr uint32_t kStatePayloadSize = 100;
// Groups are named by byte position in the verified record. Their gameplay
// meanings are not established. Integer words remain uninterpreted 32-bit data.
struct StatePayload {
  uint32_t word0;
  std::array<uint32_t, 7> words4;
  std::array<float, 3> floats32;
  uint32_t word44;
  std::array<float, 2> floats48;
  std::array<uint32_t, 7> words56;
  std::array<float, 3> floats84;
  uint32_t word96;
};
static_assert(sizeof(StatePayload) == kStatePayloadSize);
static_assert(offsetof(StatePayload, words4) == 4 && offsetof(StatePayload, floats32) == 32 &&
    offsetof(StatePayload, word44) == 44 && offsetof(StatePayload, floats48) == 48 &&
    offsetof(StatePayload, words56) == 56 && offsetof(StatePayload, floats84) == 84 &&
    offsetof(StatePayload, word96) == 96);
// Native copies preserve every integer bit and signed zero without arithmetic.
// Normal/zero floats are supported; exceptional/subnormal guest transfers retain
// the original load/store conversions. Caller provides valid, disjoint storage;
// overlapping records (including self-copy) reject without publishing changes.
bool SupportsStatePayload(const StatePayload& payload);
bool TryCopyStatePayload(const StatePayload& source, StatePayload& destination);
}
