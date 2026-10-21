#pragma once
#include <cstdint>

namespace n3::graphics {
struct PresentPolicy { uint32_t interval, flags; };
constexpr PresentPolicy HostPresentPolicy(bool vsync, bool tearing_supported) {
  // DXGI_PRESENT_RESTART=4, DO_NOT_WAIT=8, ALLOW_TEARING=512.
  // The UI thread must stay responsive when the presentation queue is full.
  return vsync ? PresentPolicy{1, 8} : PresentPolicy{0, 4u | (tearing_supported ? 512u : 0u)};
}
}
