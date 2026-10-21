#include "graphics_policy.h"
#include <dxgi.h>
#include <iostream>
#include <stdexcept>

int main() {
  try {
    for (bool tearing : {false, true}) {
      const auto on = n3::graphics::HostPresentPolicy(true, tearing);
      if (on.interval != 1 || on.flags != DXGI_PRESENT_DO_NOT_WAIT) throw std::runtime_error("VSync must synchronize without blocking, tearing or queue restart");
      const auto off = n3::graphics::HostPresentPolicy(false, tearing);
      if (off.interval != 0 || off.flags != (DXGI_PRESENT_RESTART | (tearing ? DXGI_PRESENT_ALLOW_TEARING : 0)))
        throw std::runtime_error("Immediate mode must allow tearing only when supported");
    }
    std::cout << "Host VSync and tearing capability policy passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
