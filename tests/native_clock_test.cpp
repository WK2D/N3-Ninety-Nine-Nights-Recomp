#include "native/frame_clock.h"
#include "frame_rate_policy.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
bool Equal(const n3::native::FrameClock& a, const n3::native::FrameClock& b) {
  return a.frames_per_second == b.frames_per_second && a.step_seconds_bits == b.step_seconds_bits &&
      a.ticks_per_second == b.ticks_per_second && a.frame_started_at == b.frame_started_at &&
      a.ticks_per_frame == b.ticks_per_frame;
}
int main() {
  try {
    for (uint32_t fps : {30u, 40u, 60u}) {
      for (uint64_t frequency : std::array<uint64_t, 5>{0, 1, 1000, 50000000, 0xffffffff}) {
        n3::native::FrameClock clock{fps, 0x3ccccccdu};
        Require(n3::native::InitializeFrameClock(clock, frequency, 123456789), "Known clock rejected");
        Require(clock.ticks_per_second == frequency && clock.frame_started_at == 123456789 &&
                clock.ticks_per_frame == frequency / fps, "Clock units or truncation changed");
        Require(clock.step_seconds_bits == 0x3ccccccdu, "Initializer overwrote original step bits");
        if (!frequency) {
          const auto before = clock;
          Require(!n3::native::ChangeFrameClockRate(clock, 60, 0, 999) && Equal(clock, before),
                  "Unavailable timebase must leave the clock untouched");
        }
      }
    }
    n3::native::FrameClock clock{40, n3::frames::StepSecondsBits(40)};
    Require(n3::native::InitializeFrameClock(clock, 50000000, 17), "Clock init failed");
    for (uint32_t fps : {60u, 30u, 40u, 60u, 40u, 30u}) {
      const auto step = n3::frames::StepSecondsBits(fps);
      Require(n3::native::ChangeFrameClockRate(clock, fps, step, 987654321), "Live rate rejected");
      Require(clock.frames_per_second == fps && clock.step_seconds_bits == step &&
              clock.ticks_per_frame == 50000000 / fps && clock.frame_started_at == 987654321,
              "Live rate did not reset its own deadline");
    }
    for (uint32_t invalid : {0u, 1u, 24u, 120u, std::numeric_limits<uint32_t>::max()}) {
      const auto before = clock;
      Require(!n3::native::ChangeFrameClockRate(clock, invalid, 0, 0) && Equal(clock, before),
              "Invalid live rate mutated state");
      auto unknown = clock;
      unknown.frames_per_second = invalid;
      const auto unchanged = unknown;
      Require(!n3::native::InitializeFrameClock(unknown, 999, 0) && Equal(unknown, unchanged),
              "Unknown clock must remain on the translated path");
    }
    std::cout << "Native clock units, integer periods, original step bits, live changes and rejection passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
