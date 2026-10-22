## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.11/N3-Recompiled-v0.1.0-alpha.11-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files, saves, profiles or developer logs are bundled. Existing local
setup and saves are reused.

### Native deterministic chance decisions

Alpha 11 converts `0x8209E230` to a typed C++ operation with explicit random
sequence state, editable recurrence parameters and signed normal/alternate
thresholds. Its six translated call sites now use that native decision.
The default path preserves the game's shared seed, modulo-100 samples and one
sequence advance per call, including guaranteed outcomes. Argument-home writes,
threshold selection and unsupported-layout fallback match the retained code.
This is code-level control; no player-facing chance setting is added.

The project now has **46 typed native entry points**, including **44 with
guest-memory fixtures** and two scalar-only operations. Other inlined random
draws and the surrounding gameplay callers remain translated. Full game-code
conversion continues in the
[conversion plan](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/blob/v0.1.0-alpha.11/docs/CONVERSION-PLAN.md).

### Validation and limits

Both current Windows builds pass **42 suites**. The new comparison checks
**4,096 calls** (3,072 native and 1,024 retained), **1,024 chained replay calls**
and **16 inaccessible unused-threshold cases** against the actual original.
It compares whole fixture memory and every register/control result without
normalizing scratch. High addresses, offsets above 4 GiB, aliases, wrap,
threshold extremes and independent FP controls are covered. Portable fixtures
check 20,000 threshold combinations and a 100,000-roll replay.
**Nineteen portable component suites pass on Windows, Linux and Apple Silicon**;
the playable game remains **Windows x64 with Direct3D 12**.

One broader run reported an intermittent mismatch in the older direction-basis
comparison. It has not repeated in subsequent builds, 120 process replays or
runs pinned to each available logical processor. Diagnostic output now records
the first differing guest byte. Its cause remains open; this release does not
claim a direction-code fix. See the
[validation record](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/blob/v0.1.0-alpha.11/docs/STATUS.md).

Package hashes, embedded icon, corresponding sources and private-data exclusions
are checked. The extracted importer and fresh-profile Direct3D startup are
verified. These are synthetic/startup checks, with no new gameplay or performance
claim. Later missions/characters, a full playthrough and other PCs remain
unvalidated. 40/60 FPS remain experimental; animations can appear accelerated.
Vulkan remains parked.

### Updating

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which contains saves, settings, imported data and the remembered game path.
Update desktop shortcuts if the application folder changes; shortcuts made
from `n3.exe` inherit its embedded icon. No Python, Node.js or build tools are
needed to play. Live frame-rate, filtering, resolution and VSync controls remain.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.11/ReXGlue-bd833a2-corresponding-source.zip)
are supplied for rebuilding and licensing; **they are not needed to play**.
The project's native modules are in the matching source tag.
[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.11/SHA256SUMS.txt)
cover both ZIPs. Third-party notices are included in the Windows package.
