#include "frame_countdown.h"
#include <bit>
#pragma STDC FENV_ACCESS ON
namespace n3::native {
bool CountdownNeedsProgress(const FrameCountdown& countdown) {
  return countdown.active_id != 0xffffffffu &&
      std::bit_cast<int32_t>(countdown.frames_remaining - 1) > 0;
}
CountdownOutcome AdvanceFrameCountdown(FrameCountdown& countdown) {
  if (countdown.active_id == 0xffffffffu) return CountdownOutcome::Inactive;
  const auto previous = countdown.frames_remaining--;
  if (std::bit_cast<int32_t>(countdown.frames_remaining) <= 0) {
    countdown.active_id = 0xffffffffu;
    return CountdownOutcome::Expired;
  }
  const float numerator = static_cast<float>(double(std::bit_cast<int32_t>(previous)));
  const float denominator = static_cast<float>(double(std::bit_cast<int32_t>(countdown.total_frames)));
  const float progress = static_cast<float>(double(numerator) / double(denominator));
  countdown.progress_bits = std::bit_cast<uint32_t>(progress);
  return CountdownOutcome::Advanced;
}
}
