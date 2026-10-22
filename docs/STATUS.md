# Development status

Recorded 22 October 2026. The opening mission, following cutscene, and writing a
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
- Five JavaScript extractor tests and thirty-four native regression suites pass.
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
40, and 60 FPS choices plus 1x / 2x / 4x / 8x / 16x anisotropic
filtering. New installations default to 30 FPS. Settings apply without a restart
and persist through the existing local profile configuration. Original Options
retains its audio, camera, vibration, and contrast controls.

Anisotropic filtering now uses only numeric choices. 1x explicitly disables
anisotropy; the other choices select their matching sampler limits. Old Game
default, legacy 1x-enum and unsupported overrides normalize to 1x on startup and
when opening Graphics. Existing 2x/4x/8x/16x overrides retain their values. The
RelWithDebInfo build passes all twenty-three suites. A fresh-profile 55-second
Direct3D graphics audit verifies all five actual sampler limits, all display and
render presets, and both VSync presentation policies. This is renderer evidence,
not a visual gameplay or texture-quality comparison. Alpha 7 packages these numeric choices; Alpha 6 retains its previous labels.

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

## Native frame-clock module

The game clock initializer at `0x82248D38` now uses a portable typed C++ module
for recognized rates, with a separate big-endian guest-memory adapter and the
original translated entry retained for unknown modes. The native model also
handles live rate/period updates without enabling suspended clocks. An 18-case
comparison against the translated initializer checks the full clock object,
original step bits, timebase sampling, frequency truncation and calling convention.
The portable suite checks invalid inputs without mutation. See
[native code boundaries](NATIVE-CODE.md). This change does not establish corrected
animation/combat timing or improved frame rates.

Release and RelWithDebInfo pass nine native suites. An isolated 26-second Release
startup uses Direct3D 12, exercises the native initializer at 60 FPS, and continues
presenting until the harness stops its own child. That check uses a fresh profile
and does not establish gameplay correctness or a timing fix. The Release binary
passes the existing build-machine identifier scan.

The active development source and launcher use Direct3D 12. The Vulkan prototype
is parked locally after a system-freeze report; no Vulkan change is released.

## Native shape-bounds batch

Five geometry helpers now use portable typed sphere, capsule and box bounds
operations with a separate guest-memory adapter. The original routines remain
available for nonfinite/subnormal input and overlapping source/output ranges.
The native library uses strict FP arithmetic with contraction disabled, retaining
the original signed-zero, negative-dimension and directed-rounding behavior.
The equivalence suite compares 12,800 synthetic cases against the translated
entries and checks whole-buffer writes, FP controls, calling convention and
indirect registration. The portable geometry suite needs no runtime or game data.
See [native interfaces and routine map](NATIVE-CODE.md). These checks do not
establish a performance gain or full collision/mission coverage.

Both Windows configurations pass eleven native suites. An isolated 24-second
Release startup presents through Direct3D 12 without logged errors until its own
harness deadline; this does not exercise every shape during gameplay. Executable
and GPU plugin privacy scans pass.

## Alpha 5 package

Alpha 5 packages the portable importer and native frame-clock/shape-bounds work
on Windows x64 with Direct3D 12. Both Windows configurations pass eleven suites;
the portable component CI passes on Windows, Linux and Apple Silicon. Packaged
file hashes, embedded icon resources and binary privacy scans are checked before
publication. An isolated packaged startup check uses a fresh local profile and
the supported game data; it does not establish further mission coverage.
Higher-FPS animation timing remains unresolved. Vulkan is not packaged.

## Native vectors and cached channels

Seven more engine entries now use typed C++ vector accumulation and synchronized
three/four-component setters. The adapter preserves double input-scalar precision,
float32 intermediate rounding, cache-first writes and fallback for exceptional
values, aliases and unmasked FP exceptions. Vector/control types have no dependency
on guest registers or the graphics API. The routine map and editable interfaces
are in [native module notes](NATIVE-CODE.md).

The new equivalence suite passes 14,336 cases against the retained translations;
the portable tests exercise explicit vector edits and synchronized values. These
interfaces provide code-level control over verified operations; identification
of gameplay roles and in-mission coverage remain pending. No animation-speed or
performance improvement is claimed. Direct3D 12 remains the active backend.

Both Windows configurations pass thirteen suites. A separate 24-second Release
startup presents through Direct3D 12 without logged errors until the harness
stops its own child; it does not identify or exercise every converted routine's
gameplay role. Executable and GPU plugin privacy scans pass.

