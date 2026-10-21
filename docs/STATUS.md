# Development status

Recorded 21 October 2026. The opening mission, following cutscene, and writing a
save have been exercised successfully. Loading saved progress also succeeds on
the tested opening route. Full-game coverage remains unvalidated.

## Verified

- XDVDFS game partition found at byte offset `0x0FD90000` in the supplied disc.
- Extracted 3,342 files / 5,770,038,003 bytes, with per-file SHA-256 hashes.
- Identified title `4D5307DB`, media `5D48689C`, executable version `0.0.0.3`.
- Strict ReXGlue analysis/codegen succeeds without `--force`.
- Generated 13,762 function assignments across 87 translated C++ source files.
- Windows x64 Release and RelWithDebInfo compile and link succeed with Clang 20.1.8.
- Runtime initializes Direct3D 12, input, audio, and XEX imports.
- Five JavaScript extractor tests and eight native regression suites pass.
- A 30-second boot test survived until the harness stopped it, without the prior
  unregistered-function or floating-point failure.
- Windowed startup reaches the intro/title-screen presentation.
- Main menu and character selection reached, followed by the opening cutscene.
- Replay progresses past the previous opening-cutscene crash and reaches gameplay.
- Opening mission completed, the next cutscene played, and a save written.
- Saved progress loads successfully on the tested opening route.
- Desktop shortcut follows the stable launcher, using a 1280 x 720 window.
- The packaged Release binary initializes GPU, audio, input, and guest execution
  from a remembered game path and survives a 58-second isolated startup run.
  The harness stops that child; this check does not establish visual correctness.
- Native ISO import produces all 3,342 files with matching SHA-256 hashes against
  the original verified extraction. No game material is bundled with the release.
- Import cancellation cleans up its newly created output; existing destinations
  are rejected without alteration. Wrong revisions and malformed disc entries
  are rejected. Explicit and remembered game folders are revalidated.

## Corrections made during first boot

| Guest entry | Evidence | Treatment |
| --- | --- | --- |
| `0x8253F950` | Direct branch at `0x8254698C` in an object-adjustment thunk | Seed function discovery; generated body ends at `0x8253FA18` |
| `0x82588FF8` | Startup indirect-call trap | Seed discovery; generated body is a small tail-call initializer |
| `0x8253ED08` | Indirect-call trap during audio initialization | Seed discovery; preserve generated implementation |
| `0x8215B150` | Runtime indirect-call trap during the opening cutscene | Seed discovery; original function generated and registered; replay reaches gameplay |

The SDK's audio worker invokes guest callback `0x8253A590`. A first-chance fault
trace found `0xC000008F` with host MXCSR `0x8060` while executing the generated
`vcfsx` path in `sub_8254C600`. Host FP exception masks were clear. The local
callback wrapper initializes the host mask policy while preserving the guest's
rounding and flush bits, then calls the original generated callback. The next
30-second boot survived, and the windowed visual check reached intro/title.

Windows FP fault diagnostics write module offsets and a stack under the active
profile's `logs/floating-point-fault.txt`: `private/user/logs/` for the development
launcher or `%LOCALAPPDATA%\N3Recompiled\logs` for default release profiles.
Stack frames record module basenames rather than absolute installation paths.
`tools/symbolize-fault.ps1` resolves executable frames against the local build/PDB,
supports `-Executable` for another build, and accepts older reports with full
module paths. Diagnostics installation failure does not prevent startup.

The diagnostics regression raises a synthetic FP exception in an isolated
process, verifies report location and path omission, and confirms that the
exception continues to the next handler with its original context. This is
diagnostics coverage, not evidence of an additional gameplay fault.
The same probes pass on a thread with a 64 KiB stack. Large diagnostic scratch
buffers use serialized static storage so exception entry does not exhaust small
runtime thread stacks. The updated development build passes a 30-second startup
check using a separate profile/cache; the harness stops only its test child.

## Opening cutscene

The first cutscene run exhibited presentation glitches and terminated at an
unregistered indirect target, `0x8215B150`. The target is now included in strict
function discovery, emitted as an original-code translation, and present in both
generated function registration tables. The native rebuild and callback FP test
pass. Replay progresses past the previous failure point and reaches gameplay.
The opening route now extends through mission completion and the following
cutscene. This does not establish broader cutscene coverage or resolve every
previously observed presentation glitch.

## Alpha distribution

The first Windows x64 alpha adds native first-launch selection of an ISO or an
extracted `default.xex`, executable hash verification, optional one-time disc
import, and a remembered local game path. Direct EXE startup defaults to a
1280 x 720 window. Explicit runtime settings retain precedence. Release profiles
default to `%LOCALAPPDATA%\N3Recompiled`; development launcher profiles stay
under `private/`.

The package uses an explicit file allowlist, includes runtime dependencies and
third-party notices, omits PDBs and local game/profile/log data, and checks binary
strings for build-machine identifiers. Corresponding SDK/dependency sources and
their pinned revision manifest accompany the release. The picker and progress
dialog still need a completed visual interaction check; importer and path logic
have native automated coverage.

