#include "fp_policy.h"
#include <cstdio>

int main() {
  using Platform = rex::platform::FPSCRPlatform;
  const auto saved_host = Platform::getcsr();
  const auto guest_mask = rex::ppc::FPSCRRegister::GuestMask;
  // Cover a fresh host callback plus each guest rounding mode with FTZ/DAZ.
  for (uint32_t mode = 0; mode < 5; ++mode) {
    PPCContext context{};
    context.fpscr.csr = mode == 0 ? 0 :
        static_cast<uint32_t>(Platform::GuestToHost[mode - 1] | Platform::FlushMask);
    const auto guest = context.fpscr.csr & guest_mask;
    InitializeCallbackFpState(context);
    const auto host = Platform::getcsr();
#if defined(__x86_64__) || defined(_M_X64)
    const bool masks_correct = (host & Platform::ExceptionMask) == Platform::ExceptionMask;
#else
    const bool masks_correct = (host & Platform::ExceptionMask) == 0;
#endif
    if (!masks_correct || (host & guest_mask) != guest || context.fpscr.csr != host) {
      Platform::setcsr(saved_host);
      std::fputs("Callback FP masks or guest controls are incorrect\n", stderr);
      return 1;
    }
  }
  Platform::setcsr(saved_host);
  std::puts("Callback FP policy preserves guest controls and masks host exceptions");
}
