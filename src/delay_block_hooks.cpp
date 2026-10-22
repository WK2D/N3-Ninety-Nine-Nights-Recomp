#include "native/delay_block.h"
#include "guest_native_support.h"
#include <rex/memory/utils.h>
#include <bit>

namespace {
using namespace n3::native;
struct Range { uint32_t start, size; };
struct DelayWorkspace {
  DelayBlockState state;
  std::array<float, 2048> history;
  std::array<float, kDelayBlockSamples> input;
  std::array<float, kDelayBlockSamples + 1> mixed, auxiliary;
};
uint32_t ReadWord(uint8_t* base, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(base + address);
}
float Read(uint8_t* base, uint32_t address) { return std::bit_cast<float>(ReadWord(base, address)); }
void WriteWord(uint8_t* base, uint32_t address, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(base + address, value);
}
void Write(uint8_t* base, uint32_t address, float value) { WriteWord(base, address, std::bit_cast<uint32_t>(value)); }
bool Process(PPCContext& ctx, uint8_t* base, uint32_t history_size) {
  using namespace n3::guest;
  if (!ScalarExceptionsMasked(ctx) || ctx.r1.u32 < 48) return false;
  const std::array<Range, 5> ranges{{{ctx.r3.u32, 44 + history_size * 4},
      {ctx.r4.u32, kDelayBlockSamples * 4}, {ctx.r5.u32, (kDelayBlockSamples + 1) * 4},
      {ctx.r6.u32, (kDelayBlockSamples + 1) * 4}, {ctx.r1.u32 - 48, 48}}};
  for (const auto range : ranges) if (!FitsGuestRange(range.start, range.size)) return false;
  for (size_t first = 0; first < ranges.size(); ++first)
    for (size_t second = first + 1; second < ranges.size(); ++second)
      if (!DisjointGuestRanges(ranges[first].start, ranges[first].size, ranges[second].start, ranges[second].size)) return false;
  // No callbacks/reentrancy occur while processing. Per-thread storage avoids
  // placing the history/output snapshots on small runtime callback stacks.
  thread_local DelayWorkspace workspace;
  const auto object = ctx.r3.u32;
  workspace.state = {{DelayTap{ReadWord(base, object), Read(base, object + 4)},
      DelayTap{ReadWord(base, object + 12), Read(base, object + 16)},
      DelayTap{ReadWord(base, object + 24), Read(base, object + 28)}},
      Read(base, object + 8), Read(base, object + 32), ReadWord(base, object + 36)};
  for (uint32_t index = 0; index < history_size; ++index) workspace.history[index] = Read(base, object + 44 + index * 4);
  for (uint32_t index = 0; index < kDelayBlockSamples; ++index) workspace.input[index] = Read(base, ctx.r4.u32 + index * 4);
  const auto cursor = workspace.state.cursor;
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  if (!TryProcessDelayBlock(workspace.state, std::span(workspace.history.data(), history_size),
                           workspace.input, workspace.mixed, workspace.auxiliary)) {
    ctx.fpscr.csr = saved_guest;
    rex::platform::FPSCRPlatform::setcsr(saved_host);
    return false;
  }
  for (uint32_t index = 0; index < kDelayBlockSamples; ++index)
    Write(base, object + 44 + (cursor + index) * 4, workspace.history[cursor + index]);
  for (uint32_t index = 0; index <= kDelayBlockSamples; ++index) {
    Write(base, ctx.r5.u32 + index * 4, workspace.mixed[index]);
    Write(base, ctx.r6.u32 + index * 4, workspace.auxiliary[index]);
  }
  Write(base, object + 8, workspace.state.previous_mixed);
  Write(base, object + 32, workspace.state.previous_auxiliary);
  WriteWord(base, object + 36, workspace.state.cursor);
  ctx.f1.f64 = double(workspace.state.previous_auxiliary);
  ctx.f2.f64 = double(workspace.state.previous_mixed);
  ctx.r4.u64 += kDelayBlockSamples * 4; ctx.r5.u64 += kDelayBlockSamples * 4; ctx.r6.u64 += kDelayBlockSamples * 4;
  ctx.r7.u64 = workspace.state.cursor;
  ctx.cr6.compare<uint32_t>(0, 0, ctx.xer);
  return true;
}
bool ProcessDual(PPCContext& ctx, uint8_t* base) {
  using namespace n3::guest;
  if (!ScalarExceptionsMasked(ctx) || ctx.r1.u32 < 32) return false;
  const std::array<Range, 5> ranges{{{ctx.r3.u32, 4128}, {ctx.r4.u32, 1024},
      {ctx.r5.u32, 1028}, {ctx.r6.u32, 1028}, {ctx.r1.u32 - 32, 32}}};
  for (const auto range : ranges) if (!FitsGuestRange(range.start, range.size)) return false;
  for (size_t a = 0; a < ranges.size(); ++a)
    for (size_t b = a + 1; b < ranges.size(); ++b)
      if (!DisjointGuestRanges(ranges[a].start, ranges[a].size, ranges[b].start, ranges[b].size)) return false;
  struct Workspace {
    DualTapDelayState state;
    std::array<float, 1024> history;
    std::array<float, 256> input;
    std::array<float, 257> first, second;
  };
  thread_local Workspace workspace;
  const auto object = ctx.r3.u32;
  workspace.state = {{DelayTap{ReadWord(base, object), Read(base, object + 4)},
      DelayTap{ReadWord(base, object + 12), Read(base, object + 16)}},
      {Read(base, object + 8), Read(base, object + 20)}, ReadWord(base, object + 24)};
  for (uint32_t i = 0; i < 1024; ++i) workspace.history[i] = Read(base, object + 32 + i * 4);
  for (uint32_t i = 0; i < 256; ++i) workspace.input[i] = Read(base, ctx.r4.u32 + i * 4);
  const auto cursor = workspace.state.cursor;
  const auto saved_host = rex::platform::FPSCRPlatform::getcsr(), saved_guest = ctx.fpscr.csr;
  ctx.fpscr.disableFlushMode();
  if (!TryProcessDualTapDelay(workspace.state, workspace.history, workspace.input, workspace.first, workspace.second)) {
    ctx.fpscr.csr = saved_guest; rex::platform::FPSCRPlatform::setcsr(saved_host); return false;
  }
  for (uint32_t i = 0; i < 256; ++i) Write(base, object + 32 + (cursor + i) * 4, workspace.history[cursor + i]);
  for (uint32_t i = 0; i < 257; ++i) {
    Write(base, ctx.r5.u32 + i * 4, workspace.first[i]); Write(base, ctx.r6.u32 + i * 4, workspace.second[i]);
  }
  Write(base, object + 8, workspace.state.previous[0]); Write(base, object + 20, workspace.state.previous[1]);
  WriteWord(base, object + 24, workspace.state.cursor);
  ctx.f0.f64 = double(workspace.state.previous[0]); ctx.f13.f64 = double(workspace.state.previous[1]);
  ctx.r4.u64 += 1024; ctx.r5.u64 += 1024; ctx.r6.u64 += 1024;
  ctx.r7.u64 = cursor + 256; ctx.r9.u64 = workspace.state.cursor;
  ctx.cr6.compare<uint32_t>(0, 0, ctx.xer);
  return true;
}
}
extern "C" void __imp__sub_8253BED8(PPCContext&, uint8_t*);
extern "C" void sub_8253BED8(PPCContext& ctx, uint8_t* base) {
  if (!Process(ctx, base, 512)) __imp__sub_8253BED8(ctx, base);
}
extern "C" void __imp__sub_8253C240(PPCContext&, uint8_t*);
extern "C" void sub_8253C240(PPCContext& ctx, uint8_t* base) {
  if (!Process(ctx, base, 2048)) __imp__sub_8253C240(ctx, base);
}
extern "C" void __imp__sub_8253C758(PPCContext&, uint8_t*);
extern "C" void sub_8253C758(PPCContext& ctx, uint8_t* base) {
  if (!ProcessDual(ctx, base)) __imp__sub_8253C758(ctx, base);
}
