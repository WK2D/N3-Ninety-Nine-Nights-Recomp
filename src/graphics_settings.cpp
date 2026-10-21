#include "graphics_settings.h"
#include "frame_rate.h"
#include "display_settings.h"
#include <rex/cvar.h>
#include <array>

bool ApplyGraphicsChoice(uint32_t row, uint32_t index) {
  constexpr std::array<uint32_t, 3> rates{30, 40, 60};
  constexpr std::array<int, 6> filters{-1, 0, 2, 3, 4, 5};
  constexpr std::array<uint32_t, 4> heights{720, 1080, 1440, 2160};
  if (row == 1 && index < rates.size()) return SetFrameRate(rates[index]);
  if (row == 2 && index < filters.size())
    return rex::cvar::SetFlagByName("anisotropic_override", std::to_string(filters[index]));
  if (row == 3 && index < heights.size()) return SetDisplayHeight(heights[index]);
  if (row == 4 && index < 3) {
    const auto scale = std::to_string(index + 1);
    for (const auto* flag : {"draw_resolution_scale_x", "draw_resolution_scale_y", "resolution_scale", "n3_live_resolution_request"})
      if (!rex::cvar::SetFlagByName(flag, scale)) return false;
    return true;
  }
  return row == 5 && index < 2 && rex::cvar::SetFlagByName("n3_vsync", index ? "true" : "false");
}
