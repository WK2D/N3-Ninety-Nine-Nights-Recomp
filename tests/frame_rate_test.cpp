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
extern "C" void sub_8223FE70(PPCContext&, uint8_t*);
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
    auto run = [&](uint32_t target_fps, uint32_t fps, bool configure = true) {
      if (configure) {
        Require(rex::cvar::SetFlagByName("n3_frame_rate", std::to_string(target_fps)), "Cannot set FPS mode");
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
    Require(run(0, 40) == 40 && read(object + 8) == std::bit_cast<uint32_t>(0.025f), "Original mode changed");
    for (const auto target : {30u, 40u, 60u}) {
      for (const auto native : {30u, 40u, 60u}) {
        Require(run(target, native) == target && read(object + 8) == n3::frames::StepSecondsBits(target), "Selected FPS step was not applied");
      }
    }
    Require(run(60, 24) == 24, "Unexpected clock was overwritten");
    Require(run(30, 40) == 30, "Cannot initialize live-switch test");
    for (const auto target : {60u, 40u, 30u, 40u, 60u, 30u}) {
      Require(SetFrameRate(target), "Cannot request live FPS change");
      const auto old = read(object + 4);
      Require(old != target, "Test must switch between different targets");
      const auto enabled = bytes[object + 1];
      ApplyFrameRateAtBoundary(bytes.data());
      Require(read(object + 4) == target && read(object + 8) == n3::frames::StepSecondsBits(target), "Live clock step/target mismatch");
      Require(rex::memory::load_and_swap<uint64_t>(bytes.data() + object + 40) == frequency / target, "Live clock deadline period mismatch");
      Require(bytes[object + 1] == enabled, "Live switching changed clock enable state");
    }
    bytes[object + 1] = 0;
    Require(SetFrameRate(60), "Cannot switch suspended clock");
    ApplyFrameRateAtBoundary(bytes.data());
    Require(bytes[object + 1] == 0, "Suspended clock was enabled by live switching");
    Require(!SetFrameRate(24) && !SetFrameRate(0) && ConfiguredFrameRate() == 60, "Invalid menu FPS accepted");
    Require(rex::cvar::SetFlagByName("n3_frame_rate", "40"), "Cannot set FPS through the shared registry");
    ApplyFrameRateAtBoundary(bytes.data());
    Require(read(object + 4) == 40 && ConfiguredFrameRate() == 40, "SDK overlay and native menu settings diverged");
    Require(rex::chrono::Clock::guest_tick_frequency() == frequency &&
            rex::chrono::Clock::guest_time_scalar() == scalar, "Global guest/audio clock was changed");
    for (const auto interval : {0u, 1u, 2u, 3u, 4u, 0x80000000u}) {
      Require(n3::frames::PresentInterval(interval, 30) == interval, "Original presentation interval changed");
      Require(n3::frames::PresentInterval(interval, 60) == (interval == 2 ? 1u : interval), "Unexpected presentation mode changed");
    }
    bool registered = false, presentation_registered = false, trace_registered = false, boundary_registered = false;
    for (auto* mapping = PPCFuncMappings; mapping->guest; ++mapping) {
      if (mapping->guest == 0x82248D38) registered = mapping->host == sub_82248D38 && mapping->host != __imp__sub_82248D38;
      if (mapping->guest == 0x823AA3B0) presentation_registered = mapping->host == sub_823AA3B0;
      if (mapping->guest == 0x823AA520) trace_registered = mapping->host == sub_823AA520;
      if (mapping->guest == 0x8223FE70) boundary_registered = mapping->host == sub_8223FE70;
    }
    Require(registered && presentation_registered && trace_registered && boundary_registered, "Indirect calls do not resolve to the frame-rate hooks");
    const auto* setting = rex::cvar::GetFlagInfo("n3_frame_rate");
    Require(setting && setting->lifecycle == rex::cvar::Lifecycle::kHotReload && !setting->is_debug_only && setting->default_value == "30",
            "FPS mode is not available as a live player setting");
    std::cout << "Original translated clock, live 30/40/60 switching, global clocks, interval policy, and hook registration passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