## Native countdown, arithmetic and bit-field batch

Five further entries now expose typed C++ for a frame countdown, scalar addition
and subtraction with explicit signed-zero flushing, a little-endian bit-window
reader, and ratio-based vector accumulation. The countdown preserves the original
pre-decrement progress numerator, signed comparisons and unsigned wraparound.
Subtraction retains early float32 rounding of its second operand. Unsupported
FP inputs, unmasked traps, ambiguous bit windows and vector aliases retain the
original path. The guest adapters preserve condition/carry flags and keep
addresses outside the portable models. See [routine contracts](NATIVE-CODE.md).

The new equivalence suite passes 20,560 cases against the retained translations,
including all rounding/flush modes, counter boundaries, scalar edge patterns,
bit offsets/widths and unmasked-exception fallback with exact arithmetic.
Portable tests cover explicit state/input edits and rejection without mutation.
Both Windows configurations pass fifteen suites. An isolated 24-second Release
startup presents through Direct3D 12 at requested 60 FPS without logged errors
until the harness stops its own child. Binary privacy scans pass. These checks
do not identify every caller's gameplay role, establish further mission coverage,
or correct accelerated animation. This batch is included in Alpha 6.

## Alpha 6 package

Alpha 6 packages the vector/cached-channel and countdown/scalar/bit-field batches,
bringing the typed native replacements to eighteen. Both Windows configurations
pass fifteen suites, and six portable component suites pass CI on Windows,
Linux and Apple Silicon. The playable package remains Windows x64 with Direct3D
12. Package hashes, embedded icon resources, source correspondence and privacy
checks are verified, followed by an isolated packaged startup with a fresh local
profile. The package contains no game files or profiles and requires the player's
own supported disc data. These checks do not extend the previously tested
opening-route gameplay coverage or resolve higher-FPS animation timing.

## Native transformed-box geometry

Two further entries now use a portable affine-transform/box model: world-space
box bounds at `0x823602C0` and signed-coordinate corner/sample transformation at
`0x823604B8`. Basis rows, translation and half-extents are explicit editable C++
inputs, separate from guest registers and field offsets. The model preserves
Z/Y/X sum order, float32 intermediate rounding and explicit double FMAs; it also
supports scale/shear and retains negative-extent arithmetic. Exceptional sources,
unsupported scaled samples, aliases, stack overlaps and unmasked traps retain
the original path. See [native geometry contracts](NATIVE-CODE.md).

Both Windows configurations pass seventeen suites. The new equivalence suite
passes 16,416 comparisons against the two retained translations across all
rounding/flush modes, including integer boundaries, FP edge patterns, aliases,
all nonvolatile registers and explicit native-path checks. Portable tests cover
editable geometry and rejection without output mutation. An isolated 24-second
Release startup presents through Direct3D 12 at requested 60 FPS without logged
errors; the harness stops only its own child. Binary privacy scans pass. This
brings the development build to twenty typed native entry points. Alpha 6 remains
the published release with eighteen. No further gameplay coverage, player-hitbox
identification, performance gain or animation-timing fix is established.

## Native capsule projection and box sphere radius

Two more entries now use typed portable geometry: oriented capsule bounds at
`0x82366808` and the box enclosing-sphere calculation at `0x823603B0`. Capsule
center, axis, radius and half-height are explicit inputs; endpoint bounds and
their union are available to native callers. Strict endpoint comparisons preserve
signed-zero ties. The box radius retains float32 square/FMA stages, a double
square root and the original final float32 store, including overflow/underflow.
No generic length rewrite or axis normalization changes the original behavior.
Adapters retain exceptional/alias/unmasked paths and preserve `f1`/condition flags.
See [projection contracts](NATIVE-CODE.md).

Both Windows configurations pass nineteen suites. The new equivalence suite
passes 16,416 comparisons against the retained translations, across all rounding
and starting flush modes. It checks signed-zero ties, arbitrary/edge values,
aliases, all nonvolatile registers, FP result/control bits and native-path use.
Portable fixtures exercise editable dimensions and radius precision. An isolated
24-second Release startup presents through Direct3D 12 at requested 60 FPS without
logged errors until its harness stops its own child. Binary privacy scans pass.
The development build now has twenty-two typed native entry points; Alpha 6
remains the published release with eighteen. These checks do not establish
player-hitbox roles, further mission coverage, improved performance or corrected
higher-FPS animation timing.

## Native fixed-block delay/mixing kernels

