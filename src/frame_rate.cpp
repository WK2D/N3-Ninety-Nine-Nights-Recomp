#include "frame_rate.h"
#include "frame_rate_policy.h"
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <chrono>

REXCVAR_DEFINE_BOOL(n3_60fps, false, "N3/Graphics",
    "Experimental 60 FPS: adjusts frame timing and presentation; restart after changing")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(frame_pacing_trace, false, "N3/Graphics",
    "Log guest presentation counts and intervals for development").debug_only();

namespace {
bool use_60fps = false;
uint32_t Read32(uint8_t* base, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(base + address);
}
void Write32(uint8_t* base, uint32_t address, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(base + address, value);
}
void TracePresent() {
  if (!REXCVAR_GET(frame_pacing_trace)) return;
  thread_local uint64_t frames = 0;
  thread_local auto start = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  ++frames;
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
  if (elapsed >= 2000000) {
    REXLOG_INFO("N3 frame pacing: {} presentations in {} us", frames, elapsed);
    start = now;
    frames = 0;
  }
}
}

// Strong definitions replace the generated weak entries for both direct and
// indirect calls; __imp__ always retains the original translated implementation.
extern "C" void __imp__sub_82248D38(PPCContext& ctx, uint8_t* base);
extern "C" void sub_82248D38(PPCContext& ctx, uint8_t* base) {
  const auto clock = ctx.r3.u32;
  if (use_60fps) {
    const auto native_fps = Read32(base, clock + 4);
    if (n3::frames::SupportsClock(native_fps)) {
      Write32(base, clock + 4, n3::frames::kTargetFps);
      Write32(base, clock + 8, n3::frames::kStepSecondsBits);
      REXLOG_INFO("N3 60 FPS: frame clock set to 60 Hz with a 1/60-second simulation step");
    } else {
      REXLOG_WARN("N3 60 FPS: unexpected frame clock {}; keeping the game's clock", native_fps);
    }
  }
  // Recomputes frequency / FPS and initializes the guest deadline. Do not scale
  // the host/guest clocks globally: audio and movie playback use those clocks.
  __imp__sub_82248D38(ctx, base);
}

extern "C" void __imp__sub_823AA3B0(PPCContext& ctx, uint8_t* base);
extern "C" void sub_823AA3B0(PPCContext& ctx, uint8_t* base) {
  const auto address = ctx.r3.u32 + n3::frames::kPresentIntervalOffset;
  const auto original = Read32(base, address);
  const auto interval = n3::frames::PresentInterval(original, use_60fps);
  if (interval != original) Write32(base, address, interval);
  __imp__sub_823AA3B0(ctx, base);
  if (interval != original) Write32(base, address, original);
}

extern "C" void __imp__sub_823AA520(PPCContext& ctx, uint8_t* base);
extern "C" void sub_823AA520(PPCContext& ctx, uint8_t* base) {
  TracePresent();
  __imp__sub_823AA520(ctx, base);
}

void InitializeFrameRate() {
  // Cache at initialization: changing the saved setting takes effect on restart.
  use_60fps = REXCVAR_GET(n3_60fps);
  REXLOG_INFO("N3 frame-rate mode: {}", use_60fps ? "60 FPS (experimental)" : "original");
}