## Graphics menu and live frame-rate control

The development build adds Graphics below Tutorial on the main menu and a
separate native page derived from the Options layout. The page contains 30,
40, and 60 FPS choices plus Game default / Off / 2x / 4x / 8x / 16x anisotropic
filtering. New installations default to 30 FPS. Settings apply without a restart
and persist through the existing local profile configuration. Original Options
retains its audio, camera, vibration, and contrast controls.

The page also separates display/output size (720p, 1080p, 1440p, 4K) from internal
rendering (720p/1x, 1440p/2x, 4K/3x). Display changes resize a normal window on the
UI thread, accounting for Windows display scaling. Maximized/fullscreen windows
use desktop output dimensions, and logical-pixel rounding may slightly change
the requested client dimensions. Explicit launch dimensions retain precedence.

The menu layout and text are generated locally from the player's Menu/Text
files into a profile cache, mounted read only through runtime VFS directory
links. The source game folder is not modified. No derived menu assets or game
text are tracked or included in release packages. Installation failure removes
its new VFS links/device and retains the original menus. Added labels currently
use English across the supported menu-language tables.

| Entry | Treatment |
| --- | --- |
| `0x8218CDC8` | Route the main-menu Graphics button through the native Options transition |
| `0x82272470` | Select the separate Graphics layout for that transition |
| `0x821A8030` | Configure all four native selectors on every Graphics entry, including cached layout reuse |
| `0x821A8370` | Focus the first Graphics control after the entry transition completes |
| `0x821A8B30` | Apply Graphics selector changes without changing audio/camera settings |
| `0x82248D38` | Initialize the selected supported clock target and matching step |
| `0x8223FE70` | Apply a changed target, step, period, and deadline before the next game update |
| `0x823AA3B0` | Lift an explicit two-vblank interval for targets above 30, restoring requested state afterward |
| `0x823AA520` | Pace presentations, including menus that bypass the frame clock; optional timing trace |

The live clock update retains the enable/suspend flags and global guest clock.
The original native clock constructor uses 40 Hz and a 0.025-second step;
`-FrameRate Original` preserves that native policy and skips the added limiter.
The older `n3_60fps` flag is retained as a hidden compatibility alias. The shared
`n3_frame_rate` setting can also be changed through the SDK overlay.

Anisotropic choices map to the SDK's hot-reloadable `anisotropic_override` enum,
not literal sample counts: 16x is enum 5. D3D12 evaluates the override while
building sampler parameters on draws and refreshes descriptors when those
parameters change. The override affects eligible mipmapped linear-filtered
textures; it does not force filtering onto every texture.

The native regressions execute the actual translated clock initializer and
switch repeatedly among 30/40/60 without restarting. They check period/step
consistency, suspended-clock preservation, global-clock preservation, shared
registry updates, rejected rates, and strong hook registration. Layout tests
cover source/metadata preservation, malformed input, truncation, and duplicate
extension rejection. The supported local title/options transformations also
pass against the private game files.

The main-menu Graphics entry opens its separate native page. The original two controls
respond to navigation after focus is assigned at transition completion and
hidden controls are disabled. The final layout keeps selected labels on one
line. Live switching to 40 and 60 FPS works, and 16x filtering persists as SDK
enum 5 in the local profile. These are tested menu/settings outcomes, not
full-game rendering or animation validation.

Re-entering Graphics previously bypassed selector initialization when the native
layout cache skipped a fresh file load. Initialization now follows the explicit
Graphics transition, independent of the loader. The four sidebar labels align
with their banners, and a short noninteractive notification confirms local saves.

The pinned SDK serializer wrote raw Windows paths inside TOML basic strings,
making some generated profiles unreadable on restart. Graphics now writes typed
TOML with escaping and replaces the completed file atomically. Startup repairs
the identifiable legacy flat format when invalid; other malformed files are
left alone. The new regression checks string round trips through the SDK loader,
legacy recovery, malformed-file preservation, and resetting values to defaults.

## Live render resolution

The project builds its own Xenos plugin from the pinned SDK sources. At the end
of a completed swap it waits for GPU work, allocates replacement render targets,
texture caches and scale-dependent shader pipelines, reloads active shader
microcode, and updates EDRAM descriptors. Guest memory, GPU shared memory,
registers, the command ring and the presenter remain alive. Allocation or device
limit failures retain the previous resources and setting. This path is intended
for the main-menu Graphics page, whose next frame redraws render-target contents.

An isolated run switched from 1x to 3x (3840 x 2160) without restarting, logged the
applied scale, and continued presenting until the harness stopped its own child.
The subsequent interactive check confirmed live 720p/1440p/4K changes, retained
values after leaving with B and reopening Graphics, and the saved-settings
notification. Subsequent gameplay and broader rendering coverage remain pending.
The first published alpha
continues to use the original SDK plugin and does not include these controls.

