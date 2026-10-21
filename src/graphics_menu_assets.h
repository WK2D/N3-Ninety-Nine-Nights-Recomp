#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace n3::graphics {
inline constexpr uint32_t kButtonId = 0x02901006;
inline constexpr uint32_t kTextId = 1999;
std::vector<uint8_t> ExtendTitleLayout(std::span<const uint8_t> original);
std::vector<uint8_t> BuildGraphicsLayout(std::span<const uint8_t> original);
}
