#include "display_settings.h"
#include <rex/cvar.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>
#include <rex/logging.h>
#include <charconv>
#include <Windows.h>

REXCVAR_DEFINE_UINT32(n3_display_resolution, 720, "N3/Graphics", "Window/output resolution height")
    .allowed({"720", "1080", "1440", "2160"});
REXCVAR_DEFINE_BOOL(n3_vsync, true, "N3/Graphics", "Synchronize Windows presentation to the display");
REXCVAR_DEFINE_BOOL(graphics_trace, false, "N3/Graphics", "Trace actual graphics output and sampler state").debug_only();

uint32_t ConfiguredDisplayHeight() { return REXCVAR_GET(n3_display_resolution); }
bool SetDisplayHeight(uint32_t height) {
  if (height != 720 && height != 1080 && height != 1440 && height != 2160) return false;
  return rex::cvar::SetFlagByName("n3_display_resolution", std::to_string(height));
}
void InstallDisplaySettings(rex::ui::WindowedAppContext& context, rex::ui::Window& window) {
  auto apply = [&window](uint32_t height, bool selected = false) {
    const uint32_t width = height * 16 / 9;
    // The presets are physical output pixels; the SDK window API uses logical
    // pixels, so account for Windows display scaling before requesting a size.
    const auto logical_w = window.SizeToLogical(width), logical_h = window.SizeToLogical(height);
    rex::cvar::SetFlagByName("window_width", std::to_string(logical_w));
    rex::cvar::SetFlagByName("window_height", std::to_string(logical_h));
    window.SetDesiredLogicalSize(logical_w, logical_h);
    // The native menu selects a window size. Restore a maximized window first,
    // then apply exact physical client pixels instead of a rounded DPI size.
    if (selected && window.IsFullscreen()) {
      rex::cvar::SetFlagByName("fullscreen", "false");
      window.SetFullscreen(false);
    }
    const auto handle = static_cast<HWND>(window.GetNativeWindowHandle());
    if (handle && !window.IsFullscreen()) {
      if (selected && IsZoomed(handle)) ShowWindow(handle, SW_RESTORE);
      if (!IsZoomed(handle) && !IsIconic(handle)) {
        RECT rectangle{0, 0, LONG(width), LONG(height)};
        if (AdjustWindowRectExForDpi(&rectangle, DWORD(GetWindowLongPtrW(handle, GWL_STYLE)),
            FALSE, DWORD(GetWindowLongPtrW(handle, GWL_EXSTYLE)), GetDpiForWindow(handle)))
          SetWindowPos(handle, nullptr, 0, 0, rectangle.right - rectangle.left,
              rectangle.bottom - rectangle.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
      }
    }
    REXLOG_INFO("Display size requested: {}x{} pixels", width, height);
  };
  rex::cvar::RegisterChangeCallback("n3_display_resolution",
      [&context, apply](std::string_view, std::string_view value) {
        uint32_t height = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), height);
        if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size())
          context.CallInUIThread([apply, height] { apply(height, true); });
      });
  auto explicit_override = [](const char* flag) {
    const auto source = rex::cvar::GetFlagSource(flag);
    return source == rex::cvar::Source::kCommandLine || source == rex::cvar::Source::kEnvironment;
  };
  if (!explicit_override("window_width") && !explicit_override("window_height") && !explicit_override("resolution"))
    apply(ConfiguredDisplayHeight());
}
void ShutdownDisplaySettings() { rex::cvar::UnregisterChangeCallbacks("n3_display_resolution"); }
void RecordDisplayOutputSize(uint32_t width, uint32_t height) {
  if (width && height) REXLOG_INFO("Display output size: {}x{} pixels", width, height);
}
