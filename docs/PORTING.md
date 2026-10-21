# Native code and platform roadmap

The translated PowerPC functions already compile into native machine code.
Portability depends on the runtime, graphics backend, platform services, and
game-specific timing as well as the CPU target. Replacing a function with more
readable C++ is useful when it removes a measured bottleneck or provides a tested
gameplay interface; it does not by itself establish a faster or correct port.

## First portable components

Configure without the SDK, original executable, or generated translations:

```sh
cmake -S . -B out/portable-core -DN3_BUILD_GAME=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build out/portable-core --config Release
ctest --test-dir out/portable-core -C Release --output-on-failure
```

This builds the native disc verifier/importer and menu asset transformations.
Windows uses CNG for SHA-256, macOS uses CommonCrypto, and Linux uses OpenSSL
Crypto (install its development package). Windows keeps wide-character command
line paths. Destination checks reject symbolic links and Windows reparse points.
Tests use synthetic fixtures; CI never needs or uploads game files.
The portable-components workflow checks Windows, Linux, and an Apple Silicon
macOS runner. Passing these tests does not mean the game runs on those platforms.

## Vulkan migration

The pinned [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk/tree/bd833a2ab69d305f99ddf57418fc38ffec8af93d)
contains Vulkan and macOS/ARM64 support. Its macOS route uses Vulkan through
MoltenVK. N3 currently packages a D3D12-only Windows runtime and a custom Xenos
plugin; enabling a compiler flag alone is insufficient.

1. Build a separate Vulkan-capable runtime/plugin from pinned SDK/dependency
   sources, preserving the working D3D12 release.
2. Make renderer selection explicit. Preserve a D3D12 fallback while validating
   Vulkan on Windows with isolated profiles and shader caches.
3. Implement the same Graphics behavior on Vulkan: synchronized presentation,
   filtering overrides, live render-scale changes, failure rollback and backend
   traces. Do not silently accept a setting that the selected backend ignores.
4. Compare title/menu, opening cutscene, first mission, effects, save/load and
   frame times. Measure shader compilation stalls, CPU/GPU time and memory before
   choosing optimizations or making Vulkan the default.
5. Replace Windows picker/import progress, display sizing, crash reporting and
   packaging adapters. Build the translated game/runtime for ARM64, stage
   MoltenVK in a macOS application bundle, then validate on Apple Silicon hardware.

The SDK's [macOS release notes](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0)
describe the upstream backend. N3's existing live-resolution and VSync extensions
are D3D12-specific and require separate Vulkan implementations.

## Deeper game changes

The first extracted engine primitive is the [native frame clock](NATIVE-CODE.md).
It is independent of the graphics backend and is checked against the retained
translated initializer. The active game build remains on Direct3D 12; Vulkan
development is parked following a system-freeze report.

First establish original-speed timing: trace animation, combat and scripted
event updates at the same gameplay point across 30/40/60 FPS. The current clock
hooks do not establish correct animation speed. Extract understood routines into
typed native modules with explicit guest-memory adapters, units and ownership;
compare outputs against the retained translated implementation before switching.

Use that boundary to introduce gameplay settings, asset overrides and mod hooks.
Keep save formats and original data compatible until a migration is deliberately
implemented and tested. Profiles, traces and original assets stay local.

Milestones are a rendered Vulkan Windows build, matching Graphics controls,
normal-speed high-FPS gameplay, a rendered ARM64 Mac build, then mission coverage
and measured performance improvements. None is established by a core-only build.
