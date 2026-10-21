# Development status

Recorded 21 October 2026. This is an initial boot milestone, not a finished port.

## Verified

- XDVDFS game partition found at byte offset `0x0FD90000` in the supplied disc.
- Extracted 3,342 files / 5,770,038,003 bytes, with per-file SHA-256 hashes.
- Identified title `4D5307DB`, media `5D48689C`, executable version `0.0.0.3`.
- Strict ReXGlue analysis/codegen succeeds without `--force`.
- Generated 13,761 function assignments across 87 translated C++ source files.
- Windows x64 RelWithDebInfo compile and link succeeds with portable Clang 20.1.8.
- Runtime initializes Direct3D 12, input, audio, and XEX imports.
- Five extractor tests and the callback floating-point regression test pass.
- A 30-second boot test survived until the harness stopped it, without the prior
  unregistered-function or floating-point failure.
- Windowed startup reaches the intro/title-screen presentation.
- Desktop shortcut follows the stable launcher, using a 1280 x 720 window.

## Corrections made during first boot

| Guest entry | Evidence | Treatment |
| --- | --- | --- |
| `0x8253F950` | Direct branch at `0x8254698C` in an object-adjustment thunk | Seed function discovery; generated body ends at `0x8253FA18` |
| `0x82588FF8` | Startup indirect-call trap | Seed discovery; generated body is a small tail-call initializer |
| `0x8253ED08` | Indirect-call trap during audio initialization | Seed discovery; preserve generated implementation |

The SDK's audio worker invokes guest callback `0x8253A590`. A first-chance fault
trace found `0xC000008F` with host MXCSR `0x8060` while executing the generated
`vcfsx` path in `sub_8254C600`. Host FP exception masks were clear. The local
callback wrapper initializes the host mask policy while preserving the guest's
rounding and flush bits, then calls the original generated callback. The next
30-second boot survived, and the windowed visual check reached intro/title.

Windows FP fault diagnostics write module offsets and a stack to
`logs/floating-point-fault.txt`; `tools/symbolize-fault.ps1` resolves executable
frames against the locally built PDB.

## Next validation work

1. Navigate menus, start a mission, and verify controller input and in-game audio.
2. Exercise rendering and combat, including effects and large enemy counts.
3. Verify save/load in a disposable profile, then repeat across restarts.
4. Compare scenes and timing against console or emulator reference captures.
5. Investigate additional indirect targets or translation problems from logs.

Other game revisions, title updates, platforms, and full-game playthroughs have
not been validated. Keep performance and visual claims tied to measured scenes.
