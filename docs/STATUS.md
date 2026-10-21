# Development status

Recorded 21 October 2026. Initial gameplay is reached; full-game coverage remains
unvalidated.

## Verified

- XDVDFS game partition found at byte offset `0x0FD90000` in the supplied disc.
- Extracted 3,342 files / 5,770,038,003 bytes, with per-file SHA-256 hashes.
- Identified title `4D5307DB`, media `5D48689C`, executable version `0.0.0.3`.
- Strict ReXGlue analysis/codegen succeeds without `--force`.
- Generated 13,762 function assignments across 87 translated C++ source files.
- Windows x64 RelWithDebInfo compile and link succeeds with portable Clang 20.1.8.
- Runtime initializes Direct3D 12, input, audio, and XEX imports.
- Five extractor tests and the callback floating-point regression test pass.
- A 30-second boot test survived until the harness stopped it, without the prior
  unregistered-function or floating-point failure.
- Windowed startup reaches the intro/title-screen presentation.
- Main menu and character selection reached, followed by the opening cutscene.
- Replay progresses past the previous opening-cutscene crash and reaches gameplay.
- Desktop shortcut follows the stable launcher, using a 1280 x 720 window.

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

Windows FP fault diagnostics write module offsets and a stack to
`logs/floating-point-fault.txt`; `tools/symbolize-fault.ps1` resolves executable
frames against the locally built PDB.

## Opening cutscene

The first cutscene run exhibited presentation glitches and terminated at an
unregistered indirect target, `0x8215B150`. The target is now included in strict
function discovery, emitted as an original-code translation, and present in both
generated function registration tables. The native rebuild and callback FP test
pass. Replay progresses past the previous failure point and reaches gameplay.
This validates the correction on the opening route; it does not establish broader
cutscene coverage or resolve the earlier presentation glitches.

## Next validation work

1. Validate sustained gameplay, input responsiveness, audio, and mission completion.
2. Exercise rendering and combat, including effects and large enemy counts.
3. Verify save/load in a disposable profile, then repeat across restarts.
4. Compare scenes and timing against console or emulator reference captures.
5. Investigate additional indirect targets or translation problems from logs.

Other game revisions, title updates, platforms, and full-game playthroughs have
not been validated. Keep performance and visual claims tied to measured scenes.
