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
- Five JavaScript extractor tests and six native regression suites pass.
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
| `0x821A8030` | Configure native arrow/text selectors for FPS and filtering |
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

The main-menu Graphics entry opens its separate native page. Both controls now
respond to navigation after focus is assigned at transition completion and
hidden controls are disabled. The final layout keeps selected labels on one
line. Live switching to 40 and 60 FPS works, and 16x filtering persists as SDK
enum 5 in the local profile. These are tested menu/settings outcomes, not
full-game rendering or animation validation.

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
