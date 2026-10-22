#include "native/feedback_delay.h"
#include "guest_native_support.h"
#include "guest_memory_address.h"
#include <rex/memory/utils.h>
#include <array>
#include <bit>
#ifdef N3_FEEDBACK_DELAY_TEST
extern thread_local bool n3_feedback_delay_native_used;
#endif

extern "C" void __imp__sub_8253C310(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253BFA8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C480(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8253C5F0(PPCContext&, uint8_t*);
namespace {
uint32_t ReadWord(uint8_t* base, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address));
}
float Read(uint8_t* base, uint32_t address) { return std::bit_cast<float>(ReadWord(base, address)); }
void WriteWord(uint8_t* base, uint32_t address, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(n3::guest::GuestHostAddress(base, address), value);
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
  if (!ScalarExceptionsMasked(ctx) || !FitsLinearGuestRange(source, object_size) || !FitsLinearGuestRange(buffer, 1028) ||
      stack < 32 || !FitsLinearGuestRange(stack - 32, 32) ||
      !DisjointGuestHostRanges(source, object_size, buffer, 1028) ||
      !DisjointGuestHostRanges(source, object_size, stack - 32, 32) || !DisjointGuestHostRanges(buffer, 1028, stack - 32, 32)) return false;
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
#ifdef N3_FEEDBACK_DELAY_TEST
  n3_feedback_delay_native_used = true;
#endif
  return true;
}
bool ProcessSeparate(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  const auto source = ctx.r3.u32, input = ctx.r4.u32, output = ctx.r5.u32, stack = ctx.r1.u32;
  if (!ScalarExceptionsMasked(ctx) || stack < 40) return false;
  struct Range { uint32_t start, size; };
  const std::array<Range, 4> ranges{{{source, 2072}, {input, 1028}, {output, 1024}, {stack - 40, 40}}};
  for (auto range : ranges) if (!FitsLinearGuestRange(range.start, range.size)) return false;
  for (size_t a = 0; a < ranges.size(); ++a)
    for (size_t b = a + 1; b < ranges.size(); ++b)
      if (!DisjointGuestHostRanges(ranges[a].start, ranges[a].size, ranges[b].start, ranges[b].size)) return false;
  struct SeparateWorkspace {
    n3::native::FeedbackDelayState state;
    std::array<float, 512> history;
    std::array<float, 256> input, output;
    float final_output;
  };
  // Guest snapshots stay off the small runtime callback stack. No callbacks
  // occur while processing or publishing this block.
  thread_local SeparateWorkspace workspace;
  workspace.state = {ReadWord(base, source), Read(base, source + 4), Read(base, source + 8),
                     Read(base, source + 12), ReadWord(base, source + 16)};
  const auto cursor = workspace.state.cursor;
  for (uint32_t i = 0; i < 512; ++i) workspace.history[i] = Read(base, source + 24 + i * 4);
  for (uint32_t i = 0; i < 256; ++i) workspace.input[i] = Read(base, input + i * 4);
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  if (!n3::native::TryProcessFeedbackDelaySeparate(workspace.state, workspace.history,
                                                workspace.input, workspace.output, workspace.final_output)) {
    ctx.fpscr.csr = saved_guest; rex::platform::FPSCRPlatform::setcsr(saved_host); return false;
  }
  for (uint32_t i = 0; i < 256; ++i) {
    Write(base, source + 24 + (cursor + i) * 4, workspace.history[cursor + i]);
    Write(base, output + i * 4, workspace.output[i]);
  }
  // Preserve the original split buffer contract: its final output goes in the
  // input tail, while the separate output contains only the prefix and 255 results.
  Write(base, input + 1024, workspace.final_output);
  Write(base, source + 12, workspace.state.previous_output);
  WriteWord(base, source + 16, workspace.state.cursor);
  ctx.r4.u64 += 1024; ctx.r5.u64 = workspace.state.cursor;
  ctx.cr6.compare<uint32_t>(0, 0, ctx.xer);
#ifdef N3_FEEDBACK_DELAY_TEST
  n3_feedback_delay_native_used = true;
#endif
  return true;
}
}
extern "C" void sub_8253C310(PPCContext& ctx, uint8_t* base) {
  if (!Process<256>(ctx, base)) __imp__sub_8253C310(ctx, base);
}
extern "C" void sub_8253BFA8(PPCContext& ctx, uint8_t* base) {
  if (!Process<128>(ctx, base)) __imp__sub_8253BFA8(ctx, base);
}
extern "C" void sub_8253C480(PPCContext& ctx, uint8_t* base) {
  if (!Process<512>(ctx, base)) __imp__sub_8253C480(ctx, base);
}
extern "C" void sub_8253C5F0(PPCContext& ctx, uint8_t* base) {
  if (!ProcessSeparate(ctx, base)) __imp__sub_8253C5F0(ctx, base);
}
