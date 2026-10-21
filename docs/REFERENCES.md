# Reference projects and design notes

Reviewed 21 October 2026. These are reference projects, not game-specific code
dependencies. No Lost Odyssey or Sonic patches or addresses have been transplanted.

## Lost Odyssey Recomp

- [Repository](https://github.com/freefrank/LostOdysseyRecomp)
- [Build workflow](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/BUILDING.md)
- [Recompilation notes](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/notes/recomp.md)

Useful practices: keep original game files and generated translations local;
record manual function/switch corrections; diagnose runtime-only indirect targets;
separate build success from scene correctness; isolate profiles for regression
runs. Its notes specifically discuss functions referenced through pointers that
static analysis misses. That is directly relevant to the N3 startup traps.

Its custom runtime, shader pipeline, importer, configuration, saves, and packaging
are useful longer-term references. N3 currently uses the reusable ReXGlue runtime
to establish a first boot before considering subsystem replacements.

## Unleashed Recompiled

- [Repository and port features](https://github.com/hedge-dev/UnleashedRecomp)
- [Build structure](https://github.com/hedge-dev/UnleashedRecomp/blob/main/CMakeLists.txt)
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp)
- [XenosRecomp](https://github.com/hedge-dev/XenosRecomp)

Useful practices: build translated game code as its own target, keep the host
runtime distinct, supply an asset importer, and develop display/input/settings
features alongside a validated rendering pipeline. Native shader recompilation
is a possible future direction; N3 presently uses ReXGlue's Xenos GPU plugin.

## Current SDK

[ReXGlue](https://github.com/rexglue/rexglue-sdk), pinned to commit
`bd833a2ab69d305f99ddf57418fc38ffec8af93d`, provides the analyser, C++ generator,
runtime kernel, and GPU/audio/input integration. Its bundled CLI differs from
older wiki examples: this revision uses `n3_manifest.toml` with `[project]` and
`[entrypoint]` sections. The local scripts follow the installed CLI/parser.
