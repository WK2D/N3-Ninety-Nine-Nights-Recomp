#include "app_icon.h"

#include <rex/ui/window.h>

#ifdef _WIN32
#include <Windows.h>
#endif

void ApplyAppIcon(rex::ui::Window& window) {
#ifdef _WIN32
  const auto hwnd = static_cast<HWND>(window.GetNativeWindowHandle());
  const auto module = GetModuleHandleW(nullptr);
  // Resource-loaded shared icons remain valid for the lifetime of the module.
  const auto small_icon = LoadImageW(module, MAKEINTRESOURCEW(101), IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON),
                               GetSystemMetrics(SM_CYSMICON), LR_SHARED);
  const auto large_icon = LoadImageW(module, MAKEINTRESOURCEW(101), IMAGE_ICON,
                               GetSystemMetrics(SM_CXICON),
                               GetSystemMetrics(SM_CYICON), LR_SHARED);
  if (hwnd && small_icon)
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
  if (hwnd && large_icon)
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon));
#endif
}
