#pragma once
#include <rex/ppc.h>
#include <cstdint>

namespace n3::guest {
inline bool ScalarExceptionsMasked(const PPCContext& ctx) {
  using Platform = rex::platform::FPSCRPlatform;
  // disableFlushMode writes csr only if a flush bit was set. Match the control
  // word that the original routine would use before allowing reordered work.
  const auto controls = (ctx.fpscr.csr & Platform::FlushMask) ? ctx.fpscr.csr : Platform::getcsr();
#if defined(__x86_64__) || defined(_M_X64)
  return (controls & Platform::ExceptionMask) == Platform::ExceptionMask;
#else
  return (controls & Platform::ExceptionMask) == 0;
#endif
}
inline bool FitsGuestRange(uint32_t address, uint32_t size) {
  return uint64_t(address) + size <= 0x100000000ull;
}
inline bool DisjointGuestRanges(uint32_t a, uint32_t a_size, uint32_t b, uint32_t b_size) {
  return uint64_t(a) + a_size <= b || uint64_t(b) + b_size <= a;
}
}