Entries `0x8253BED8` and `0x8253C240` now use a typed native delay-block model with
512/2048-sample histories. Three tap delays/gains, cached outputs and cursor are
explicit editable C++ state. The model preserves the original 256-sample block,
257-sample outputs, read-before-write order, rounded tap products, double FMA and
cached-output prefix. Unsupported cursors, exceptional sources, aliases and
unmasked traps retain the translations. Per-thread workspace avoids placing
large history snapshots on callback stacks. See [delay contracts](NATIVE-CODE.md).

Both Windows configurations pass twenty-one suites. The equivalence suite passes
5,024 block comparisons against the retained translations across all rounding
and starting flush modes, including eight-block chains through cursor wraparound.
It checks whole-buffer history/state/output bytes, native-path use, unsupported
cases, all nonvolatile registers, output registers/pointers and FP controls.
Portable tests exercise delay/gain edits, continuity and rejection without
mutation. An isolated 24-second Release startup presents through Direct3D 12 at
requested 60 FPS without logged errors; its harness stops only its own child.
Binary privacy scans pass. This brings development to twenty-four typed native
entry points; Alpha 6 remains the published release with eighteen. Effect routing,
listening comparisons, performance and broader gameplay coverage remain unvalidated.
No animation-timing change is made.

## Native feedback-delay kernel

Entry `0x8253C310` now uses typed portable feedback-delay state: delay, two gains,
cached output and cursor. The model preserves rounded double FMAs, in-place
256-input/257-output behavior, feedback history and cached-output continuity.
It stages a full block before committing, so a nonfinite trial can fall back
without partial native writes. Generated subnormals and directed-rounding finite
saturation are preserved. Other cursors, unsupported source values, aliases and
unmasked traps retain the translation. See [feedback contracts](NATIVE-CODE.md).

Both Windows configurations pass twenty-three suites. The new equivalence suite
passes 2,512 block comparisons, including eight-block chains, impulse decay,
positive/negative overflow, rollback, aliases and all rounding/starting flush
modes. It checks whole-buffer state/history/output bytes, native-path use, guest
calling convention, nonvolatile registers and FP controls. Portable tests cover
editable feedback state and rejection without mutation. An isolated 24-second
Release startup presents through Direct3D 12 at requested 60 FPS without logged
errors; its harness stops only its own child. Binary privacy scans pass. The
development build now has twenty-five typed native entry points; Alpha 6 remains
the published release with eighteen. Listening comparisons, effect-role tracing,
performance and broader gameplay coverage remain unvalidated. No animation-speed
change is made.

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


## Native 128-sample feedback history

Entry `0x8253BFA8` now shares the typed feedback state and transactional native
core with the 256-sample kernel. It processes 256 inputs through two passes over
128 history samples, retaining the updated first-pass history for the second
pass. The adapter preserves its distinct final input loads in `f1/f2`, unsigned
carry result, pointer advance and cursor reset. Unsupported cursors, exceptional
sources, aliases, guest range wrapping and unmasked traps retain the translation.
A failed trial restores FP state before fallback and publishes no partial writes.

Both Windows configurations pass all twenty-three suites. Feedback equivalence
now covers both entries in 5,024 block cases across every rounding/starting flush
mode, including eight-block chains, modulo boundaries, input/history aliases,
late overflow after history wrap, generated subnormals, unmasked exact cases,
registers/flags and indirect registration. Portable fixtures exercise both history
sizes, gain/delay edits, read-before-write behavior and rejection without mutation.
A fresh-profile 24-second Direct3D Release startup reaches menu installation and
frame pacing with no logged errors; only the harness's child is stopped. EXE and
Direct3D plugin privacy scans pass in both configurations. These are kernel and
startup checks, not new listening/gameplay coverage or a speedup measurement.
Development now has twenty-six typed native entry points; published Alpha 6
still contains eighteen. Vulkan remains parked.


## Alpha 7 distribution

Alpha 7 packages twenty-six typed native engine entries, numeric anisotropic
filtering choices and the existing embedded N3 icon. It remains Windows x64 with
Direct3D 12. The native core passes ten portable suites on Windows, Linux and
Apple Silicon at conversion commit `40ed8d2`; both Windows game configurations
pass twenty-three suites. The numeric filtering audit verifies actual sampler
limits from 1x through 16x, display/render dimensions and VSync presentation.
See the release notes for packaging checks and unchanged gameplay limits.

## Native 512-sample feedback history

