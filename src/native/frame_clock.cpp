#include "frame_clock.h"
#include "frame_rate_policy.h"

namespace n3::native {
bool InitializeFrameClock(FrameClock& clock, uint64_t frequency, uint64_t now) {
  if (!frames::SupportsClock(clock.frames_per_second)) return false;
  clock.ticks_per_second = frequency;
  clock.frame_started_at = now;
  clock.ticks_per_frame = frequency / clock.frames_per_second;
  return true;
}

bool ChangeFrameClockRate(FrameClock& clock, uint32_t fps,
                          uint32_t step_seconds_bits, uint64_t now) {
  if (!clock.ticks_per_second || !frames::SupportsClock(fps)) return false;
  clock.frames_per_second = fps;
  clock.step_seconds_bits = step_seconds_bits;
  clock.ticks_per_frame = clock.ticks_per_second / fps;
  clock.frame_started_at = now;
  return true;
}
}
