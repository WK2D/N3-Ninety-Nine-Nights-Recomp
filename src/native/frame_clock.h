#pragma once
#include <cstdint>

namespace n3::native {
// Rates are frames/second; all integer time values use the game's timebase.
// step_seconds_bits preserves the original float exactly when restoring a rate.
struct FrameClock {
  uint32_t frames_per_second = 0;
  uint32_t step_seconds_bits = 0;
  uint64_t ticks_per_second = 0;
  uint64_t frame_started_at = 0;
  uint64_t ticks_per_frame = 0;
};

// Only the understood 30/40/60 Hz clocks use the native implementation. Other
// clock modes retain the translated routine, including its exception behavior.
bool InitializeFrameClock(FrameClock& clock, uint64_t frequency, uint64_t now);

// Reconfigure at a frame boundary. Preserve a suspended clock's enable flag in
// the guest adapter; this operation only changes the fields represented above.
bool ChangeFrameClockRate(FrameClock& clock, uint32_t fps,
                          uint32_t step_seconds_bits, uint64_t now);
}
