#pragma once
#include <array>
#include <cstdint>

namespace n3::graphics {
// Xenos override values: disabled anisotropy has an actual 1x sampler limit.
inline constexpr std::array<int, 5> kAnisotropicOverrides{0, 2, 3, 4, 5};
constexpr uint32_t AnisotropicChoiceIndex(int value) {
  for (uint32_t index = 0; index < kAnisotropicOverrides.size(); ++index)
    if (kAnisotropicOverrides[index] == value) return index;
  return 0;
}
}
