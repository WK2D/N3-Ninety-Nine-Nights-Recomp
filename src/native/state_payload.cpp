#include "state_payload.h"
#include <algorithm>
#include <bit>
#include <cstddef>

namespace n3::native {
namespace {
bool Supported(float value) {
  const auto bits = std::bit_cast<uint32_t>(value) & 0x7fffffffu;
  return bits < 0x7f800000u && (bits == 0 || bits >= 0x00800000u);
}
}
bool SupportsStatePayload(const StatePayload& payload) {
  return std::all_of(payload.floats32.begin(), payload.floats32.end(), Supported) &&
      std::all_of(payload.floats48.begin(), payload.floats48.end(), Supported) &&
      std::all_of(payload.floats84.begin(), payload.floats84.end(), Supported);
}
bool TryCopyStatePayload(const StatePayload& source, StatePayload& destination) {
  const auto a = reinterpret_cast<uintptr_t>(&source), b = reinterpret_cast<uintptr_t>(&destination);
  if ((a <= b ? b - a : a - b) < sizeof(StatePayload) || !SupportsStatePayload(source)) return false;
  destination = source;
  return true;
}
}
