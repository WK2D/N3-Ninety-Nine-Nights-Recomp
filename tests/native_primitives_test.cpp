#include "native/bit_window.h"
#include "native/frame_countdown.h"
#include "native/scalar_math.h"
#include "native/vector_ops.h"
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace n3::native;
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
int main() {
  try {
    Require(AddScalar(2.5, 1.25).value == 3.75f, "Scalar sum changed");
    Require(SubtractScalar(2.5, 1.25).value == 1.25f, "Scalar difference changed");
    Require(SubtractScalar(1, 1 + 0x1p-24).value == 0, "Second operand must narrow before subtraction");
    for (bool negative : {false, true}) {
      const double tiny = negative ? -0x1p-149 : 0x1p-149;
      for (auto result : {AddScalar(tiny, 0), SubtractScalar(tiny, 0)}) {
        Require(result.flushed_subnormal, "Subnormal result was not classified");
        Require(std::bit_cast<uint32_t>(result.value) == (negative ? 0x80000000u : 0),
                "Subnormal result must become signed zero");
      }
    }
    Require(!AddScalar(1, -1).flushed_subnormal, "Exact zero is not subnormal");
    Require(std::bit_cast<uint32_t>(AddScalar(-0.0, -0.0).value) == 0x80000000u,
            "Negative zero sum changed");

    FrameCountdown countdown{17, 3, 3, 0xdeadbeef};
    Require(CountdownNeedsProgress(countdown), "Active countdown requires progress");
    Require(AdvanceFrameCountdown(countdown) == CountdownOutcome::Advanced &&
            countdown.frames_remaining == 2 && std::bit_cast<float>(countdown.progress_bits) == 1,
            "Progress must use the pre-decrement frame count");
    AdvanceFrameCountdown(countdown);
    Require(countdown.frames_remaining == 1 && std::bit_cast<float>(countdown.progress_bits) == 2.0f / 3.0f,
            "Countdown second step changed");
    const auto last_progress = countdown.progress_bits;
    Require(!CountdownNeedsProgress(countdown) && AdvanceFrameCountdown(countdown) == CountdownOutcome::Expired &&
            countdown.active_id == 0xffffffffu && countdown.frames_remaining == 0 &&
            countdown.progress_bits == last_progress, "Expiry must retain progress");
    Require(AdvanceFrameCountdown(countdown) == CountdownOutcome::Inactive &&
            countdown.frames_remaining == 0, "Inactive countdown must not change");
    countdown = {0, 0, 0, 42};
    Require(AdvanceFrameCountdown(countdown) == CountdownOutcome::Expired &&
            countdown.frames_remaining == 0xffffffffu && countdown.progress_bits == 42,
            "Zero counter must wrap before signed expiry comparison");
    countdown = {0, 0x80000000u, 1, 0};
    Require(AdvanceFrameCountdown(countdown) == CountdownOutcome::Advanced &&
            countdown.frames_remaining == 0x7fffffffu && std::bit_cast<float>(countdown.progress_bits) == -0x1p31f,
            "Signed numerator or wrapping decrement changed");
    countdown = {0, 3, 0, 0};
    AdvanceFrameCountdown(countdown);
    Require(std::isinf(std::bit_cast<float>(countdown.progress_bits)), "Zero duration must retain division behavior");

    const std::array<uint8_t, 4> bytes{0x12, 0x34, 0x56, 0x78};
    Require(ReadBitWindow(bytes, 0, 32) == 0x78563412u && ReadBitWindow(bytes, 4, 8) == 0x41,
            "Bit reader must use little-endian, LSB-first fields");
    Require(!ReadBitWindow(bytes, 8, 1) && !ReadBitWindow(bytes, 0, 0) &&
            !ReadBitWindow(bytes, 0, 33) && !ReadBitWindow(bytes, 7, 26), "Invalid bit window accepted");
    Vec3 output{99, 98, 97};
    Require(TryAccumulateVectorRatio({1, 2, 3}, {2, -4, 8}, 1, 2, output) &&
            output.x == 2 && output.y == 0 && output.z == 7, "Ratio vector update changed");
    Require(!TryAccumulateVectorRatio({}, {}, 1, 0, output) && output.x == 2 && output.z == 7,
            "Zero denominator must reject without mutation");
    Require(!TryAccumulateVectorRatio({}, {}, 0x1p1023, 0x1p-1022, output) && output.z == 7,
            "Overflowing ratio must use the original path without mutation");
    std::cout << "Countdown transitions, scalar narrowing/flush, bit fields and editable ratio updates passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