Entry `0x8253C480` now uses the typed feedback model with a 512-sample history.
Each call processes 256 inputs, writes one contiguous half, preserves the other
half, and alternates the cursor between 0 and 256. The original call sites chain
separate state objects over shared sample buffers; the native interface keeps
delay, gains, history and cached output explicit without changing that routing.
The adapter preserves pointer/cursor results, `f1/f2`, carry, condition flags and
nonvolatile registers. Unsupported cursors and exceptional/alias/unmasked cases
retain the original translation. Trial rejection publishes no partial writes.

Both Windows configurations pass twenty-three suites. Feedback equivalence now
compares all three history lengths in 7,536 block cases across every rounding and
starting flush mode. The 512-sample cases cover both initial halves, eight-block
chains, modulo-delay boundaries, untouched history, source exceptions, aliases,
late-overflow rollback from either half, finite saturation and unmasked exact
arithmetic. Portable fixtures also check cursor alternation, cached-output
continuity and preservation of the untouched half. A fresh-profile 24-second
Direct3D Release startup reaches menu installation and frame pacing without
logged errors; its harness stops only its own child. Both EXE and Direct3D plugin
privacy scans pass. This brings development to twenty-seven typed native entries;
Alpha 7 contains twenty-six. These checks do not establish listening quality,
new gameplay coverage, a speedup or corrected higher-FPS animation timing.

## Native independent two-tap processing

Entry `0x8253C758` now uses `DualTapDelayState`: two independent delay/gain pairs,
previous outputs and a cursor over 1024 history samples. The core processes 256
inputs per call, reads both delayed values before writing input history, and
preserves the untouched three quarters of that history. The guest adapter keeps
the two original cursor forms, buffer advances, product registers, `f1/f2`,
condition/carry flags and nonvolatile state. Unsupported inputs retain the
translation without publishing a rejected native result.

Both Windows configurations pass twenty-three suites. Delay equivalence now
compares three entries in 7,536 block cases across all rounding/starting flush
modes, including eight-block chains, source exceptions, finite extremes,
unaligned fields, input/output/state/stack aliases, unmasked exact cases and
indirect registration. Portable fixtures check separate gain edits, modulo
delays, four-block wrap, old-history reads, output prefixes, untouched history
and rejection without mutation. A fresh-profile 24-second Direct3D Release
startup reaches menu installation and frame pacing without logged errors. Both
EXE and Direct3D plugin privacy scans pass. These are synthetic/startup checks;
listening validation and broader gameplay coverage remain pending.

Development now has twenty-eight typed native engine entry points. The
[conversion plan](CONVERSION-PLAN.md) records coverage and work toward replacing
the remaining generated game routines; entry counts are not a completion
percentage. Alpha 7 remains the published build with twenty-six entries.

## Native separate-output feedback processing

Entry `0x8253C5F0` now uses the typed 512-sample feedback model with an explicit
separate output and final-output reference. It preserves the original split:
the separate 256-sample output contains a cached prefix and 255 new results;
the last new result is written to the input's extra slot and cached in state.
The 256 input samples stay unchanged. Cursors alternate between 0 and 256,
and unsupported inputs retain the original translation. Rejected trials publish
no history, state or output changes, and the adapter restores FP state before
fallback. Original stack scratch is 40 bytes, distinct from the in-place kernels.

Both Windows configurations pass twenty-three suites. Feedback equivalence now
covers four entries in 10,048 block cases, including split-buffer aliases,
unchanged inputs, both history halves, chained continuity, late overflow,
generated subnormals, unmasked exact arithmetic and calling convention checks.
Portable fixtures verify output canaries and the final-output reference's alias
rules. Test temporary files are isolated under the workspace. A fresh-profile
24-second Direct3D Release startup reaches menu installation and frame pacing
without logged errors; EXE and Direct3D plugin privacy scans pass. Independent
source review found no contract mismatch. Listening quality, broader gameplay,
performance and higher-FPS animation timing remain unvalidated.

Development has twenty-nine typed native entry points. The full readable-C++
conversion remains in progress; Alpha 7 still packages twenty-six entries.

## Native table-derived limit and bounded value

Entries `0x8209DBF0` and `0x8209D718` now use explicit channel selection,
`ValueLimitParameters` and `BoundedValueState`. The first selects a signed table
value, applies the original rounded percentage/baseline arithmetic and adds an
unsigned offset with 64-bit carry. The second changes float field `+664` by a
signed delta and applies lower then upper comparisons, preserving negative-bound
behavior. Their surrounding gameplay class and channel meanings are not claimed.
Unknown table/index layouts and exceptional arithmetic retain the translations.