Alpha 2 packages the four-row Graphics page, live render extension, cached-menu
re-entry correction and profile persistence fixes. Both build configurations pass
seven native suites, and the supported private menu transformations pass. A
separate Release run loaded a saved 40 FPS profile and switched from 3x to 1x
rendering without restarting, continuing until its own harness deadline.
The extracted alpha 2 Windows ZIP passes all packaged-file hash checks, reuses a
remembered local game folder, installs Graphics, and applies live 2x rendering.
It continues presenting until the isolated harness stops its child at 24 seconds;
this packaged startup check does not extend gameplay compatibility claims.

## Graphics audit and host VSync (development)

The native Graphics page now includes VSync On/Off below render resolution.
The pinned SDK's `vsync` flag controls guest vblank scheduling, while its original
Windows presenter always uses immediate presentation. The project presenter
controls DXGI directly with a separate saved `n3_vsync` setting. On uses interval 1,
a one-frame latency limit, a waitable swap chain polled before rendering, and
`DXGI_PRESENT_DO_NOT_WAIT`; Off uses interval 0 with tearing only when supported.
The shared callback types and runtime/provider remain unchanged. The initial
blocking interval-1 implementation slowed menus; the nonblocking path has been
checked interactively and restores responsiveness with VSync On.
See Microsoft's [frame latency guidance](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject).

Menu presentation now reads the requested FPS independently of gameplay-clock
initialization. Anisotropic Off disables anisotropy consistently, including
textures outside the forced-filter eligibility policy. Positive overrides retain
point/base-map behavior for ineligible textures. A GPU-thread probe uses isolated
fetch registers, the actual sampler-parameter implementation and real D3D12
sampler creation to check each override and eligibility case, without modifying
game registers or settings files.

Renderer audit traces show 1280x720, 1920x1080, 2560x1440 and 3840x2160 physical
window clients, and 1280x720/2560x1440/3840x2160 rendered swap sources. Exact sizing
corrects DPI rounding and restores a maximized window when selecting a preset;
oversized normal windows can extend beyond the display. Runtime-loaded plugin
staging now runs even when only GPU sources changed. Eight native suites pass.
The audit is an isolated developer probe; video cadence, descriptor verification
and successful resizing do not establish full-game graphics or animation timing.
The final Release audit completed all six sequences with every required backend
check passing through `tools/verify-graphics-audit.py`. Alpha 3 includes these
corrections and the nonblocking VSync menu control.

## Frame-rate validation limits

Higher frame-rate modes remain experimental. Earlier 60 FPS gameplay testing
reported smooth menus/gameplay and normal video playback; a sampled trace
measured approximately 60.02 guest presentations per second. Guest present-call
counts do not independently establish unique rendered frames. The updated
presentation limiter measures approximately 30 presentations per second in the
initial 30 FPS menu/startup check. A live switch to 40 FPS measures approximately
40 presentations per second on the Graphics page. Broader scene measurements
and filtering image-quality comparisons remain pending.

Animations appear accelerated at 60 FPS. Correct animation/combat timing is not
established. The configured step reaches the engine worker through `0x8225FBE8`,
`0x82316A78`, and `0x8230E560`; separate paths also use integer frame counters and
fixed increments. No unverified animation multiplier or shared-constant patch
has been applied. Matched gameplay comparisons with original pacing are still
needed, together with later-scene, scripted-event, performance, and audio tests.

## Application icon

The application embeds an emerald-and-silver N3 icon with 16, 24, 32, 48, 64,
128 and 256 pixel variants. The window applies the embedded resource, and the
development shortcut uses the matching ICO. The full-resolution transparent
PNG contains pixel data only, without image metadata. Both build configurations
pass the eight native suites. Alpha 4 packages the icon inside n3.exe, so
Windows shortcuts made from the downloaded executable inherit it without a
separate image file. The development desktop shortcut has been refreshed.
Packaged icon resources load at all seven sizes; packaged file hashes and binary
privacy checks pass. Alpha 3 remains available unchanged.

## Portable native components

The disc parser/importer and menu asset transformations now build independently
with `N3_BUILD_GAME=OFF`, without ReXGlue or game data. Hashing and destination
checks use platform adapters; macOS uses CommonCrypto and Linux uses OpenSSL.
The Windows full build still passes eight suites, including added empty and
multi-block hash vectors and destination-link checks where privileges allow.
A workflow exercises the portable subset on Windows, Linux and Apple Silicon.
These components do not establish a Mac game build. See [porting roadmap](PORTING.md)
for the Vulkan/runtime work and native gameplay boundaries.

## Next validation work

1. Measure original-mode gameplay in the same scene and correct the accelerated
   animation paths in experimental 60 FPS. Then compare combat, cutscenes, and
   audio across missions.
2. Repeat the opening route on the packaged Release build and other PCs.
3. Exercise later missions, characters, rendering effects, and large enemy counts.
4. Extend save/load coverage across later missions, repeated restarts, and profiles.
5. Compare scenes and timing against console or emulator reference captures.
6. Investigate additional indirect targets or translation problems from logs.

Other game revisions, title updates, platforms, and full-game playthroughs have
not been validated. Keep performance and visual claims tied to measured scenes.
