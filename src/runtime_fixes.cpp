#include "runtime_fixes.h"
#include "fp_policy.h"

#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <stdexcept>

namespace {
constexpr uint32_t kAudioCallback = 0x8253A590;
PPCFunc* original_audio_callback = nullptr;

void AudioCallback(PPCContext& context, uint8_t* base) {
  InitializeCallbackFpState(context);
  original_audio_callback(context, base);
}
}  // namespace

void InstallRuntimeFixes(rex::Runtime& runtime) {
  auto* dispatcher = runtime.function_dispatcher();
  original_audio_callback = dispatcher->GetFunction(kAudioCallback);
  if (!original_audio_callback || !dispatcher->SetFunction(kAudioCallback, AudioCallback)) {
    throw std::runtime_error("Cannot install N3 audio callback FP initialization");
  }
}
