#include "frame_rate.h"
#include "frame_rate_policy.h"
#include <rex/ppc.h>
#include <rex/cvar.h>
#include <rex/chrono/clock.h>
#include <rex/memory/utils.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

extern "C" void sub_82248D38(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82248D38(PPCContext&, uint8_t*);
extern "C" void sub_823AA3B0(PPCContext&, uint8_t*);
extern "C" void sub_823AA520(PPCContext&, uint8_t*);
extern PPCFuncMapping PPCFuncMappings[];

void Require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }

int main() {
  try {
    // Execute the actual translated clock initializer against an isolated guest
    // stack/object. This exercises its timebase query, division, and saved regs.
    std::array<uint8_t, 65536> bytes{};
    constexpr uint32_t object = 256;
    constexpr uint64_t frequency = 50000000;
    rex::chrono::Clock::set_guest_tick_frequency(frequency);
    const auto scalar = rex::chrono::Clock::guest_time_scalar();
    auto read = [&](uint32_t at) { return rex::memory::load_and_swap<uint32_t>(bytes.data() + at); };
    auto write = [&](uint32_t at, uint32_t value) { rex::memory::store_and_swap<uint32_t>(bytes.data() + at, value); };
    auto run = [&](bool enabled, uint32_t fps, bool configure = true) {
      if (configure) {
        Require(rex::cvar::SetFlagByName("n3_60fps", enabled ? "true" : "false"), "Cannot set FPS mode");
        InitializeFrameRate();
      }
      write(object + 4, fps);
      write(object + 8, std::bit_cast<uint32_t>(1.0f / fps));
      PPCContext ctx{};
      ctx.r1.u32 = 61440;
      ctx.r3.u32 = object;
      ctx.r31.u64 = 0x12345678;
      sub_82248D38(ctx, bytes.data());
      Require(ctx.r1.u32 == 61440 && ctx.r31.u64 == 0x12345678, "Guest stack or nonvolatile register changed");
      const auto target = read(object + 4);
      const auto ticks = rex::memory::load_and_swap<uint64_t>(bytes.data() + object + 40);
      Require(ticks == frequency / target && bytes[object + 1] == 1, "Translated clock period was not initialized correctly");
      return target;
    };
    Require(run(false, 40) == 40 && read(object + 8) == std::bit_cast<uint32_t>(0.025f), "Original mode changed");
    for (const auto native : {30u, 40u, 60u}) {
      Require(run(true, native) == 60 && read(object + 8) == n3::frames::kStepSecondsBits, "60 FPS step was not applied");
      const auto seconds = std::bit_cast<float>(read(object + 8));
      Require(std::abs(double(seconds) * 60.0 - 1.0) < 0.000001, "60 simulation steps do not represent one second");
    }
    Require(run(true, 24) == 24, "Unexpected clock was overwritten");
    Require(rex::cvar::SetFlagByName("n3_60fps", "false"), "Cannot change saved mode");
    Require(run(false, 40, false) == 60, "Restart-required setting changed the active mode");
    Require(run(false, 40) == 40, "Original mode did not restore after reinitialization");
    Require(rex::chrono::Clock::guest_tick_frequency() == frequency &&
            rex::chrono::Clock::guest_time_scalar() == scalar, "Global guest/audio clock was changed");
    for (const auto interval : {0u, 1u, 2u, 3u, 4u, 0x80000000u}) {
      Require(n3::frames::PresentInterval(interval, false) == interval, "Original presentation interval changed");
      Require(n3::frames::PresentInterval(interval, true) == (interval == 2 ? 1u : interval), "Unexpected presentation mode changed");
    }
    bool registered = false, presentation_registered = false, trace_registered = false;
    for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping) {
      if (mapping->guest == 0x82248D38) registered = mapping->host == sub_82248D38 && mapping->host != __imp__sub_82248D38;
      if (mapping->guest == 0x823AA3B0) presentation_registered = mapping->host == sub_823AA3B0;
      if (mapping->guest == 0x823AA520) trace_registered = mapping->host == sub_823AA520;
    }
    Require(registered && presentation_registered && trace_registered, "Indirect calls do not resolve to the frame-rate hooks");
    const auto* setting = rex::cvar::GetFlagInfo("n3_60fps");
    Require(setting && setting->lifecycle == rex::cvar::Lifecycle::kRequiresRestart && !setting->is_debug_only && setting->default_value == "false",
            "FPS mode is not available as a restart-required player setting");
    std::cout << "Original translated clock, 60 Hz step/deadline, global-clock preservation, restart behavior, interval policy, and hook registration passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