Both Windows configurations pass twenty-five suites. A new equivalence suite
compares 4,128 cases against both retained routines, including the retained nested
callee in the adjustment oracle. It checks whole sparse-memory fixtures, aliases,
unaligned fields, all rounding/starting flush modes, integer extremes, source
exceptions, unmasked exact cases, return/condition/carry state, preserved registers
and indirect registration. Of these fixtures, 3,001 take the native path.
An additional 2,064 executions test production composition, including 96 cases
where a retained outer caller uses a native nested limit calculation.
Portable tests cover editable limit inputs, 64-bit carry, explicit bound order,
signed zeros and rejection without mutation. A fresh-profile 24-second Direct3D
Release startup reaches menu installation and frame pacing without logged errors;
both EXE and Direct3D plugin privacy scans pass. These checks do not establish
new gameplay coverage, a performance gain or corrected high-FPS animation timing.

Development now has thirty-one typed native entry points. Alpha 7 remains the
published release with twenty-six. Full readable-C++ conversion remains active.

## Native parallel table values and modifier updates

Three more typed entries now use the native limit model. Entries `0x8209DCF0`
and `0x8209DDE8` share the explicit channel selection and rounded percentage
calculation with the existing limit model. They read different 48-byte record
tables and use percentage fields `+736` and `+740`, respectively. Both return
the zero-extended truncated result directly, without the additive offset used
by `0x8209DBF0`.

Entry `0x820A13B0` uses `ValueLimitState` to replace the percentage field `+728`
when the mode's low byte is nonzero, or the offset field `+732` otherwise. It
recomputes the limit and applies only an upper comparison to float field `+664`.
No delta addition or lower clamp occurs. The model preserves signed zero,
negative limits and the full 64-bit return carry. A rejected native trial
publishes neither modifier nor value changes. Guest-memory aliases require
the original modifier-write-before-source-read order or the retained translation.
Surrounding gameplay classes and individual channel meanings remain unclaimed.

Both Windows configurations pass all twenty-five suites. Limit equivalence now
compares five entries in 10,640 fixtures, including 7,483 native paths. Another
4,448 executions check production composition, including 432 retained outer
callers with native nested calculations. Checks cover all rounding/starting
flush modes, mode upper bytes, integer/float extremes, partial/full source
aliases, unaligned fields, return carry, getter register results, preserved
`f3/f4`, modifier `f13`, condition flags and FP controls. New prologue-alias
fixtures use initially uncommitted root/descriptor targets that become valid
after the original saved-LR write. Guards reject before following stale pointers.
Portable fixtures cover editable modifier state, upper-only comparisons,
negative zero, directed rounding and rejection without partial publication.
A fresh-profile 24-second Direct3D Release startup reaches menu installation and
frame pacing without logged errors. Both EXE and Direct3D plugin privacy scans
pass. These checks do not establish new gameplay coverage, speed improvements
or corrected higher-FPS animation timing.
Development now includes thirty-four typed native entry points. Alpha 7 still
packages twenty-six entries. Full game-code conversion remains in progress.
Generated game translations already compile as C++; this work exposes readable
typed state and behavior while Python build/release tooling remains intact.

## Native percentage updates and capped accumulation

Entry `0x8209D688` now computes an integer percentage of the table-derived limit
and applies that delta to the bounded value at `+664`. Both signed integers
round to float32 before two separately rounded multiplies. The limit is computed
again before the lower/upper comparisons. Its eleven visible callers include
fixed percentages and signed record values; their broader gameplay units still
need matched-scene identification.

Entry `0x820B4DC8` uses the parallel table getter and optionally adds a percentage
when byte `+1336` or `+719` is nonzero, using signed percentage `+1376`. The base
and truncated increment bits are zero-extended before a 64-bit addition, retaining
carry even for negative increments. The inactive branch still calculates the
base while skipping unused percentage arithmetic. Original data contains function
pointers to this entry; its indirect use has not been observed at runtime.

The two hooks exclude all nested scratch (256 and 128 bytes respectively), keep
`f1` through `f4`, and restore controls before rejected trials. A retained outer
call keeps its entire original nested chain: scratch writes can change later
flags or percentage reads outside a leaf getter's object range. Tests compare
all bytes and all GPR/FPR results for these fallback calls without normalizing
scratch, then verify a later ordinary call still selects native code.

