#pragma once
#include <cstdint>
namespace n3::native {
struct FrameCountdown {
  uint32_t active_id, frames_remaining, total_frames, progress_bits;
};
enum class CountdownOutcome { Inactive, Expired, Advanced };
bool CountdownNeedsProgress(const FrameCountdown& countdown);
// This is a frame counter, not elapsed-time integration. Preserve wraparound,
// signed comparisons and the original pre-decrement progress numerator.
// Progress division uses caller rounding with host FP exceptions masked; a zero
// duration is deliberately not clamped. Ownership stays with the update thread.
CountdownOutcome AdvanceFrameCountdown(FrameCountdown& countdown);
}
