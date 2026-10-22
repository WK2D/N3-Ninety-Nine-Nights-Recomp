#include "frame_rate.h"
#include "frame_rate_policy.h"
#include "guest_frame_clock.h"
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <charconv>
#include <rex/chrono/clock.h>

#ifdef N3_FRAME_RATE_TEST
extern thread_local bool n3_frame_rate_native_used;
#endif

REXCVAR_DEFINE_BOOL(n3_60fps, false, "N3/Graphics",
    "Compatibility alias for the previous 60 FPS setting").debug_only();
REXCVAR_DEFINE_UINT32(n3_frame_rate, 30, "N3/Graphics", "Frame rate")
    .allowed({"0", "30", "40", "60"});
REXCVAR_DEFINE_BOOL(frame_pacing_trace, false, "N3/Graphics",
    "Log guest presentation counts and intervals for development").debug_only();

namespace {
std::atomic<uint32_t> requested_fps{30};
std::atomic<uint32_t> active_fps{30};
uint32_t frame_clock = 0;
uint32_t native_fps = 40;
uint32_t native_step = n3::frames::StepSecondsBits(40);
uint32_t Read32(uint8_t* base, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address));
}
void Write32(uint8_t* base, uint32_t address, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address), value);
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
void PacePresent() {
  using Clock = std::chrono::steady_clock;
  thread_local uint32_t previous_target = 0;
  thread_local Clock::time_point deadline{};
  const auto target = ConfiguredFrameRate();
  if (!target) { previous_target = 0; return; }
  const auto period = std::chrono::nanoseconds(1000000000 / target);
  auto now = Clock::now();
  if (previous_target != target) {
    previous_target = target;
    deadline = now + period;
    return;
  }
  if (now < deadline) std::this_thread::sleep_until(deadline);
  now = Clock::now();
  deadline += period;
  // Movies, loading and slow frames may present less often; never catch up in a burst.
  if (now > deadline) deadline = now + period;
}
}

// Strong definitions replace the generated weak entries for both direct and
// indirect calls; __imp__ always retains the original translated implementation.
extern "C" void __imp__sub_82248D38(PPCContext& ctx, uint8_t* base);
extern "C" void sub_82248D38(PPCContext& ctx, uint8_t* base) {
  const auto clock = ctx.r3.u32;
  // The initializer reserves 112 bytes and its frequency helper another 96.
  // Preserve all prologue aliases before reading a clock snapshot.
  const auto stack = ctx.r1.u32;
  if (!n3::guest::FitsLinearGuestRange(clock, 48) || stack < 208 ||
      !n3::guest::FitsLinearGuestRange(stack - 208, 208) ||
      !n3::guest::DisjointGuestHostRanges(clock, 48, stack - 208, 208))
    return __imp__sub_82248D38(ctx, base);
  const n3::guest::FrameClockView view(base, clock);
  auto state = view.Read();
  const auto original = state.frames_per_second;
  if (n3::frames::SupportsClock(original)) {
    // Scene changes may reinitialize the already-overridden clock.
    if (clock != frame_clock) {
      frame_clock = clock;
      native_fps = original;
      native_step = state.step_seconds_bits;
    }
    const auto target = requested_fps.load();
    state.frames_per_second = target ? target : native_fps;
    state.step_seconds_bits = target ? n3::frames::StepSecondsBits(target) : native_step;
    // The pinned Xbox import returns a 32-bit frequency. Keep that behavior,
    // and keep audio/movie/global clock scaling untouched.
    const auto frequency = static_cast<uint32_t>(rex::chrono::Clock::guest_tick_frequency());
    if (n3::native::InitializeFrameClock(state, frequency, rex::chrono::Clock::QueryGuestTickCount())) {
      view.Write(state);
      view.Enable();
      active_fps.store(target);
      REXLOG_DEBUG("Native frame clock initialized: {} FPS, {} ticks/second, {} ticks/frame",
                   state.frames_per_second, state.ticks_per_second, state.ticks_per_frame);
      // The original's final timebase helper returns success in r3. Other
      // caller-saved registers are unspecified; nonvolatile state is untouched.
      ctx.r3.u64 = 1;
      ctx.lr = uint32_t(ctx.lr);
#ifdef N3_FRAME_RATE_TEST
      n3_frame_rate_native_used = true;
#endif
      return;
    }
  }
  // Keep unrecognized clock modes on the original implementation.
  __imp__sub_82248D38(ctx, base);
}