Entry `0x8209D7A8` now exposes scaled accumulation and optional accounting as typed
state. It adds the converted increment to field `+680` with 32-bit wrapping, then
caps its signed result at 999999 without a lower clamp. An enabled owner updates
the total/count at global object offsets `+4712` and `+4708`; stored words wrap
while register results preserve 64-bit carries. Aliases that would change a
later source retain the original; aliases with already-consumed sources remain
supported. No score, currency or experience role is assumed from this arithmetic.

Both Windows configurations pass all twenty-eight suites. Percentage equivalence
compares 8,352 cases (4,783 native, 3,569 complete retained chains), plus sixteen
post-fallback native probes. Accumulator equivalence compares 2,064 cases (1,152
native) and every GPR/FPR result. They cover all rounding/starting flush modes,
conversion boundaries, negative values, carry, flags, unaligned fields, source
and scratch aliases, exceptional trials and exact unmasked calls. Portable
fixtures include a two-rounding counterexample, editable percentage/cap inputs,
inactive unused sources, accounting wrap and rejection without publication.
The existing limit comparisons also pass after the shared truncation refactor.
A fresh-profile 24-second Direct3D Release startup reaches menu installation and
frame pacing without logged errors; both Windows EXEs and Direct3D plugins pass
binary privacy scans. These checks do not add gameplay coverage, establish a
speedup or correct higher-FPS animation timing.

Development now has thirty-seven typed native entries. Alpha 7 remains the
published build with twenty-six; the full game-code conversion is ongoing.

## Native handle and payload interfaces

Five connected entries now have typed implementations, bringing development to
forty-two distinct addresses verified against the source hooks. Alpha 7 remains
the published build with twenty-six entries, using Direct3D 12.

Entries `0x82081138` and `0x82080780` expose bounded 12-byte handle records,
packed-key/index extraction and full-word key matching. The index uses the key's
sign-extended low 16 bits before an unsigned bounds comparison. The guest
adapters retain unknown table geometry and invalid cases, and account for the
validator's original 112-byte frame plus its separate positive argument homes.

Entry `0x8209D358` copies a typed 100-byte payload with explicit integer and
float groups. Normal floats and signed zeros use native copies; exceptional
transfers, unmasked traps and overlapping guest fields retain the ordered
translation. Entry `0x8209D280` installs that payload at `+336`, optionally sets
the enable word at `+412` and vector at `+420`, and conditionally multiplies words
`+356/+360`. Multiplication requires signed own kind at least 6, a matching handle,
related kind 1000 and variant other than `0xFFFFFFFF`. The low product is stored;
the full signed product and branch-dependent return registers are retained.

Entry `0x8209DEE0` selects category 2 when a matching related object has kind 6
or 1000; otherwise it keeps category byte `+969`. It reads the 48-byte table at
descriptor offset `+216`, uses signed percentage `+784`, and returns truncated
bits without an additive offset. Both outer entries exclude all 240 bytes of
nested stack writes and preserve complete retained chains for source/write aliases.

Both Windows configurations pass thirty-four suites. Handle equivalence compares
3,584 calls over all rounding, flush and exception-mask combinations, including
1,024 native packed validations and 768 retained wrapper chains. Payload-copy
equivalence compares 2,064 cases (1,344 native). The two outer state operations
compare 4,896 cases (3,312 native), with explicit coverage of signed products
wider than 32 bits, sentinel skips, optional vectors, stale keys, category changes,
unaligned fields and ordered source/descriptor/record/stack aliases. All compare
whole fixtures; native-only normalization removes untouched original scratch.
Retained calls compare every byte and scalar register. The installer also checks
all native scalar register outputs; the related table calculation checks its
return and index/base registers, `f0/f13`, `f1` through `f4` and nonvolatile state.
Upper pointer halves, condition/carry flags, LR and FP controls are covered.
Read-only guard fixtures use uncommitted old pointers to require rejection before
traversal. Subsequent native calls verify retained scopes are released.

Portable fixtures exercise editable typed records, source/optional float
rejection, signed index/key boundaries, multiplier gates and transactional
publication. A fresh-profile 24-second Direct3D Release startup reaches menu
installation and frame pacing without logged errors. Both Windows EXEs and
Direct3D plugins pass binary privacy scans. No new gameplay coverage, performance
result or full-platform support is claimed. Broader progression and callback-driven state remains
translated until explicit ownership, side-effect and callback interfaces are
established. The full readable-C++ game-code goal remains active; Python
build/release tooling is unchanged and Vulkan remains parked.
