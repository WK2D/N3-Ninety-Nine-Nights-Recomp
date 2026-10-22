#pragma once
#include "native/frame_clock.h"
#include "guest_memory_address.h"
#include <rex/memory/utils.h>

namespace n3::guest {
// Verified layout for executable 0.0.0.3, clock initializer 0x82248D38.
// This view does not own guest memory. Call it on the game's clock thread,
// after validating the complete 48-byte span with FitsLinearGuestRange.
class FrameClockView {
 public:
  FrameClockView(uint8_t* base, uint32_t address) : memory_(GuestHostAddress(base, address)) {}

  native::FrameClock Read() const {
    return {Load<uint32_t>(4), Load<uint32_t>(8), Load<uint64_t>(16),
            Load<uint64_t>(24), Load<uint64_t>(40)};
  }
  void Write(const native::FrameClock& clock) const {
    Store(4, clock.frames_per_second);
    Store(8, clock.step_seconds_bits);
    Store(16, clock.ticks_per_second);
    Store(24, clock.frame_started_at);
    Store(40, clock.ticks_per_frame);
  }
  void Enable() const { memory_[1] = 1; }

 private:
  template<class T> T Load(uint32_t offset) const {
    return rex::memory::load_and_swap<T>(memory_ + offset);
  }
  template<class T> void Store(uint32_t offset, T value) const {
    rex::memory::store_and_swap<T>(memory_ + offset, value);
  }
  uint8_t* memory_;
};
}