extern "C" void __imp__sub_823AA3B0(PPCContext& ctx, uint8_t* base);
extern "C" void sub_823AA3B0(PPCContext& ctx, uint8_t* base) {
  const auto address = ctx.r3.u32 + n3::frames::kPresentIntervalOffset;
  if (!n3::guest::FitsLinearGuestRange(address, 4))
    return __imp__sub_823AA3B0(ctx, base);
  const auto original = Read32(base, address);
  const auto interval = n3::frames::PresentInterval(original, ConfiguredFrameRate());
  if (interval != original) Write32(base, address, interval);
  __imp__sub_823AA3B0(ctx, base);
  if (interval != original) Write32(base, address, original);
}

extern "C" void __imp__sub_823AA520(PPCContext& ctx, uint8_t* base);
extern "C" void sub_823AA520(PPCContext& ctx, uint8_t* base) {
  PacePresent();
  TracePresent();
  __imp__sub_823AA520(ctx, base);
}

void InitializeFrameRate() {
  auto target = REXCVAR_GET(n3_frame_rate);
  if (REXCVAR_GET(n3_60fps) && rex::cvar::GetFlagSource("n3_frame_rate") == rex::cvar::Source::kDefault) {
    target = 60;
    rex::cvar::SetFlagByName("n3_frame_rate", "60");
  }
  requested_fps.store(target);
  active_fps.store(target);
  frame_clock = 0;
  REXLOG_INFO("N3 frame-rate target: {}", target);
}

uint32_t ConfiguredFrameRate() {
  const auto configured = rex::cvar::Query<uint32_t>("n3_frame_rate");
  return !configured || n3::frames::SupportsClock(configured) ? configured : requested_fps.load();
}

bool SetFrameRate(uint32_t fps) {
  if (!n3::frames::SupportsClock(fps)) return false;
  if (!rex::cvar::SetFlagByName("n3_frame_rate", std::to_string(fps))) return false;
  rex::cvar::SetFlagByName("n3_60fps", "false");
  requested_fps.store(fps);
  return true;
}

void ApplyFrameRateAtBoundary(uint8_t* base) {
  // Read through the registry so changes from either the native menu or the
  // SDK settings overlay use the same frame-boundary update.
  const auto value = rex::cvar::GetFlagByName("n3_frame_rate");
  uint32_t configured = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), configured);
  if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() &&
      (!configured || n3::frames::SupportsClock(configured))) requested_fps.store(configured);
  const auto target = requested_fps.load();
  if (!frame_clock || target == active_fps.load() ||
      !n3::guest::FitsLinearGuestRange(frame_clock, 48)) return;
  const auto fps = target ? target : native_fps;
  const n3::guest::FrameClockView view(base, frame_clock);
  auto state = view.Read();
  if (!n3::native::ChangeFrameClockRate(state, fps,
      target ? n3::frames::StepSecondsBits(fps) : native_step,
      rex::chrono::Clock::QueryGuestTickCount())) return;
  // Start a fresh deadline without enabling a clock the game has suspended.
  view.Write(state);
  active_fps.store(target);
  REXLOG_INFO("N3 frame-rate target applied: {}", fps);
}

extern "C" void __imp__sub_8223FE70(PPCContext& ctx, uint8_t* base);
extern "C" void sub_8223FE70(PPCContext& ctx, uint8_t* base) {
  ApplyFrameRateAtBoundary(base);
  __imp__sub_8223FE70(ctx, base);
}
