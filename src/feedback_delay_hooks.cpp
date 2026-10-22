#include "native/feedback_delay.h"
#include "guest_native_support.h"
#include <rex/memory/utils.h>
#include <array>
#include <bit>

extern "C" void __imp__sub_8253C310(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253BFA8(PPCContext&, uint8_t*);
namespace {
uint32_t ReadWord(uint8_t* base, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(base + address);
}
float Read(uint8_t* base, uint32_t address) { return std::bit_cast<float>(ReadWord(base, address)); }
void WriteWord(uint8_t* base, uint32_t address, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(base + address, value);
}
void Write(uint8_t* base, uint32_t address, float value) { WriteWord(base, address, std::bit_cast<uint32_t>(value)); }
template<size_t HistorySize> struct Workspace {
  n3::native::FeedbackDelayState state;
  std::array<float, HistorySize> history;
  std::array<float, 257> buffer;
};
template<size_t HistorySize> bool Process(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  const auto source = ctx.r3.u32, buffer = ctx.r4.u32, stack = ctx.r1.u32;
  constexpr uint32_t object_size = 24 + HistorySize * 4;
  if (!ScalarExceptionsMasked(ctx) || !FitsGuestRange(source, object_size) || !FitsGuestRange(buffer, 1028) ||
      !DisjointGuestRanges(source, object_size, buffer, 1028) || stack < 32 ||
      !DisjointGuestRanges(source, object_size, stack - 32, 32) || !DisjointGuestRanges(buffer, 1028, stack - 32, 32)) return false;
  // This routine performs no callbacks. Per-thread adapter storage keeps its
  // guest snapshots off the runtime's small callback stacks.
  thread_local Workspace<HistorySize> workspace;
  workspace.state = {ReadWord(base, source), Read(base, source + 4), Read(base, source + 8),
                     Read(base, source + 12), ReadWord(base, source + 16)};
  for (uint32_t index = 0; index < HistorySize; ++index) workspace.history[index] = Read(base, source + 24 + index * 4);
  for (uint32_t index = 0; index < 257; ++index) workspace.buffer[index] = Read(base, buffer + index * 4);
  // The shorter original kernel leaves its last unrolled input loads in f1/f2.
  const auto input248 = workspace.buffer[248], input249 = workspace.buffer[249];
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  if (!n3::native::TryProcessFeedbackDelay(workspace.state, workspace.history, workspace.buffer)) {
    ctx.fpscr.csr = saved_guest;
    rex::platform::FPSCRPlatform::setcsr(saved_host);
    return false;
  }
  for (uint32_t index = 0; index < HistorySize; ++index) Write(base, source + 24 + index * 4, workspace.history[index]);
  for (uint32_t index = 0; index < 257; ++index) Write(base, buffer + index * 4, workspace.buffer[index]);
  Write(base, source + 12, workspace.state.previous_output);
  WriteWord(base, source + 16, workspace.state.cursor);
  ctx.r4.u64 += 1024;
  ctx.r5.u64 = workspace.state.cursor;
  if constexpr (HistorySize == 128) {
    ctx.f1.f64 = double(input249);
    ctx.f2.f64 = double(input248);
    ctx.xer.ca = workspace.state.delay <= 2;
  }
  ctx.cr6.compare<uint32_t>(0, 0, ctx.xer);
  return true;
}
}
extern "C" void sub_8253C310(PPCContext& ctx, uint8_t* base) {
  if (!Process<256>(ctx, base)) __imp__sub_8253C310(ctx, base);
}
extern "C" void sub_8253BFA8(PPCContext& ctx, uint8_t* base) {
  if (!Process<128>(ctx, base)) __imp__sub_8253BFA8(ctx, base);
}
