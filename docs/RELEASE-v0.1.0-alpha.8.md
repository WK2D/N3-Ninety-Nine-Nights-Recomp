## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.8/N3-Recompiled-v0.1.0-alpha.8-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files, saves, profiles, local paths or developer logs are bundled.
Existing local setup and saves are reused.

### More readable C++ game and engine behavior

Alpha 8 adds **19 typed native entry points**, bringing the total to **45**:

- Additional delay/feedback processing with explicit gains, histories and outputs.
- Table-derived limits, parallel and related table values, bounded changes,
  percentage updates and modifier replacement.
- Capped integer accumulation with optional total/count accounting.
- Bounded handle records, packed-key validation, typed payload transfer and
  related-object payload installation.
- Staged direction normalization, conditional direction accumulation and
  direction-basis composition from signed input axes.

The new interfaces expose verified state and arithmetic independently of Xbox
registers and memory layouts. Unsupported cases retain the original translations.
Generated game translations already compile as C++; this work makes their
behavior readable and editable. Build/release tooling remains unchanged apart
from the release version. These changes add no new gameplay mod controls.

The three direction adapters also preserve the pinned runtime's physical-memory
shadow mapping. Forty earlier memory-backed entries still need equivalent
high-address coverage; their existing comparisons exercise virtual memory.
The [conversion plan](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/blob/v0.1.0-alpha.8/docs/CONVERSION-PLAN.md)
records that remaining work without treating function counts as a completion percentage.

### Graphics and updating

The release retains the embedded N3 icon and live Graphics controls: **30/40/60
FPS**, **1x/2x/4x/8x/16x** filtering, display sizes through **4K**, internal
**720p/1440p/4K** rendering and **VSync On/Off**. Settings save locally and apply
without a restart. **The playable release is Windows x64 with Direct3D 12.**

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which holds saves, settings, imported data and the remembered game path. Update
desktop shortcuts if the application folder changes; shortcuts made from
`n3.exe` inherit its embedded icon. No Python, Node.js or build tools are needed
to play. Windows 10/11 x64, Direct3D 12 and an Xbox-compatible controller remain
the target.

### Validation and limits

Both Windows configurations pass **40 test suites**. The direction suites compare
**12,384 calls** against the retained routines, plus **384** retained outer calls
using the production normalizer. They cover all scalar/vector registers, every
fixture byte including scratch and argument homes, rounding/flush modes,
unaligned fields, aliases, wrapping slots, high-memory canaries and fallback.
Table, handle, payload, accounting, geometry and audio-kernel comparisons also pass.
**Eighteen portable component suites pass on Windows, Linux and Apple Silicon**;
this does not establish a playable Mac or Linux build.

The Windows ZIP, embedded icon, corresponding-source hashes and private-data
exclusions are checked. A fresh-profile startup check runs the packaged
application with Direct3D 12. Startup and synthetic checks do not establish
additional gameplay coverage or a performance gain.

**40/60 FPS remain experimental:** animations can appear accelerated. Full-game
coverage, later characters/missions, listening validation of converted audio
kernels and other PCs remain unvalidated. The opening mission, following
cutscene, saving and loading were previously exercised. Vulkan remains parked.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.8/ReXGlue-bd833a2-corresponding-source.zip)
are provided for rebuilding and licensing; **they are not needed to play**.
The native modules are in this project's matching source tag.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.8/SHA256SUMS.txt)
cover both ZIPs. The Windows package includes third-party notices.
